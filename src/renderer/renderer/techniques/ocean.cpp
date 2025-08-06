#include "renderer/techniques/ocean.hpp"
#include "renderer/resource_state_tracker.hpp"
#include "renderer/asset/asset_repository.hpp"
#include "renderer/gpu_transfer.hpp"
#include "renderer/imgui/imgui_util.hpp"

#include <rhi/command_list.hpp>
#include <shared/ocean_shared_types.h>
#include <shared/fft_shared_types.h>

#include <imgui.h>

namespace ren::techniques
{
Ocean::Ocean(Asset_Repository& asset_repository, GPU_Transfer_Context& gpu_transfer_context,
    Render_Resource_Blackboard& render_resource_blackboard, uint32_t width, uint32_t height)
    : m_asset_repository(asset_repository)
    , m_gpu_transfer_context(gpu_transfer_context)
    , m_render_resource_blackboard(render_resource_blackboard)
{
    m_spectrum_parameters_buffer = m_render_resource_blackboard.create_buffer(
        SPECTRUM_PARAMETERS_BUFFER_NAME,
        {
            .size = sizeof(Ocean_Initial_Spectrum_Data),
            .heap = rhi::Memory_Heap_Type::GPU
        });
    m_spectrum_state_texture = m_render_resource_blackboard.create_image(
        SPECTRUM_STATE_TEXTURE_NAME, m_options.generate_create_info(true));
    m_spectrum_angular_frequency_texture = m_render_resource_blackboard.create_image(
        SPECTRUM_ANGULAR_FREQUENCY_TEXTURE_NAME, m_options.generate_create_info(false));
    m_displacement_x_y_z_xdx_texture = m_render_resource_blackboard.create_image(
        DISPLACEMENT_X_Y_Z_XDX_TEXTURE_NAME, m_options.generate_create_info(true));
    m_displacement_ydx_zdx_zdz_zdy_texture = m_render_resource_blackboard.create_image(
        DISPLACEMENT_YDX_ZDX_YDY_ZDY_TEXTURE_NAME, m_options.generate_create_info(true));
    const rhi::Image_Create_Info depth_stencil_create_info = {
        .format = rhi::Image_Format::D32_SFLOAT,
        .width = width,
        .height = height,
        .depth = 1,
        .array_size = 1,
        .mip_levels = 1,
        .usage = rhi::Image_Usage::Depth_Stencil_Attachment,
        .primary_view_type = rhi::Image_View_Type::Texture_2D
    };
    m_forward_pass_depth_render_target = m_render_resource_blackboard.create_image(
        FORWARD_PASS_DEPTH_RENDER_TARGET_NAME, depth_stencil_create_info);

    m_displacement_sampler = m_render_resource_blackboard.get_sampler({
        .filter_min = rhi::Sampler_Filter::Linear,
        .filter_mag = rhi::Sampler_Filter::Linear,
        .filter_mip = rhi::Sampler_Filter::Linear,
        .address_mode_u = rhi::Image_Sample_Address_Mode::Wrap,
        .address_mode_v = rhi::Image_Sample_Address_Mode::Wrap,
        .address_mode_w = rhi::Image_Sample_Address_Mode::Wrap,
        .mip_lod_bias = 0.f,
        .max_anisotropy = 16,
        .comparison_func = rhi::Comparison_Func::None,
        .reduction = rhi::Sampler_Reduction_Type::Standard,
        .border_color = {},
        .min_lod = .0f,
        .max_lod = .0f,
        .anisotropy_enable = true
    });
}

Ocean::~Ocean()
{
    m_render_resource_blackboard.destroy_buffer(m_spectrum_parameters_buffer);
    m_render_resource_blackboard.destroy_image(m_spectrum_state_texture);
    m_render_resource_blackboard.destroy_image(m_spectrum_angular_frequency_texture);
    m_render_resource_blackboard.destroy_image(m_displacement_x_y_z_xdx_texture);
    m_render_resource_blackboard.destroy_image(m_displacement_ydx_zdx_zdz_zdy_texture);
    m_render_resource_blackboard.destroy_image(m_forward_pass_depth_render_target);
}

void Ocean::simulate(
    rhi::Command_List* cmd,
    Resource_State_Tracker& tracker,
    const float dt)
{
    if (!m_options.enabled) return;

    if (m_options.update_time)
        m_simulation_data.total_time += dt;

    cmd->begin_debug_region("ocean:simulation", 0.5f, 0.5f, 1.f);
    Ocean_Initial_Spectrum_Data gpu_spectrum_data = {
        .spectra = {
            {
                .u = m_simulation_data.full_spectrum_parameters.single_spectrum_parameters[0].wind_speed,
                .f = m_simulation_data.full_spectrum_parameters.single_spectrum_parameters[0].fetch,
                .phillips_alpha = m_simulation_data.full_spectrum_parameters.single_spectrum_parameters[0].phillips_alpha,
                .generalized_a = m_simulation_data.full_spectrum_parameters.single_spectrum_parameters[0].generalized_a,
                .generalized_b = m_simulation_data.full_spectrum_parameters.single_spectrum_parameters[0].generalized_b,
                .contribution = m_simulation_data.full_spectrum_parameters.single_spectrum_parameters[0].contribution,
                .wind_direction = m_simulation_data.full_spectrum_parameters.single_spectrum_parameters[0].wind_direction
            },
            {
                .u = m_simulation_data.full_spectrum_parameters.single_spectrum_parameters[1].wind_speed,
                .f = m_simulation_data.full_spectrum_parameters.single_spectrum_parameters[1].fetch,
                .phillips_alpha = m_simulation_data.full_spectrum_parameters.single_spectrum_parameters[1].phillips_alpha,
                .generalized_a = m_simulation_data.full_spectrum_parameters.single_spectrum_parameters[1].generalized_a,
                .generalized_b = m_simulation_data.full_spectrum_parameters.single_spectrum_parameters[1].generalized_b,
                .contribution = m_simulation_data.full_spectrum_parameters.single_spectrum_parameters[1].contribution,
                .wind_direction = m_simulation_data.full_spectrum_parameters.single_spectrum_parameters[1].wind_direction
            }
        },
        .active_cascades = m_simulation_data.full_spectrum_parameters.active_cascades,
        .length_scales = m_simulation_data.full_spectrum_parameters.length_scales,
        .spectrum = m_simulation_data.full_spectrum_parameters.oceanographic_spectrum,
        .directional_spreading_function = m_simulation_data.full_spectrum_parameters.directional_spreading_function,
        .texture_size = m_options.texture_size,
        .g = m_simulation_data.full_spectrum_parameters.gravity,
        .h = m_simulation_data.full_spectrum_parameters.depth
    };
    m_gpu_transfer_context.enqueue_immediate_upload(m_spectrum_parameters_buffer, gpu_spectrum_data);

    cmd->begin_debug_region("ocean:simulation:initial_spectrum", 0.25f, 0.0f, 1.0f);
    tracker.use_resource(m_spectrum_state_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.use_resource(m_spectrum_angular_frequency_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);
    auto initial_spectrum_pipeline = m_asset_repository.get_compute_pipeline("initial_spectrum");
    uint32_t dispatch_group_count = m_options.texture_size / initial_spectrum_pipeline.get_group_size_x();
    cmd->set_pipeline(initial_spectrum_pipeline);
    cmd->set_push_constants<Ocean_Initial_Spectrum_Push_Constants>({
        .data = m_spectrum_parameters_buffer,
        .spectrum_tex = m_spectrum_state_texture,
        .angular_frequency_tex = m_spectrum_angular_frequency_texture },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(
        dispatch_group_count,
        dispatch_group_count,
        m_options.cascade_count);
    cmd->end_debug_region(); // ocean:simulation:initial_spectrum

    cmd->begin_debug_region("ocean:simulation:time_dependent_spectrum", 0.25f, 0.25f, 1.0f);
    tracker.use_resource(m_spectrum_state_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(m_spectrum_angular_frequency_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(m_displacement_x_y_z_xdx_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.use_resource(m_displacement_ydx_zdx_zdz_zdy_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);
    cmd->set_pipeline(m_asset_repository.get_compute_pipeline("time_dependent_spectrum"));
    cmd->set_push_constants<Ocean_Time_Dependent_Spectrum_Push_Constants>({
        .initial_spectrum_tex = m_spectrum_state_texture,
        .angular_frequency_tex = m_spectrum_angular_frequency_texture,
        .x_y_z_xdx_tex = m_displacement_x_y_z_xdx_texture,
        .ydx_zdx_ydy_zdy_tex = m_displacement_ydx_zdx_zdz_zdy_texture,
        .texture_size = m_options.texture_size,
        .time = m_simulation_data.total_time},
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(
        dispatch_group_count,
        dispatch_group_count,
        m_options.cascade_count);
    cmd->end_debug_region(); // ocean:simulation:time_dependent_spectrum

    cmd->begin_debug_region("ocean:simulation:inverse_fft", 0.25f, 0.5f, 1.0f);
    auto select_fft_variant = [&]()
    {
        std::stringstream ss;
        ss << "fft_";
        ss << std::to_string(m_options.texture_size);
        if (m_options.use_fp16_maths) ss << "_fp16";
        ss << "_float4";
        return ss.str();
    };
    cmd->set_pipeline(m_asset_repository.get_compute_pipeline("fft").set_variant(select_fft_variant()));
    cmd->begin_debug_region("ocean:simulation:inverse_fft:vertical", 0.25f, 0.5f, 1.0f);
    tracker.use_resource(
        m_displacement_x_y_z_xdx_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Read,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.use_resource(
        m_displacement_ydx_zdx_zdz_zdy_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Read,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);
    cmd->set_push_constants<FFT_Push_Constants>({
            .image = m_displacement_x_y_z_xdx_texture,
            .vertical_or_horizontal = FFT_VERTICAL,
            .inverse = true },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(1, m_options.texture_size, m_options.cascade_count);
    cmd->set_push_constants<FFT_Push_Constants>({
            .image = m_displacement_ydx_zdx_zdz_zdy_texture,
            .vertical_or_horizontal = FFT_VERTICAL,
            .inverse = true },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(1, m_options.texture_size, m_options.cascade_count);
    tracker.set_resource_state(
        m_displacement_x_y_z_xdx_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.set_resource_state(
        m_displacement_ydx_zdx_zdz_zdy_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    cmd->end_debug_region(); // ocean:simulation:inverse_fft:vertical

    cmd->begin_debug_region("ocean:simulation:inverse_fft:horizontal", 0.25f, 0.5f, 1.0f);
    tracker.use_resource(
        m_displacement_x_y_z_xdx_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Read,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.use_resource(
        m_displacement_ydx_zdx_zdz_zdy_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Read,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);
    cmd->set_push_constants<FFT_Push_Constants>({
            .image = m_displacement_x_y_z_xdx_texture,
            .vertical_or_horizontal = FFT_HORIZONTAL,
            .inverse = true },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(1, m_options.texture_size, m_options.cascade_count);
    cmd->set_push_constants<FFT_Push_Constants>({
            .image = m_displacement_ydx_zdx_zdz_zdy_texture,
            .vertical_or_horizontal = FFT_HORIZONTAL,
            .inverse = true },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(1, m_options.texture_size, m_options.cascade_count);
    tracker.set_resource_state(
        m_displacement_x_y_z_xdx_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.set_resource_state(
        m_displacement_ydx_zdx_zdz_zdy_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    cmd->end_debug_region(); // ocean:simulation:inverse_fft:horizontal
    cmd->end_debug_region(); // ocean:simulation:inverse_fft

    cmd->end_debug_region(); // ocean:simulation
}

void Ocean::opaque_forward_pass(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker,
        const Buffer& camera,
        const Image& shaded_scene_render_target,
        const Image& shaded_scene_depth_render_target)
{
    if (!m_options.enabled) return;

    cmd->begin_debug_region("ocean:render:opaque_pass", 0.25f, 0.0f, 1.0f);

    tracker.use_resource(
        m_displacement_x_y_z_xdx_texture,
        rhi::Barrier_Pipeline_Stage::Pixel_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(
        m_displacement_ydx_zdx_zdz_zdy_texture,
        rhi::Barrier_Pipeline_Stage::Pixel_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(
        shaded_scene_render_target,
        rhi::Barrier_Pipeline_Stage::Color_Attachment_Output,
        rhi::Barrier_Access::Color_Attachment_Write,
        rhi::Barrier_Image_Layout::Color_Attachment);
    tracker.use_resource(
        shaded_scene_depth_render_target,
        rhi::Barrier_Pipeline_Stage::Early_Fragment_Tests,
        rhi::Barrier_Access::Depth_Stencil_Attachment_Write,
        rhi::Barrier_Image_Layout::Depth_Stencil_Write);
    tracker.flush_barriers(cmd);

    auto color_attachment_infos = std::to_array<rhi::Render_Pass_Color_Attachment_Info>(
        {{
            .attachment = shaded_scene_render_target,
            .load_op = rhi::Render_Pass_Attachment_Load_Op::Load,
            .store_op = rhi::Render_Pass_Attachment_Store_Op::Store,
            .clear_value = {}
        }});
    const rhi::Render_Pass_Begin_Info render_pass_info = {
        .color_attachments = color_attachment_infos,
        .depth_stencil_attachment = {
            .attachment = shaded_scene_depth_render_target,
            .depth_load_op = rhi::Render_Pass_Attachment_Load_Op::Load,
            .depth_store_op = rhi::Render_Pass_Attachment_Store_Op::Store,
            .stencil_load_op = rhi::Render_Pass_Attachment_Load_Op::No_Access,
            .stencil_store_op = rhi::Render_Pass_Attachment_Store_Op::No_Access,
            .clear_value = {}
        }
    };
    cmd->begin_render_pass(render_pass_info);

    const uint32_t width = static_cast<rhi::Image*>(shaded_scene_render_target)->width;
    const uint32_t height = static_cast<rhi::Image*>(shaded_scene_render_target)->height;

    cmd->set_viewport(0.f, 0.f,
        static_cast<float>(width), static_cast<float>(height), 0.f, 1.f);
    cmd->set_scissor(0, 0, width, height);

    constexpr static auto SIZE = 2048;
    cmd->set_pipeline(m_asset_repository.get_graphics_pipeline("ocean_render_patch"));
    cmd->set_push_constants<Ocean_Render_Patch_Push_Constants>({
        .length_scales = m_simulation_data.full_spectrum_parameters.length_scales,
        .tex_sampler = m_displacement_sampler,
        .camera = camera,
        .x_y_z_xdx_tex = m_displacement_x_y_z_xdx_texture,
        .ydx_zdx_ydy_zdy_tex = m_displacement_ydx_zdx_zdz_zdy_texture,
        .vertex_position_dist = .25f,
        .field_size = SIZE
        }, rhi::Pipeline_Bind_Point::Graphics);
    cmd->draw(6 * SIZE * SIZE,1,0,0);
    cmd->end_render_pass();
    cmd->end_debug_region(); // ocean:render:opaque_forward_pass
}

glm::vec4 calculate_default_length_scales()
{
    auto calculate_length_scale = [](const uint32_t factor) {
        auto length_scale = 1024.f;
        for (auto i = 0; i < factor; ++i)
        {
            length_scale = length_scale * (1.f - 1.f / 1.681033988f);
        }
        return length_scale;
    };
    return {
        calculate_length_scale(0),
        calculate_length_scale(2),
        calculate_length_scale(4),
        calculate_length_scale(6)
    };
}

rhi::Image_Create_Info Ocean::Options::generate_create_info(bool four_components) const noexcept
{
    rhi::Image_Format target_format;
    if (four_components)
    {
        if (use_fp16_textures)
            target_format = rhi::Image_Format::R16G16B16A16_SFLOAT;
        else
            target_format = rhi::Image_Format::R32G32B32A32_SFLOAT;
    }
    else
    {
        if (use_fp16_textures)
            target_format = rhi::Image_Format::R16_SFLOAT;
        else
            target_format = rhi::Image_Format::R32_SFLOAT;
    }
    return {
        .format = target_format,
        .width = texture_size,
        .height = texture_size,
        .depth = 1,
        .array_size = static_cast<uint16_t>(cascade_count),
        .mip_levels = 1,
        .usage = rhi::Image_Usage::Unordered_Access | rhi::Image_Usage::Sampled,
        .primary_view_type = rhi::Image_View_Type::Texture_2D_Array
    };
}

Ocean::Simulation_Data::Full_Spectrum_Parameters::Full_Spectrum_Parameters()
    : single_spectrum_parameters({
          {
              .wind_speed = 2.5f,
              .fetch = 3.5f,
              .phillips_alpha = 0.000125f,
              .generalized_a = 1.0f,
              .generalized_b = 1.0f,
              .contribution = 1.0f,
              .wind_direction = 0.f
          },
          {
              .wind_speed = 10.5f,
              .fetch = 70.0f,
              .phillips_alpha = 0.00025f,
              .generalized_a = 1.f,
              .generalized_b = 1.f,
              .contribution = 1.0f,
              .wind_direction = 135.f
          }}
      )
    , active_cascades(glm::uvec4(1,1,1,1))
    , length_scales(calculate_default_length_scales())
    , oceanographic_spectrum(static_cast<uint32_t>(Ocean_Spectrum::TMA))
    , directional_spreading_function(static_cast<uint32_t>(Ocean_Directional_Spreading_Function::Donelan_Banner))
    , gravity(9.81f)
    , depth(150.f)
{}

constexpr static auto OCEAN_HELP_TEXT_TEXTURE_SIZE =
"Texture size used for the simulation.";

constexpr static auto OCEAN_HELP_TEXT_CASCADES =
"Amount of simultaneously simulated domains.";

constexpr static auto OCEAN_HELP_TEXT_FP16_TEXTURES =
"Use fp16 textures instead of fp32 textures. "
"Expect loss of precision when used with a high size value.";

constexpr static auto OCEAN_HELP_TEXT_FP16_MATH =
"Use shader permutations that utilize fp16 maths (not yet implemented). "
"Expect loss of precision when used with a high size value.";

constexpr static auto OCEAN_HELP_TEXT_SPECTRUM =
"The spectrum describes the statistical model used to control the wave generation. "
"Some spectra require different parameters.\n"
"The phillips spectrum only makes use of the Phillips alpha value and should only be used with a symmetrical directional spreading function.";

constexpr static auto OCEAN_HELP_TEXT_DIRECTIONAL_SPREAD =
"The directional spreading function describes how the direction of the wind affects the wave generation. "
"As with the spectra, not all directional spreading functions take the same parameters.";

constexpr static auto OCEAN_HELP_TEXT_WIND_SPEED =
"Wind speed describes the average speed of the wind in meters per second at 10 meters above the ocean surface.";

constexpr const char* OCEAN_HELP_TEXT_WIND_DIRECTION =
"Wind direction in degrees. 0 degrees means that the wind is blowing towards positive x.";

constexpr static auto OCEAN_HELP_TEXT_GRAVITY =
"Strength of the gravity of the surface, in m/s^2.";

constexpr static auto OCEAN_HELP_TEXT_FETCH =
"\"Dimensionless\" fetch describes the area over which the wind blows; The distance (in km) from a lee shore. "
"A higher value corresponds with higher waves.";

constexpr static auto OCEAN_HELP_TEXT_DEPTH =
"Depth is the average depth of the ocean, in meters.";

constexpr static auto OCEAN_HELP_TEXT_PHILLIPS_ALPHA =
"alpha-value used in the phillips spectrum.";

constexpr static auto OCEAN_HELP_TEXT_GENERALIZED_A =
"a-value used in the generalized A,B spectrum.";

constexpr static auto OCEAN_HELP_TEXT_GENERALIZED_B =
"b-value used in the generalized A,B spectrum.";

constexpr static auto OCEAN_HELP_TEXT_CONTRIBUTION =
"Non-physical multiplier to the spectrum's strength.";

constexpr static auto OCEAN_HELP_TEXT_LENGTH_SCALE =
"Size of the domain of the simulation in meters.";

void Ocean::process_gui()
{
    if (ImGui::CollapsingHeader("Ocean Simulation"))
    {
        process_gui_options();
        process_gui_simulation_settings();
        ImGui::SeparatorText("Debug");
        {
            ImGui::Checkbox("Update Time", &m_options.update_time);
            ImGui::Checkbox("Enabled##Ocean", &m_options.enabled);
        }
    }
}

void Ocean::process_gui_options()
{
    ImGui::SeparatorText("Options");

    auto options = m_options;
    {
        constexpr static auto size_values = std::to_array({ 64, 128, 256, 512, 1024 });
        constexpr static auto size_value_texts = std::to_array({ "64", "128", "256", "512", "1024" });

        auto size_text_idx = 0;
        for (auto i = 0; i < size_value_texts.size(); ++i)
        {
            if (size_values[i] == options.texture_size) size_text_idx = i;
        }
        imutil::push_negative_padding();
        if (ImGui::BeginCombo("Size", size_value_texts[size_text_idx]))
        {
            for (auto i = 0; i < size_values.size(); ++i)
            {
                bool selected = options.texture_size == size_values[i];
                if (ImGui::Selectable(size_value_texts[i], selected, ImGuiSelectableFlags_None))
                {
                    options.texture_size = size_values[i];
                }
            }
            ImGui::EndCombo();
        }
        imutil::help_marker(OCEAN_HELP_TEXT_TEXTURE_SIZE);
    }
    {
        constexpr static auto cascade_values = std::to_array({ 1, 2, 3, 4 });
        constexpr static auto cascade_value_texts = std::to_array({ "1", "2", "3", "4" });

        auto cascade_text_idx = 0;
        for (auto i = 0; i < cascade_value_texts.size(); ++i)
        {
            if (cascade_values[i] == options.cascade_count) cascade_text_idx = i;
        }
        imutil::push_negative_padding();
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
        ImGui::Checkbox("Use fp16 textures", &options.use_fp16_textures);
        imutil::help_marker(OCEAN_HELP_TEXT_FP16_TEXTURES);
    }
    {
        ImGui::Checkbox("Use fp16 maths", &options.use_fp16_maths);
        imutil::help_marker(OCEAN_HELP_TEXT_FP16_MATH);
    }

    const bool recreate_textures =
        m_options.texture_size != options.texture_size ||
        m_options.use_fp16_textures != options.use_fp16_textures ||
        m_options.cascade_count != options.cascade_count;
    if (recreate_textures)
    {

        auto recreate_texture = [&](Image& image)
        {
            auto create_info = image.get_create_info();
            create_info.width = options.texture_size;
            create_info.height = options.texture_size;
            image.recreate(create_info);
        };
        recreate_texture(m_spectrum_state_texture);
        recreate_texture(m_spectrum_angular_frequency_texture);
        recreate_texture(m_displacement_x_y_z_xdx_texture);
        recreate_texture(m_displacement_ydx_zdx_zdz_zdy_texture);
    }
    m_options = options;
}

void Ocean::process_gui_simulation_settings()
{
    ImGui::SeparatorText("Simulation Settings");

    imutil::push_negative_padding();
    ImGui::SliderFloat("Gravity", &m_simulation_data.full_spectrum_parameters.gravity, .001f, 30.f);
    imutil::help_marker(OCEAN_HELP_TEXT_GRAVITY);

    auto length_scales = std::to_array({
        &m_simulation_data.full_spectrum_parameters.length_scales.x,
        &m_simulation_data.full_spectrum_parameters.length_scales.y,
        &m_simulation_data.full_spectrum_parameters.length_scales.z,
        &m_simulation_data.full_spectrum_parameters.length_scales.w,
        });
    for (auto i = 0; i < 4; ++i)
    {
        auto lengthscale_str = std::string("Lengthscale ") + std::to_string(i + 1);
        imutil::push_negative_padding();
        ImGui::SliderFloat(lengthscale_str.c_str(), length_scales[i], .001f, 5000.f);
        imutil::help_marker(OCEAN_HELP_TEXT_LENGTH_SCALE);
    }

    auto depth_str = std::string("Depth");
    imutil::push_negative_padding();
    ImGui::SliderFloat(depth_str.c_str(), &m_simulation_data.full_spectrum_parameters.depth, 1.0f, 150.f);
    imutil::help_marker(OCEAN_HELP_TEXT_DEPTH);

    constexpr static auto spectrum_values = std::to_array({
        Ocean_Spectrum::Phillips,
        Ocean_Spectrum::Pierson_Moskowitz,
        Ocean_Spectrum::Generalized_A_B,
        Ocean_Spectrum::Jonswap,
        Ocean_Spectrum::TMA });
    constexpr static auto spectrum_value_texts = std::to_array({
        "Phillips",
        "Pierson Moskowitz",
        "Generalized A,B",
        "Jonswap",
        "TMA" });

    auto spectrum_text_idx = 0;
    for (auto i = 0; i < spectrum_value_texts.size(); ++i)
    {
        if (uint32_t(spectrum_values[i]) == m_simulation_data.full_spectrum_parameters.oceanographic_spectrum) spectrum_text_idx = i;
    }
    imutil::push_negative_padding();
    if (ImGui::BeginCombo("Oceanographic Spectrum", spectrum_value_texts[spectrum_text_idx]))
    {
        for (auto i = 0; i < spectrum_values.size(); ++i)
        {
            bool selected = m_simulation_data.full_spectrum_parameters.oceanographic_spectrum == uint32_t(spectrum_values[i]);
            if (ImGui::Selectable(spectrum_value_texts[i], selected, ImGuiSelectableFlags_None))
            {
                m_simulation_data.full_spectrum_parameters.oceanographic_spectrum = uint32_t(spectrum_values[i]);
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
        if (uint32_t(dirspread_values[i]) == m_simulation_data.full_spectrum_parameters.directional_spreading_function) dirspread_text_idx = i;
    }
    imutil::push_negative_padding();
    if (ImGui::BeginCombo("Directional Spread", dirspread_value_texts[dirspread_text_idx]))
    {
        for (auto i = 0; i < dirspread_values.size(); ++i)
        {
            bool selected = m_simulation_data.full_spectrum_parameters.directional_spreading_function == uint32_t(dirspread_values[i]);
            if (ImGui::Selectable(dirspread_value_texts[i], selected, ImGuiSelectableFlags_None))
            {
                m_simulation_data.full_spectrum_parameters.directional_spreading_function = uint32_t(dirspread_values[i]);
            }
        }
        ImGui::EndCombo();
    }
    imutil::help_marker(OCEAN_HELP_TEXT_DIRECTIONAL_SPREAD);

    uint32_t spectrum_count = 0;
    for (auto& spectrum : m_simulation_data.full_spectrum_parameters.single_spectrum_parameters)
    {
        if (spectrum_count == 0)
        {
            ImGui::SeparatorText("Primary spectrum parameters");
        }
        else
        {
            ImGui::SeparatorText("Secondary spectrum parameters");
        }

        auto wind_speed_str = std::string("Wind Speed##") + std::to_string(spectrum_count);
        imutil::push_negative_padding();
        ImGui::SliderFloat(wind_speed_str.c_str(), &spectrum.wind_speed, .001f, 60.f);
        imutil::help_marker(OCEAN_HELP_TEXT_WIND_SPEED);

        auto fetch_str = std::string("Fetch##") + std::to_string(spectrum_count);
        imutil::push_negative_padding();
        ImGui::SliderFloat(fetch_str.c_str(), &spectrum.fetch, 1.0f, 125.f);
        imutil::help_marker(OCEAN_HELP_TEXT_FETCH);

        auto phillips_alpha_str = std::string("Phillips Coefficient Alpha##") + std::to_string(spectrum_count);
        imutil::push_negative_padding();
        ImGui::SliderFloat(phillips_alpha_str.c_str(), &spectrum.phillips_alpha, .00001f, 0.001f, "%.7f");
        imutil::help_marker(OCEAN_HELP_TEXT_PHILLIPS_ALPHA);

        auto generalized_a_str = std::string("Generalized Coefficient A##") + std::to_string(spectrum_count);
        imutil::push_negative_padding();
        ImGui::SliderFloat(generalized_a_str.c_str(), &spectrum.generalized_a, .001f, 100.f);
        imutil::help_marker(OCEAN_HELP_TEXT_GENERALIZED_A);

        auto generalized_b_str = std::string("Generalized Coefficient B##") + std::to_string(spectrum_count);
        imutil::push_negative_padding();
        ImGui::SliderFloat(generalized_b_str.c_str(), &spectrum.generalized_b, .001f, 100.f);
        imutil::help_marker(OCEAN_HELP_TEXT_GENERALIZED_B);

        auto contribution_str = std::string("Contribution##") + std::to_string(spectrum_count);
        imutil::push_negative_padding();
        ImGui::SliderFloat(contribution_str.c_str(), &spectrum.contribution, .0f, 1.f);
        imutil::help_marker(OCEAN_HELP_TEXT_CONTRIBUTION);

        ++spectrum_count;
    }
}
}
