#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace rhi
{
struct Shader_Blob;
}

namespace ren
{
struct Pipeline_Library;
struct Compute_Library;

struct Named_Shader
{
    std::string name;
    rhi::Shader_Blob* blob;
};

struct Shader_Library
{
    std::vector<Named_Shader> shaders;
    std::vector<Pipeline_Library*> referenced_pipeline_libraries;
    Compute_Library* referenced_compute_library;

    // TODO: If there are a lot of shaders this should be a map and not a vector with linear search. However, right now this suffices
    [[nodiscard]] rhi::Shader_Blob* get_shader(const std::string_view& name) const;
};
}
