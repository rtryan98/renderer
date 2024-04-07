#pragma once

#include <vector>

namespace ren
{
class CBT_CPU_Vis
{
public:
    CBT_CPU_Vis() noexcept;
    void imgui_window(bool& open) noexcept;

private:
    void imgui_init_at_depth() noexcept;
    void imgui_show_tree() noexcept;

    uint32_t access_value(uint32_t heap_idx) noexcept;
    void write_value(uint32_t heap_idx, uint32_t value) noexcept;
    uint32_t heap_successor_left(uint32_t heap_idx) noexcept;
    uint32_t heap_successor_right(uint32_t heap_idx) noexcept;

    void sum_reduction() noexcept;
    void init_for_depth(uint32_t depth) noexcept;

    uint32_t calculate_num_bits(uint32_t depth) noexcept;
    uint32_t calculate_binary_heap_size(uint32_t depth) noexcept;

private:
    uint32_t m_max_depth;
    std::vector<uint32_t> m_binary_heap;
};
}
