#include "renderer/cbt/cbt_cpu.hpp"

#include <imgui.h>
#include <string>

namespace ren
{
constexpr static auto CBT_VIS_TITLE = "CBT and LEB Visualization";
constexpr static auto CBT_VIS_MIN_DEPTH = 2;
constexpr static auto CBT_VIS_MAX_DEPTH = 6;

CBT_CPU_Vis::CBT_CPU_Vis() noexcept
    : m_max_depth(6)
    , m_binary_heap(calculate_binary_heap_size(m_max_depth), 0u)
{}

void CBT_CPU_Vis::imgui_window(bool& open) noexcept
{
    ImGui::SetNextWindowSizeConstraints(
        {   980.0f,   350.0f },
        { 99999.9f, 99999.9f });
    if (ImGui::Begin(
        CBT_VIS_TITLE,
        &open,
        ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoDocking))
    {
        imgui_init_at_depth();
        imgui_show_tree();
        ImGui::End();
    }
}

void CBT_CPU_Vis::imgui_init_at_depth() noexcept
{
    static int32_t depth_init_value = m_max_depth;
    if (ImGui::InputInt("Tree depth", &depth_init_value, 1, 1, ImGuiInputTextFlags_None))
    {
        depth_init_value = std::max(CBT_VIS_MIN_DEPTH, std::min(depth_init_value, CBT_VIS_MAX_DEPTH));
        if (m_max_depth < uint32_t(depth_init_value))
        {
            m_binary_heap = std::vector<uint32_t>(calculate_binary_heap_size(m_max_depth), 0u);
        }
        m_max_depth = uint32_t(depth_init_value);
        sum_reduction();
    }
    if (ImGui::Button("Fill Tree"))
    {
        init_for_depth(depth_init_value);
    }
}

void CBT_CPU_Vis::imgui_show_tree() noexcept
{
    auto button_col = ImGui::GetStyleColorVec4(ImGuiCol_Button);
    button_col.w = 1.0f;
    ImGui::PushStyleColor(ImGuiCol_Button, button_col);
    auto button_col_hovered = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
    button_col_hovered.w = 1.0f;
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_col);
    auto button_col_active = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
    button_col_active.w = 1.0f;
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, button_col);

    uint32_t heap_idx = 0;
    auto button_width = 30.0f;
    auto button_size = ImVec2{ button_width, button_width };
    auto button_gap_mul = float(1 << (m_max_depth - 1));
    auto button_pad_y = button_width / 3.0f;
    auto start_pos = ImGui::GetCursorPos();
    auto start_screen_pos = ImGui::GetCursorScreenPos();

    for (auto level = 0; level < m_max_depth; ++level)
    {
        if (level == m_max_depth - 1)
        {
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_col_hovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, button_col_active);
        }

        for (auto element = 0; element < 1 << level; ++element)
        {
            auto next_cursor_pos = ImVec2{
                start_pos.x + element * button_gap_mul * button_width,
                start_pos.y + level * button_size.y + level * button_pad_y
            };
            ImGui::SetCursorPos(next_cursor_pos);


            if (level != m_max_depth - 1)
            {
                const static uint32_t LINE_COLOR = ImGui::ColorConvertFloat4ToU32(
                    { 1.0f, 1.0f, 1.0f, 0.5f });

                auto draw_list = ImGui::GetWindowDrawList();
                auto draw_line_start_pos = ImVec2{
                    next_cursor_pos.x + start_screen_pos.x - start_pos.x,
                    next_cursor_pos.y + start_screen_pos.y - start_pos.y
                };
                draw_line_start_pos.x += button_size.x / 2.0f;
                draw_line_start_pos.y += button_size.y / 2.0f;
                auto draw_line_child_left_pos = draw_line_start_pos;
                draw_line_child_left_pos.y += button_pad_y + button_size.y;
                auto draw_line_child_right_pos = draw_line_child_left_pos;
                draw_line_child_right_pos.x += (button_gap_mul / 2.0f) * button_width;
                auto draw_line_child_right_above_pos = draw_line_child_right_pos;
                draw_line_child_right_above_pos.y -= button_pad_y + button_size.y;
                draw_list->AddLine(draw_line_start_pos, draw_line_child_left_pos, LINE_COLOR, 1.0f);
                draw_list->AddLine(draw_line_start_pos, draw_line_child_right_above_pos, LINE_COLOR, 1.0f);
                draw_list->AddLine(draw_line_child_right_above_pos, draw_line_child_right_pos, LINE_COLOR, 1.0f);
            }

            ImGui::PushID(heap_idx);
            std::string value = std::to_string(access_value(heap_idx));
            if (ImGui::Button(value.c_str(), button_size) && level == m_max_depth - 1)
            {
                write_value(heap_idx, access_value(heap_idx) == 1 ? 0u : 1u);
                sum_reduction();
            }
            ImGui::PopID();

            heap_idx += 1;
        }

        if (level == m_max_depth - 1)
        {
            ImGui::PopStyleColor(2);
        }

        button_gap_mul /= 2.0f;
    }

    ImGui::PopStyleColor(3);
}

uint32_t CBT_CPU_Vis::access_value(uint32_t heap_idx) noexcept
{
    // uint32_t bit_range_start = 0;
    // uint32_t bit_range_length = 0;
    // uint32_t lower = 0;
    // uint32_t upper = 0;
    return m_binary_heap[heap_idx];
}

void CBT_CPU_Vis::write_value(uint32_t heap_idx, uint32_t value) noexcept
{
    m_binary_heap[heap_idx] = value;
}

uint32_t CBT_CPU_Vis::heap_successor_left(uint32_t heap_idx) noexcept
{
    return heap_successor_right(heap_idx) - 1u;
}

uint32_t CBT_CPU_Vis::heap_successor_right(uint32_t heap_idx) noexcept
{
    return 2 * (heap_idx + 1);
}

void CBT_CPU_Vis::sum_reduction() noexcept
{
    auto end = (1 << (m_max_depth - 1)) - 2;
    for (auto heap_idx = end; heap_idx >= 0; --heap_idx)
    {
        auto succ_l = heap_successor_left(heap_idx);
        auto succ_r = heap_successor_right(heap_idx);
        auto succ_l_val = access_value(succ_l);
        auto succ_r_val = access_value(succ_r);
        write_value(heap_idx, succ_l_val + succ_r_val);
    }
}

void CBT_CPU_Vis::init_for_depth(uint32_t depth) noexcept
{
    memset(m_binary_heap.data(), 0, m_binary_heap.size());
    auto start = (1 << (m_max_depth - 1)) - 1;
    auto end = (1 << (m_max_depth)) - 1;
    for (uint32_t heap_idx = start; heap_idx < end; ++heap_idx)
    {
        write_value(heap_idx, 1);
    }
    sum_reduction();
}

uint32_t CBT_CPU_Vis::calculate_num_bits(uint32_t depth) noexcept
{
    return (1 << (depth + 2)) - (depth + 3);
}

uint32_t CBT_CPU_Vis::calculate_binary_heap_size(uint32_t depth) noexcept
{
    // return calculate_num_bits(m_max_depth) / (sizeof(uint32_t) * 8) + 1;
    return (1 << (depth + 1)) - 1;
}

}
