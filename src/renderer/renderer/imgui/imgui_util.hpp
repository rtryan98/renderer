#pragma once

namespace ren::imutil
{
class Context_Wrapper
{
public:
    Context_Wrapper() noexcept;
    ~Context_Wrapper() noexcept;
};

void help_marker(const char* text, bool is_same_line = true);
void push_negative_padding();
void push_minimum_window_size();

void set_dpi_scale(float scale);
float get_dpi_scale();
}
