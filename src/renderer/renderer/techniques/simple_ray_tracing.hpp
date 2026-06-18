#pragma once

#include "renderer/techniques/technique_base.hpp"
#include "renderer/render_resource_blackboard.hpp"

namespace ren
{
class Asset_Repository;
class GPU_Transfer_Context;
class Resource_State_Tracker;
struct Fly_Camera;

namespace techniques
{
class Simple_Ray_Tracing final : public Technique_Base
{
public:
    Simple_Ray_Tracing(Asset_Repository& asset_repository,
        GPU_Transfer_Context& gpu_transfer_context,
        Render_Resource_Blackboard& render_resource_blackboard);
    ~Simple_Ray_Tracing() override;

    void trace_rays(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker,
        const Buffer& camera_buffer,
        const Image& render_target,
        uint32_t tlas_index);

private:
    Buffer m_sbt_buffer;
};
}
}
