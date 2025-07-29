#pragma once

#include <array>
#include <memory>
#include <DirectXMath.h>
#include "renderer/input_codes.hpp"

namespace ren
{
using namespace DirectX;

enum class Mouse_Button : uint8_t;

struct Window_Create_Info
{
    uint32_t width;
    uint32_t height;
    const char* title;
    bool dpi_aware_size;
};

struct Window_Data
{
    uint32_t width;
    uint32_t height;
    bool is_alive;
    bool dpi_aware_size;
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

class Input_State
{
public:
    Input_State(Window& window);

    void update();

    bool is_key_released(Key_Code key) const noexcept;
    bool is_key_pressed(Key_Code key) const noexcept;
    bool is_key_clicked(Key_Code key) const noexcept;

    bool is_mouse_released(Mouse_Button mb) const noexcept;
    bool is_mouse_pressed(Mouse_Button mb) const noexcept;
    bool is_mouse_clicked(Mouse_Button mb) const noexcept;

    const XMFLOAT2 get_mouse_pos() const noexcept;
    const XMFLOAT2 get_mouse_pos_delta() const noexcept;

private:
    Window& m_window;
    std::array<bool, 512> m_current_state = {};
    std::array<bool, 512> m_last_state = {};
    uint32_t m_current_mouse_state = {};
    uint32_t m_last_mouse_state = {};
    XMFLOAT2 m_current_mouse_pos = {};
    XMFLOAT2 m_last_mouse_pos = {};
};
}
