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
}
