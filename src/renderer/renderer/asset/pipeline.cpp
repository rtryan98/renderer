#include "renderer/asset/pipeline.hpp"

#include "graphics_pipeline_library.hpp"
#include "renderer/asset/compute_library.hpp"
#include "renderer/asset/graphics_pipeline_library.hpp"

namespace ren
{
Compute_Pipeline::Compute_Pipeline(Compute_Library* compute_library)
    : m_compute_library(compute_library)
    , m_active_pipeline(compute_library->pipeline_ptrs[0])
{}

Compute_Pipeline::operator rhi::Pipeline*() const
{
    return m_active_pipeline->pipeline;
}

Compute_Pipeline& Compute_Pipeline::set_variant(std::string_view name)
{
    for (auto& pipeline : m_compute_library->pipelines)
    {
        if (pipeline.name == name)
        {
            m_active_pipeline = &pipeline;
            break;
        }
    }
    return *this;
}

Graphics_Pipeline::Graphics_Pipeline(Graphics_Pipeline_Library* graphics_pipeline_library)
    : m_graphics_pipeline_library(graphics_pipeline_library)
    , m_active_pipeline(m_graphics_pipeline_library->pipeline)
{}

Graphics_Pipeline::operator rhi::Pipeline*() const
{
    return m_active_pipeline;
}
}
