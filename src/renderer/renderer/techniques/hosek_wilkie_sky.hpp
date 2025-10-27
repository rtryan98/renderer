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
class Static_Scene_Data;

namespace techniques
{
class Hosek_Wilkie_Sky
{
public:
    constexpr static auto PARAMETERS_BUFFER_NAME = "hosek_wilkie:parameters";
    constexpr static auto SKY_CUBEMAP_TEXTURE_NAME = "hosek_wilkie:sky_cubemap_texture";

    Hosek_Wilkie_Sky(
        Asset_Repository& asset_repository,
        GPU_Transfer_Context& gpu_transfer_context,
        Render_Resource_Blackboard& render_resource_blackboard);
    ~Hosek_Wilkie_Sky();

    Hosek_Wilkie_Sky(const Hosek_Wilkie_Sky&) = delete;
    Hosek_Wilkie_Sky& operator=(const Hosek_Wilkie_Sky&) = delete;
    Hosek_Wilkie_Sky(Hosek_Wilkie_Sky&&) = delete;
    Hosek_Wilkie_Sky& operator=(Hosek_Wilkie_Sky&&) = delete;

    void update(const glm::vec3& sun_direction);

    void generate_cubemap(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker) const;

    void skybox_render(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker,
        const Buffer& camera,
        const Image& shaded_geometry_render_target,
        const Image& geometry_depth_buffer) const;

    void process_gui();

private:
    Asset_Repository& m_asset_repository;
    GPU_Transfer_Context& m_gpu_transfer_context;
    Render_Resource_Blackboard& m_render_resource_blackboard;

    Buffer m_parameters;
    Image m_cubemap;

    float m_turbidity = 5.f;
    glm::vec3 m_albedo = { 0.12f, 0.12f, 0.5f };
    glm::vec3 m_sun_direction = {};
    bool m_use_xyz = true;
};
}
}
