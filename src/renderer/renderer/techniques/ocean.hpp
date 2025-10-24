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
    constexpr static auto TILE_INDEX_BUFFER_NAME = "ocean:tile_index_buffer";

    constexpr static auto FFT_MIN_MAX_TEXTURE_NAME = "ocean:fft_min_max_texture";
    constexpr static auto FFT_MINMAX_BUFFER_NAME = "ocean:fft_minmax_buffer";
    constexpr static auto FFT_MINMAX_READBACK_BUFFER_NAME = "ocean:fft_minmax_readback_buffer";
    constexpr static auto PACKED_DISPLACEMENT_TEXTURE_NAME = "ocean:packed_displacement_texture";
    constexpr static auto FOAM_WEIGHT_TEXTURE_NAME = "ocean:foam_weight_texture";
    constexpr static auto PACKED_DERIVATIVES_TEXTURE_NAME = "ocean:packed_derivatives_texture";

    Ocean(Asset_Repository& asset_repository,
        GPU_Transfer_Context& gpu_transfer_context,
        Render_Resource_Blackboard& render_resource_blackboard,
        uint32_t width, uint32_t height);
    ~Ocean();

    Ocean(const Ocean&) = delete;
    Ocean& operator=(const Ocean&) = delete;
    Ocean(Ocean&&) = delete;
    Ocean& operator=(Ocean&&) = delete;

    void update(float dt, const Fly_Camera& cull_camera);

    void simulate(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker);

    void depth_pre_pass(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker,
        const Buffer& camera,
        const Image& shaded_scene_depth_render_target);

    // TODO: add proper translucent pass instead.
    void opaque_forward_pass(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker,
        const Buffer& camera,
        const Image& shaded_scene_render_target,
        const Image& shaded_scene_depth_render_target);

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
    Buffer m_tile_index_buffer;

    Image m_minmax_texture;
    Buffer m_minmax_buffer;
    Buffer m_minmax_readback_buffer;
    Image m_packed_displacement_texture;
    Image m_packed_derivatives_texture;
    Image m_packed_xdx_texture;

    struct Surface_Quad_Tree
    {
        struct Grid
        {
            Grid() = default;
            Grid(const glm::vec2& center, uint32_t size, uint8_t value);

            glm::vec2 center;
            std::vector<std::vector<uint8_t>> cells;
        };

        std::vector<Grid> grids;

        void propagate_cell_value(uint32_t x, uint32_t y, uint32_t level, uint32_t value);
        glm::vec2 get_tile_position(uint32_t x, uint32_t y, uint32_t level) const;
    };

    struct Drawable_Tile
    {
        glm::vec2 position;
        float size;
        glm::u8vec4 lod_differences;
    };

    std::vector<Drawable_Tile> m_drawable_tiles;

    glm::vec3 m_min_displacement = { -64.f, -64.f, -64.f };
    glm::vec3 m_max_displacement = {  64.f,  64.f,  64.f };

    // TODO: Style? Should this be done via a function instead?
public:
    struct Options
    {
        bool update_time = true;
        bool enabled = true;
        bool wireframe = false;
        uint32_t texture_size = 256;
        uint32_t cascade_count = 4;

        float lod_factor = 1.f;

        auto operator<=>(const Options& other) const = default;

        [[nodiscard]] rhi::Image_Create_Info generate_create_info(rhi::Image_Format format, uint16_t mip_levels = 1) const noexcept;
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

    void generate_drawable_cells(const Fly_Camera& cull_camera);
    void draw_all_tiles(rhi::Command_List* cmd, const Buffer& camera);

    void process_gui_options();
    void process_gui_simulation_settings();
};
}
}
