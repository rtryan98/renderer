#include "renderer/renderer.hpp"

#include "renderer/scene/scene.hpp"
#include "renderer/resource_state_tracker.hpp"
#include "shared/camera_shared_types.h"
#include "renderer/gpu_transfer.hpp"
#include "renderer/asset/asset_repository.hpp"

#include <rhi/swapchain.hpp>
#include <shared/tonemap_shared_types.h>

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
    Render_Resource_Blackboard& resource_blackboard,
    const Imgui_Renderer_Create_Info& imgui_renderer_create_info)
    : m_gpu_transfer_context(gpu_transfer_context)
    , m_swapchain(swapchain)
    , m_asset_repository(asset_repository)
    , m_resource_blackboard(resource_blackboard)
    , m_fly_cam{
        .fov_y = 75.f,
        .aspect = calculate_aspect_ratio(m_swapchain),
        .near_plane = .01f,
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
    , m_imgui_renderer(imgui_renderer_create_info, m_asset_repository)
    // , m_ocean_renderer(m_resource_blackboard, m_asset_repository)
    , m_g_buffer(
        m_asset_repository,
        m_resource_blackboard,
        m_swapchain.get_width(),
        m_swapchain.get_height())
    , m_ocean(
        m_asset_repository,
        m_gpu_transfer_context,
        m_resource_blackboard,
        m_swapchain.get_width(),
        m_swapchain.get_height())
{
    m_imgui_renderer.create_fonts_texture();

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
}

void Renderer::update(const Input_State& input_state, double t, double dt) noexcept
{
    if (!(ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard))
    {
        // No input captured here from UI
        m_fly_cam.process_inputs(input_state, static_cast<float>(dt));
    }
    // set aspect ratio in case of resize
    m_fly_cam.aspect = calculate_aspect_ratio(m_swapchain);
    m_fly_cam.update();
}

void Renderer::setup_frame()
{
    m_imgui_renderer.next_frame();
    m_swapchain_image = Image(m_swapchain);

    GPU_Camera_Data camera_data = {
        .view = m_fly_cam.camera_data.view,
        .proj = m_fly_cam.camera_data.proj,
        .view_proj = m_fly_cam.camera_data.view_proj,
        .position = m_fly_cam.camera_data.position
    };
    m_gpu_transfer_context.enqueue_immediate_upload(m_camera_buffer, camera_data);
}

void Renderer::render(
    const Static_Scene_Data& scene,
    rhi::Command_List* cmd,
    const double t,
    const double dt) noexcept
{
    Resource_State_Tracker tracker;

    m_ocean.simulate(
        cmd,
        tracker,
        static_cast<float>(dt));
    m_g_buffer.render_scene_cpu(
        cmd,
        tracker,
        m_camera_buffer,
        scene);
    m_g_buffer.resolve(
        cmd,
        tracker,
        m_shaded_geometry_render_target);
    m_ocean.opaque_forward_pass(
        cmd,
        tracker,
        m_camera_buffer,
        m_shaded_geometry_render_target,
        m_resource_blackboard.get_image(techniques::G_Buffer::DEPTH_BUFFER_NAME));
    render_swapchain_pass(cmd, tracker);
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

void Renderer::render_swapchain_pass(rhi::Command_List* cmd, Resource_State_Tracker& tracker)
{
    cmd->begin_debug_region("swapchain_pass", 0.5f, 0.25f, 0.25f);
    tracker.use_resource(
        m_swapchain_image,
        rhi::Barrier_Pipeline_Stage::Color_Attachment_Output,
        rhi::Barrier_Access::Color_Attachment_Write,
        rhi::Barrier_Image_Layout::Color_Attachment,
        true);
    tracker.use_resource(
        m_shaded_geometry_render_target,
        rhi::Barrier_Pipeline_Stage::Pixel_Shader,
        rhi::Barrier_Access::Shader_Sampled_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.flush_barriers(cmd);
    rhi::Render_Pass_Color_Attachment_Info swapchain_attachment_info = {
        .attachment = m_swapchain_image,
        .load_op = rhi::Render_Pass_Attachment_Load_Op::Clear,
        .store_op = rhi::Render_Pass_Attachment_Store_Op::Store,
        .clear_value = {
            .color = { 0.0f, 0.0f, 0.0f, 1.0f }
        }
    };
    const rhi::Render_Pass_Begin_Info render_pass_info = {
        .color_attachments = { &swapchain_attachment_info, 1 },
        .depth_stencil_attachment = {}
    };

    cmd->begin_render_pass(render_pass_info);
    cmd->set_viewport(0.f, 0.f, static_cast<float>(m_swapchain.get_width()), static_cast<float>(m_swapchain.get_height()), 0.f, 1.f);
    cmd->set_scissor(0, 0, m_swapchain.get_width(), m_swapchain.get_height());

    cmd->begin_debug_region("swapchain_pass:tonemap", .75f, 0.f, .25f);
    cmd->set_pipeline(m_asset_repository.get_graphics_pipeline("tonemap"));
    cmd->set_push_constants<Tonemap_Push_Constants>({
            .source_texture = m_shaded_geometry_render_target,
            .texture_sampler = m_resource_blackboard.get_sampler({
                .filter_min = rhi::Sampler_Filter::Nearest,
                .filter_mag = rhi::Sampler_Filter::Nearest,
                .filter_mip = rhi::Sampler_Filter::Nearest,
                .address_mode_u = rhi::Image_Sample_Address_Mode::Clamp,
                .address_mode_v = rhi::Image_Sample_Address_Mode::Clamp,
                .address_mode_w = rhi::Image_Sample_Address_Mode::Clamp,
                .mip_lod_bias = 0.f,
                .max_anisotropy = 0,
                .comparison_func = rhi::Comparison_Func::None,
                .reduction = rhi::Sampler_Reduction_Type::Standard,
                .border_color = {},
                .min_lod = .0f,
                .max_lod = .0f,
                .anisotropy_enable = false})
        }, rhi::Pipeline_Bind_Point::Graphics);
    cmd->draw(3, 1, 0, 0);
    cmd->end_debug_region();

    cmd->begin_debug_region("swapchain_pass:imgui", .5f, 1.f, .0f);
    m_imgui_renderer.render(cmd);
    cmd->end_debug_region(); // imgui
    cmd->end_render_pass();

    tracker.use_resource(
        m_swapchain_image,
        rhi::Barrier_Pipeline_Stage::None,
        rhi::Barrier_Access::None,
        rhi::Barrier_Image_Layout::Present);
    tracker.flush_barriers(cmd);
    cmd->end_debug_region(); // swapchain_pass
}
}
