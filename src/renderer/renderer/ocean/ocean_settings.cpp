#include "renderer/ocean/ocean_settings.hpp"
#include "renderer/ocean/ocean_resources.hpp"
#include "renderer/asset_manager.hpp"
#include "renderer/shader_manager.hpp"
#include "renderer/imgui/imgui_util.hpp"

#include <array>
#include <imgui.h>

namespace ren
{
constexpr const char* OCEAN_HELP_TEXT_SIZE =
"Lorem ipsum dolor sit amet.";

constexpr const char* OCEAN_HELP_TEXT_CASCADES =
"Lorem ipsum dolor sit amet.";

constexpr const char* OCEAN_HELP_TEXT_FP16_TEXTURES =
"Use fp16 textures instead of fp32 textures. "
"Expect loss of precision when used with a high size value.";

constexpr const char* OCEAN_HELP_TEXT_FP16_MATH =
"Use shader permutations that utilize fp16 maths. "
"Expect loss of precision when used with a high size value.";

Ocean_Settings::Ocean_Settings(
    Ocean_Resources& resources,
    Asset_Manager& asset_manager,
    Shader_Library& shader_library)
    : Settings_Base("Ocean")
    , m_resources(resources)
    , m_asset_manager(asset_manager)
    , m_shader_library(shader_library)
{}

void Ocean_Settings::process_gui()
{
    process_gui_options();
    process_gui_simulation_settings();
}

void Ocean_Settings::process_gui_options()
{
    ImGui::SeparatorText("Options");

    auto options = m_resources.options;

    static float dir = 0.f;
    {
        constexpr static auto size_values = std::to_array({ 64, 128, 256, 512, 1024 });
        constexpr static auto size_value_texts = std::to_array({ "64", "128", "256", "512", "1024" });

        auto size_text_idx = 0;
        for (auto i = 0; i < size_value_texts.size(); ++i)
        {
            if (size_values[i] == options.size) size_text_idx = i;
        }
        ImGui::PushItemWidth(CONTENT_NEGATIVE_PAD);
        if (ImGui::BeginCombo("Size", size_value_texts[size_text_idx]))
        {
            for (auto i = 0; i < size_values.size(); ++i)
            {
                bool selected = options.size == size_values[i];
                if (ImGui::Selectable(size_value_texts[i], selected, ImGuiSelectableFlags_None))
                {
                    options.size = size_values[i];
                }
            }
            ImGui::EndCombo();
        }
        imutil::help_marker(OCEAN_HELP_TEXT_SIZE);
    }
    {
        constexpr static auto cascade_values = std::to_array({ 1, 2, 3, 4 });
        constexpr static auto cascade_value_texts = std::to_array({ "1", "2", "3", "4" });

        auto cascade_text_idx = 0;
        for (auto i = 0; i < cascade_value_texts.size(); ++i)
        {
            if (cascade_values[i] == options.cascade_count) cascade_text_idx = i;
        }
        ImGui::PushItemWidth(CONTENT_NEGATIVE_PAD);
        if (ImGui::BeginCombo("Cascade count", cascade_value_texts[cascade_text_idx]))
        {
            for (auto i = 0; i < cascade_values.size(); ++i)
            {
                bool selected = options.cascade_count == cascade_values[i];
                if (ImGui::Selectable(cascade_value_texts[i], selected, ImGuiSelectableFlags_None))
                {
                    options.cascade_count = cascade_values[i];
                }
            }
            ImGui::EndCombo();
        }
        imutil::help_marker(OCEAN_HELP_TEXT_CASCADES);
    }
    {
        ImGui::PushItemWidth(CONTENT_NEGATIVE_PAD);
        ImGui::Checkbox("Use fp16 textures", &options.use_fp16_textures);
        imutil::help_marker(OCEAN_HELP_TEXT_FP16_TEXTURES);
    }
    {
        ImGui::PushItemWidth(CONTENT_NEGATIVE_PAD);
        ImGui::Checkbox("Use fp16 maths", &options.use_fp16_maths);
        imutil::help_marker(OCEAN_HELP_TEXT_FP16_MATH);
    }

    auto options_before = m_resources.options;

    m_resources.options = options;

    bool recreate_textures =
        options_before.size != options.size ||
        options_before.use_fp16_textures != options.use_fp16_textures ||
        options_before.cascade_count != options.cascade_count;
    if (recreate_textures)
    {
        m_resources.create_textures(m_asset_manager);
    }

    bool recreate_pipelines =
        options_before.use_fp16_maths != options.use_fp16_maths ||
        options_before.size != options.size;
    if (recreate_pipelines)
    {
        m_resources.create_pipelines(m_asset_manager, m_shader_library);
    }
}

void Ocean_Settings::process_gui_simulation_settings()
{
    ImGui::SeparatorText("Simulation Settings");
}
}
