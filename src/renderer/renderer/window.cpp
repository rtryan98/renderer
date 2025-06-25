#include "renderer/window.hpp"

#include <Windows.h>
#include <immintrin.h>
#include <backends/imgui_impl_win32.h>
#include <ShellScalingApi.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace ren
{
LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    Window_Data* data = reinterpret_cast<Window_Data*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
        return true;

    switch (msg)
    {
    case WM_CLOSE:
    {
        if (data)
        {
            data->is_alive = false;
        }
        break;
    }
    case WM_GETMINMAXINFO:
    {
        auto& size = reinterpret_cast<LPMINMAXINFO>(lparam)->ptMinTrackSize;
        size.x = 256;
        size.y = 144;
        break;
    }
    case WM_SIZE:
    {
        if (data)
        {
            RECT rect;
            GetClientRect(hwnd, &rect);
            data->width = rect.right;
            data->height = rect.bottom;
        }
        break;
    }
    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

class Window_Win32 final : public Window
{
public:
    Window_Win32(const Window_Create_Info& create_info)
        : m_hwnd(nullptr)
        , m_data{
            .width = create_info.width,
            .height = create_info.height,
            .is_alive = true,
            .dpi_aware_size = create_info.dpi_aware_size
        }
    {
        auto dpi_scale = m_data.dpi_aware_size ? get_dpi_scale() : 1.f;
        RECT wr = {
            .left = LONG((GetSystemMetrics(SM_CXSCREEN) - dpi_scale * create_info.width) / 2),
            .top = LONG((GetSystemMetrics(SM_CYSCREEN) - dpi_scale * create_info.height) / 2),
            .right = LONG(dpi_scale * create_info.width),
            .bottom = LONG(dpi_scale * create_info.height)
        };
        WNDCLASSEX wc = {
            .cbSize = sizeof(WNDCLASSEX),
            .style = 0,
            .lpfnWndProc = wnd_proc,
            .cbClsExtra = 0,
            .cbWndExtra = 0,
            .hInstance = GetModuleHandle(NULL),
            .hIcon = LoadIcon(NULL, IDI_WINLOGO),
            .hCursor = LoadCursor(NULL, IDC_ARROW),
            .hbrBackground = HBRUSH(GetStockObject(BLACK_BRUSH)),
            .lpszMenuName = nullptr,
            .lpszClassName = create_info.title,
            .hIconSm = wc.hIcon
        };
        RegisterClassEx(&wc);
        m_hwnd = CreateWindowEx(
            0,
            wc.lpszClassName,
            wc.lpszClassName,
            WS_OVERLAPPEDWINDOW,
            wr.left,
            wr.top,
            wr.right,
            wr.bottom,
            nullptr,
            nullptr,
            GetModuleHandle(nullptr),
            0);
        SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)(&m_data));
        ShowWindow(m_hwnd, SW_SHOWDEFAULT);
        SetForegroundWindow(m_hwnd);
        SetFocus(m_hwnd);
        m_data.is_alive = true;

        ImGui_ImplWin32_Init(m_hwnd);
    }

    virtual ~Window_Win32() noexcept override
    {
        ImGui_ImplWin32_Shutdown();
        DestroyWindow(m_hwnd);
    }

    virtual void update() noexcept override
    {
        MSG msg = {};
        ZeroMemory(&msg, sizeof(MSG));
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        ImGui_ImplWin32_NewFrame();
    }

    virtual float get_dpi_scale() noexcept
    {
        DEVICE_SCALE_FACTOR scale_factor = {};
        if (m_hwnd)
            GetScaleFactorForMonitor(MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST), &scale_factor);
        else
            GetScaleFactorForMonitor(MonitorFromPoint({}, MONITOR_DEFAULTTOPRIMARY), &scale_factor);

        switch (scale_factor)
        {
        case DEVICE_SCALE_FACTOR_INVALID:
            return 1.f;
        case SCALE_100_PERCENT:
            return 1.f;
        case SCALE_120_PERCENT:
            return 1.2f;
        case SCALE_125_PERCENT:
            return 1.25f;
        case SCALE_140_PERCENT:
            return 1.4f;
        case SCALE_150_PERCENT:
            return 1.5f;
        case SCALE_160_PERCENT:
            return 1.6f;
        case SCALE_175_PERCENT:
            return 1.75f;
        case SCALE_180_PERCENT:
            return 1.8f;
        case SCALE_200_PERCENT:
            return 2.f;
        case SCALE_225_PERCENT:
            return 2.25f;
        case SCALE_250_PERCENT:
            return 2.5f;
        case SCALE_300_PERCENT:
            return 3.f;
        case SCALE_350_PERCENT:
            return 3.5f;
        case SCALE_400_PERCENT:
            return 4.f;
        case SCALE_450_PERCENT:
            return 4.5f;
        case SCALE_500_PERCENT:
            return 5.f;
        default:
            return 1.f;
        }
    }

    virtual void* get_native_handle() noexcept override
    {
        return static_cast<void*>(m_hwnd);
    }

    virtual const Window_Data& get_window_data() const noexcept override
    {
        return m_data;
    }

private:
    HWND m_hwnd;
    Window_Data m_data;
};

std::unique_ptr<Window> Window::create(const Window_Create_Info& create_info) noexcept
{
    return std::make_unique<Window_Win32>(create_info);
}

Input_State::Input_State(Window& window)
    : m_window(window)
{}

void Input_State::update()
{
    auto hwnd = reinterpret_cast<HWND>(m_window.get_native_handle());

    m_last_state = m_current_state;
    [[maybe_unused]] auto state_result = GetKeyboardState(m_current_state.data());

    m_last_mouse_pos = m_current_mouse_pos;
    POINT cursor_pos;
    GetCursorPos(&cursor_pos);
    ScreenToClient(hwnd, &cursor_pos);
    m_current_mouse_pos = { float(cursor_pos.x), float(cursor_pos.y) };
}

uint8_t high_bit(uint8_t x)
{
    return x & 0x80;
}

bool Input_State::is_key_released(Key_Code key) const noexcept
{
    return high_bit(m_last_state[static_cast<uint8_t>(key)])
        && !high_bit(m_current_state[static_cast<uint8_t>(key)]);
}

bool Input_State::is_key_pressed(Key_Code key) const noexcept
{
    return high_bit(m_current_state[static_cast<uint8_t>(key)]);
}

bool Input_State::is_key_clicked(Key_Code key) const noexcept
{
    return !high_bit(m_last_state[static_cast<uint8_t>(key)])
        && high_bit(m_current_state[static_cast<uint8_t>(key)]);
}

bool Input_State::is_mouse_released(Mouse_Button mb) const noexcept
{
    return high_bit(m_last_state[static_cast<uint8_t>(mb)])
        && !high_bit(m_current_state[static_cast<uint8_t>(mb)]);
}

bool Input_State::is_mouse_pressed(Mouse_Button mb) const noexcept
{
    return high_bit(m_current_state[static_cast<uint8_t>(mb)]);
}

bool Input_State::is_mouse_clicked(Mouse_Button mb) const noexcept
{
    return !high_bit(m_last_state[static_cast<uint8_t>(mb)])
        && high_bit(m_current_state[static_cast<uint8_t>(mb)]);
}
const XMFLOAT2 Input_State::get_mouse_pos() const noexcept
{
    return m_current_mouse_pos;
}

const XMFLOAT2 Input_State::get_mouse_pos_delta() const noexcept
{
    return { m_current_mouse_pos.x - m_last_mouse_pos.x, m_current_mouse_pos.y - m_last_mouse_pos.y };
}
}
