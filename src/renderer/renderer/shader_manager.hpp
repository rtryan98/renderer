#pragma once

#include "renderer/generated/compute_pipeline_library.hpp"
#include <rhi/resource.hpp>

namespace ren
{
class Shader_Library
{
public:
    rhi::Shader_Blob* get_shader(Shaders shader) const;

private:
    std::array<rhi::Shader_Blob*, shader_metadata.size()> m_predefined_shaders = {};
};
}
