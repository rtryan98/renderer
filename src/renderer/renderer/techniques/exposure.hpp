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
class Exposure
{
public:
    constexpr static auto LUMINANCE_HISTOGRAM_BUFFER_NAME = "exposure:luminance_histogram_buffer";

    Exposure(Asset_Repository& asset_repository,
        GPU_Transfer_Context& gpu_transfer_context,
        Render_Resource_Blackboard& render_resource_blackboard);
    ~Exposure();

    void compute_luminance_histogram(rhi::Command_List* cmd, Resource_State_Tracker& tracker, const Image& target, float dt) const;
    void apply_exposure(rhi::Command_List* cmd, Resource_State_Tracker& tracker, const Image& target) const;

    void process_gui();

private:
    Asset_Repository& m_asset_repository;
    GPU_Transfer_Context& m_gpu_transfer_context;
    Render_Resource_Blackboard& m_render_resource_blackboard;

    Buffer m_luminance_histogram_buffer;

    bool m_use_camera_exposure = false;
    float m_aperture = 16.f; // F-stop
    float m_shutter = 100.f;
    float m_iso = 100.f;

    float m_auto_exposure_min_log2_luminance = -10.f;
    float m_auto_exposure_log2_luminance_range = 30.f;
    float m_auto_exposure_adaption_rate = 1.5f;
    float m_auto_exposure_log2_luminance_cutoff_low = 1.f;
    float m_auto_exposure_log2_luminance_cutoff_high = 13.f;
    float m_auto_exposure_exposure_compensation = 2.f;
};
}
}
