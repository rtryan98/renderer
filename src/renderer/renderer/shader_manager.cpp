#include "renderer/shader_manager.hpp"

namespace ren
{

rhi::Shader_Blob* Shader_Library::get_shader(Shaders shader) const
{
    auto shader_idx = std::to_underlying(shader);
    return m_predefined_shaders[shader_idx];
}
}
