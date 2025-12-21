#include "renderer/techniques/exposure.hpp"

#include "renderer/asset/asset_repository.hpp"
#include "renderer/resource_state_tracker.hpp"
#include "renderer/gpu_transfer.hpp"

#include <rhi/command_list.hpp>
#include <imgui.h>
#include <shared/exposure_shared_types.h>

namespace ren::techniques
{
Exposure::Exposure(Asset_Repository& asset_repository,
    GPU_Transfer_Context& gpu_transfer_context,
    Render_Resource_Blackboard& render_resource_blackboard)
    : m_asset_repository(asset_repository)
    , m_gpu_transfer_context(gpu_transfer_context)
    , m_render_resource_blackboard(render_resource_blackboard)
    , m_luminance_histogram_buffer(m_render_resource_blackboard.create_buffer(
        LUMINANCE_HISTOGRAM_BUFFER_NAME, {
        .size = sizeof(Luminance_Histogram),
        .heap = rhi::Memory_Heap_Type::GPU,
        .acceleration_structure_memory = false
        }))
{}

Exposure::~Exposure()
{
    m_render_resource_blackboard.destroy_buffer(m_luminance_histogram_buffer);
}

void Exposure::compute_luminance_histogram(rhi::Command_List* cmd, Resource_State_Tracker& tracker, const Image& target, float dt) const
{
    cmd->begin_debug_region("exposure:compute_luminance_histogram", 0.25f, 0.25f, 0.6f);
    tracker.use_resource(
        target,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Read,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.use_resource(
        m_luminance_histogram_buffer,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Read);
    tracker.flush_barriers(cmd);

    auto compute_luminance_histogram_pipeline = m_asset_repository.get_compute_pipeline("compute_luminance_histogram");
    cmd->set_pipeline(compute_luminance_histogram_pipeline);
    cmd->set_push_constants<Calculate_Luminance_Histogram_Push_Constants>({
        .image_width = target.get_create_info().width,
        .image_height = target.get_create_info().height,
        .source_image = target,
        .luminance_histogram_buffer = m_luminance_histogram_buffer,
        .min_log_luminance = m_auto_exposure_min_log2_luminance,
        .log_luminance_range = m_auto_exposure_log2_luminance_range }, rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(
        target.get_create_info().width / compute_luminance_histogram_pipeline.get_group_size_x(),
        target.get_create_info().height / compute_luminance_histogram_pipeline.get_group_size_y(),
        1);

    tracker.use_resource(
        m_luminance_histogram_buffer,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Read);
    tracker.flush_barriers(cmd);

    auto compute_average_luminance_pipeline = m_asset_repository.get_compute_pipeline("compute_luminance_histogram_average");
    cmd->set_pipeline(compute_average_luminance_pipeline);
    cmd->set_push_constants<Calculate_Average_Luminance_Push_Constants>({
        .luminance_histogram_buffer = m_luminance_histogram_buffer,
        .pixel_count = target.get_create_info().width * target.get_create_info().height,
        .delta_time = dt,
        .tau = m_auto_exposure_adaption_rate,
        .min_log_luminance = m_auto_exposure_min_log2_luminance,
        .log_luminance_range = m_auto_exposure_log2_luminance_range }, rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(1, 1, 1);

    tracker.use_resource(
        m_luminance_histogram_buffer,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Read);
    tracker.flush_barriers(cmd);

    cmd->end_debug_region();
}

void Exposure::apply_exposure(rhi::Command_List* cmd, Resource_State_Tracker& tracker, const Image& target) const
{
    cmd->begin_debug_region("exposure:apply", 0.25f, 0.25f, 0.5f);
    tracker.use_resource(
        target,
        rhi::Barrier_Pipeline_Stage::Compute_Shader,
        rhi::Barrier_Access::Unordered_Access_Read,
        rhi::Barrier_Image_Layout::Unordered_Access);
    tracker.flush_barriers(cmd);

    auto apply_exposure_pipeline = m_asset_repository.get_compute_pipeline("apply_exposure");
    cmd->set_pipeline(apply_exposure_pipeline);
    cmd->set_push_constants<Apply_Exposure_Push_Constants>({
        .image_size = { target.get_create_info().width, target.get_create_info().height },
        .image = target,
        .luminance_histogram_buffer = m_luminance_histogram_buffer,
        .use_camera_exposure = static_cast<uint32_t>(m_use_camera_exposure),
        .aperture = m_aperture,
        .shutter = m_shutter,
        .iso = m_iso,
        .auto_exposure_compensation = m_auto_exposure_exposure_compensation }, rhi::Pipeline_Bind_Point::Compute);
    cmd->dispatch(
        target.get_create_info().width / apply_exposure_pipeline.get_group_size_x(),
        target.get_create_info().height / apply_exposure_pipeline.get_group_size_y(),
        1);
    cmd->end_debug_region(); // exposure:apply
}

void Exposure::process_gui()
{
    if (ImGui::CollapsingHeader("Exposure"))
    {
        ImGui::Checkbox("Use physical camera exposure", &m_use_camera_exposure);
        ImGui::InputFloat("Aperture", &m_aperture);
        ImGui::InputFloat("Shutter", &m_shutter);
        ImGui::InputFloat("ISO", &m_iso);
        ImGui::InputFloat("Auto exposure min EV", &m_auto_exposure_min_log2_luminance);
        ImGui::InputFloat("Auto exposure EV range", &m_auto_exposure_log2_luminance_range);
        ImGui::InputFloat("Auto exposure compensation", &m_auto_exposure_exposure_compensation);
    }
}
}
