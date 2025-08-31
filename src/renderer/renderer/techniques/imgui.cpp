#include "renderer/techniques/imgui.hpp"

#include "renderer/gpu_transfer.hpp"
#include "renderer/asset/asset_repository.hpp"

#include <rhi/command_list.hpp>
#include <imgui.h>

namespace ren::techniques
{
constexpr static uint64_t VERTEX_BUFFER_ELEMENT_SIZE = 1024ull * 1024 * sizeof(ImDrawVert); // ~1 million vertices
constexpr static uint64_t INDEX_BUFFER_ELEMENT_SIZE = 1024ull * 1024 * 6 / 4 * sizeof(ImDrawIdx); // 6 indices per vertex

Imgui::Imgui(Asset_Repository& asset_repository,
    GPU_Transfer_Context& gpu_transfer_context,
    Render_Resource_Blackboard& render_resource_blackboard)
    : m_asset_repository(asset_repository)
    , m_gpu_transfer_context(gpu_transfer_context)
    , m_render_resource_blackboard(render_resource_blackboard)
{
    setup_style();

    auto& io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Backend already initialized!");
    io.BackendRendererUserData = static_cast<void*>(this);
    io.BackendRendererName = "imgui_impl_rhi";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    rhi::Buffer_Create_Info vertex_buffer_create_info = {
        .size = VERTEX_BUFFER_ELEMENT_SIZE,
        .heap = rhi::Memory_Heap_Type::GPU
    };
    m_vertex_buffer = m_render_resource_blackboard.create_buffer(VERTEX_BUFFER_NAME, vertex_buffer_create_info);

    rhi::Buffer_Create_Info index_buffer_create_info = {
        .size = INDEX_BUFFER_ELEMENT_SIZE,
        .heap = rhi::Memory_Heap_Type::GPU
    };
    m_index_buffer = m_render_resource_blackboard.create_buffer(INDEX_BUFFER_NAME, index_buffer_create_info);

    io.Fonts->ClearFonts();
    io.Fonts->AddFontFromFileTTF("../assets/fonts/RobotoMono-Regular.ttf", 24.0f);
    uint8_t* pixels = nullptr;
    int32_t width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    rhi::Image_Create_Info font_image_create_info = {
        .format = rhi::Image_Format::R8G8B8A8_UNORM,
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
        .depth = 1,
        .array_size = 1,
        .mip_levels = 1,
        .usage = rhi::Image_Usage::Sampled,
        .primary_view_type = rhi::Image_View_Type::Texture_2D
    };
    void* data = pixels;
    m_font_texture = m_render_resource_blackboard.create_image(FONT_TEXTURE_NAME, font_image_create_info);
    m_gpu_transfer_context.enqueue_immediate_upload(m_font_texture, &data);
    io.Fonts->SetTexID(m_font_texture);

    m_texture_sampler = m_render_resource_blackboard.get_sampler({
        .filter_min = rhi::Sampler_Filter::Linear,
        .filter_mag = rhi::Sampler_Filter::Linear,
        .filter_mip = rhi::Sampler_Filter::Linear,
        .address_mode_u = rhi::Image_Sample_Address_Mode::Wrap,
        .address_mode_v = rhi::Image_Sample_Address_Mode::Wrap,
        .address_mode_w = rhi::Image_Sample_Address_Mode::Wrap,
        .mip_lod_bias = 0.0f,
        .max_anisotropy = 0,
        .comparison_func = rhi::Comparison_Func::None,
        .reduction = rhi::Sampler_Reduction_Type::Standard,
        .border_color = {},
        .min_lod = 0.0f,
        .max_lod = 0.0f,
        .anisotropy_enable = false});
}

Imgui::~Imgui()
{
    m_render_resource_blackboard.destroy_buffer(m_vertex_buffer);
    m_render_resource_blackboard.destroy_buffer(m_index_buffer);
    m_render_resource_blackboard.destroy_image(m_font_texture);
    auto& io = ImGui::GetIO();
    io.BackendRendererUserData = nullptr;
}

void Imgui::render(rhi::Command_List* cmd, const Image& target)
{
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (!draw_data) return;

    cmd->begin_debug_region("imgui", .5f, 1.f, .0f);
    rhi::Render_Pass_Color_Attachment_Info swapchain_attachment_info = {
        .attachment = target,
        .load_op = rhi::Render_Pass_Attachment_Load_Op::Load,
        .store_op = rhi::Render_Pass_Attachment_Store_Op::Store
    };
    const rhi::Render_Pass_Begin_Info render_pass_info = {
        .color_attachments = { &swapchain_attachment_info, 1 },
        .depth_stencil_attachment = {}
    };

    const auto width = target.get_create_info().width;
    const auto height = target.get_create_info().height;

    cmd->begin_render_pass(render_pass_info);
    cmd->set_viewport(0.f, 0.f, static_cast<float>(width), static_cast<float>(height), 0.f, 1.f);
    cmd->set_scissor(0, 0, width, height);

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

    std::size_t vertex_copy_offset = 0;
    std::size_t index_copy_offset = 0;
    for (auto cmd_list_index = 0; cmd_list_index < draw_data->CmdListsCount; ++cmd_list_index)
    {
        const auto* draw_list = draw_data->CmdLists[cmd_list_index];
        m_gpu_transfer_context.enqueue_immediate_upload(
            m_vertex_buffer,
            draw_list->VtxBuffer.Data,
            draw_list->VtxBuffer.Size * sizeof(ImDrawVert),
            vertex_copy_offset);
        m_gpu_transfer_context.enqueue_immediate_upload(
            m_index_buffer,
            draw_list->IdxBuffer.Data,
            draw_list->IdxBuffer.Size * sizeof(ImDrawIdx),
            index_copy_offset);
        vertex_copy_offset += draw_list->VtxBuffer.Size * sizeof(ImDrawVert);
        index_copy_offset += draw_list->IdxBuffer.Size * sizeof(ImDrawIdx);
    }

    const float left = draw_data->DisplayPos.x;
    const float top = draw_data->DisplayPos.y;
    const float right = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    const float bottom = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

    setup_render_state(cmd, target);
    Imgui_Push_Constants push_consts = {
        .vertex_buffer = m_vertex_buffer,
        .texture = static_cast<uint32_t>(draw_data->CmdLists[0]->CmdBuffer[0].GetTexID()),
        .sampler = m_texture_sampler,
        .left = left,
        .top = top,
        .right = right,
        .bottom = bottom
    };
    cmd->set_push_constants(push_consts, rhi::Pipeline_Bind_Point::Graphics);

    auto global_vertex_offset = 0;
    auto global_index_offset = 0;
    const ImVec2 clip_off = draw_data->DisplayPos;
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
                    setup_render_state(cmd, target);
                    push_consts = {
                        .vertex_buffer = m_vertex_buffer,
                        .texture = static_cast<uint32_t>(imgui_cmd.GetTexID()),
                        .sampler = m_texture_sampler,
                        .left = left,
                        .top = top,
                        .right = right,
                        .bottom = bottom
                    };
                    cmd->set_push_constants(push_consts, rhi::Pipeline_Bind_Point::Graphics);
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

                cmd->set_scissor(clip_min.x, clip_min.y, clip_max.x - clip_min.x, clip_max.y - clip_min.y);
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
    cmd->end_render_pass();
    cmd->end_debug_region(); // imgui
}

void Imgui::setup_render_state(rhi::Command_List* cmd, const Image& image) const
{
    const auto format = image.get_create_info().format;
    const auto* draw_data = ImGui::GetDrawData();
    cmd->set_viewport(0.0f, 0.0f, draw_data->DisplaySize.x, draw_data->DisplaySize.y, 0.0f, 1.0f);
    if (format == rhi::Image_Format::A2R10G10B10_UNORM_PACK32)
        cmd->set_pipeline(m_asset_repository.get_graphics_pipeline("imgui_hdr"));
    else
        cmd->set_pipeline(m_asset_repository.get_graphics_pipeline("imgui"));
    cmd->set_index_buffer(m_index_buffer, rhi::Index_Type::U16);
}

void Imgui::setup_style()
{
    // Deep Dark theme https://github.com/ocornut/imgui/issues/707#issuecomment-917151020
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
    colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
    colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.25f);

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(8.00f, 8.00f);
    style.FramePadding = ImVec2(5.00f, 2.00f);
    style.CellPadding = ImVec2(6.00f, 6.00f);
    style.ItemSpacing = ImVec2(6.00f, 6.00f);
    style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
    style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
    style.IndentSpacing = 25;
    style.ScrollbarSize = 15;
    style.GrabMinSize = 10;
    style.WindowBorderSize = 1;
    style.ChildBorderSize = 1;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 1;
    style.TabBorderSize = 1;
    style.WindowRounding = 7;
    style.ChildRounding = 4;
    style.FrameRounding = 3;
    style.PopupRounding = 4;
    style.ScrollbarRounding = 9;
    style.GrabRounding = 3;
    style.LogSliderDeadzone = 4;
    style.TabRounding = 4;
}
}
