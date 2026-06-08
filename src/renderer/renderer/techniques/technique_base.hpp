#pragma once

namespace rhi
{
class Command_List;
}

namespace ren
{
class Asset_Repository;
class GPU_Transfer_Context;
class Resource_State_Tracker;
class Render_Resource_Blackboard;

class Buffer;
class Image;

namespace techniques
{
class Technique_Base
{
protected:
    Technique_Base(Asset_Repository& asset_repository,
        GPU_Transfer_Context& gpu_transfer_context,
        Render_Resource_Blackboard& render_resource_blackboard)
        : m_asset_repository(asset_repository)
        , m_gpu_transfer_context(gpu_transfer_context)
        , m_render_resource_blackboard(render_resource_blackboard)
    {}

    virtual ~Technique_Base() = default;

protected:
    Asset_Repository& m_asset_repository;
    GPU_Transfer_Context& m_gpu_transfer_context;
    Render_Resource_Blackboard& m_render_resource_blackboard;
};
}
}
