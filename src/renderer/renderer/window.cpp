#include "renderer/window.hpp"

#include <Windows.h>
#include <immintrin.h>

namespace ren
{
LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    Window_Data* data = reinterpret_cast<Window_Data*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

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
            .is_alive = true
        }
    {
        RECT wr = {
            .left = LONG((GetSystemMetrics(SM_CXSCREEN) - create_info.width) / 2),
            .top = LONG((GetSystemMetrics(SM_CYSCREEN) - create_info.height) / 2),
            .right = LONG(create_info.width),
            .bottom = LONG(create_info.height)
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
    }

    virtual ~Window_Win32() noexcept override
    {
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
}
