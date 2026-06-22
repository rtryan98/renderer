#include <cstdint>
#include <TClap/CmdLine.h>

#include "renderer/imgui/imgui_util.hpp"
#include "renderer/application.hpp"

#include <imgui.h>

int32_t main(uint32_t argc, const char* argv[])
{
    constexpr static uint32_t WINDOW_DEFAULT_WIDTH = 2560;
    constexpr static uint32_t WINDOW_DEFAULT_HEIGHT = 1440;

    ren::Application_Create_Info app_create_info = {
        .width = WINDOW_DEFAULT_WIDTH,
        .height = WINDOW_DEFAULT_HEIGHT,
        .enable_validation = false,
        .enable_gpu_validation = false,
        .graphics_api = rhi::Graphics_API::D3D12
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
        TCLAP::ValueArg<int32_t> log_level_arg(
            "l",
            "log-level",
            "Set log level. (0-5: [trace, debug, info, warn, error, critical])",
            false,
            2,
            "int");
        cmd.add(log_level_arg);
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
        TCLAP::SwitchArg fullscreen_arg(
            "f",
            "fullscreen",
            "Start in fullscreen.",
            false);
        cmd.add(fullscreen_arg);
        TCLAP::ValueArg<int32_t> graphics_api_arg(
            "g",
            "graphics-api",
            "Sets the graphics API. '0' for D3D12, '1' for Vulkan.",
            false,
            0,
            "int");
        cmd.add(graphics_api_arg);
        cmd.parse(argc, argv);

        app_create_info.width = window_width_arg.getValue();
        app_create_info.height = window_height_arg.getValue();
        app_create_info.fullscreen = fullscreen_arg.getValue();
        app_create_info.enable_validation = validation_arg.getValue();
        app_create_info.enable_gpu_validation = gpu_validation_arg.getValue();
        app_create_info.log_level = log_level_arg.getValue();
        app_create_info.graphics_api = static_cast<rhi::Graphics_API>(graphics_api_arg.getValue());

        // Validate that a valid graphics API was passed.
        if (static_cast<uint32_t>(app_create_info.graphics_api) > 1u) app_create_info.graphics_api = rhi::Graphics_API::D3D12;

        if (app_create_info.graphics_api == rhi::Graphics_API::D3D12)
            printf("Using D3D12.\n");
        else
            printf("Using Vulkan.\n");
        if (app_create_info.enable_validation) printf("Validation enabled.\n");
        if (app_create_info.enable_gpu_validation) printf("GPU Validation enabled.\n");
    }
    catch (...)
    {
        printf("Failed to parse command line arguments. Starting with default options.\n");
    }

    auto imgui_ctx = ren::imutil::Context_Wrapper();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;
    io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;

    auto application = ren::Application(app_create_info);
    application.run();

    return 0;
}
