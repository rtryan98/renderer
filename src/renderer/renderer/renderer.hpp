#pragma once

#include "renderer/scene/camera.hpp"
#include "renderer/render_resource_blackboard.hpp"

#include "renderer/techniques/g_buffer.hpp"
#include "renderer/techniques/image_based_lighting.hpp"
#include "renderer/techniques/imgui.hpp"
#include "renderer/techniques/ocean.hpp"
#include "renderer/techniques/tone_map.hpp"

namespace rhi
{
class Command_List;
class Swapchain;
}

namespace ren
{
class Asset_Repository;
class GPU_Transfer_Context;
class Input_State;
struct Render_Attachment;
class Render_Resource_Blackboard;
class Static_Scene_Data;

class Renderer
{
public:
    Renderer(GPU_Transfer_Context& gpu_transfer_context,
        rhi::Swapchain& swapchain,
        Asset_Repository& asset_repository,
        Render_Resource_Blackboard& resource_blackboard);
    ~Renderer();

    void process_gui();
    void update(const Input_State& input_state, double t, double dt) noexcept;
    void setup_frame();
    void render(const Static_Scene_Data& scene, rhi::Command_List* cmd, double t, double dt) noexcept;
    void on_resize(uint32_t width, uint32_t height) noexcept;

    void set_hdr_state(bool enabled, float display_peak_luminance_nits) noexcept;

private:
    GPU_Transfer_Context& m_gpu_transfer_context;
    rhi::Swapchain& m_swapchain;
    Asset_Repository& m_asset_repository;
    Render_Resource_Blackboard& m_resource_blackboard;

    Fly_Camera m_fly_cam;
    Buffer m_camera_buffer;

    bool m_enable_hdr = false;
    float m_render_scale = 1.f;
    Image m_swapchain_image = {};
    Image m_shaded_geometry_render_target = {};

    techniques::G_Buffer m_g_buffer;
    techniques::Image_Based_Lighting m_image_based_lighting;
    techniques::Imgui m_imgui;
    techniques::Ocean m_ocean;
    techniques::Tone_Map m_tone_map;
};
}
