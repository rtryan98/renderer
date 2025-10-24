#include "renderer/renderer.hpp"

#include "renderer/scene/scene.hpp"
#include "renderer/resource_state_tracker.hpp"
#include "shared/camera_shared_types.h"
#include "renderer/gpu_transfer.hpp"
#include "renderer/asset/asset_repository.hpp"

#include <rhi/swapchain.hpp>
#include <imgui.h>

#undef near
#undef far

namespace ren
{
constexpr static auto SHADED_GEOMETRY_RENDER_TARGET_NAME = "shaded_geometry_render_target";

float calculate_aspect_ratio(const rhi::Swapchain& swapchain)
{
    return static_cast<float>(swapchain.get_width()) / static_cast<float>(swapchain.get_height());
}

Renderer::Renderer(GPU_Transfer_Context& gpu_transfer_context,
    rhi::Swapchain& swapchain,
    Asset_Repository& asset_repository,
    Render_Resource_Blackboard& resource_blackboard)
    : m_gpu_transfer_context(gpu_transfer_context)
    , m_swapchain(swapchain)
    , m_asset_repository(asset_repository)
    , m_resource_blackboard(resource_blackboard)
    , m_fly_cam{
        .fov_y = 75.f,
        .aspect = calculate_aspect_ratio(m_swapchain),
        .near_plane = .01f,
        .far_plane = 500.f,
        .sensitivity = .25f,
        .movement_speed = 10.f,
        .pitch = 0.f,
        .yaw = 0.f,
        .position = { .0f, .0f, .5f }
    }
    , m_camera_buffer(m_resource_blackboard.create_buffer("Camera Buffer", {
        .size = sizeof(GPU_Camera_Data),
        .heap = rhi::Memory_Heap_Type::GPU
        }))
    , m_g_buffer(
        m_asset_repository,
        m_resource_blackboard,
        m_swapchain.get_width(),
        m_swapchain.get_height())
    , m_image_based_lighting(
        m_asset_repository,
        m_gpu_transfer_context,
        m_resource_blackboard)
    , m_imgui(
        m_asset_repository,
        m_gpu_transfer_context,
        m_resource_blackboard)
    , m_ocean(
        m_asset_repository,
        m_gpu_transfer_context,
        m_resource_blackboard,
        m_swapchain.get_width(),
        m_swapchain.get_height())
    , m_tone_map(
        m_asset_repository,
        m_gpu_transfer_context,
        m_resource_blackboard,
        false,
        techniques::Tone_Map::SDR_DEFAULT_PAPER_WHITE)
{
    const rhi::Image_Create_Info render_target_create_info = {
        .format = rhi::Image_Format::R16G16B16A16_SFLOAT,
        .width = m_swapchain.get_width(),
        .height = m_swapchain.get_height(),
        .depth = 1,
        .array_size = 1,
        .mip_levels = 1,
        .usage = rhi::Image_Usage::Color_Attachment
            | rhi::Image_Usage::Sampled
            | rhi::Image_Usage::Unordered_Access,
        .primary_view_type = rhi::Image_View_Type::Texture_2D
    };
    m_shaded_geometry_render_target = m_resource_blackboard.create_image(
        SHADED_GEOMETRY_RENDER_TARGET_NAME,
        render_target_create_info);
}

Renderer::~Renderer()
{
    m_resource_blackboard.destroy_image(m_shaded_geometry_render_target);
}

void Renderer::process_gui()
{
    m_ocean.process_gui();
    m_tone_map.process_gui();
    debug_gui();
}

void Renderer::update(const Input_State& input_state, double t, double dt) noexcept
{
    // set aspect ratio in case of resize
    m_fly_cam.aspect = calculate_aspect_ratio(m_swapchain);
    m_fly_cam.update();

    if (!m_cull_cam_locked)
    {
        m_cull_cam = m_fly_cam;
    }

    switch (m_benchmark_mode)
    {
    case ren::Benchmark_Mode::None:
    {
        if (!(ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard))
        {
            // No input captured here from UI
            m_fly_cam.process_inputs(input_state, static_cast<float>(dt));
        }
        m_ocean.update(static_cast<float>(dt), m_cull_cam);
        break;
    }
    case ren::Benchmark_Mode::Ocean:
    {
        {
            auto ocean_options = m_ocean.options;
            auto ocean_simulation_data = m_ocean.simulation_data;

            m_ocean.options.update_time = false;
            m_ocean.simulation_data.total_time = 0.0f;
            m_ocean.simulation_data.full_spectrum_parameters.single_spectrum_parameters[0].wind_speed = 7.5f;
            m_ocean.simulation_data.full_spectrum_parameters.single_spectrum_parameters[1].wind_speed = 15.0f;

            m_ocean.update(static_cast<float>(dt), m_cull_cam);

            m_ocean.options = ocean_options;
            m_ocean.simulation_data = ocean_simulation_data;
        }
        {
            m_fly_cam.position = glm::vec3(0.0f, -250.0f, 7.5f);
            m_fly_cam.pitch = -9.75f;
            m_fly_cam.yaw = 0.0f;
        }

        break;
    }
    default:
        break;
    }

    // upload camera data at the end, in case a benchmark changes it
    GPU_Camera_Data camera_data = {
        .world_to_camera = m_fly_cam.camera_data.world_to_camera,
        .camera_to_clip = m_fly_cam.camera_data.camera_to_clip,
        .world_to_clip = m_fly_cam.camera_data.world_to_clip,
        .clip_to_camera = m_fly_cam.camera_data.clip_to_camera,
        .camera_to_world = m_fly_cam.camera_data.camera_to_world,
        .clip_to_world = m_fly_cam.camera_data.clip_to_world,
        .position = m_fly_cam.camera_data.position,
        .near_plane = m_fly_cam.near_plane,
        .far_plane = m_fly_cam.far_plane,
    };
    m_gpu_transfer_context.enqueue_immediate_upload(m_camera_buffer, camera_data);
}

void Renderer::setup_frame()
{
    m_swapchain_image = Image(m_swapchain);
}

void Renderer::render(
    const Static_Scene_Data& scene,
    rhi::Command_List* cmd,
    const double t,
    const double dt) noexcept
{
    Resource_State_Tracker tracker;

    m_image_based_lighting.bake(
        cmd,
        tracker);
    m_ocean.simulate(
        cmd,
        tracker);
    m_g_buffer.render_scene_cpu(
        cmd,
        tracker,
        m_camera_buffer,
        scene);
    m_g_buffer.resolve(
        cmd,
        tracker,
        m_camera_buffer,
        m_shaded_geometry_render_target);
    m_image_based_lighting.skybox_render(
        cmd,
        tracker,
        m_camera_buffer,
        m_shaded_geometry_render_target,
        m_resource_blackboard.get_image(techniques::G_Buffer::DEPTH_BUFFER_NAME));
    m_ocean.depth_pre_pass(
        cmd,
        tracker,
        m_camera_buffer,
        m_resource_blackboard.get_image(techniques::G_Buffer::DEPTH_BUFFER_NAME));
    m_ocean.opaque_forward_pass(
        cmd,
        tracker,
        m_camera_buffer,
        m_shaded_geometry_render_target,
        m_resource_blackboard.get_image(techniques::G_Buffer::DEPTH_BUFFER_NAME));
    m_tone_map.render_debug(
        cmd,
        tracker,
        m_shaded_geometry_render_target,
        m_camera_buffer);
    m_tone_map.blit_apply(
        cmd,
        tracker,
        m_shaded_geometry_render_target,
        m_swapchain_image);
    m_imgui.render(cmd, m_swapchain_image);
    tracker.use_resource(
        m_swapchain_image,
        rhi::Barrier_Pipeline_Stage::None,
        rhi::Barrier_Access::None,
        rhi::Barrier_Image_Layout::Present);
    tracker.flush_barriers(cmd);
}

void Renderer::on_resize(uint32_t width, uint32_t height) noexcept
{
    auto resize_target = [width, height, this](Image& image)
    {
        auto create_info = image.get_create_info();
        create_info.width = width;
        create_info.height = height;
        image.recreate(create_info);
    };
    // TODO: resize techniques
}

void Renderer::set_hdr_state(const bool enabled, const float display_peak_luminance_nits) noexcept
{
    m_enable_hdr = enabled;
    m_tone_map.set_hdr_state(m_enable_hdr, display_peak_luminance_nits);
}

void Renderer::debug_gui()
{
    ImGui::Checkbox("Lock cull camera", &m_cull_cam_locked);
}
}
