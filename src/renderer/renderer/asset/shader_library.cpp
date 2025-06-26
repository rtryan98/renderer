#include "renderer/asset/shader_library.hpp"

namespace ren
{
rhi::Shader_Blob* Shader_Library::get_shader(const std::string_view& name) const
{
    for (const auto& [shader_name, shader] : shaders)
    {
        if (shader_name == name)
        {
            return shader;
        }
    }
    return nullptr;
}
}
