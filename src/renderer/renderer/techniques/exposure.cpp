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
        .iso = m_iso }, rhi::Pipeline_Bind_Point::Compute);
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
    }
}
}
