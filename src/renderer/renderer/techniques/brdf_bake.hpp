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
class BRDF_LUT
{
public:
    constexpr static auto LUT_TEXTURE_NAME = "pbr:brdf_lut_texture";

    BRDF_LUT(Asset_Repository& asset_repository,
        Render_Resource_Blackboard& render_resource_blackboard);
    ~BRDF_LUT();

    BRDF_LUT(const BRDF_LUT&) = delete;
    BRDF_LUT& operator=(const BRDF_LUT&) = delete;
    BRDF_LUT(BRDF_LUT&&) = delete;
    BRDF_LUT& operator=(BRDF_LUT&&) = delete;

    void bake_brdf_lut(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker);

private:
    Asset_Repository& m_asset_repository;
    Render_Resource_Blackboard& m_render_resource_blackboard;

    Image m_brdf_lut = {};

    bool m_baked = false;
};
}
}
