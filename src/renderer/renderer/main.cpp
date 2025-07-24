#include <cstdint>
#include "renderer/imgui/imgui_util.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <TClap/CmdLine.h>

#include "renderer/application.hpp"

int32_t main(uint32_t argc, const char* argv[])
{
    constexpr static uint32_t WINDOW_DEFAULT_WIDTH = 2560;
    constexpr static uint32_t WINDOW_DEFAULT_HEIGHT = 1440;

    ren::Application_Create_Info app_create_info = {
        .width = WINDOW_DEFAULT_WIDTH,
        .height = WINDOW_DEFAULT_HEIGHT,
        .enable_validation = false,
        .enable_gpu_validation = false
    };

    try
    {
        TCLAP::CmdLine cmd("Renderer", ' ', "0.1", false);
        TCLAP::ValueArg<int32_t> window_width_arg(
            "w",
            "width",
            "Set window width.",
            false,
            WINDOW_DEFAULT_WIDTH,
            "int");
        cmd.add(window_width_arg);
        TCLAP::ValueArg<int32_t> window_height_arg(
            "h",
            "height",
            "Set window height.",
            false,
            WINDOW_DEFAULT_HEIGHT,
            "int");
        cmd.add(window_height_arg);
        TCLAP::SwitchArg validation_arg(
            "v",
            "validation-enable",
            "Enable validation layers.",
            false);
        cmd.add(validation_arg);
        TCLAP::SwitchArg gpu_validation_arg(
            "V",
            "validation-enable-gpu-based",
            "Enable gpu-based validation layers.",
            false);
        cmd.add(gpu_validation_arg);
        cmd.parse(argc, argv);

        app_create_info.width = window_width_arg.getValue();
        app_create_info.height = window_height_arg.getValue();
        app_create_info.enable_validation = validation_arg.getValue();
        app_create_info.enable_gpu_validation = gpu_validation_arg.getValue();
    }
    catch (...)
    {} // do nothing on error.

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

    auto imgui_ctx = ren::imutil::Context_Wrapper();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    auto application = ren::Application(app_create_info);
    application.run();

    return 0;
}
