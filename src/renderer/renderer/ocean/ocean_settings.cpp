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
"Use shader permutations that utilize fp16 maths (not yet implemented). "
"Expect loss of precision when used with a high size value.";

constexpr const char* OCEAN_HELP_TEXT_SPECTRUM =
"Lorem ipsum dolor sit amet.";

constexpr const char* OCEAN_HELP_TEXT_DIRECTIONAL_SPREAD =
"Lorem ipsum dolor sit amet.";

constexpr const char* OCEAN_HELP_TEXT_WIND_SPEED =
"Lorem ipsum dolor sit amet.";

constexpr const char* OCEAN_HELP_TEXT_GRAVITY =
"Lorem ipsum dolor sit amet.";

constexpr const char* OCEAN_HELP_TEXT_FETCH =
"Lorem ipsum dolor sit amet.";

constexpr const char* OCEAN_HELP_TEXT_DEPTH =
"Lorem ipsum dolor sit amet.";

constexpr const char* OCEAN_HELP_TEXT_PHILLIPS_ALPHA =
"Lorem ipsum dolor sit amet.";

constexpr const char* OCEAN_HELP_TEXT_GENERALIZED_A =
"Lorem ipsum dolor sit amet.";

constexpr const char* OCEAN_HELP_TEXT_GENERALIZED_B =
"Lorem ipsum dolor sit amet.";

constexpr const char* OCEAN_HELP_TEXT_CONTRIBUTION =
"Lorem ipsum dolor sit amet.";

constexpr const char* OCEAN_HELP_TEXT_LENGTHSCALE =
"Lorem ipsum dolor sit amet.";

Ocean_Settings::Ocean_Settings(
    Ocean_Resources& resources,
    Asset_Manager& asset_manager,
    Shader_Library_Legacy& shader_library)
    : Settings_Base("Ocean")
    , m_resources(resources)
    , m_asset_manager(asset_manager)
    , m_shader_library(shader_library)
{}

void Ocean_Settings::process_gui()
{
    process_gui_options();
    process_gui_simulation_settings();

    ImGui::SeparatorText("Debug");
    {
        ImGui::PushItemWidth(get_pad_scale());
        ImGui::Checkbox("Update Time", &m_resources.data.update_time);
    }
    {
        ImGui::PushItemWidth(get_pad_scale());
        ImGui::Checkbox("Debug Render Slopes", &m_resources.data.debug_render_slope);
    }
    {
        ImGui::PushItemWidth(get_pad_scale());
        ImGui::Checkbox("Debug Render Normals", &m_resources.data.debug_render_normal);
    }
}

void Ocean_Settings::process_gui_options()
{
    ImGui::SeparatorText("Options");

    auto options = m_resources.options;

    {
        constexpr static auto size_values = std::to_array({ 64, 128, 256, 512, 1024 });
        constexpr static auto size_value_texts = std::to_array({ "64", "128", "256", "512", "1024" });

        auto size_text_idx = 0;
        for (auto i = 0; i < size_value_texts.size(); ++i)
        {
            if (size_values[i] == options.size) size_text_idx = i;
        }
        ImGui::PushItemWidth(get_pad_scale());
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
        ImGui::PushItemWidth(get_pad_scale());
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
        ImGui::PushItemWidth(get_pad_scale());
        ImGui::Checkbox("Use fp16 textures", &options.use_fp16_textures);
        imutil::help_marker(OCEAN_HELP_TEXT_FP16_TEXTURES);
    }
    {
        ImGui::PushItemWidth(get_pad_scale());
        ImGui::Checkbox("Use fp16 maths", &options.use_fp16_maths);
        imutil::help_marker(OCEAN_HELP_TEXT_FP16_MATH);
    }

    auto options_before = m_resources.options;

    m_resources.options = options;
    m_resources.data.initial_spectrum_data.texture_size = options.size;

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
        m_resources.create_compute_pipelines(m_asset_manager, m_shader_library);
    }
}

void Ocean_Settings::process_gui_simulation_settings()
{
    ImGui::SeparatorText("Simulation Settings");

    auto& data = m_resources.data;

    ImGui::PushItemWidth(get_pad_scale());
    ImGui::SliderFloat("Gravity", &data.initial_spectrum_data.g, .001f, 100.f);
    imutil::help_marker(OCEAN_HELP_TEXT_GRAVITY);

    auto length_scales = std::to_array({
        &data.initial_spectrum_data.length_scales.x,
        &data.initial_spectrum_data.length_scales.y,
        &data.initial_spectrum_data.length_scales.z,
        &data.initial_spectrum_data.length_scales.w,
        });
    for (auto i = 0; i < 4; ++i)
    {
        auto lengthscale_str = std::string("Lengthscale ") + std::to_string(i + 1);
        ImGui::PushItemWidth(get_pad_scale());
        ImGui::SliderFloat(lengthscale_str.c_str(), length_scales[i], .001f, 10000.f);
        imutil::help_marker(OCEAN_HELP_TEXT_LENGTHSCALE);
    }

    auto depth_str = std::string("Depth");
    ImGui::PushItemWidth(get_pad_scale());
    ImGui::SliderFloat(depth_str.c_str(), &data.initial_spectrum_data.h, .001f, 1000.f);
    imutil::help_marker(OCEAN_HELP_TEXT_DEPTH);

    constexpr static auto spectrum_values = std::to_array({
        Ocean_Spectrum::Phillips,
        Ocean_Spectrum::Pierson_Moskowitz,
        Ocean_Spectrum::Generalized_A_B,
        Ocean_Spectrum::Jonswap,
        Ocean_Spectrum::TMA });
    constexpr static auto spectrum_value_texts = std::to_array({
        "Phillips",
        "Pierson_Moskowitz",
        "Generalized A,B",
        "Jonswap",
        "TMA" });

    auto spectrum_text_idx = 0;
    for (auto i = 0; i < spectrum_value_texts.size(); ++i)
    {
        if (uint32_t(spectrum_values[i]) == data.initial_spectrum_data.spectrum) spectrum_text_idx = i;
    }
    ImGui::PushItemWidth(get_pad_scale());
    if (ImGui::BeginCombo("Oceanographic Spectrum", spectrum_value_texts[spectrum_text_idx]))
    {
        for (auto i = 0; i < spectrum_values.size(); ++i)
        {
            bool selected = data.initial_spectrum_data.spectrum == uint32_t(spectrum_values[i]);
            if (ImGui::Selectable(spectrum_value_texts[i], selected, ImGuiSelectableFlags_None))
            {
                data.initial_spectrum_data.spectrum = uint32_t(spectrum_values[i]);
            }
        }
        ImGui::EndCombo();
    }
    imutil::help_marker(OCEAN_HELP_TEXT_SPECTRUM);

    constexpr static auto dirspread_values = std::to_array({
        Ocean_Directional_Spreading_Function::Positive_Cosine_Squared,
        Ocean_Directional_Spreading_Function::Mitsuyasu,
        Ocean_Directional_Spreading_Function::Hasselmann,
        Ocean_Directional_Spreading_Function::Donelan_Banner,
        Ocean_Directional_Spreading_Function::Flat });
    constexpr static auto dirspread_value_texts = std::to_array({
        "Positive Cosine Squared",
        "Mitsuyasu",
        "Hasselmann",
        "Donelan Banner",
        "Flat" });

    auto dirspread_text_idx = 0;
    for (auto i = 0; i < dirspread_value_texts.size(); ++i)
    {
        if (uint32_t(dirspread_values[i]) == data.initial_spectrum_data.directional_spreading_function) dirspread_text_idx = i;
    }
    ImGui::PushItemWidth(get_pad_scale());
    if (ImGui::BeginCombo("Directional Spread", dirspread_value_texts[dirspread_text_idx]))
    {
        for (auto i = 0; i < dirspread_values.size(); ++i)
        {
            bool selected = data.initial_spectrum_data.directional_spreading_function == uint32_t(dirspread_values[i]);
            if (ImGui::Selectable(dirspread_value_texts[i], selected, ImGuiSelectableFlags_None))
            {
                data.initial_spectrum_data.directional_spreading_function = uint32_t(dirspread_values[i]);
            }
        }
        ImGui::EndCombo();
    }
    imutil::help_marker(OCEAN_HELP_TEXT_DIRECTIONAL_SPREAD);

    uint32_t spectrum_count = 0;
    for (auto& spectrum : data.initial_spectrum_data.spectra)
    {
        if (spectrum_count == 0)
        {
            ImGui::SeparatorText("Main spectrum options");
        }
        else
        {
            ImGui::SeparatorText("Second spectrum options");
        }

        auto wind_speed_str = std::string("Wind Speed##") + std::to_string(spectrum_count);
        ImGui::PushItemWidth(get_pad_scale());
        ImGui::SliderFloat(wind_speed_str.c_str(), &spectrum.u, .001f, 100.f);
        imutil::help_marker(OCEAN_HELP_TEXT_WIND_SPEED);

        auto fetch_str = std::string("Fetch##") + std::to_string(spectrum_count);
        ImGui::PushItemWidth(get_pad_scale());
        ImGui::SliderFloat(fetch_str.c_str(), &spectrum.f, .001f, 1000.f);
        imutil::help_marker(OCEAN_HELP_TEXT_FETCH);

        auto phillips_alpha_str = std::string("Phillips Coefficient Alpha##") + std::to_string(spectrum_count);
        ImGui::PushItemWidth(get_pad_scale());
        ImGui::SliderFloat(phillips_alpha_str.c_str(), &spectrum.phillips_alpha, .001f, 100.f);
        imutil::help_marker(OCEAN_HELP_TEXT_PHILLIPS_ALPHA);

        auto generalized_a_str = std::string("Generalized Coefficient A##") + std::to_string(spectrum_count);
        ImGui::PushItemWidth(get_pad_scale());
        ImGui::SliderFloat(generalized_a_str.c_str(), &spectrum.generalized_a, .001f, 100.f);
        imutil::help_marker(OCEAN_HELP_TEXT_GENERALIZED_A);

        auto generalized_b_str = std::string("Generalized Coefficient B##") + std::to_string(spectrum_count);
        ImGui::PushItemWidth(get_pad_scale());
        ImGui::SliderFloat(generalized_b_str.c_str(), &spectrum.generalized_b, .001f, 100.f);
        imutil::help_marker(OCEAN_HELP_TEXT_GENERALIZED_B);

        auto contribution_str = std::string("Contribution##") + std::to_string(spectrum_count);
        ImGui::PushItemWidth(get_pad_scale());
        ImGui::SliderFloat(contribution_str.c_str(), &spectrum.contribution, .0f, 1.f);
        imutil::help_marker(OCEAN_HELP_TEXT_CONTRIBUTION);

        ++spectrum_count;
    }
}
}
