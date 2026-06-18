#include "renderer/techniques/simple_ray_tracing.hpp"

#include "renderer/resource_state_tracker.hpp"
#include "renderer/asset/asset_repository.hpp"
#include "renderer/gpu_transfer.hpp"

#include <rhi/command_list.hpp>
#include <rhi/shader_binding_table.hpp>
#include <rhi/graphics_device.hpp>

namespace ren::techniques
{
Simple_Ray_Tracing::Simple_Ray_Tracing(
    Asset_Repository& asset_repository,
    GPU_Transfer_Context& gpu_transfer_context,
    Render_Resource_Blackboard& render_resource_blackboard)
    : Technique_Base(asset_repository, gpu_transfer_context, render_resource_blackboard)
{
    auto* device = m_render_resource_blackboard.get_graphics_device();
    const auto& properties = device->get_ray_tracing_pipeline_properties();

    auto sbt_layout = rhi::compute_shader_binding_table_layout(properties, 1, 1, 1, 0);
    m_sbt_buffer = m_render_resource_blackboard.create_buffer(
        "simple_ray_tracing:sbt_buffer",
        {
            .size = sbt_layout.total_size,
            .heap = rhi::Memory_Heap_Type::CPU_Visible_GPU,
            .acceleration_structure_memory = false
        });
    auto pipeline = m_asset_repository.get_ray_tracing_pipeline("simple_ray_tracing");
    std::vector<uint8_t> handles;
    handles.resize(properties.shader_group_handle_size * 3);
    device->get_ray_tracing_shader_group_handles(pipeline, 0, 3, handles.data());
    rhi::write_shader_binding_table(sbt_layout, handles.data(), m_sbt_buffer);
}

Simple_Ray_Tracing::~Simple_Ray_Tracing()
{
    m_render_resource_blackboard.destroy_buffer(m_sbt_buffer);
}

void Simple_Ray_Tracing::trace_rays(
    rhi::Command_List* cmd,
    Resource_State_Tracker& tracker,
    const Buffer& camera_buffer,
    const Image& render_target,
    uint32_t tlas_index)
{
    auto* device = m_render_resource_blackboard.get_graphics_device();
    const auto& properties = device->get_ray_tracing_pipeline_properties();

    tracker.use_resource(
        render_target,
        rhi::Barrier_Pipeline_Stage::Ray_Tracing_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);

    auto pipeline = m_asset_repository.get_ray_tracing_pipeline("simple_ray_tracing");
    cmd->set_pipeline(pipeline);

    struct Push_Constants
    {
        uint32_t tlas;
        uint32_t camera;
        uint32_t output;
    } push_constants = {
        .tlas = tlas_index,
        .camera = camera_buffer,
        .output = render_target
    };
    cmd->set_push_constants(push_constants, rhi::Pipeline_Bind_Point::Ray_Tracing);
    const auto render_target_info = render_target.get_create_info();

    auto sbt_layout = rhi::compute_shader_binding_table_layout(properties, 1, 1, 1, 0);
    auto sbt = rhi::make_shader_binding_table(sbt_layout, static_cast<rhi::Buffer*>(m_sbt_buffer)->gpu_address);

    cmd->dispatch_rays(render_target_info.width, render_target_info.height, 1, sbt);
}
}
