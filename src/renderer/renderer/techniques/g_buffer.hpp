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
    constexpr static auto COLOR_RENDER_TARGET_NAME = "g_buffer:color_render_target";
    constexpr static auto NORMAL_RENDER_TARGET_NAME = "g_buffer:normal_render_target";
    constexpr static auto METALLIC_ROUGHNESS_RENDER_TARGET_NAME = "g_buffer:metallic_roughness_render_target";
    constexpr static auto DEPTH_BUFFER_NAME = "g_buffer:depth_buffer";

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
        const Image& resolve_target);

private:
    Asset_Repository& m_asset_repository;
    Render_Resource_Blackboard& m_render_resource_blackboard;

    Image m_color_render_target;
    Image m_normal_render_target;
    Image m_metallic_roughness_render_target;
    Image m_depth_buffer;
    Sampler m_resolve_sampler;
};

}
}
