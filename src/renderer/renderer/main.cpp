#include <cstdint>
#include <imgui.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "renderer/application.hpp"

int32_t main([[maybe_unused]] uint32_t argc, [[maybe_unused]] const char* argv[])
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    auto application = std::make_unique<ren::Application>();
    application->run();
    application = nullptr; // explicit delete for imgui

    ImGui::DestroyContext();
    return 0;
}
