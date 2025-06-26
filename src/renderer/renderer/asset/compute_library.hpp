#pragma once

#include <rhi/common/array_vector.hpp>
#include <vector>
#include <string>

namespace rhi
{
struct Pipeline;
class Graphics_Device;
}

namespace ren
{
struct Shader_Library;

struct Compute_Pipeline_Wrapper
{
    rhi::Pipeline* pipeline;
    std::string name;
};

struct Compute_Library
{
    rhi::Array_Vector<Compute_Pipeline_Wrapper, 16> pipelines;

    void create_pipelines(rhi::Graphics_Device* device, Shader_Library* shader_library);
    void destroy_pipelines(rhi::Graphics_Device* device);
};
}
