#include "renderer/imgui_renderer.hpp"

#include "renderer/generated/imgui.vs.dxil.hpp"
#include "renderer/generated/imgui.vs.spv.hpp"
#include "renderer/generated/imgui.ps.dxil.hpp"
#include "renderer/generated/imgui.ps.spv.hpp"

namespace ren
{
constexpr static uint64_t BUFFER_SIZE_STEP = 1024ull * 1024 * 4; // 4MiB

struct Imgui_Push_Constants
{
    uint32_t vertex_buffer;
    uint32_t texture;
    uint32_t sampler;
    float left;
    float top;
    float right;
    float bottom;
};

Imgui_Renderer::Imgui_Renderer(const Imgui_Renderer_Create_Info& create_info)
    : m_device(create_info.device)
    , m_vertex_buffers()
    , m_index_buffers()
    , m_images()
    , m_vertex_shader(nullptr)
    , m_pixel_shader(nullptr)
    , m_pipeline(nullptr)
    , m_sampler(nullptr)
    , m_frames_in_flight(create_info.frames_in_flight)
    , m_frame_index(0u)
{
    auto& io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Backend already initialized!");
    io.BackendRendererUserData = static_cast<void*>(this);
    io.BackendRendererName = "imgui_impl_rhi";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    m_vertex_buffers.resize(create_info.frames_in_flight);
    m_index_buffers.resize(create_info.frames_in_flight);

    bool is_d3d12 = m_device->get_graphics_api() == rhi::Graphics_API::D3D12;
    rhi::Shader_Blob_Create_Info vertex_shader_create_info = {
        .data = static_cast<void*>(is_d3d12 ? imgui_vs_dxil.data() : imgui_vs_spv.data()),
        .data_size = uint32_t(is_d3d12 ? imgui_vs_dxil.size() : imgui_vs_spv.size()),
        .groups_x = 0,
        .groups_y = 0,
        .groups_z = 0
    };
    m_vertex_shader = m_device->create_shader_blob(vertex_shader_create_info).value_or(nullptr);
    rhi::Shader_Blob_Create_Info pixel_shader_create_info = {
        .data = static_cast<void*>(is_d3d12 ? imgui_ps_dxil.data() : imgui_ps_spv.data()),
        .data_size = uint32_t(is_d3d12 ? imgui_ps_dxil.size() : imgui_ps_spv.size()),
        .groups_x = 0,
        .groups_y = 0,
        .groups_z = 0
    };
    m_pixel_shader = m_device->create_shader_blob(pixel_shader_create_info).value_or(nullptr);

    rhi::Graphics_Pipeline_Create_Info pipeline_create_info = {
        .vs = m_vertex_shader,
        .hs = nullptr,
        .ds = nullptr,
        .gs = nullptr,
        .ps = m_pixel_shader,
        .blend_state_info = {
            .independent_blend_enable = false,
            .color_attachments = {
                {
                    /* .blend_enable */ true,
                    /* .logic_op_enable */ false,
                    /* .color_src_blend */ rhi::Blend_Factor::Src_Alpha,
                    /* .color_dst_blend */ rhi::Blend_Factor::One_Minus_Src_Alpha,
                    /* .color_blend_op */ rhi::Blend_Op::Add,
                    /* .alpha_src_blend */ rhi::Blend_Factor::One,
                    /* .alpha_dst_blend */ rhi::Blend_Factor::One_Minus_Src_Alpha,
                    /* .alpha_blend_op */ rhi::Blend_Op::Add,
                    /* .logic_op */ rhi::Logic_Op::Noop,
                    /* .color_write_mask */ rhi::Color_Component::Enable_All
                }
            }
        },
        .rasterizer_state_info = {
            .fill_mode = rhi::Fill_Mode::Solid,
            .cull_mode = rhi::Cull_Mode::None,
            .winding_order = rhi::Winding_Order::Front_Face_CW,
            .depth_bias = 0.0f,
            .depth_bias_clamp = 0.0f,
            .depth_bias_slope_scale = 0.0f,
            .depth_clip_enable = false
        },
        .depth_stencil_info = {
            .depth_enable = false,
            .depth_write_enable = false,
            .stencil_enable = false,
            .depth_bounds_test_mode = rhi::Depth_Bounds_Test_Mode::Disabled
        },
        .primitive_topology = rhi::Primitive_Topology_Type::Triangle,
        .color_attachment_count = 1,
        .color_attachment_formats = create_info.swapchain_image_format,
        .depth_stencil_format = rhi::Image_Format::Undefined
    };
    m_pipeline = m_device->create_pipeline(pipeline_create_info).value_or(nullptr);

    rhi::Sampler_Create_Info sampler_create_info = {
        .filter_min = rhi::Sampler_Filter::Linear,
        .filter_mag = rhi::Sampler_Filter::Linear,
        .filter_mip = rhi::Sampler_Filter::Linear,
        .address_mode_u = rhi::Image_Sample_Address_Mode::Wrap,
        .address_mode_v = rhi::Image_Sample_Address_Mode::Wrap,
        .address_mode_w = rhi::Image_Sample_Address_Mode::Wrap,
        .mip_lod_bias = 0.0f,
        .max_anisotropy = 0,
        .comparison_func = rhi::Comparison_Func::Always,
        .reduction = rhi::Sampler_Reduction_Type::Standard,
        .border_color = {},
        .min_lod = 0.0f,
        .max_lod = 0.0f,
        .anisotropy_enable = false
    };
    m_sampler = m_device->create_sampler(sampler_create_info).value_or(nullptr);
}

Imgui_Renderer::~Imgui_Renderer() noexcept
{
    m_device->wait_idle();
    for (auto buffer : m_vertex_buffers)
    {
        m_device->destroy_buffer(buffer);
    }
    for (auto buffer : m_index_buffers)
    {
        m_device->destroy_buffer(buffer);
    }
    for (auto image : m_images)
    {
        m_device->destroy_image(image);
    }
    auto& io = ImGui::GetIO();
    io.BackendRendererUserData = nullptr;
}

void Imgui_Renderer::next_frame() noexcept
{
    m_frame_index = (m_frame_index + 1) % m_frames_in_flight;
}

void Imgui_Renderer::render(rhi::Command_List* cmd) noexcept
{
    ImDrawData* draw_data = ImGui::GetDrawData();
    auto vertex_buffer = m_vertex_buffers[m_frame_index];
    auto index_buffer = m_vertex_buffers[m_frame_index];

    auto calculate_next_buffer_size = [](auto requested_size) {
        return (requested_size / BUFFER_SIZE_STEP + 1) * BUFFER_SIZE_STEP;
    };

    // Resize vertex buffer if required
    if (vertex_buffer == nullptr || vertex_buffer->size < draw_data->TotalVtxCount)
    {
        // HACK: add mechanism to defer deletion in case this becomes slow
        m_device->wait_idle();
        auto next_buffer_size = calculate_next_buffer_size(draw_data->TotalVtxCount);
        m_device->destroy_buffer(vertex_buffer);
        rhi::Buffer_Create_Info vertex_buffer_create_info = {
            .size = next_buffer_size,
            .heap = rhi::Memory_Heap_Type::CPU_Upload
        };
        // HACK: we don't check if creation fails
        vertex_buffer = m_device->create_buffer(vertex_buffer_create_info).value_or(nullptr);
        m_vertex_buffers[m_frame_index] = vertex_buffer;
    }

    // Resize index buffer if requried
    if (index_buffer == nullptr || index_buffer->size < draw_data->TotalIdxCount)
    {
        m_device->wait_idle();
        auto next_buffer_size = calculate_next_buffer_size(draw_data->TotalVtxCount);
        m_device->destroy_buffer(index_buffer);
        rhi::Buffer_Create_Info index_buffer_create_info = {
            .size = next_buffer_size,
            .heap = rhi::Memory_Heap_Type::CPU_Upload
        };
        // HACK: we don't check if creation fails
        index_buffer = m_device->create_buffer(index_buffer_create_info).value_or(nullptr);
        m_index_buffers[m_frame_index] = index_buffer;
    }

    auto vertex_copy_dst = static_cast<ImDrawVert*>(vertex_buffer->data);
    auto index_copy_dst = static_cast<ImDrawIdx*>(index_buffer->data);
    for (auto cmd_list_index = 0; cmd_list_index < draw_data->CmdListsCount; ++cmd_list_index)
    {
        auto cmd_list = draw_data->CmdLists[cmd_list_index];
        memcpy(vertex_copy_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(index_copy_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vertex_copy_dst += cmd_list->VtxBuffer.Size;
        index_copy_dst += cmd_list->IdxBuffer.Size;
    }

    setup_render_state(cmd, index_buffer, draw_data);

    float left = draw_data->DisplayPos.x;
    float top = draw_data->DisplayPos.y;
    float right = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float bottom = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

    auto global_vertex_offset = 0;
    auto global_index_offset = 0;
    ImVec2 clip_off = draw_data->DisplayPos;
    for (auto cmd_list_index = 0; cmd_list_index < draw_data->CmdListsCount; ++cmd_list_index)
    {
        const auto cmd_list = draw_data->CmdLists[cmd_list_index];
        for (auto cmd_index = 0; cmd_index < cmd_list->CmdBuffer.Size; ++cmd_index)
        {
            const auto& imgui_cmd = cmd_list->CmdBuffer[cmd_index];
            if (imgui_cmd.UserCallback != nullptr)
            {
                if (imgui_cmd.UserCallback == ImDrawCallback_ResetRenderState)
                {
                    setup_render_state(cmd, index_buffer, draw_data);
                }
                else
                {
                    imgui_cmd.UserCallback(cmd_list, &imgui_cmd);
                }
            }
            else
            {
                auto clip_min = ImVec2(imgui_cmd.ClipRect.x - clip_off.x, imgui_cmd.ClipRect.y - clip_off.y);
                auto clip_max = ImVec2(imgui_cmd.ClipRect.z - clip_off.x, imgui_cmd.ClipRect.w - clip_off.y);

                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                {
                    continue;
                }

                // TODO: only send changed data
                Imgui_Push_Constants push_consts = {
                    .vertex_buffer = vertex_buffer->buffer_view->bindless_index,
                    .texture = uint32_t(reinterpret_cast<uint64_t>(imgui_cmd.GetTexID())),
                    .sampler = 0,
                    .left = left,
                    .top = top,
                    .right = right,
                    .bottom = bottom
                };
                cmd->set_push_constants(push_consts, rhi::Pipeline_Bind_Point::Graphics);
                cmd->set_scissor(clip_min.x, clip_min.y, clip_max.x, clip_max.y);
                cmd->draw_indexed(
                    imgui_cmd.ElemCount,
                    1,
                    imgui_cmd.IdxOffset + global_index_offset,
                    imgui_cmd.VtxOffset + global_vertex_offset,
                    0);
            }
        }
        global_vertex_offset += cmd_list->VtxBuffer.Size;
        global_index_offset += cmd_list->IdxBuffer.Size;
    }
}

void Imgui_Renderer::setup_render_state(
    rhi::Command_List* cmd,
    rhi::Buffer* index_buffer,
    ImDrawData* draw_data) noexcept
{
    cmd->set_viewport(0.0f, 0.0f, draw_data->DisplaySize.x, draw_data->DisplaySize.y, 0.0f, 1.0f);
    cmd->set_pipeline(m_pipeline);
    cmd->set_index_buffer(index_buffer, rhi::Index_Type::U16);
}
}
