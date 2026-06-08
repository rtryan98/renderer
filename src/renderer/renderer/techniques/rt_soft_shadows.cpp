#include "renderer/techniques/rt_soft_shadows.hpp"

#include "renderer/asset/asset_repository.hpp"
#include "renderer/resource_state_tracker.hpp"

#include <rhi/command_list.hpp>

namespace ren::techniques
{
RT_Soft_Shadows::RT_Soft_Shadows(
    Asset_Repository& asset_repository,
    GPU_Transfer_Context& gpu_transfer_context,
    Render_Resource_Blackboard& render_resource_blackboard,
    uint32_t width, uint32_t height)
    : Technique_Base(asset_repository, gpu_transfer_context, render_resource_blackboard)
{
    rhi::Image_Create_Info sun_visibility_texture_create_info = {
        .format = rhi::Image_Format::R8_UINT,
        .width = width / 8,
        .height = height / 4,
        .depth = 1,
        .array_size = 1,
        .mip_levels = 1,
        .usage = rhi::Image_Usage::Sampled | rhi::Image_Usage::Unordered_Access,
        .primary_view_type = rhi::Image_View_Type::Texture_2D
    };
    m_sun_visibility_texture = m_render_resource_blackboard.create_image(
        SUN_VISIBILITY_TEXTURE_NAME, sun_visibility_texture_create_info);
}

RT_Soft_Shadows::~RT_Soft_Shadows()
{
    m_render_resource_blackboard.destroy_image(m_sun_visibility_texture);
}

void RT_Soft_Shadows::trace_shadow_rays(
    rhi::Command_List* cmd,
    Resource_State_Tracker& tracker,
    const Image& normals_render_target,
    const Image& depth_render_target)
{
    cmd->begin_debug_region("rt_soft_shadows:trace_shadow_rays", 0.2f, 0.2f, 0.2f);

    tracker.use_resource(
        normals_render_target,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(
        depth_render_target,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(
        m_sun_visibility_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);

    auto pipeline = m_asset_repository.get_compute_pipeline("trace_shadow_rays");
    cmd->set_pipeline(pipeline);
    cmd->dispatch(
        normals_render_target.get_create_info().width / pipeline.get_group_size_x(),
        normals_render_target.get_create_info().height / pipeline.get_group_size_y(),
        1);

    cmd->end_debug_region();
}
}
