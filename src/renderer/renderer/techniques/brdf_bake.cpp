#include "renderer/techniques/brdf_bake.hpp"

#include "renderer/resource_state_tracker.hpp"
#include "renderer/asset/asset_repository.hpp"

#include <shared/ibl_shared_types.h>
#include <shared/shared_resources.h>

namespace ren::techniques
{
BRDF_LUT::BRDF_LUT(
    Asset_Repository& asset_repository,
    Render_Resource_Blackboard& render_resource_blackboard)
    : m_asset_repository(asset_repository)
    , m_render_resource_blackboard(render_resource_blackboard)
{
    constexpr rhi::Image_Create_Info brdf_lut_create_info = {
        .format = rhi::Image_Format::R16G16_SFLOAT,
        .width = 256,
        .height = 256,
        .depth = 1,
        .array_size = 1,
        .mip_levels = 1,
        .usage = rhi::Image_Usage::Sampled | rhi::Image_Usage::Unordered_Access,
        .primary_view_type = rhi::Image_View_Type::Texture_2D
    };
    m_brdf_lut = m_render_resource_blackboard.create_image(
        LUT_TEXTURE_NAME,
        brdf_lut_create_info,
        REN_LIGHTING_BRDF_LUT_TEXTURE);
}

BRDF_LUT::~BRDF_LUT()
{
    m_render_resource_blackboard.destroy_image(m_brdf_lut);
}

void BRDF_LUT::bake_brdf_lut(rhi::Command_List* cmd, Resource_State_Tracker& tracker)
{
    if (m_baked)
        return;

    cmd->begin_debug_region("pbr:bake_brdf_lut", 0.1f, 0.25f, 0.1f);

    const auto width = m_brdf_lut.get_create_info().width;
    const auto height = m_brdf_lut.get_create_info().height;
    const auto pipeline = m_asset_repository.get_compute_pipeline("brdf_bake");
    tracker.use_resource(m_brdf_lut,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);
    cmd->set_pipeline(pipeline);
    cmd->set_push_constants<BRDF_LUT_Bake_Push_Constants>({
        .image_size = { width, height },
        .lut = m_brdf_lut,
    }, rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(width / pipeline.get_group_size_x(), height / pipeline.get_group_size_y(), 1);
    tracker.use_resource(m_brdf_lut,
        rhi::Barrier_Pipeline_Stage::All_Commands,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.flush_barriers(cmd);

    m_baked = true;

    cmd->end_debug_region(); // pbr:bake_brdf_lut
}
}
