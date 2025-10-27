#include "renderer/techniques/image_based_lighting.hpp"

#include "renderer/resource_state_tracker.hpp"
#include "renderer/asset/asset_repository.hpp"
#include "renderer/gpu_transfer.hpp"

#include <shared/serialized_asset_formats.hpp>
#include <shared/ibl_shared_types.h>
#include <shared/shared_resources.h>
#include <shared/mipmap_gen_shared_types.h>

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

    constexpr uint32_t SIZE = 2048;
    rhi::Image_Create_Info cubemap_create_info = {
        .format = hdri_texture->format,
        .width = SIZE,
        .height = SIZE,
        .depth = 1,
        .array_size = 6,
        .mip_levels = 8,
        .usage = rhi::Image_Usage::Sampled | rhi::Image_Usage::Unordered_Access,
        .primary_view_type = rhi::Image_View_Type::Texture_Cube
    };
    m_environment_cubemap = render_resource_blackboard.create_image(ENVIRONMENT_CUBEMAP_TEXTURE_NAME, cubemap_create_info);
    m_environment_cubemap_views.reserve(cubemap_create_info.mip_levels);
    for (uint16_t i = 0; i < cubemap_create_info.mip_levels; ++i)
    {
        Image_View_Subresource_Info subresource = {
            .mip_level = i,
            .first_array_level = 0,
            .array_levels = 6,
            .view_type = rhi::Image_View_Type::Texture_2D_Array,
        };
        m_environment_cubemap_views.push_back(m_environment_cubemap.create_image_view(subresource));
    }

    cubemap_create_info.width = cubemap_create_info.height = 512;
    cubemap_create_info.mip_levels = 1;
    m_prefiltered_diffuse_irradiance_cubemap = render_resource_blackboard.create_image(
        PREFILTERED_DIFFUSE_IRRADIANCE_CUBEMAP_TEXTURE_NAME,
        cubemap_create_info,
        REN_LIGHTING_DIFFUSE_IRRADIANCE_CUBEMAP);

    cubemap_create_info.mip_levels = 5;
    m_prefiltered_specular_irradiance_cubemap = render_resource_blackboard.create_image(
        PREFILTERED_SPECULAR_IRRADIANCE_CUBEMAP_TEXTURE_NAME,
        cubemap_create_info,
        REN_LIGHTING_SPECULAR_IRRADIANCE_CUBEMAP);

    for (uint16_t i = 0; i < cubemap_create_info.mip_levels; ++i)
    {
        Image_View_Subresource_Info subresource = {
            .mip_level = i,
            .first_array_level = 0,
            .array_levels = 6,
            .view_type = rhi::Image_View_Type::Texture_Cube,
        };
        m_prefiltered_specular_irradiance_cubemap_views[i] = m_prefiltered_specular_irradiance_cubemap.create_image_view(subresource);
    }
}

Image_Based_Lighting::~Image_Based_Lighting()
{
    m_render_resource_blackboard.destroy_image(m_hdri);
    m_render_resource_blackboard.destroy_image(m_environment_cubemap);
    m_render_resource_blackboard.destroy_image(m_prefiltered_diffuse_irradiance_cubemap);
    m_render_resource_blackboard.destroy_image(m_prefiltered_specular_irradiance_cubemap);
}

void Image_Based_Lighting::equirectangular_to_cubemap(rhi::Command_List* cmd, Resource_State_Tracker& tracker)
{
    cmd->begin_debug_region("image_based_lighting:bake:equirectangular_to_cubemap", 0.1f, 0.25f, 0.1f);
    const auto cube_size = m_environment_cubemap.get_create_info().width;
    const auto mip_levels = m_environment_cubemap.get_create_info().mip_levels;
    const auto equirectangular_to_cubemap_pipeline = m_asset_repository.get_compute_pipeline("equirectangular_to_cubemap");
    const auto mipmap_gen_pipeline = m_asset_repository.get_compute_pipeline("mipmap_gen");

    tracker.set_resource_state(m_hdri,
        rhi::Barrier_Pipeline_Stage::All_Commands,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(m_hdri,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(m_environment_cubemap,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);
    cmd->set_pipeline(equirectangular_to_cubemap_pipeline);
    cmd->set_push_constants<Equirectangular_To_Cubemap_Push_Constants>({
        .image_size = { cube_size, cube_size },
        .source_image = m_hdri,
        .target_cubemap = m_environment_cubemap,
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
    cmd->dispatch(
        cube_size / equirectangular_to_cubemap_pipeline.get_group_size_x(),
        cube_size / equirectangular_to_cubemap_pipeline.get_group_size_y(),
        6);
    cmd->set_pipeline(mipmap_gen_pipeline);
    for (auto i = 1; i < mip_levels; ++i)
    {
        tracker.use_resource(m_environment_cubemap,
            rhi::Barrier_Pipeline_Stage::Compute_Shader,
            rhi::Barrier_Access::Unordered_Access_Read | rhi::Barrier_Access::Unordered_Access_Write,
            rhi::Barrier_Image_Layout::Unordered_Access);
        tracker.flush_barriers(cmd);
        cmd->set_push_constants<Mipmap_Gen_Push_Constants>({
            .src = m_environment_cubemap_views[i - 1],
            .dst = m_environment_cubemap_views[i],
            .is_array = true
        }, rhi::Pipeline_Bind_Point::Compute);
        cmd->dispatch(
            (cube_size >> i) / mipmap_gen_pipeline.get_group_size_x(),
            (cube_size >> i) / mipmap_gen_pipeline.get_group_size_y(),
            6);
    }
    tracker.use_resource(m_environment_cubemap,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.flush_barriers(cmd);
    cmd->end_debug_region();
}

void Image_Based_Lighting::prefilter_diffuse_irradiance(rhi::Command_List* cmd, Resource_State_Tracker& tracker)
{
    cmd->begin_debug_region("image_based_lighting:bake:prefilter_diffuse_irradiance", 0.1f, 0.25f, 0.1f);

    const auto cube_size = m_prefiltered_diffuse_irradiance_cubemap.get_create_info().width;
    const auto pipeline = m_asset_repository.get_compute_pipeline("ibl_prefilter_diffuse");
    tracker.use_resource(m_prefiltered_diffuse_irradiance_cubemap,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);
    cmd->set_pipeline(pipeline);
    cmd->set_push_constants<Prefilter_Diffuse_Irradiance_Push_Constants>({
        .image_size = { cube_size, cube_size },
        .source_cubemap = m_environment_cubemap,
        .target_cubemap = m_prefiltered_diffuse_irradiance_cubemap,
        .samples = 4096
    }, rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(cube_size / pipeline.get_group_size_x(), cube_size / pipeline.get_group_size_y(), 6);
    tracker.use_resource(m_prefiltered_diffuse_irradiance_cubemap,
        rhi::Barrier_Pipeline_Stage::All_Commands,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.flush_barriers(cmd);

    cmd->end_debug_region();
}

void Image_Based_Lighting::prefilter_specular_irradiance(rhi::Command_List* cmd, Resource_State_Tracker& tracker)
{
    cmd->begin_debug_region("image_based_lighting:bake:prefilter_specular_irradiance", 0.1f, 0.25f, 0.1f);

    const auto cube_size = m_prefiltered_specular_irradiance_cubemap.get_create_info().width;
    const auto mip_count = m_prefiltered_specular_irradiance_cubemap.get_create_info().mip_levels;
    const auto pipeline = m_asset_repository.get_compute_pipeline("ibl_prefilter_specular");
    tracker.use_resource(m_prefiltered_specular_irradiance_cubemap,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);
    cmd->set_pipeline(pipeline);
    for (auto i = 0; i < mip_count; ++i)
    {
        cmd->set_push_constants<Prefilter_Specular_Irradiance_Push_Constants>({
            .image_size = { cube_size >> i, cube_size >> i },
            .source_cubemap = m_environment_cubemap,
            .target_cubemap = m_prefiltered_specular_irradiance_cubemap_views[i],
            .roughness = static_cast<float>(i) / static_cast<float>(mip_count - 1),
            .samples = 4096
        }, rhi::Pipeline_Bind_Point::Compute);
        cmd->dispatch((cube_size >> i) / pipeline.get_group_size_x(), (cube_size >> i) / pipeline.get_group_size_y(), 6);
    }
    tracker.use_resource(m_prefiltered_specular_irradiance_cubemap,
        rhi::Barrier_Pipeline_Stage::All_Commands,
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
    prefilter_specular_irradiance(
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
        .cubemap = m_environment_cubemap,
        .camera_buffer = camera
    }, rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(width / pipeline.get_group_size_x(), height / pipeline.get_group_size_y(), 1);
    cmd->end_debug_region();
}
}
