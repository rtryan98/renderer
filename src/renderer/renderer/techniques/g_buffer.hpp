#pragma once

#include "renderer/render_resource_blackboard.hpp"

namespace rhi
{
class Command_List;
}

namespace ren
{
class Asset_Repository;
class Resource_State_Tracker;
class Static_Scene_Data;

namespace techniques
{
class G_Buffer
{
public:
    constexpr static auto G_BUFFER_0_RENDER_TARGET_NAME = "g_buffer:g_buffer_0_render_target";
    constexpr static auto G_BUFFER_1_RENDER_TARGET_NAME = "g_buffer:g_buffer_1_render_target";
    constexpr static auto G_BUFFER_2_RENDER_TARGET_NAME = "g_buffer:g_buffer_2_render_target";
    constexpr static auto G_BUFFER_3_RENDER_TARGET_NAME = "g_buffer:g_buffer_3_render_target";
    constexpr static auto G_BUFFER_DEPTH_BUFFER_NAME = "g_buffer:depth_buffer";

    G_Buffer(Asset_Repository& asset_repository, Render_Resource_Blackboard& render_resource_blackboard,
        uint32_t width, uint32_t height);
    ~G_Buffer();

    G_Buffer(const G_Buffer&) = delete;
    G_Buffer& operator=(const G_Buffer&) = delete;
    G_Buffer(G_Buffer&&) = delete;
    G_Buffer& operator=(G_Buffer&&) = delete;

    void render_scene_cpu(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker,
        const Buffer& camera,
        const Static_Scene_Data& scene_data) const;
    void resolve(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker,
        const Buffer& camera,
        const Image& resolve_target);

private:
    Asset_Repository& m_asset_repository;
    Render_Resource_Blackboard& m_render_resource_blackboard;

    Image m_g_buffer_0_render_target; // R8G8B8A8 SRGB [albedo.xyz, 1.0]
    Image m_g_buffer_1_render_target; // R10G10B10A2 [oct_n.x, oct_n.y, 0., oct_n.z]
    Image m_g_buffer_2_render_target; // R8G8 [metallic, roughness]
    Image m_g_buffer_3_render_target; // R16G16 [mv]
    Image m_depth_buffer;
};

}
}
