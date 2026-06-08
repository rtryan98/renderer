#pragma once

#include "renderer/techniques/technique_base.hpp"
#include "renderer/render_resource_blackboard.hpp"

namespace ren::techniques
{
class RT_Soft_Shadows final : public Technique_Base
{
public:
    constexpr static auto SUN_VISIBILITY_TEXTURE_NAME = "rt_soft_shadows:sun_visibility_texture";

    RT_Soft_Shadows(Asset_Repository& asset_repository,
        GPU_Transfer_Context& gpu_transfer_context,
        Render_Resource_Blackboard& render_resource_blackboard,
        uint32_t width, uint32_t height);
    ~RT_Soft_Shadows() override;

    void trace_shadow_rays(rhi::Command_List* cmd,
        Resource_State_Tracker& tracker,
        const Image& normals_render_target,
        const Image& depth_render_target);

private:
    Image m_sun_visibility_texture;
};
}
