#include "renderer/asset/compute_library.hpp"
#include "renderer/asset/shader_library.hpp"

#include <rhi/graphics_device.hpp>

namespace ren
{
void Compute_Library::create_pipelines(rhi::Graphics_Device* device, Shader_Library* shader_library)
{
    destroy_pipelines(device);
    for (const auto& [name, blob] : shader_library->shaders)
    {
        auto& pipeline = *pipelines.acquire();
        pipeline.name = name;
        rhi::Compute_Pipeline_Create_Info create_info = {
            blob
        };
        pipeline.pipeline = device->create_pipeline(create_info).value_or(nullptr);
    }
}

void Compute_Library::destroy_pipelines(rhi::Graphics_Device* device)
{
    for (auto& wrapper : pipelines)
    {
        device->destroy_pipeline(wrapper.pipeline);
        pipelines.release(&wrapper);
    }
}
}
