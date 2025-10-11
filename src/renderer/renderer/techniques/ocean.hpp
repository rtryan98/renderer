#pragma once

#include "renderer/render_resource_blackboard.hpp"
#include <glm/glm.hpp>

namespace rhi
{
class Command_List;
}

namespace ren
{
class Asset_Repository;
class GPU_Transfer_Context;
class Resource_State_Tracker;
struct Fly_Camera;

namespace techniques
{
class Ocean
{
public:
    constexpr static auto SPECTRUM_PARAMETERS_BUFFER_NAME = "ocean:spectrum_parameters_buffer";
    constexpr static auto SPECTRUM_STATE_TEXTURE_NAME = "ocean:spectrum_initial_state_texture";
    constexpr static auto SPECTRUM_ANGULAR_FREQUENCY_TEXTURE_NAME = "ocean:spectrum_angular_frequency_texture";
    constexpr static auto DISPLACEMENT_X_Y_Z_XDX_TEXTURE_NAME = "ocean:displacement_x_y_z_xdx";
    constexpr static auto DISPLACEMENT_YDX_ZDX_YDY_ZDY_TEXTURE_NAME = "ocean:displacement_ydx_zdx_ydy_zdy_texture";
    constexpr static auto FORWARD_PASS_DEPTH_RENDER_TARGET_NAME = "ocean:forward_pass_depth_render_target";

    Ocean(Asset_Repository& asset_repository,
        GPU_Transfer_Context& gpu_transfer_context,
        Render_Resource_Blackboard& render_resource_blackboard,
        uint32_t width, uint32_t height);
    ~Ocean();

    Ocean(const Ocean&) = delete;
    Ocean& operator=(const Ocean&) = delete;
    Ocean(Ocean&&) = delete;
    Ocean& operator=(Ocean&&) = delete;

    void update(float dt);

    void simulate(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker);

    void depth_pre_pass(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker,
        const Buffer& camera,
        const Image& shaded_scene_depth_render_target,
        const Fly_Camera& cull_camera);

    // TODO: add proper translucent pass instead.
    void opaque_forward_pass(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker,
        const Buffer& camera,
        const Image& shaded_scene_render_target,
        const Image& shaded_scene_depth_render_target,
        const Fly_Camera& cull_camera);

    void process_gui();

private:
    Asset_Repository& m_asset_repository;
    GPU_Transfer_Context& m_gpu_transfer_context;
    Render_Resource_Blackboard& m_render_resource_blackboard;

    Buffer m_spectrum_parameters_buffer;
    Image m_spectrum_state_texture;
    Image m_spectrum_angular_frequency_texture;
    Image m_displacement_x_y_z_xdx_texture;
    Image m_displacement_ydx_zdx_zdz_zdy_texture;
    Image m_forward_pass_depth_render_target;
    Sampler m_displacement_sampler;

    // TODO: Style? Should this be done via a function instead?
public:
    struct Options
    {
        bool use_fp16_textures = true;
        bool use_fp16_maths = true;
        bool update_time = true;
        bool enabled = true;
        bool wireframe = false;
        uint32_t texture_size = 256;
        uint32_t cascade_count = 4;

        float horizontal_cull_grace = 8.f;
        float vertical_cull_grace = 8.f;

        auto operator<=>(const Options& other) const = default;

        [[nodiscard]] rhi::Image_Create_Info generate_create_info(bool four_components) const noexcept;
    } options;

    struct Simulation_Data
    {
        struct Full_Spectrum_Parameters
        {
            Full_Spectrum_Parameters();

            struct Single_Spectrum_Parameters
            {
                float wind_speed; // U10, m/s
                float fetch; // f, km (dimensionless)
                float phillips_alpha;
                float generalized_a;
                float generalized_b;
                float contribution;
                float wind_direction;
            };
            std::array<Single_Spectrum_Parameters, 2> single_spectrum_parameters;
            glm::uvec4 active_cascades;
            glm::vec4 length_scales;
            uint32_t oceanographic_spectrum;
            uint32_t directional_spreading_function;
            float gravity;
            float depth;
        } full_spectrum_parameters;

        float total_time = 0.0f;
    } simulation_data;

private:

    void draw_all_tiles(rhi::Command_List* cmd, const Buffer& camera, const Fly_Camera& cull_camera);

    void process_gui_options();
    void process_gui_simulation_settings();
};
}
}
