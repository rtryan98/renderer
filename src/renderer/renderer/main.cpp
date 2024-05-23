#include <cstdint>
#include "renderer/imgui/imgui_util.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "renderer/application.hpp"

int32_t main([[maybe_unused]] uint32_t argc, [[maybe_unused]] const char* argv[])
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

    auto imgui_ctx = ren::imutil::Context_Wrapper();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    auto application = ren::Application();
    application.run();

    return 0;
}
