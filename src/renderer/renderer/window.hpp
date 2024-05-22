#pragma once

#include <memory>

namespace ren
{
struct Window_Create_Info
{
    uint32_t width;
    uint32_t height;
    const char* title;
};

struct Window_Data
{
    uint32_t width;
    uint32_t height;
    bool is_alive;
};

class Window
{
public:
    static std::unique_ptr<Window> create(const Window_Create_Info& create_info) noexcept;


    virtual ~Window() noexcept = default;

    virtual void update() noexcept = 0;

    virtual float get_dpi_scale() noexcept = 0;
    virtual void* get_native_handle() noexcept = 0;
    virtual const Window_Data& get_window_data() const noexcept = 0;
};
}
