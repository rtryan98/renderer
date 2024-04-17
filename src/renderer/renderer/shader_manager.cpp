#include "renderer/shader_manager.hpp"

namespace ren
{
rhi::Pipeline* Pipeline_Manager::get_compute_pipeline(Compute_Pipelines pipeline) const
{
    auto pipeline_index = std::to_underlying(pipeline);
    return m_predefined_compute_pipelines[pipeline_index];
}
}
