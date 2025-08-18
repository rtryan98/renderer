#include "renderer/window.hpp"

#include <backends/imgui_impl_sdl3.h>
#include "SDL3/SDL.h"

namespace ren
{
constexpr static auto SDL_WINDOW_CREATE_FLAGS = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

class Window_SDL3 : public Window
{
public:
    Window_SDL3(const Window_Create_Info& create_info)
        : m_sdl_window(SDL_CreateWindow(
            create_info.title,
            static_cast<int32_t>(create_info.width),
            static_cast<int32_t>(create_info.height),
            create_info.borderless ? SDL_WINDOW_BORDERLESS : SDL_WINDOW_CREATE_FLAGS))
        , m_data {
            .width = create_info.width,
            .height = create_info.height,
            .is_alive = true,
            .dpi_aware_size = create_info.dpi_aware_size
        }
    {
        ImGui_ImplSDL3_InitForOther(m_sdl_window);
    }

    virtual ~Window_SDL3() noexcept override
    {
        ImGui_ImplSDL3_Shutdown();
        SDL_DestroyWindow(m_sdl_window);
    }

    void update() noexcept override
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                m_data.is_alive = false;
                break;
            default:
                ImGui_ImplSDL3_ProcessEvent(&event);
                break;
            }
        }
        int32_t w, h;
        SDL_GetWindowSize(m_sdl_window, &w, &h);
        m_data.width = static_cast<uint32_t>(w);
        m_data.height = static_cast<uint32_t>(h);
        ImGui_ImplSDL3_NewFrame();
    }

    virtual float get_dpi_scale() noexcept override
    {
        return SDL_GetWindowDisplayScale(m_sdl_window);
    }

    void* get_native_handle() noexcept override
    {
        const auto properties = SDL_GetWindowProperties(m_sdl_window);
        void* hwnd = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        return hwnd;
    }

    [[nodiscard]] const Window_Data& get_window_data() const noexcept override
    {
        return m_data;
    }
private:
    SDL_Window* m_sdl_window;
    Window_Data m_data;
};

std::unique_ptr<Window> Window::create(const Window_Create_Info& create_info) noexcept
{
    return std::make_unique<Window_SDL3>(create_info);
}

Input_State::Input_State(Window& window)
    : m_window(window)
{}

void Input_State::update()
{
    m_last_state = m_current_state;
    auto* keys = SDL_GetKeyboardState(nullptr);
    memcpy(m_current_state.data(), keys, m_current_state.size());

    m_last_mouse_pos = m_current_mouse_pos;
    m_last_mouse_state = m_current_mouse_state;
    m_current_mouse_state = SDL_GetMouseState(&m_current_mouse_pos.x, &m_current_mouse_pos.y);
}

bool Input_State::is_key_released(Key_Code key) const noexcept
{
    return m_last_state[static_cast<std::size_t>(key)]
        && !m_current_state[static_cast<std::size_t>(key)];
}

bool Input_State::is_key_pressed(Key_Code key) const noexcept
{
    return m_current_state[static_cast<std::size_t>(key)];
}

bool Input_State::is_key_clicked(Key_Code key) const noexcept
{
    return !m_last_state[static_cast<std::size_t>(key)]
        && m_current_state[static_cast<std::size_t>(key)];
}

bool Input_State::is_mouse_released(Mouse_Button mb) const noexcept
{
    return (SDL_BUTTON_MASK(static_cast<int32_t>(mb)) & m_last_mouse_state
        & (!SDL_BUTTON_MASK(static_cast<int32_t>(mb)) & m_current_mouse_state)) > 0;
}

bool Input_State::is_mouse_pressed(Mouse_Button mb) const noexcept
{
    return (SDL_BUTTON_MASK(static_cast<int32_t>(mb)) & m_current_mouse_state) > 0;
}

bool Input_State::is_mouse_clicked(Mouse_Button mb) const noexcept
{
    return (!(SDL_BUTTON_MASK(static_cast<int32_t>(mb)) & m_last_mouse_state)
        & SDL_BUTTON_MASK(static_cast<int32_t>(mb)) & m_current_mouse_state) > 0;
}

const glm::vec2 Input_State::get_mouse_pos() const noexcept
{
    return m_current_mouse_pos;
}

const glm::vec2 Input_State::get_mouse_pos_delta() const noexcept
{
    return { m_current_mouse_pos.x - m_last_mouse_pos.x, m_current_mouse_pos.y - m_last_mouse_pos.y };
}
}
