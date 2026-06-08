#pragma once

#include "renderer/techniques/technique_base.hpp"
#include "renderer/render_resource_blackboard.hpp"

namespace ren::techniques
{
class Hillaire_Sky final : public Technique_Base
{
public:
    Hillaire_Sky(Asset_Repository& asset_repository,
        GPU_Transfer_Context& gpu_transfer_context,
        Render_Resource_Blackboard& render_resource_blackboard);
    ~Hillaire_Sky() override;
};
}
