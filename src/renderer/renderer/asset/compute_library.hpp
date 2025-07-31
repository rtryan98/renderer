#pragma once

#include <plf_colony.h>
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
    plf::colony<Compute_Pipeline_Wrapper> pipelines;
    std::vector<Compute_Pipeline_Wrapper*> pipeline_ptrs;

    void create_pipelines(rhi::Graphics_Device* device, Shader_Library* shader_library);
    void destroy_pipelines(rhi::Graphics_Device* device);
};
}
