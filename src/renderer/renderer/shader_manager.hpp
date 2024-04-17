#pragma once

#include "renderer/generated/compute_pipeline_library.hpp"
#include <rhi/resource.hpp>

namespace ren
{
class Pipeline_Manager
{
public:
    rhi::Pipeline* get_compute_pipeline(Compute_Pipelines pipeline) const;

private:
    std::array<rhi::Pipeline*, compute_pipeline_metadata.size()> m_predefined_compute_pipelines = {};
};
}
