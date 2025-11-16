#pragma once

#include "renderer/render_resource_blackboard.hpp"

namespace rhi
{
class Command_List;
}

namespace ren
{
class Asset_Repository;
class GPU_Transfer_Context;
class Resource_State_Tracker;

namespace techniques
{
class Tone_Map
{
public:
    constexpr static auto TONE_MAP_PARAMETERS_BUFFER_NAME = "tone_map:parameters_buffer";

    constexpr static auto SDR_DEFAULT_PAPER_WHITE = 250.0f; // in nits
    constexpr static auto IMAGE_REFERENCE_LUMINANCE = 100.0f; // value of 1.0 in nits

    Tone_Map(Asset_Repository& asset_repository,
        GPU_Transfer_Context& gpu_transfer_context,
        Render_Resource_Blackboard& render_resource_blackboard,
        bool hdr, float display_peak_luminance_nits);
    ~Tone_Map();

    // Overwrites all contents in swapchain_image
    void blit_apply(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker,
        const Image& source_image,
        const Image& swapchain_image);
    void render_debug(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker,
        const Image& render_target,
        const Buffer& camera);

    void set_hdr_state(bool hdr, float display_peak_luminance_nits);


    void process_gui();

private:
    Asset_Repository& m_asset_repository;
    GPU_Transfer_Context& m_gpu_transfer_context;
    Render_Resource_Blackboard& m_render_resource_blackboard;

    Buffer m_tone_map_parameters_buffer;

    bool m_is_hdr;
    bool m_render_debug = false;
    bool m_is_enabled = true;

    float m_sdr_paper_white = SDR_DEFAULT_PAPER_WHITE;

    // Curve data
    float m_peak_intensity; // in nits
    float m_alpha = 0.25f;
    float m_mid_point = 0.538f;
    float m_linear_section = 0.444f;
    float m_toe_strength = 1.280f;
    float m_ka;
    float m_kb;
    float m_kc;

    // Tone mapping data
    float m_sdr_correction_factor = 1.0f;
    float m_luminance_target; // scaled, such that 1.0 = m_peak_intensity
    float m_luminance_target_ictcp;
    float m_luminance_target_jzazbz;
    float m_blend_ratio = 0.6f;
    float m_fade_start = 0.98f;
    float m_fade_end = 1.16f;

    void initialize();
    static float pq_ieotf(float value, float exponent_scale);
    static float calculate_peak_luminance_ictcp(float peak_luminance);
    static float calculate_peak_luminance_jzazbz(float peak_luminance);
    static float physical_luminance_to_reference_luminance(float luminance);
    static float reference_luminance_to_physical_luminance(float luminance);
};
}
}
