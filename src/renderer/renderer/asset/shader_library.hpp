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

struct Shader_Library
{
    std::vector<std::pair<std::string, rhi::Shader_Blob*>> shaders;
    std::vector<Pipeline_Library*> referenced_pipeline_libraries;
    std::vector<Compute_Library*> referenced_compute_libraries;

    // TODO: If there are a lot of shaders this should be a map and not a vector with linear search. However, right now this suffices
    rhi::Shader_Blob* get_shader(const std::string_view name) const;
};
}
