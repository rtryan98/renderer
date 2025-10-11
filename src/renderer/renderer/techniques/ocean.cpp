#include "renderer/techniques/ocean.hpp"
#include "renderer/resource_state_tracker.hpp"
#include "renderer/asset/asset_repository.hpp"
#include "renderer/gpu_transfer.hpp"
#include "renderer/imgui/imgui_util.hpp"

#include "renderer/scene/camera.hpp"

#include <rhi/command_list.hpp>
#include <shared/ocean_shared_types.h>
#include <shared/fft_shared_types.h>

#include <imgui.h>

namespace ren::techniques
{
constexpr static auto FIELD_SIZE = 2048;
constexpr static auto TILES_PER_AXIS = 16u;
constexpr static auto TILE_VERTEX_COUNT = FIELD_SIZE / TILES_PER_AXIS + 1;
constexpr static auto VERTEX_DIST = 0.25f;
constexpr static auto TILE_SIZE = static_cast<float>(TILE_VERTEX_COUNT - 1) * VERTEX_DIST;

Ocean::Ocean(Asset_Repository& asset_repository, GPU_Transfer_Context& gpu_transfer_context,
    Render_Resource_Blackboard& render_resource_blackboard, uint32_t width, uint32_t height)
    : m_asset_repository(asset_repository)
    , m_gpu_transfer_context(gpu_transfer_context)
    , m_render_resource_blackboard(render_resource_blackboard)
{
    m_spectrum_parameters_buffer = m_render_resource_blackboard.create_buffer(
        SPECTRUM_PARAMETERS_BUFFER_NAME,
        {
            .size = sizeof(Ocean_Initial_Spectrum_Data),
            .heap = rhi::Memory_Heap_Type::GPU
        });
    m_spectrum_state_texture = m_render_resource_blackboard.create_image(
        SPECTRUM_STATE_TEXTURE_NAME, options.generate_create_info(rhi::Image_Format::R16G16B16A16_SFLOAT));
    m_spectrum_angular_frequency_texture = m_render_resource_blackboard.create_image(
        SPECTRUM_ANGULAR_FREQUENCY_TEXTURE_NAME, options.generate_create_info(rhi::Image_Format::R16_SFLOAT));
    m_displacement_x_y_z_xdx_texture = m_render_resource_blackboard.create_image(
        DISPLACEMENT_X_Y_Z_XDX_TEXTURE_NAME, options.generate_create_info(rhi::Image_Format::R16G16B16A16_SFLOAT));
    m_displacement_ydx_zdx_zdz_zdy_texture = m_render_resource_blackboard.create_image(
        DISPLACEMENT_YDX_ZDX_YDY_ZDY_TEXTURE_NAME, options.generate_create_info(rhi::Image_Format::R16G16B16A16_SFLOAT));
    const rhi::Image_Create_Info depth_stencil_create_info = {
        .format = rhi::Image_Format::D32_SFLOAT,
        .width = width,
        .height = height,
        .depth = 1,
        .array_size = 1,
        .mip_levels = 1,
        .usage = rhi::Image_Usage::Depth_Stencil_Attachment,
        .primary_view_type = rhi::Image_View_Type::Texture_2D
    };
    m_forward_pass_depth_render_target = m_render_resource_blackboard.create_image(
        FORWARD_PASS_DEPTH_RENDER_TARGET_NAME, depth_stencil_create_info);

    {
        std::vector<uint16_t> index_buffer;
        index_buffer.reserve(3 * 2 * (TILE_VERTEX_COUNT - 1) * (TILE_VERTEX_COUNT - 1));
        for (auto i = 0; i < (TILE_VERTEX_COUNT - 1) * (TILE_VERTEX_COUNT - 1); ++i)
        {
            uint16_t x = _pext_u32(i, 0x55555555);
            uint16_t y = _pext_u32(i, 0xaaaaaaaa);
            index_buffer.push_back(y * TILE_VERTEX_COUNT + x);
            index_buffer.push_back(y * TILE_VERTEX_COUNT + x + 1);
            index_buffer.push_back(y * TILE_VERTEX_COUNT + x + TILE_VERTEX_COUNT);
            index_buffer.push_back(y * TILE_VERTEX_COUNT + x + 1);
            index_buffer.push_back(y * TILE_VERTEX_COUNT + x + 1 + TILE_VERTEX_COUNT);
            index_buffer.push_back(y * TILE_VERTEX_COUNT + x + TILE_VERTEX_COUNT);
        }

        m_tile_index_buffer = m_render_resource_blackboard.create_buffer(
            TILE_INDEX_BUFFER_NAME,
            {
                .size = sizeof(uint16_t) * index_buffer.size(),
                .heap = rhi::Memory_Heap_Type::GPU
            });
        m_gpu_transfer_context.enqueue_immediate_upload(m_tile_index_buffer, index_buffer.data(), m_tile_index_buffer.size(), 0);
    }

    rhi::Image_Create_Info min_max_texture_create_info = {
        .format = rhi::Image_Format::R32G32B32A32_SFLOAT,
        .width = 1024,
        .height = 2,
        .depth = 1,
        .array_size = 2 * 4,
        .mip_levels = 1,
        .usage = rhi::Image_Usage::Unordered_Access | rhi::Image_Usage::Sampled,
        .primary_view_type = rhi::Image_View_Type::Texture_2D_Array
    };
    m_minmax_texture = m_render_resource_blackboard.create_image(
        FFT_MIN_MAX_TEXTURE_NAME, min_max_texture_create_info);
    m_minmax_buffer = m_render_resource_blackboard.create_buffer(
        FFT_MINMAX_BUFFER_NAME,
        {
            .size = sizeof(glm::vec4) * 2 * 2 * 4,
            .heap = rhi::Memory_Heap_Type::GPU
        });
    m_packed_displacement_texture = m_render_resource_blackboard.create_image(
        PACKED_DISPLACEMENT_TEXTURE_NAME, options.generate_create_info(rhi::Image_Format::A2R10G10B10_UNORM_PACK32));
    m_packed_derivatives_texture = m_render_resource_blackboard.create_image(
        PACKED_DERIVATIVES_TEXTURE_NAME, options.generate_create_info(rhi::Image_Format::R8G8B8A8_UNORM));
    m_packed_xdx_texture = m_render_resource_blackboard.create_image(
        FOAM_WEIGHT_TEXTURE_NAME, options.generate_create_info(rhi::Image_Format::R8_UNORM));
}

Ocean::~Ocean()
{
    m_render_resource_blackboard.destroy_buffer(m_spectrum_parameters_buffer);
    m_render_resource_blackboard.destroy_image(m_spectrum_state_texture);
    m_render_resource_blackboard.destroy_image(m_spectrum_angular_frequency_texture);
    m_render_resource_blackboard.destroy_image(m_displacement_x_y_z_xdx_texture);
    m_render_resource_blackboard.destroy_image(m_displacement_ydx_zdx_zdz_zdy_texture);
    m_render_resource_blackboard.destroy_image(m_forward_pass_depth_render_target);
}

void Ocean::update(float dt)
{
    if (options.update_time)
        simulation_data.total_time += dt;

    Ocean_Initial_Spectrum_Data gpu_spectrum_data = {
        .spectra = {
            {
                .u = simulation_data.full_spectrum_parameters.single_spectrum_parameters[0].wind_speed,
                .f = simulation_data.full_spectrum_parameters.single_spectrum_parameters[0].fetch,
                .phillips_alpha = simulation_data.full_spectrum_parameters.single_spectrum_parameters[0].phillips_alpha,
                .generalized_a = simulation_data.full_spectrum_parameters.single_spectrum_parameters[0].generalized_a,
                .generalized_b = simulation_data.full_spectrum_parameters.single_spectrum_parameters[0].generalized_b,
                .contribution = simulation_data.full_spectrum_parameters.single_spectrum_parameters[0].contribution,
                .wind_direction = simulation_data.full_spectrum_parameters.single_spectrum_parameters[0].wind_direction
            },
            {
                .u = simulation_data.full_spectrum_parameters.single_spectrum_parameters[1].wind_speed,
                .f = simulation_data.full_spectrum_parameters.single_spectrum_parameters[1].fetch,
                .phillips_alpha = simulation_data.full_spectrum_parameters.single_spectrum_parameters[1].phillips_alpha,
                .generalized_a = simulation_data.full_spectrum_parameters.single_spectrum_parameters[1].generalized_a,
                .generalized_b = simulation_data.full_spectrum_parameters.single_spectrum_parameters[1].generalized_b,
                .contribution = simulation_data.full_spectrum_parameters.single_spectrum_parameters[1].contribution,
                .wind_direction = simulation_data.full_spectrum_parameters.single_spectrum_parameters[1].wind_direction
            }
        },
        .active_cascades = simulation_data.full_spectrum_parameters.active_cascades,
        .length_scales = simulation_data.full_spectrum_parameters.length_scales,
        .spectrum = simulation_data.full_spectrum_parameters.oceanographic_spectrum,
        .directional_spreading_function = simulation_data.full_spectrum_parameters.directional_spreading_function,
        .texture_size = options.texture_size,
        .g = simulation_data.full_spectrum_parameters.gravity,
        .h = simulation_data.full_spectrum_parameters.depth
    };
    m_gpu_transfer_context.enqueue_immediate_upload(m_spectrum_parameters_buffer, gpu_spectrum_data);
}

void Ocean::simulate(
    rhi::Command_List* cmd,
    Resource_State_Tracker& tracker)
{
    if (!options.enabled) return;

    cmd->begin_debug_region("ocean:simulation", 0.5f, 0.5f, 1.f);

    cmd->begin_debug_region("ocean:simulation:initial_spectrum", 0.25f, 0.0f, 1.0f);
    tracker.use_resource(m_spectrum_state_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.use_resource(m_spectrum_angular_frequency_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);
    auto initial_spectrum_pipeline = m_asset_repository.get_compute_pipeline("initial_spectrum");
    uint32_t dispatch_group_count = options.texture_size / initial_spectrum_pipeline.get_group_size_x();
    cmd->set_pipeline(initial_spectrum_pipeline);
    cmd->set_push_constants<Ocean_Initial_Spectrum_Push_Constants>({
        .data = m_spectrum_parameters_buffer,
        .spectrum_tex = m_spectrum_state_texture,
        .angular_frequency_tex = m_spectrum_angular_frequency_texture },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(
        dispatch_group_count,
        dispatch_group_count,
        options.cascade_count);
    cmd->end_debug_region(); // ocean:simulation:initial_spectrum

    cmd->begin_debug_region("ocean:simulation:time_dependent_spectrum", 0.25f, 0.125f, 1.0f);
    tracker.use_resource(m_spectrum_state_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(m_spectrum_angular_frequency_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(m_displacement_x_y_z_xdx_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.use_resource(m_displacement_ydx_zdx_zdz_zdy_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);
    cmd->set_pipeline(m_asset_repository.get_compute_pipeline("time_dependent_spectrum"));
    cmd->set_push_constants<Ocean_Time_Dependent_Spectrum_Push_Constants>({
        .initial_spectrum_tex = m_spectrum_state_texture,
        .angular_frequency_tex = m_spectrum_angular_frequency_texture,
        .x_y_z_xdx_tex = m_displacement_x_y_z_xdx_texture,
        .ydx_zdx_ydy_zdy_tex = m_displacement_ydx_zdx_zdz_zdy_texture,
        .texture_size = options.texture_size,
        .time = simulation_data.total_time},
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(
        dispatch_group_count,
        dispatch_group_count,
        options.cascade_count);
    cmd->end_debug_region(); // ocean:simulation:time_dependent_spectrum

    auto select_fft_variant = [&](bool minmax = false)
    {
        std::stringstream ss;
        ss << "fft_";
        ss << std::to_string(options.texture_size);
        ss << "_float4";
        if (minmax) ss << "_minmax";
        return ss.str();
    };
    cmd->set_pipeline(m_asset_repository.get_compute_pipeline("fft").set_variant(select_fft_variant()));
    cmd->begin_debug_region("ocean:simulation:inverse_fft:vertical", 0.25f, 0.25f, 1.0f);
    tracker.use_resource(
        m_displacement_x_y_z_xdx_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Read,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.use_resource(
        m_displacement_ydx_zdx_zdz_zdy_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Read,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);
    cmd->set_push_constants<FFT_Push_Constants>({
            .image = m_displacement_x_y_z_xdx_texture,
            .vertical_or_horizontal = FFT_VERTICAL,
            .inverse = true },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(1, options.texture_size, options.cascade_count);
    tracker.flush_barriers(cmd);
    cmd->set_push_constants<FFT_Push_Constants>({
            .image = m_displacement_ydx_zdx_zdz_zdy_texture,
            .vertical_or_horizontal = FFT_VERTICAL,
            .inverse = true },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(1, options.texture_size, options.cascade_count);
    tracker.set_resource_state(
        m_displacement_x_y_z_xdx_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.set_resource_state(
        m_displacement_ydx_zdx_zdz_zdy_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    cmd->end_debug_region(); // ocean:simulation:inverse_fft:vertical

    cmd->begin_debug_region("ocean:simulation:inverse_fft:horizontal", 0.25f, 0.375f, 1.0f);
    cmd->set_pipeline(m_asset_repository.get_compute_pipeline("fft").set_variant(select_fft_variant(true)));
    tracker.use_resource(
        m_displacement_x_y_z_xdx_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Read,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.use_resource(
        m_displacement_ydx_zdx_zdz_zdy_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Read,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.use_resource(
        m_minmax_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);
    cmd->set_push_constants<FFT_Push_Constants>({
            .image = m_displacement_x_y_z_xdx_texture,
            .vertical_or_horizontal = FFT_HORIZONTAL,
            .inverse = true,
            .min_max_tex = m_minmax_texture,
            .min_max_tex_store_offset = 0},
            rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(1, options.texture_size, options.cascade_count);
    tracker.flush_barriers(cmd);
    cmd->set_push_constants<FFT_Push_Constants>({
            .image = m_displacement_ydx_zdx_zdz_zdy_texture,
            .vertical_or_horizontal = FFT_HORIZONTAL,
            .inverse = true,
            .min_max_tex = m_minmax_texture,
            .min_max_tex_store_offset = 4 },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(1, options.texture_size, options.cascade_count);
    tracker.set_resource_state(
        m_displacement_x_y_z_xdx_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.set_resource_state(
        m_displacement_ydx_zdx_zdz_zdy_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    cmd->end_debug_region(); // ocean:simulation:inverse_fft:horizontal
    cmd->begin_debug_region("ocean:simulation:inverse_fft:min_max_resolve", 0.25f, 0.5f, 1.0f);
    tracker.use_resource(
        m_minmax_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(
        m_minmax_buffer,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write);
    auto select_fft_min_max_resolve_variant = [&]()
        {
            std::stringstream ss;
            ss << "fft_min_max_resolve";
            ss << std::to_string(options.texture_size);
            return ss.str();
        };
    cmd->set_pipeline(m_asset_repository.get_compute_pipeline("fft_min_max_resolve").set_variant(select_fft_min_max_resolve_variant()));
    cmd->set_push_constants<FFT_Min_Max_Resolve_Push_Constants>({
        .min_max_tex = m_minmax_texture,
        .min_max_tex_load_offset = 0,
        .min_max_buffer = m_minmax_buffer,
        .min_max_buffer_store_offset = 0},
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(1, 1, options.cascade_count);
    cmd->set_push_constants<FFT_Min_Max_Resolve_Push_Constants>({
        .min_max_tex = m_minmax_texture,
        .min_max_tex_load_offset = 4,
        .min_max_buffer = m_minmax_buffer,
        .min_max_buffer_store_offset = 2 * 4 },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(1, 1, options.cascade_count);

    cmd->end_debug_region(); // ocean:simulation:inverse_fft:min_max_resolve

    cmd->begin_debug_region("ocean:simulation:reorder_textures", 0.25f, 0.625f, 1.0f);
    tracker.use_resource(
        m_displacement_x_y_z_xdx_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(
        m_displacement_ydx_zdx_zdz_zdy_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(
        m_minmax_buffer,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Read);
    tracker.use_resource(
        m_displacement_ydx_zdx_zdz_zdy_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.use_resource(
        m_displacement_ydx_zdx_zdz_zdy_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.use_resource(
        m_displacement_ydx_zdx_zdz_zdy_texture,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);
    auto reorder_pipeline = m_asset_repository.get_compute_pipeline("ocean_texture_reorder");
    dispatch_group_count = options.texture_size / reorder_pipeline.get_group_size_x();
    cmd->set_pipeline(reorder_pipeline);
    cmd->set_push_constants<Ocean_Reorder_Push_Constants>({
        .min_max_buffer = m_minmax_buffer,
        .x_y_z_xdx_tex = m_displacement_x_y_z_xdx_texture,
        .ydx_zdx_ydy_zdy_tex = m_displacement_ydx_zdx_zdz_zdy_texture,
        .displacement_tex = m_packed_displacement_texture,
        .derivatives_tex = m_packed_derivatives_texture,
        .foam_tex = m_packed_xdx_texture },
        rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(dispatch_group_count, dispatch_group_count, options.cascade_count);

    cmd->end_debug_region(); // ocean:simulation:reorder_textures

    cmd->end_debug_region(); // ocean:simulation
}

void Ocean::depth_pre_pass(rhi::Command_List* cmd, Resource_State_Tracker& tracker, const Buffer& camera,
    const Image& shaded_scene_depth_render_target, const Fly_Camera& cull_camera)
{
    if (!options.enabled) return;

    cmd->begin_debug_region("ocean:render:depth_pre_pass", 0.25f, 0.0f, 1.0f);

    const uint32_t width = static_cast<rhi::Image*>(m_forward_pass_depth_render_target)->width;
    const uint32_t height = static_cast<rhi::Image*>(m_forward_pass_depth_render_target)->height;

    tracker.use_resource(
        shaded_scene_depth_render_target,
        rhi::Barrier_Pipeline_Stage::Copy,
        rhi::Barrier_Access::Transfer_Read,
        rhi::Barrier_Image_Layout::Copy_Src);
    tracker.use_resource(
        m_forward_pass_depth_render_target,
        rhi::Barrier_Pipeline_Stage::Copy,
        rhi::Barrier_Access::Transfer_Write,
        rhi::Barrier_Image_Layout::Copy_Dst);
    tracker.flush_barriers(cmd);

    cmd->copy_image(
        shaded_scene_depth_render_target, {}, 0, 0,
        m_forward_pass_depth_render_target, {}, 0, 0,
        { width, height, 1 });

    tracker.use_resource(
        m_forward_pass_depth_render_target,
        rhi::Barrier_Pipeline_Stage::Early_Fragment_Tests,
        rhi::Barrier_Access::Depth_Stencil_Attachment_Write,
        rhi::Barrier_Image_Layout::Depth_Stencil_Write);
    tracker.use_resource(
        m_packed_displacement_texture,
        rhi::Barrier_Pipeline_Stage::Pixel_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(
        m_packed_derivatives_texture,
        rhi::Barrier_Pipeline_Stage::Pixel_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(
        m_packed_xdx_texture,
        rhi::Barrier_Pipeline_Stage::Pixel_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);

    tracker.flush_barriers(cmd);

    const rhi::Render_Pass_Begin_Info render_pass_info = {
        .color_attachments = {},
        .depth_stencil_attachment = {
            .attachment = m_forward_pass_depth_render_target,
            .depth_load_op = rhi::Render_Pass_Attachment_Load_Op::Load,
            .depth_store_op = rhi::Render_Pass_Attachment_Store_Op::Store,
            .stencil_load_op = rhi::Render_Pass_Attachment_Load_Op::No_Access,
            .stencil_store_op = rhi::Render_Pass_Attachment_Store_Op::No_Access,
            .clear_value = {}
        }
    };
    cmd->begin_render_pass(render_pass_info);

    cmd->set_viewport(0.f, 0.f,
        static_cast<float>(width), static_cast<float>(height), 0.f, 1.f);
    cmd->set_scissor(0, 0, width, height);

    constexpr static auto SIZE = 2048;
    if (options.wireframe)
        cmd->set_pipeline(m_asset_repository.get_graphics_pipeline("ocean_render_patch_depth_prepass_wireframe"));
    else
        cmd->set_pipeline(m_asset_repository.get_graphics_pipeline("ocean_render_patch_depth_prepass"));
    draw_all_tiles(cmd, camera, cull_camera);
    cmd->end_render_pass();
    cmd->end_debug_region(); // ocean:render:depth_pre_pass
}

void Ocean::opaque_forward_pass(
        rhi::Command_List* cmd,
        Resource_State_Tracker& tracker,
        const Buffer& camera,
        const Image& shaded_scene_render_target,
        const Image& shaded_scene_depth_render_target,
        const Fly_Camera& cull_camera)
{
    if (!options.enabled) return;

    cmd->begin_debug_region("ocean:render:opaque_pass", 0.25f, 0.0f, 1.0f);

    tracker.use_resource(
        shaded_scene_render_target,
        rhi::Barrier_Pipeline_Stage::Color_Attachment_Output,
        rhi::Barrier_Access::Color_Attachment_Write,
        rhi::Barrier_Image_Layout::Color_Attachment);
    tracker.use_resource(
        shaded_scene_depth_render_target,
        rhi::Barrier_Pipeline_Stage::Pixel_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(
        m_forward_pass_depth_render_target,
        rhi::Barrier_Pipeline_Stage::Early_Fragment_Tests,
        rhi::Barrier_Access::Depth_Stencil_Attachment_Read,
        rhi::Barrier_Image_Layout::Depth_Stencil_Write);
    tracker.flush_barriers(cmd);

    auto color_attachment_infos = std::to_array<rhi::Render_Pass_Color_Attachment_Info>(
        {{
            .attachment = shaded_scene_render_target,
            .load_op = rhi::Render_Pass_Attachment_Load_Op::Load,
            .store_op = rhi::Render_Pass_Attachment_Store_Op::Store,
            .clear_value = {}
        }});
    const rhi::Render_Pass_Begin_Info render_pass_info = {
        .color_attachments = color_attachment_infos,
        .depth_stencil_attachment = {
            .attachment = m_forward_pass_depth_render_target,
            .depth_load_op = rhi::Render_Pass_Attachment_Load_Op::Load,
            .depth_store_op = rhi::Render_Pass_Attachment_Store_Op::Discard,
            .stencil_load_op = rhi::Render_Pass_Attachment_Load_Op::No_Access,
            .stencil_store_op = rhi::Render_Pass_Attachment_Store_Op::No_Access,
            .clear_value = {}
        }
    };
    cmd->begin_render_pass(render_pass_info);

    const uint32_t width = static_cast<rhi::Image*>(shaded_scene_render_target)->width;
    const uint32_t height = static_cast<rhi::Image*>(shaded_scene_render_target)->height;

    cmd->set_viewport(0.f, 0.f,
        static_cast<float>(width), static_cast<float>(height), 0.f, 1.f);
    cmd->set_scissor(0, 0, width, height);

    constexpr static auto SIZE = 2048;
    if (options.wireframe)
        cmd->set_pipeline(m_asset_repository.get_graphics_pipeline("ocean_render_patch_wireframe"));
    else
        cmd->set_pipeline(m_asset_repository.get_graphics_pipeline("ocean_render_patch"));
    draw_all_tiles(cmd, camera, cull_camera);
    cmd->end_render_pass();
    cmd->end_debug_region(); // ocean:render:opaque_forward_pass
}

void Ocean::draw_all_tiles(rhi::Command_List* cmd, const Buffer& camera, const Fly_Camera& cull_camera)
{
    float starting_offset = 0.0f;
    for (uint32_t i = 0; i < TILES_PER_AXIS / 2; ++i)
    {
        starting_offset -= glm::pow(0.5f, i + 1);
    }
    starting_offset *= static_cast<float>(FIELD_SIZE) * VERTEX_DIST / 2.0f;

    cmd->set_index_buffer(m_tile_index_buffer, rhi::Index_Type::U16);
    for (uint32_t i = 0; i < TILES_PER_AXIS; ++i)
    {
        for (uint32_t j = 0; j < TILES_PER_AXIS; ++j)
        {
            glm::vec2 offset = glm::vec2(starting_offset);
            offset.x += static_cast<float>(i) * TILE_SIZE;
            offset.y += static_cast<float>(j) * TILE_SIZE;

            const float horizontal = TILE_SIZE / 2.f + options.horizontal_cull_grace;

            glm::vec3 box_min = { offset.x - horizontal, offset.y - horizontal, -options.vertical_cull_grace };
            glm::vec3 box_max = { offset.x + horizontal, offset.y + horizontal,  options.vertical_cull_grace };

            if (cull_camera.box_in_frustum(box_min, box_max))
            {
                cmd->set_push_constants<Ocean_Render_Patch_Push_Constants>({
                    .length_scales = simulation_data.full_spectrum_parameters.length_scales,
                    .camera = camera,
                    .min_max_buffer = m_minmax_buffer,
                    .packed_displacement_tex = m_packed_displacement_texture,
                    .packed_derivatives_tex = m_packed_derivatives_texture,
                    .packed_xdx_tex = m_packed_xdx_texture,
                    .vertex_position_dist = VERTEX_DIST,
                    .field_size = TILE_VERTEX_COUNT,
                    .offset_x = offset.x,
                    .offset_y = offset.y
                    }, rhi::Pipeline_Bind_Point::Graphics);
                cmd->draw_indexed(3 * 2 * (TILE_VERTEX_COUNT - 1) * (TILE_VERTEX_COUNT - 1), 1, 0, 0, 0);
            }
        }
    }
}

rhi::Image_Create_Info Ocean::Options::generate_create_info(rhi::Image_Format format) const noexcept
{
    return {
        .format = format,
        .width = texture_size,
        .height = texture_size,
        .depth = 1,
        .array_size = static_cast<uint16_t>(cascade_count),
        .mip_levels = 1,
        .usage = rhi::Image_Usage::Unordered_Access | rhi::Image_Usage::Sampled,
        .primary_view_type = rhi::Image_View_Type::Texture_2D_Array
    };
}

Ocean::Simulation_Data::Full_Spectrum_Parameters::Full_Spectrum_Parameters()
    : single_spectrum_parameters({
          {
              .wind_speed = 2.5f,
              .fetch = 3.5f,
              .phillips_alpha = 0.000125f,
              .generalized_a = 1.0f,
              .generalized_b = 1.0f,
              .contribution = 1.0f,
              .wind_direction = 110.f
          },
          {
              .wind_speed = 10.5f,
              .fetch = 70.0f,
              .phillips_alpha = 0.00025f,
              .generalized_a = 1.f,
              .generalized_b = 1.f,
              .contribution = 1.0f,
              .wind_direction = 135.f
          }}
      )
    , active_cascades(glm::uvec4(1,1,1,1))
    , length_scales({ 753.53f, 237.43f, 79.12f, 14.33f })
    , oceanographic_spectrum(static_cast<uint32_t>(Ocean_Spectrum::TMA))
    , directional_spreading_function(static_cast<uint32_t>(Ocean_Directional_Spreading_Function::Donelan_Banner))
    , gravity(9.81f)
    , depth(150.f)
{}

constexpr static auto OCEAN_HELP_TEXT_TEXTURE_SIZE =
"Texture size used for the simulation.";

constexpr static auto OCEAN_HELP_TEXT_CASCADES =
"Amount of simultaneously simulated domains.";

constexpr static auto OCEAN_HELP_TEXT_FP16_TEXTURES =
"Use fp16 textures instead of fp32 textures. "
"Expect loss of precision when used with a high size value.";

constexpr static auto OCEAN_HELP_TEXT_FP16_MATH =
"Use shader permutations that utilize fp16 maths (not yet implemented). "
"Expect loss of precision when used with a high size value.";

constexpr static auto OCEAN_HELP_TEXT_SPECTRUM =
"The spectrum describes the statistical model used to control the wave generation. "
"Some spectra require different parameters.\n"
"The phillips spectrum only makes use of the Phillips alpha value and should only be used with a symmetrical directional spreading function.";

constexpr static auto OCEAN_HELP_TEXT_DIRECTIONAL_SPREAD =
"The directional spreading function describes how the direction of the wind affects the wave generation. "
"As with the spectra, not all directional spreading functions take the same parameters.";

constexpr static auto OCEAN_HELP_TEXT_WIND_SPEED =
"Wind speed describes the average speed of the wind in meters per second at 10 meters above the ocean surface.";

constexpr const char* OCEAN_HELP_TEXT_WIND_DIRECTION =
"Wind direction in degrees. 0 degrees means that the wind is blowing towards positive x.";

constexpr static auto OCEAN_HELP_TEXT_GRAVITY =
"Strength of the gravity of the surface, in m/s^2.";

constexpr static auto OCEAN_HELP_TEXT_FETCH =
"\"Dimensionless\" fetch describes the area over which the wind blows; The distance (in km) from a lee shore. "
"A higher value corresponds with higher waves.";

constexpr static auto OCEAN_HELP_TEXT_DEPTH =
"Depth is the average depth of the ocean, in meters.";

constexpr static auto OCEAN_HELP_TEXT_PHILLIPS_ALPHA =
"alpha-value used in the phillips spectrum.";

constexpr static auto OCEAN_HELP_TEXT_GENERALIZED_A =
"a-value used in the generalized A,B spectrum.";

constexpr static auto OCEAN_HELP_TEXT_GENERALIZED_B =
"b-value used in the generalized A,B spectrum.";

constexpr static auto OCEAN_HELP_TEXT_CONTRIBUTION =
"Non-physical multiplier to the spectrum's strength.";

constexpr static auto OCEAN_HELP_TEXT_LENGTH_SCALE =
"Size of the domain of the simulation in meters.";

void Ocean::process_gui()
{
    if (ImGui::CollapsingHeader("Ocean Simulation"))
    {
        process_gui_options();
        process_gui_simulation_settings();
        ImGui::SeparatorText("Debug");
        {
            ImGui::Checkbox("Update Time", &options.update_time);
            ImGui::Checkbox("Enabled##Ocean", &options.enabled);
            ImGui::Checkbox("Wireframe##Ocean", &options.wireframe);
            ImGui::SliderFloat("Horizontal cull grace", &options.horizontal_cull_grace, 0.f, 64.f);
            ImGui::SliderFloat("Vertical cull grace", &options.vertical_cull_grace, 0.f, 64.f);
        }
    }
}

void Ocean::process_gui_options()
{
    ImGui::SeparatorText("Options");

    auto options_tmp = options;
    {
        constexpr static auto size_values = std::to_array({ 64, 128, 256, 512, 1024 });
        constexpr static auto size_value_texts = std::to_array({ "64", "128", "256", "512", "1024" });

        auto size_text_idx = 0;
        for (auto i = 0; i < size_value_texts.size(); ++i)
        {
            if (size_values[i] == options_tmp.texture_size) size_text_idx = i;
        }
        imutil::push_negative_padding();
        if (ImGui::BeginCombo("Size", size_value_texts[size_text_idx]))
        {
            for (auto i = 0; i < size_values.size(); ++i)
            {
                bool selected = options_tmp.texture_size == size_values[i];
                if (ImGui::Selectable(size_value_texts[i], selected, ImGuiSelectableFlags_None))
                {
                    options_tmp.texture_size = size_values[i];
                }
            }
            ImGui::EndCombo();
        }
        imutil::help_marker(OCEAN_HELP_TEXT_TEXTURE_SIZE);
    }
    {
        constexpr static auto cascade_values = std::to_array({ 1, 2, 3, 4 });
        constexpr static auto cascade_value_texts = std::to_array({ "1", "2", "3", "4" });

        auto cascade_text_idx = 0;
        for (auto i = 0; i < cascade_value_texts.size(); ++i)
        {
            if (cascade_values[i] == options_tmp.cascade_count) cascade_text_idx = i;
        }
        imutil::push_negative_padding();
        if (ImGui::BeginCombo("Cascade count", cascade_value_texts[cascade_text_idx]))
        {
            for (auto i = 0; i < cascade_values.size(); ++i)
            {
                bool selected = options_tmp.cascade_count == cascade_values[i];
                if (ImGui::Selectable(cascade_value_texts[i], selected, ImGuiSelectableFlags_None))
                {
                    options_tmp.cascade_count = cascade_values[i];
                }
            }
            ImGui::EndCombo();
        }
        imutil::help_marker(OCEAN_HELP_TEXT_CASCADES);
    }

    const bool recreate_textures =
        options_tmp.texture_size != options.texture_size ||
        options_tmp.cascade_count != options.cascade_count;
    if (recreate_textures)
    {

        auto recreate_texture = [&](Image& image)
        {
            auto create_info = image.get_create_info();
            create_info.width = options_tmp.texture_size;
            create_info.height = options_tmp.texture_size;
            image.recreate(create_info);
        };
        recreate_texture(m_spectrum_state_texture);
        recreate_texture(m_spectrum_angular_frequency_texture);
        recreate_texture(m_displacement_x_y_z_xdx_texture);
        recreate_texture(m_displacement_ydx_zdx_zdz_zdy_texture);
        recreate_texture(m_packed_displacement_texture);
        recreate_texture(m_packed_derivatives_texture);
        recreate_texture(m_packed_xdx_texture);
    }
    options = options_tmp;
}

void Ocean::process_gui_simulation_settings()
{
    ImGui::SeparatorText("Simulation Settings");

    imutil::push_negative_padding();
    ImGui::SliderFloat("Gravity", &simulation_data.full_spectrum_parameters.gravity, .001f, 30.f);
    imutil::help_marker(OCEAN_HELP_TEXT_GRAVITY);

    auto length_scales = std::to_array({
        &simulation_data.full_spectrum_parameters.length_scales.x,
        &simulation_data.full_spectrum_parameters.length_scales.y,
        &simulation_data.full_spectrum_parameters.length_scales.z,
        &simulation_data.full_spectrum_parameters.length_scales.w,
        });
    for (auto i = 0; i < 4; ++i)
    {
        auto lengthscale_str = std::string("Lengthscale ") + std::to_string(i + 1);
        imutil::push_negative_padding();
        ImGui::SliderFloat(lengthscale_str.c_str(), length_scales[i], .001f, 5000.f);
        imutil::help_marker(OCEAN_HELP_TEXT_LENGTH_SCALE);
    }

    auto depth_str = std::string("Depth");
    imutil::push_negative_padding();
    ImGui::SliderFloat(depth_str.c_str(), &simulation_data.full_spectrum_parameters.depth, 1.0f, 150.f);
    imutil::help_marker(OCEAN_HELP_TEXT_DEPTH);

    constexpr static auto spectrum_values = std::to_array({
        Ocean_Spectrum::Phillips,
        Ocean_Spectrum::Pierson_Moskowitz,
        Ocean_Spectrum::Generalized_A_B,
        Ocean_Spectrum::Jonswap,
        Ocean_Spectrum::TMA });
    constexpr static auto spectrum_value_texts = std::to_array({
        "Phillips",
        "Pierson Moskowitz",
        "Generalized A,B",
        "Jonswap",
        "TMA" });

    auto spectrum_text_idx = 0;
    for (auto i = 0; i < spectrum_value_texts.size(); ++i)
    {
        if (uint32_t(spectrum_values[i]) == simulation_data.full_spectrum_parameters.oceanographic_spectrum) spectrum_text_idx = i;
    }
    imutil::push_negative_padding();
    if (ImGui::BeginCombo("Oceanographic Spectrum", spectrum_value_texts[spectrum_text_idx]))
    {
        for (auto i = 0; i < spectrum_values.size(); ++i)
        {
            bool selected = simulation_data.full_spectrum_parameters.oceanographic_spectrum == uint32_t(spectrum_values[i]);
            if (ImGui::Selectable(spectrum_value_texts[i], selected, ImGuiSelectableFlags_None))
            {
                simulation_data.full_spectrum_parameters.oceanographic_spectrum = uint32_t(spectrum_values[i]);
            }
        }
        ImGui::EndCombo();
    }
    imutil::help_marker(OCEAN_HELP_TEXT_SPECTRUM);

    constexpr static auto dirspread_values = std::to_array({
        Ocean_Directional_Spreading_Function::Positive_Cosine_Squared,
        Ocean_Directional_Spreading_Function::Mitsuyasu,
        Ocean_Directional_Spreading_Function::Hasselmann,
        Ocean_Directional_Spreading_Function::Donelan_Banner,
        Ocean_Directional_Spreading_Function::Flat });
    constexpr static auto dirspread_value_texts = std::to_array({
        "Positive Cosine Squared",
        "Mitsuyasu",
        "Hasselmann",
        "Donelan Banner",
        "Flat" });

    auto dirspread_text_idx = 0;
    for (auto i = 0; i < dirspread_value_texts.size(); ++i)
    {
        if (uint32_t(dirspread_values[i]) == simulation_data.full_spectrum_parameters.directional_spreading_function) dirspread_text_idx = i;
    }
    imutil::push_negative_padding();
    if (ImGui::BeginCombo("Directional Spread", dirspread_value_texts[dirspread_text_idx]))
    {
        for (auto i = 0; i < dirspread_values.size(); ++i)
        {
            bool selected = simulation_data.full_spectrum_parameters.directional_spreading_function == uint32_t(dirspread_values[i]);
            if (ImGui::Selectable(dirspread_value_texts[i], selected, ImGuiSelectableFlags_None))
            {
                simulation_data.full_spectrum_parameters.directional_spreading_function = uint32_t(dirspread_values[i]);
            }
        }
        ImGui::EndCombo();
    }
    imutil::help_marker(OCEAN_HELP_TEXT_DIRECTIONAL_SPREAD);

    uint32_t spectrum_count = 0;
    for (auto& spectrum : simulation_data.full_spectrum_parameters.single_spectrum_parameters)
    {
        if (spectrum_count == 0)
        {
            ImGui::SeparatorText("Primary spectrum parameters");
        }
        else
        {
            ImGui::SeparatorText("Secondary spectrum parameters");
        }

        auto wind_speed_str = std::string("Wind Speed##") + std::to_string(spectrum_count);
        imutil::push_negative_padding();
        ImGui::SliderFloat(wind_speed_str.c_str(), &spectrum.wind_speed, .001f, 60.f);
        imutil::help_marker(OCEAN_HELP_TEXT_WIND_SPEED);

        auto fetch_str = std::string("Fetch##") + std::to_string(spectrum_count);
        imutil::push_negative_padding();
        ImGui::SliderFloat(fetch_str.c_str(), &spectrum.fetch, 1.0f, 125.f);
        imutil::help_marker(OCEAN_HELP_TEXT_FETCH);

        auto phillips_alpha_str = std::string("Phillips Coefficient Alpha##") + std::to_string(spectrum_count);
        imutil::push_negative_padding();
        ImGui::SliderFloat(phillips_alpha_str.c_str(), &spectrum.phillips_alpha, .00001f, 0.001f, "%.7f");
        imutil::help_marker(OCEAN_HELP_TEXT_PHILLIPS_ALPHA);

        auto generalized_a_str = std::string("Generalized Coefficient A##") + std::to_string(spectrum_count);
        imutil::push_negative_padding();
        ImGui::SliderFloat(generalized_a_str.c_str(), &spectrum.generalized_a, .001f, 100.f);
        imutil::help_marker(OCEAN_HELP_TEXT_GENERALIZED_A);

        auto generalized_b_str = std::string("Generalized Coefficient B##") + std::to_string(spectrum_count);
        imutil::push_negative_padding();
        ImGui::SliderFloat(generalized_b_str.c_str(), &spectrum.generalized_b, .001f, 100.f);
        imutil::help_marker(OCEAN_HELP_TEXT_GENERALIZED_B);

        auto contribution_str = std::string("Contribution##") + std::to_string(spectrum_count);
        imutil::push_negative_padding();
        ImGui::SliderFloat(contribution_str.c_str(), &spectrum.contribution, .0f, 1.f);
        imutil::help_marker(OCEAN_HELP_TEXT_CONTRIBUTION);

        ++spectrum_count;
    }
}
}
