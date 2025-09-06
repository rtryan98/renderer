#include "renderer/techniques/image_based_lighting.hpp"

#include "renderer/resource_state_tracker.hpp"
#include "renderer/asset/asset_repository.hpp"
#include "renderer/gpu_transfer.hpp"

#include <shared/serialized_asset_formats.hpp>
#include <shared/ibl_shared_types.h>
#include <shared/shared_resources.h>

namespace ren::techniques
{
Image_Based_Lighting::Image_Based_Lighting(
    Asset_Repository& asset_repository,
    GPU_Transfer_Context& gpu_transfer_context,
    Render_Resource_Blackboard& render_resource_blackboard)
    : m_asset_repository(asset_repository)
    , m_gpu_transfer_context(gpu_transfer_context)
    , m_render_resource_blackboard(render_resource_blackboard)
{
    auto* hdri_texture = static_cast<serialization::Image_Data_00*>(m_asset_repository.get_texture("lonely_road_afternoon_puresky_4k.rentex")->data);
    const rhi::Image_Create_Info hdri_create_info = {
        .format = hdri_texture->format,
        .width = hdri_texture->mips[0].width,
        .height = hdri_texture->mips[0].height,
        .depth = 1,
        .array_size = 1,
        .mip_levels = 1,
        .usage = rhi::Image_Usage::Sampled | rhi::Image_Usage::Unordered_Access,
        .primary_view_type = rhi::Image_View_Type::Texture_2D
    };
    m_hdri = render_resource_blackboard.create_image(HDRI_TEXTURE_NAME, hdri_create_info);
    void* image_data = hdri_texture->get_mip_data(0);
    m_gpu_transfer_context.enqueue_immediate_upload(m_hdri, &image_data);

    uint32_t size = std::min(hdri_create_info.width, hdri_create_info.height);
    rhi::Image_Create_Info cubemap_create_info = {
        .format = hdri_texture->format,
        .width = size,
        .height = size,
        .depth = 1,
        .array_size = 6,
        .mip_levels = 1,
        .usage = rhi::Image_Usage::Sampled | rhi::Image_Usage::Unordered_Access,
        .primary_view_type = rhi::Image_View_Type::Texture_Cube
    };
    m_cubemap = render_resource_blackboard.create_image(CUBEMAP_TEXTURE_NAME, cubemap_create_info);

    // cubemap_create_info.format = rhi::Image_Format::R16G16B16A16_SFLOAT;
    // const auto mip_level_count = std::countr_zero(size);
    // cubemap_create_info.mip_levels = mip_level_count >> 4; // Need size of at least 16x16 for the compute shaders
    cubemap_create_info.width = cubemap_create_info.height = 512;
    m_prefiltered_cubemap = render_resource_blackboard.create_image(PREFILTERED_CUBEMAP_TEXTURE_NAME, cubemap_create_info, REN_LIGHTING_DIFFUSE_IRRADIANCE_CUBEMAP);
}

Image_Based_Lighting::~Image_Based_Lighting()
{
    m_render_resource_blackboard.destroy_image(m_hdri);
    m_render_resource_blackboard.destroy_image(m_cubemap);
    m_render_resource_blackboard.destroy_image(m_prefiltered_cubemap);
}

void Image_Based_Lighting::equirectangular_to_cubemap(rhi::Command_List* cmd, Resource_State_Tracker& tracker)
{
    cmd->begin_debug_region("image_based_lighting:bake:equirectangular_to_cubemap", 0.1f, 0.25f, 0.1f);
    const auto cube_width = m_cubemap.get_create_info().width;
    const auto cube_height = m_cubemap.get_create_info().height;
    const auto pipeline = m_asset_repository.get_compute_pipeline("equirectangular_to_cubemap");
    tracker.use_resource(m_hdri,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(m_cubemap,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);
    cmd->set_pipeline(pipeline);
    cmd->set_push_constants<Equirectangular_To_Cubemap_Push_Constants>({
        .image_size = { cube_width, cube_height },
        .source_image = m_hdri,
        .target_cubemap = m_cubemap,
        .source_image_sampler = m_render_resource_blackboard.get_sampler({
            .filter_min = rhi::Sampler_Filter::Linear,
            .filter_mag = rhi::Sampler_Filter::Linear,
            .filter_mip = rhi::Sampler_Filter::Linear,
            .address_mode_u = rhi::Image_Sample_Address_Mode::Wrap,
            .address_mode_v = rhi::Image_Sample_Address_Mode::Wrap,
            .address_mode_w = rhi::Image_Sample_Address_Mode::Wrap,
            .mip_lod_bias = 0.0f,
            .max_anisotropy = 0,
            .comparison_func = rhi::Comparison_Func::None,
            .reduction = rhi::Sampler_Reduction_Type::Standard,
            .min_lod = 0.0,
            .max_lod = 0.0,
            .anisotropy_enable = false
        }),
    }, rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(cube_width / pipeline.get_group_size_x(), cube_height / pipeline.get_group_size_y(), 6);
    tracker.use_resource(m_cubemap,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.flush_barriers(cmd);
    cmd->end_debug_region();
}

void Image_Based_Lighting::prefilter_diffuse_irradiance(rhi::Command_List* cmd, Resource_State_Tracker& tracker)
{
    cmd->begin_debug_region("image_based_lighting:bake:prefilter_diffuse_irradiance", 0.1f, 0.25f, 0.1f);

    const auto cube_width = m_prefiltered_cubemap.get_create_info().width;
    const auto cube_height = m_prefiltered_cubemap.get_create_info().height;
    const auto pipeline = m_asset_repository.get_compute_pipeline("ibl_prefilter_diffuse");
    tracker.use_resource(m_prefiltered_cubemap,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);
    cmd->set_pipeline(pipeline);
    cmd->set_push_constants<Prefilter_Diffuse_Irradiance_Push_Constants>({
        .image_size = { cube_width, cube_height },
        .source_cubemap = m_cubemap,
        .cubemap_sampler = m_render_resource_blackboard.get_sampler({
            .filter_min = rhi::Sampler_Filter::Linear,
            .filter_mag = rhi::Sampler_Filter::Linear,
            .filter_mip = rhi::Sampler_Filter::Linear,
            .address_mode_u = rhi::Image_Sample_Address_Mode::Wrap,
            .address_mode_v = rhi::Image_Sample_Address_Mode::Wrap,
            .address_mode_w = rhi::Image_Sample_Address_Mode::Wrap,
            .mip_lod_bias = 0.0f,
            .max_anisotropy = 0,
            .comparison_func = rhi::Comparison_Func::None,
            .reduction = rhi::Sampler_Reduction_Type::Standard,
            .min_lod = 0.0,
            .max_lod = 0.0,
            .anisotropy_enable = false
        }),
        .target_cubemap = m_prefiltered_cubemap,
    }, rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(cube_width / pipeline.get_group_size_x(), cube_height / pipeline.get_group_size_y(), 6);
    tracker.use_resource(m_prefiltered_cubemap,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.flush_barriers(cmd);

    cmd->end_debug_region();
}

void Image_Based_Lighting::bake(rhi::Command_List* cmd, Resource_State_Tracker& tracker)
{
    if (m_baked)
    {
        return;
    }

    cmd->begin_debug_region("image_based_lighting:bake", 0.25f, 0.25f, 0.25f);
    equirectangular_to_cubemap(
        cmd,
        tracker);
    prefilter_diffuse_irradiance(
        cmd,
        tracker);
    cmd->end_debug_region();

    m_baked = true;
}

void Image_Based_Lighting::skybox_render(
    rhi::Command_List* cmd,
    Resource_State_Tracker& tracker,
    const Buffer& camera,
    const Image& shaded_geometry_render_target,
    const Image& geometry_depth_buffer)
{
    cmd->begin_debug_region("image_based_lighting:skybox_apply", 0.1f, 0.25f, 0.1f);

    tracker.use_resource(shaded_geometry_render_target,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.use_resource(geometry_depth_buffer,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.flush_barriers(cmd);

    const auto width = shaded_geometry_render_target.get_create_info().width;
    const auto height = shaded_geometry_render_target.get_create_info().height;
    const auto pipeline = m_asset_repository.get_compute_pipeline("skybox");

    cmd->set_pipeline(pipeline);
    cmd->set_push_constants<Skybox_Push_Constants>({
        .image_size = { width, height },
        .depth_buffer = geometry_depth_buffer,
        .target_image = shaded_geometry_render_target,
        .cubemap = m_cubemap,
        .cubemap_sampler = m_render_resource_blackboard.get_sampler({
            .filter_min = rhi::Sampler_Filter::Linear,
            .filter_mag = rhi::Sampler_Filter::Linear,
            .filter_mip = rhi::Sampler_Filter::Linear,
            .address_mode_u = rhi::Image_Sample_Address_Mode::Wrap,
            .address_mode_v = rhi::Image_Sample_Address_Mode::Wrap,
            .address_mode_w = rhi::Image_Sample_Address_Mode::Wrap,
            .mip_lod_bias = 0.0f,
            .max_anisotropy = 0,
            .comparison_func = rhi::Comparison_Func::None,
            .reduction = rhi::Sampler_Reduction_Type::Standard,
            .min_lod = 0.0,
            .max_lod = 0.0,
            .anisotropy_enable = false
        }),
        .camera_buffer = camera
    }, rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(width / pipeline.get_group_size_x(), height / pipeline.get_group_size_y(), 1);
    cmd->end_debug_region();
}
}
