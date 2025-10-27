#include "renderer/techniques/hosek_wilkie_sky.hpp"
#include "renderer/techniques/hosek_wilkie_sky_data.hpp"
#include "renderer/asset/asset_repository.hpp"
#include "renderer/resource_state_tracker.hpp"
#include "renderer/gpu_transfer.hpp"

#include <rhi/command_list.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <shared/hosek_wilkie_shared_types.h>
#include <shared/ibl_shared_types.h>

#include <imgui.h>

namespace ren::techniques
{
double evaluate_quintic_bezier(double x, const double* spline, const int32_t stride, const int32_t offset)
{
    return  1.0 * glm::pow(1.0 - x, 5.0) *                    spline[offset + 0 * stride] +
            5.0 * glm::pow(1.0 - x, 4.0) * glm::pow(x, 1.0) * spline[offset + 1 * stride] +
           10.0 * glm::pow(1.0 - x, 3.0) * glm::pow(x, 2.0) * spline[offset + 2 * stride] +
           10.0 * glm::pow(1.0 - x, 2.0) * glm::pow(x, 3.0) * spline[offset + 3 * stride] +
            5.0 * glm::pow(1.0 - x, 1.0) * glm::pow(x, 4.0) * spline[offset + 4 * stride] +
            1.0 *                          glm::pow(x, 5.0) * spline[offset + 5 * stride];
}

float evaluate_splines(double turbidity, double albedo, double solar_elevation, double* dataset, const int32_t stride, const int32_t offset)
{
    int32_t turbidity_index = glm::clamp(static_cast<int32_t>(turbidity), 1, 10);
    auto t = turbidity - static_cast<double>(turbidity_index);

    auto x = glm::pow(glm::clamp(solar_elevation / glm::half_pi<double>(), 0., 1.), 1.0f / 3.0f);

    double a0t0 = evaluate_quintic_bezier(x, dataset + stride * (          6 * (turbidity_index - 1)), stride, offset);
    double a1t0 = evaluate_quintic_bezier(x, dataset + stride * ( 6 * 10 + 6 * (turbidity_index - 1)), stride, offset);

    if (turbidity_index == 10)
    {
        return static_cast<float>((1.0 - t) * glm::mix(a0t0, a1t0, albedo));
    }

    double a0t1 = evaluate_quintic_bezier(x, dataset + stride * (          6 * (turbidity_index    )), stride, offset);
    double a1t1 = evaluate_quintic_bezier(x, dataset + stride * ( 6 * 10 + 6 * (turbidity_index    )), stride, offset);

    return static_cast<float>(glm::mix(glm::mix(a0t0, a1t0, albedo), glm::mix(a0t1, a1t1, albedo), t));
}

Hosek_Wilkie_Parameters bake_parameters(double turbidity, glm::vec3 albedo, double solar_elevation, bool use_xyz)
{
    Hosek_Wilkie_Parameters parameters = {};

    auto* datasets = use_xyz ? hosek_wilkie_data::datasetsXYZ : hosek_wilkie_data::datasetsRGB;
    auto* datasets_rad = use_xyz ? hosek_wilkie_data::datasetsXYZRad : hosek_wilkie_data::datasetsRGBRad;

    for (uint32_t i = 0; i < 9; ++i)
    {
        for (auto channel = 0; channel < 3; ++channel)
        {
            auto* dataset = datasets[channel];
            parameters.values[i][channel] += evaluate_splines(turbidity, static_cast<double>(albedo[channel]), solar_elevation, dataset, 9, i);
        }
    }
    for (auto channel = 0; channel < 3; ++channel)
    {
        auto* dataset = datasets_rad[channel];
        parameters.radiance[channel] += evaluate_splines(turbidity, static_cast<double>(albedo[channel]), solar_elevation, dataset, 1, 0);
    }

    return parameters;
}

Hosek_Wilkie_Sky::Hosek_Wilkie_Sky(
    Asset_Repository& asset_repository,
    GPU_Transfer_Context& gpu_transfer_context,
    Render_Resource_Blackboard& render_resource_blackboard)
    : m_asset_repository(asset_repository)
    , m_gpu_transfer_context(gpu_transfer_context)
    , m_render_resource_blackboard(render_resource_blackboard)
{
    m_parameters = m_render_resource_blackboard.create_buffer(PARAMETERS_BUFFER_NAME, {
        .size = sizeof(Hosek_Wilkie_Parameters),
        .heap = rhi::Memory_Heap_Type::GPU,
        .acceleration_structure_memory = false});
    rhi::Image_Create_Info cube_create_info = {
        .format = rhi::Image_Format::B10G11R11_UFLOAT_PACK32,
        .width = 256,
        .height = 256,
        .depth = 1,
        .array_size = 6,
        .mip_levels = 1,
        .usage = rhi::Image_Usage::Sampled | rhi::Image_Usage::Unordered_Access,
        .primary_view_type = rhi::Image_View_Type::Texture_Cube
    };
    m_cubemap = m_render_resource_blackboard.create_image(SKY_CUBEMAP_TEXTURE_NAME, cube_create_info);
}

Hosek_Wilkie_Sky::~Hosek_Wilkie_Sky()
{
    m_render_resource_blackboard.destroy_buffer(m_parameters);
    m_render_resource_blackboard.destroy_image(m_cubemap);
}

void Hosek_Wilkie_Sky::update(const glm::vec3& sun_direction)
{
    m_sun_direction = sun_direction;
    const float theta = glm::acos(glm::clamp(m_sun_direction.z, 0.f, 1.f));

    auto parameters = bake_parameters(m_turbidity, m_albedo, theta, m_use_xyz);

    m_gpu_transfer_context.enqueue_immediate_upload(m_parameters, parameters);
}

void Hosek_Wilkie_Sky::generate_cubemap(
    rhi::Command_List* cmd,
    Resource_State_Tracker& tracker) const
{
    cmd->begin_debug_region("hosek_wilkie_sky:generate_cubemap", 0.1f, 0.25f, 0.8f);
    tracker.use_resource(m_cubemap,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);
    auto pipeline = m_asset_repository.get_compute_pipeline("hosek_wilkie_generate_cubemap");
    auto size = m_cubemap.get_create_info().width;
    cmd->set_pipeline(pipeline);
    cmd->set_push_constants<Hosek_Wilkie_Cubemap_Gen_Push_Constants>({
        .sun_direction = { m_sun_direction.x, m_sun_direction.y, m_sun_direction.z, 0.f },
        .parameters_buffer = m_parameters,
        .target_cubemap = m_cubemap,
        .image_size = size,
        .use_xyz = static_cast<uint32_t>(m_use_xyz)
        }, rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(size / pipeline.get_group_size_x(), size / pipeline.get_group_size_y(), 6);
    cmd->end_debug_region();
}

void Hosek_Wilkie_Sky::skybox_render(
    rhi::Command_List* cmd,
    Resource_State_Tracker& tracker,
    const Buffer& camera,
    const Image& shaded_geometry_render_target,
    const Image& geometry_depth_buffer) const
{
    cmd->begin_debug_region("hosek_wilkie_sky:render_skybox", 0.1f, 0.25f, 0.1f);

    tracker.use_resource(shaded_geometry_render_target,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Write,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.use_resource(geometry_depth_buffer,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Shader_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.use_resource(m_cubemap,
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

void Hosek_Wilkie_Sky::process_gui()
{
    if (ImGui::CollapsingHeader("Hosek-Wilkie Sky"))
    {
        ImGui::SliderFloat("turbidity", &m_turbidity, 1.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat3("albedo", &m_albedo.x, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::Checkbox("Use XYZ color space", &m_use_xyz);
    }
}
}
