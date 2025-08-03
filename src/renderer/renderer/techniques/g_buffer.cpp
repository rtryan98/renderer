#include "renderer/techniques/g_buffer.hpp"
#include "renderer/scene/scene.hpp"
#include "renderer/asset/asset_repository.hpp"
#include "renderer/resource_state_tracker.hpp"

#include <rhi/command_list.hpp>
#include <shared/g_buffer_shared_types.h>

namespace ren::techniques
{
G_Buffer::G_Buffer(Asset_Repository& asset_repository, Render_Resource_Blackboard& render_resource_blackboard,
        uint32_t width, uint32_t height)
    : m_asset_repository(asset_repository)
    , m_render_resource_blackboard(render_resource_blackboard)
{
    rhi::Image_Create_Info default_image_create_info = {
        .format = rhi::Image_Format::Undefined,
        .width = width,
        .height = height,
        .depth = 1,
        .array_size = 1,
        .mip_levels = 1,
        .primary_view_type = rhi::Image_View_Type::Texture_2D
    };

    rhi::Image_Create_Info color_create_info = default_image_create_info;
    color_create_info.format = rhi::Image_Format::R8G8B8A8_SRGB;
    color_create_info.usage = rhi::Image_Usage::Color_Attachment | rhi::Image_Usage::Sampled;
    m_color_render_target = m_render_resource_blackboard.create_image(COLOR_RENDER_TARGET_NAME, color_create_info);

    rhi::Image_Create_Info normal_create_info = default_image_create_info;
    normal_create_info.format = rhi::Image_Format::R16G16B16A16_SFLOAT;
    normal_create_info.usage = rhi::Image_Usage::Color_Attachment | rhi::Image_Usage::Sampled;
    m_normal_render_target = m_render_resource_blackboard.create_image(NORMAL_RENDER_TARGET_NAME, normal_create_info);

    rhi::Image_Create_Info metallic_roughness_create_info = default_image_create_info;
    metallic_roughness_create_info.format = rhi::Image_Format::R8G8_UNORM;
    metallic_roughness_create_info.usage = rhi::Image_Usage::Color_Attachment | rhi::Image_Usage::Sampled;
    m_metallic_roughness_render_target = m_render_resource_blackboard.create_image(METALLIC_ROUGHNESS_RENDER_TARGET_NAME, metallic_roughness_create_info);

    rhi::Image_Create_Info depth_create_info = default_image_create_info;
    depth_create_info.format = rhi::Image_Format::D32_SFLOAT;
    depth_create_info.usage = rhi::Image_Usage::Depth_Stencil_Attachment | rhi::Image_Usage::Sampled;
    m_depth_buffer = m_render_resource_blackboard.create_image(DEPTH_BUFFER_NAME, depth_create_info);

    m_resolve_sampler = m_render_resource_blackboard.get_sampler({
        .filter_min = rhi::Sampler_Filter::Nearest,
        .filter_mag = rhi::Sampler_Filter::Nearest,
        .filter_mip = rhi::Sampler_Filter::Nearest,
        .address_mode_u = rhi::Image_Sample_Address_Mode::Clamp,
        .address_mode_v = rhi::Image_Sample_Address_Mode::Clamp,
        .address_mode_w = rhi::Image_Sample_Address_Mode::Clamp,
        .mip_lod_bias = 0.f,
        .max_anisotropy = 0,
        .comparison_func = rhi::Comparison_Func::None,
        .reduction = rhi::Sampler_Reduction_Type::Standard,
        .border_color = {},
        .min_lod = .0f,
        .max_lod = .0f,
        .anisotropy_enable = false
    });
}

G_Buffer::~G_Buffer()
{
    m_render_resource_blackboard.destroy_image(m_color_render_target);
    m_render_resource_blackboard.destroy_image(m_normal_render_target);
    m_render_resource_blackboard.destroy_image(m_metallic_roughness_render_target);
    m_render_resource_blackboard.destroy_image(m_depth_buffer);
}

void G_Buffer::render_scene_cpu(
    rhi::Command_List* cmd,
    Resource_State_Tracker& tracker,
    const Buffer& camera,
    const Static_Scene_Data& scene_data) const
{
    cmd->begin_debug_region("g_buffer:render_scene_cpu", 1.f, .5f, 1.f);

    tracker.use_resource(
        m_color_render_target,
        rhi::Barrier_Pipeline_Stage::Color_Attachment_Output,
        rhi::Barrier_Access::Color_Attachment_Write,
        rhi::Barrier_Image_Layout::Color_Attachment,
        true);
    tracker.use_resource(
        m_normal_render_target,
        rhi::Barrier_Pipeline_Stage::Color_Attachment_Output,
        rhi::Barrier_Access::Color_Attachment_Write,
        rhi::Barrier_Image_Layout::Color_Attachment,
        true);
    tracker.use_resource(
        m_metallic_roughness_render_target,
        rhi::Barrier_Pipeline_Stage::Color_Attachment_Output,
        rhi::Barrier_Access::Color_Attachment_Write,
        rhi::Barrier_Image_Layout::Color_Attachment,
        true);
    tracker.use_resource(
        m_depth_buffer,
        rhi::Barrier_Pipeline_Stage::Early_Fragment_Tests,
        rhi::Barrier_Access::Depth_Stencil_Attachment_Write,
        rhi::Barrier_Image_Layout::Depth_Stencil_Write,
        true);
    tracker.flush_barriers(cmd);

    auto color_attachment_infos = std::to_array<rhi::Render_Pass_Color_Attachment_Info>(
        {{
            .attachment = m_color_render_target,
            .load_op = rhi::Render_Pass_Attachment_Load_Op::Clear,
            .store_op = rhi::Render_Pass_Attachment_Store_Op::Store,
            .clear_value = {
                .color = { 0.0f, 0.0f, 0.0f, 0.0f }
            }
        },{
            .attachment = m_normal_render_target,
            .load_op = rhi::Render_Pass_Attachment_Load_Op::Clear,
            .store_op = rhi::Render_Pass_Attachment_Store_Op::Store,
            .clear_value = {
                .color = { 0.0f, 0.0f, 0.0f, 0.0f }
            }
        },{
            .attachment = m_metallic_roughness_render_target,
            .load_op = rhi::Render_Pass_Attachment_Load_Op::Clear,
            .store_op = rhi::Render_Pass_Attachment_Store_Op::Store,
            .clear_value = {
                .color = { 0.0f, 0.0f, 0.0f, 0.0f }
            }
        }});
    const rhi::Render_Pass_Begin_Info render_pass_info = {
        .color_attachments = color_attachment_infos,
        .depth_stencil_attachment = {
            .attachment = m_depth_buffer,
            .depth_load_op = rhi::Render_Pass_Attachment_Load_Op::Clear,
            .depth_store_op = rhi::Render_Pass_Attachment_Store_Op::Store,
            .stencil_load_op = rhi::Render_Pass_Attachment_Load_Op::No_Access,
            .stencil_store_op = rhi::Render_Pass_Attachment_Store_Op::No_Access,
            .clear_value = {
                .depth_stencil = { 1.0f, 0 }
            }
        }
    };
    cmd->begin_render_pass(render_pass_info);

    const uint32_t width = static_cast<rhi::Image*>(m_color_render_target)->width;
    const uint32_t height = static_cast<rhi::Image*>(m_color_render_target)->height;

    cmd->set_viewport(0.f, 0.f,
        static_cast<float>(width), static_cast<float>(height), 0.f, 1.f);
    cmd->set_scissor(0, 0, width, height);

    cmd->set_pipeline(m_asset_repository.get_graphics_pipeline("basic_draw"));
    cmd->set_index_buffer(scene_data.get_index_buffer(), rhi::Index_Type::U32);

    for (const auto& model_instance : scene_data.get_instances())
    {
        const auto* model = model_instance.model;
        for (const auto& mesh_instance : model_instance.mesh_instances)
        {
            for (const auto& submesh_instance : mesh_instance.submesh_instances)
            {
                const auto* submesh = submesh_instance.submesh;

                cmd->set_push_constants<Immediate_Draw_Push_Constants>({
                    .position_buffer = model->vertex_positions->buffer_view->bindless_index,
                    .attribute_buffer = model->vertex_attributes->buffer_view->bindless_index,
                    .camera_buffer = camera,
                    .instance_indices_buffer = scene_data.get_instance_buffer()->buffer_view->bindless_index,
                    .instance_transform_buffer = scene_data.get_transform_buffer()->buffer_view->bindless_index,
                    .material_instance_buffer = scene_data.get_material_buffer()->buffer_view->bindless_index,
                }, rhi::Pipeline_Bind_Point::Graphics);

                cmd->draw_indexed(
                    submesh->index_count,
                    1,
                    submesh->first_index + model->index_buffer_allocation.offset,
                    submesh->first_vertex,
                    submesh_instance.instance_index);
            }
        }
    }

    cmd->end_render_pass();
    cmd->end_debug_region(); // g_buffer:render_scene_cpu
}

void G_Buffer::resolve(
    rhi::Command_List* cmd,
    Resource_State_Tracker& tracker,
    const Image& resolve_target)
{
    cmd->begin_debug_region("g_buffer:resolve", 1.f, .5f, 1.f);

    tracker.use_resource(
        m_color_render_target,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Sampled_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(
        m_normal_render_target,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Sampled_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(
        m_metallic_roughness_render_target,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Sampled_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(
        m_depth_buffer,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Sampled_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(
        resolve_target,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);

    const auto resolve_pipeline = m_asset_repository.get_compute_pipeline("g_buffer_resolve");
    cmd->set_pipeline(resolve_pipeline);
    const auto target_width = resolve_target.get_create_info().width;
    const auto target_height = resolve_target.get_create_info().height;
    cmd->set_push_constants<G_Buffer_Resolve_Push_Constants>({
            .albedo = m_color_render_target,
            .normals = m_normal_render_target,
            .metallic_roughness = m_metallic_roughness_render_target,
            .depth = m_depth_buffer,
            .resolve_target = resolve_target,
            .texture_sampler = m_resolve_sampler,
            .width = target_width,
            .height = target_height
        }, rhi::Pipeline_Bind_Point::Compute);
    const auto groups_x = target_width / resolve_pipeline.get_group_size_x();
    const auto groups_y = target_height / resolve_pipeline.get_group_size_y();
    cmd->dispatch(groups_x, groups_y, 1);

    cmd->end_debug_region(); // g_buffer:resolve
}
}
