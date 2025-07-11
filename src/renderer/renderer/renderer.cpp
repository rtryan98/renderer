#include "renderer/renderer.hpp"

#include "renderer/application.hpp"
#include "renderer/window.hpp"
#include "renderer/input_codes.hpp"

#include "shared/draw_shared_types.h"
#include "shared/camera_shared_types.h"

#undef near
#undef far

namespace ren
{
float calculate_aspect_ratio(const Window& window)
{
    auto& window_data = window.get_window_data();
    return float(window_data.width) / float(window_data.height);
}

Renderer::Renderer(Application& app, rhi::Swapchain& swapchain,
    Render_Resource_Blackboard& resource_blackboard,
    const Imgui_Renderer_Create_Info& imgui_renderer_create_info)
    : m_app(app)
    , m_swapchain(swapchain)
    , m_resource_blackboard(resource_blackboard)
    , m_fly_cam{
        .fov_y = 90.f,
        .aspect = calculate_aspect_ratio(app.get_window()),
        .near = .01f,
        .far = 1000.f,
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
    , m_imgui_renderer(imgui_renderer_create_info, m_app.get_asset_repository())
    , m_ocean_renderer(m_resource_blackboard, m_app.get_asset_repository())
    , m_should_display_overlay(false)
{
    init_rendertargets();
    m_imgui_renderer.create_fonts_texture();
}

std::vector<Settings_Base*> Renderer::get_settings() noexcept
{
    std::vector<Settings_Base*> result;
    result.push_back(m_ocean_renderer.get_settings());
    return result;
}

void Renderer::update(const Input_State& input_state, double t, double dt) noexcept
{
    if (!(ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard))
    {
        // No input captured here from UI

        m_fly_cam.process_inputs(input_state, float(dt));
    }
    // set aspect ratio in case of resize
    m_fly_cam.aspect = calculate_aspect_ratio(m_app.get_window());
    m_fly_cam.update();
}

void Renderer::overlay_gui()
{
    if (m_should_display_overlay)
    {
        ImGui::SetNextWindowPos({ 50.f, 50.f });
        ImGui::SetNextWindowSizeConstraints({ 500.f, 500.f }, { 9999.f, 9999.f });
        ImGui::Begin("##InvisibleCameraWindow", nullptr,
              ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoBackground
            | ImGuiWindowFlags_NoInputs
            | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoMove);
        ImGui::Text("Camera Position: %.3f, %.3f, %.3f",
            m_fly_cam.position.x,
            m_fly_cam.position.y,
            m_fly_cam.position.z);
        ImGui::Text("Camera Forward: %.3f, %.3f, %.3f",
            m_fly_cam.forward.x,
            m_fly_cam.forward.y,
            m_fly_cam.forward.z);
        ImGui::End();
    }
}

void Renderer::setup_frame()
{
    m_imgui_renderer.next_frame();
}

void Renderer::render(rhi::Command_List* cmd, double t, double dt) noexcept
{
    constexpr static rhi::Image_Barrier_Subresource_Range RT_DEFAULT_SUBRESOURCE_RANGE = {
        .first_mip_level = 0,
        .mip_count = 1,
        .first_array_index = 0,
        .array_size = 1,
        .first_plane = 0,
        .plane_count = 1
    };

    GPU_Camera_Data camera_data = {
        .view = m_fly_cam.camera_data.view,
        .proj = m_fly_cam.camera_data.proj,
        .view_proj = m_fly_cam.camera_data.view_proj,
        .position = m_fly_cam.camera_data.position
    };
    m_app.upload_buffer_data_immediate(m_camera_buffer, &camera_data, sizeof(camera_data), 0);

    m_ocean_renderer.simulate(m_app, cmd, dt);

    /* Transition gbuffer attachments for rendering */ {
        rhi::Image_Barrier_Info gbuffer_color_barrier = {
            .stage_before = rhi::Barrier_Pipeline_Stage::None,
            .stage_after = rhi::Barrier_Pipeline_Stage::Color_Attachment_Output,
            .access_before = rhi::Barrier_Access::None,
            .access_after = rhi::Barrier_Access::Color_Attachment_Write,
            .layout_before = rhi::Barrier_Image_Layout::Undefined,
            .layout_after = rhi::Barrier_Image_Layout::Color_Attachment,
            .queue_type_ownership_transfer_target_queue = rhi::Queue_Type::Graphics,
            .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
            .image = m_g_buffer.target0,
            .subresource_range = RT_DEFAULT_SUBRESOURCE_RANGE,
            .discard = true
        };
        rhi::Image_Barrier_Info gbuffer_ds_barrier = {
            .stage_before = rhi::Barrier_Pipeline_Stage::None,
            .stage_after = rhi::Barrier_Pipeline_Stage::Early_Fragment_Tests,
            .access_before = rhi::Barrier_Access::None,
            .access_after = rhi::Barrier_Access::Depth_Stencil_Attachment_Write,
            .layout_before = rhi::Barrier_Image_Layout::Undefined,
            .layout_after = rhi::Barrier_Image_Layout::Depth_Stencil_Write,
            .queue_type_ownership_transfer_target_queue = rhi::Queue_Type::Graphics,
            .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
            .image = m_g_buffer.depth_stencil,
            .subresource_range = RT_DEFAULT_SUBRESOURCE_RANGE,
            .discard = true
        };
        auto ocean_targets = std::to_array({ gbuffer_color_barrier, gbuffer_ds_barrier });
        cmd->barrier({
            .buffer_barriers = {},
            .image_barriers = { ocean_targets },
            .memory_barriers = {}
            });
    }
    render_gbuffer_pass(cmd);
    /* Transition ocean pass attachments for rendering */ {
        rhi::Image_Barrier_Info ocean_color_barrier = {
            .stage_before = rhi::Barrier_Pipeline_Stage::None,
            .stage_after = rhi::Barrier_Pipeline_Stage::Color_Attachment_Output,
            .access_before = rhi::Barrier_Access::None,
            .access_after = rhi::Barrier_Access::Color_Attachment_Write,
            .layout_before = rhi::Barrier_Image_Layout::Undefined,
            .layout_after = rhi::Barrier_Image_Layout::Color_Attachment,
            .queue_type_ownership_transfer_target_queue = rhi::Queue_Type::Graphics,
            .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
            .image = m_ocean_rendertargets.color,
            .subresource_range = RT_DEFAULT_SUBRESOURCE_RANGE,
            .discard = true
        };
        rhi::Image_Barrier_Info ocean_ds_barrier = {
            .stage_before = rhi::Barrier_Pipeline_Stage::None,
            .stage_after = rhi::Barrier_Pipeline_Stage::Early_Fragment_Tests,
            .access_before = rhi::Barrier_Access::None,
            .access_after = rhi::Barrier_Access::Depth_Stencil_Attachment_Write,
            .layout_before = rhi::Barrier_Image_Layout::Undefined,
            .layout_after = rhi::Barrier_Image_Layout::Depth_Stencil_Write,
            .queue_type_ownership_transfer_target_queue = rhi::Queue_Type::Graphics,
            .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
            .image = m_ocean_rendertargets.depth_stencil,
            .subresource_range = RT_DEFAULT_SUBRESOURCE_RANGE,
            .discard = true
        };
        auto ocean_targets = std::to_array({ ocean_color_barrier, ocean_ds_barrier });
        cmd->barrier({
            .buffer_barriers = {},
            .image_barriers = { ocean_targets },
            .memory_barriers = {}
            });
    }
    render_ocean_pass(cmd);

    auto swapchain_image_view = m_swapchain.get_current_image_view();
    /* Transition Swapchain to Color Attachment layout and Ocean Color to Shader Resource */ {
        rhi::Image_Barrier_Info swapchain_layout_barrier = {
            .stage_before = rhi::Barrier_Pipeline_Stage::None,
            .stage_after = rhi::Barrier_Pipeline_Stage::Color_Attachment_Output,
            .access_before = rhi::Barrier_Access::None,
            .access_after = rhi::Barrier_Access::Color_Attachment_Write,
            .layout_before = rhi::Barrier_Image_Layout::Undefined,
            .layout_after = rhi::Barrier_Image_Layout::Color_Attachment,
            .queue_type_ownership_transfer_target_queue = rhi::Queue_Type::Graphics,
            .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
            .image = swapchain_image_view->image,
            .subresource_range = RT_DEFAULT_SUBRESOURCE_RANGE,
            .discard = true
        };
        rhi::Image_Barrier_Info ocean_color_barrier = {
            .stage_before = rhi::Barrier_Pipeline_Stage::Color_Attachment_Output,
            .stage_after = rhi::Barrier_Pipeline_Stage::Pixel_Shader,
            .access_before = rhi::Barrier_Access::Color_Attachment_Write,
            .access_after = rhi::Barrier_Access::Shader_Sampled_Read,
            .layout_before = rhi::Barrier_Image_Layout::Color_Attachment,
            .layout_after = rhi::Barrier_Image_Layout::Shader_Read_Only,
            .queue_type_ownership_transfer_target_queue = rhi::Queue_Type::Graphics,
            .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
            .image = m_ocean_rendertargets.color,
            .subresource_range = RT_DEFAULT_SUBRESOURCE_RANGE,
            .discard = false
        };
        rhi::Image_Barrier_Info ocean_depth_barrier = {
            .stage_before = rhi::Barrier_Pipeline_Stage::Late_Fragment_Tests,
            .stage_after = rhi::Barrier_Pipeline_Stage::Early_Fragment_Tests,
            .access_before = rhi::Barrier_Access::Depth_Stencil_Attachment_Write,
            .access_after = rhi::Barrier_Access::Depth_Stencil_Attachment_Read,
            .layout_before = rhi::Barrier_Image_Layout::Depth_Stencil_Write,
            .layout_after = rhi::Barrier_Image_Layout::Depth_Stencil_Read_Only,
            .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
            .image = m_ocean_rendertargets.depth_stencil,
            .subresource_range = RT_DEFAULT_SUBRESOURCE_RANGE,
            .discard = false
        };
        rhi::Image_Barrier_Info gbuffer_color_barrier = {
            .stage_before = rhi::Barrier_Pipeline_Stage::Color_Attachment_Output,
            .stage_after = rhi::Barrier_Pipeline_Stage::Pixel_Shader,
            .access_before = rhi::Barrier_Access::Color_Attachment_Write,
            .access_after = rhi::Barrier_Access::Shader_Sampled_Read,
            .layout_before = rhi::Barrier_Image_Layout::Color_Attachment,
            .layout_after = rhi::Barrier_Image_Layout::Shader_Read_Only,
            .queue_type_ownership_transfer_target_queue = rhi::Queue_Type::Graphics,
            .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
            .image = m_g_buffer.target0,
            .subresource_range = RT_DEFAULT_SUBRESOURCE_RANGE,
            .discard = false
        };
        rhi::Image_Barrier_Info gbuffer_depth_barrier = {
            .stage_before = rhi::Barrier_Pipeline_Stage::Late_Fragment_Tests,
            .stage_after = rhi::Barrier_Pipeline_Stage::Early_Fragment_Tests,
            .access_before = rhi::Barrier_Access::Depth_Stencil_Attachment_Write,
            .access_after = rhi::Barrier_Access::Depth_Stencil_Attachment_Read,
            .layout_before = rhi::Barrier_Image_Layout::Depth_Stencil_Write,
            .layout_after = rhi::Barrier_Image_Layout::Depth_Stencil_Read_Only,
            .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
            .image = m_g_buffer.depth_stencil,
            .subresource_range = RT_DEFAULT_SUBRESOURCE_RANGE,
            .discard = false
        };
        auto barriers = std::to_array({
            swapchain_layout_barrier,
            ocean_color_barrier,
            ocean_depth_barrier,
            gbuffer_color_barrier,
            gbuffer_depth_barrier });
        cmd->barrier({
            .buffer_barriers = {},
            .image_barriers = barriers,
            .memory_barriers = {}
            });
    }
    render_swapchain_pass(cmd);
    /* Transition Swapchain to Present layout */ {
        rhi::Image_Barrier_Info swapchain_layout_barrier = {
            .stage_before = rhi::Barrier_Pipeline_Stage::Color_Attachment_Output,
            .stage_after = rhi::Barrier_Pipeline_Stage::None,
            .access_before = rhi::Barrier_Access::Color_Attachment_Write,
            .access_after = rhi::Barrier_Access::None,
            .layout_before = rhi::Barrier_Image_Layout::Color_Attachment,
            .layout_after = rhi::Barrier_Image_Layout::Present,
            .queue_type_ownership_transfer_target_queue = rhi::Queue_Type::Graphics,
            .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
            .image = swapchain_image_view->image,
            .subresource_range = RT_DEFAULT_SUBRESOURCE_RANGE,
            .discard = false
        };
        cmd->barrier({
            .buffer_barriers = {},
            .image_barriers = { &swapchain_layout_barrier, 1 },
            .memory_barriers = {}
            });
    }
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
    resize_target(m_ocean_rendertargets.color);
    resize_target(m_ocean_rendertargets.depth_stencil);
    resize_target(m_g_buffer.target0);
    resize_target(m_g_buffer.depth_stencil);
}

void Renderer::render_gbuffer_pass(rhi::Command_List* cmd)
{
    cmd->begin_debug_region("GBuffer Pass", 1.f, .5f, 1.f);

    rhi::Render_Pass_Color_Attachment_Info ocean_color_attachment_info = {
        .attachment = m_g_buffer.target0,
        .load_op = rhi::Render_Pass_Attachment_Load_Op::Clear,
        .store_op = rhi::Render_Pass_Attachment_Store_Op::Store,
        .clear_value = {
            .color = { 0.0f, 0.0f, 0.0f, 0.0f }
        }
    };
    rhi::Render_Pass_Begin_Info render_pass_info = {
        .color_attachments = { &ocean_color_attachment_info, 1 },
        .depth_stencil_attachment = {
            .attachment = m_g_buffer.depth_stencil,
            .depth_load_op = rhi::Render_Pass_Attachment_Load_Op::Clear,
            .depth_store_op = rhi::Render_Pass_Attachment_Store_Op::Store,
            .stencil_load_op = rhi::Render_Pass_Attachment_Load_Op::No_Access,
            .stencil_store_op = rhi::Render_Pass_Attachment_Store_Op::No_Access,
            .clear_value = {
                .depth_stencil = { 1.0f, 0 }
            }
        }
    };

    cmd->begin_render_pass(render_pass_info);

    auto& window_data = m_app.get_window().get_window_data();
    auto scale = 1.f; // m_app.get_window().get_dpi_scale();
    cmd->set_viewport(0.f, 0.f, float(window_data.width) * scale, float(window_data.height) * scale, 0.f, 1.f);
    cmd->set_scissor(0, 0, window_data.width * scale, window_data.height * scale);

    // TODO: TESTING
    auto draw_pipeline = m_app.get_asset_repository().get_graphics_pipeline("basic_draw");
    cmd->set_pipeline(draw_pipeline);
    auto* model = m_app.get_asset_repository().get_model("Sponza.renmdl");
    for (auto& instance : model->instances)
    {
        cmd->set_index_buffer(model->indices, rhi::Index_Type::U32);
        for (auto i = instance.submeshes_range_start; i < instance.submeshes_range_end; ++i)
        {
            auto& submesh = model->submeshes[i];
            auto index_count = submesh.index_range[1] - submesh.index_range[0];
            cmd->set_push_constants<Immediate_Draw_Push_Constants>({
                .position_buffer = model->vertex_positions->buffer_view->bindless_index,
                .attribute_buffer = model->vertex_attributes->buffer_view->bindless_index,
                .camera_buffer = m_camera_buffer,
                .vertex_offset = submesh.vertex_position_range[0]
            }, rhi::Pipeline_Bind_Point::Graphics);
            cmd->draw_indexed(
                index_count,
                1,
                submesh.index_range[0],
                0,
                0);
        }
    }
    cmd->end_render_pass();

    cmd->end_debug_region();
}

void Renderer::render_ocean_pass(rhi::Command_List* cmd)
{
    cmd->begin_debug_region("Ocean Pass", .2f, .2f, 1.f);

    rhi::Render_Pass_Color_Attachment_Info ocean_color_attachment_info = {
        .attachment = m_ocean_rendertargets.color,
        .load_op = rhi::Render_Pass_Attachment_Load_Op::Clear,
        .store_op = rhi::Render_Pass_Attachment_Store_Op::Store,
        .clear_value = {
            .color = { 0.0f, 0.0f, 0.0f, 0.0f }
        }
    };
    rhi::Render_Pass_Begin_Info render_pass_info = {
        .color_attachments = { &ocean_color_attachment_info, 1 },
        .depth_stencil_attachment = {
            .attachment = m_ocean_rendertargets.depth_stencil,
            .depth_load_op = rhi::Render_Pass_Attachment_Load_Op::Clear,
            .depth_store_op = rhi::Render_Pass_Attachment_Store_Op::Store,
            .stencil_load_op = rhi::Render_Pass_Attachment_Load_Op::No_Access,
            .stencil_store_op = rhi::Render_Pass_Attachment_Store_Op::No_Access,
            .clear_value = {
                .depth_stencil = { 1.0f, 0 }
            }
        }
    };

    cmd->begin_render_pass(render_pass_info);
    auto& window_data = m_app.get_window().get_window_data();
    auto scale = 1.f; // m_app.get_window().get_dpi_scale();
    cmd->set_viewport(0.f, 0.f, float(window_data.width) * scale, float(window_data.height) * scale, 0.f, 1.f);
    cmd->set_scissor(0, 0, window_data.width * scale, window_data.height * scale);
    m_ocean_renderer.render_patch(cmd, m_camera_buffer);
    cmd->end_render_pass();

    cmd->end_debug_region();
}

void Renderer::render_swapchain_pass(rhi::Command_List* cmd)
{
    auto swapchain_image_view = m_swapchain.get_current_image_view();

    cmd->begin_debug_region("Swapchain Pass", 0.5f, 0.25f, 0.25f);

    rhi::Render_Pass_Color_Attachment_Info swapchain_attachment_info = {
        .attachment = swapchain_image_view,
        .load_op = rhi::Render_Pass_Attachment_Load_Op::Clear,
        .store_op = rhi::Render_Pass_Attachment_Store_Op::Store,
        .clear_value = {
            .color = { 0.0f, 0.0f, 0.0f, 1.0f }
        }
    };
    rhi::Render_Pass_Begin_Info render_pass_info = {
        .color_attachments = { &swapchain_attachment_info, 1 },
        .depth_stencil_attachment = {}
    };

    auto& window_data = m_app.get_window().get_window_data();
    auto scale = 1.f; // m_app.get_window().get_dpi_scale();
    cmd->begin_render_pass(render_pass_info);
    cmd->set_viewport(0.f, 0.f, float(window_data.width) * scale, float(window_data.height) * scale, 0.f, 1.f);
    cmd->set_scissor(0, 0, window_data.width * scale, window_data.height * scale);

    cmd->add_debug_marker("Ocean Composite", .5f, .5f, 1.f);
    m_ocean_renderer.render_composite(cmd,
        m_ocean_rendertargets.color,
        m_ocean_rendertargets.depth_stencil,
        m_g_buffer.target0,
        m_g_buffer.depth_stencil);

    cmd->begin_debug_region("Debug Renderer", 1.f, .75f, .75f);
    m_ocean_renderer.debug_render_slope(cmd, m_camera_buffer);
    m_ocean_renderer.debug_render_normal(cmd, m_camera_buffer);
    cmd->end_debug_region();

    cmd->add_debug_marker("Dear ImGui", .5f, 1.f, .0f);
    m_imgui_renderer.render(cmd);
    cmd->end_render_pass();

    cmd->end_debug_region();
}

void Renderer::init_rendertargets()
{
    rhi::Image_Create_Info default_image_create_info = {
        .format = rhi::Image_Format::Undefined,
        .width = m_app.get_window().get_window_data().width,
        .height = m_app.get_window().get_window_data().height,
        .depth = 1,
        .array_size = 1,
        .mip_levels = 1,
        .primary_view_type = rhi::Image_View_Type::Texture_2D
    };

    rhi::Image_Create_Info gbuffer_target0_create_info = default_image_create_info;
    gbuffer_target0_create_info.format = rhi::Image_Format::B10G11R11_UFLOAT_PACK32;
    gbuffer_target0_create_info.usage = rhi::Image_Usage::Color_Attachment | rhi::Image_Usage::Sampled;

    rhi::Image_Create_Info gbuffer_ds_create_info = default_image_create_info;
    gbuffer_ds_create_info.format = rhi::Image_Format::D32_SFLOAT;
    gbuffer_ds_create_info.usage = rhi::Image_Usage::Depth_Stencil_Attachment | rhi::Image_Usage::Sampled;

    m_g_buffer = {
        .target0 = m_resource_blackboard.create_image("gbuffer_color", gbuffer_target0_create_info),
        .depth_stencil = m_resource_blackboard.create_image("gbuffer_ds", gbuffer_ds_create_info)
    };

    rhi::Image_Create_Info ocean_color_create_info = default_image_create_info;
    ocean_color_create_info.format = rhi::Image_Format::B10G11R11_UFLOAT_PACK32;
    ocean_color_create_info.usage = rhi::Image_Usage::Color_Attachment | rhi::Image_Usage::Sampled;

    rhi::Image_Create_Info ocean_ds_create_info = default_image_create_info;
    ocean_ds_create_info.format = rhi::Image_Format::D32_SFLOAT;
    ocean_ds_create_info.usage = rhi::Image_Usage::Depth_Stencil_Attachment | rhi::Image_Usage::Sampled;

    m_ocean_rendertargets = {
        .color = m_resource_blackboard.create_image("ocean_color", ocean_color_create_info),
        .depth_stencil = m_resource_blackboard.create_image("ocean_ds", ocean_ds_create_info)
    };
}
}
