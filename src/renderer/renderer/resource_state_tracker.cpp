#include "renderer/resource_state_tracker.hpp"
#include "renderer/render_resource_blackboard.hpp"
#include "rhi/graphics_device.hpp"

namespace ren
{
Resource_State_Tracker::Identifier Resource_State_Tracker::decay_resource(const Buffer& buffer)
{
    return static_cast<rhi::Buffer*>(buffer);
}

Resource_State_Tracker::Identifier Resource_State_Tracker::decay_resource(const Image& image)
{
    return static_cast<rhi::Image*>(image);
}

void Resource_State_Tracker::use_resource(
    const Buffer& buffer,
    rhi::Barrier_Pipeline_Stage stage,
    rhi::Barrier_Access access)
{
    auto& resource_state = get_resource_state(decay_resource(buffer));
    resource_state = {
        .buffer = buffer,
        .stage_before = resource_state.stage_after,
        .stage_after = stage,
        .access_before = resource_state.access_after,
        .access_after = access,
        .image_layout_before = rhi::Barrier_Image_Layout::Undefined,
        .image_layout_after = rhi::Barrier_Image_Layout::Undefined,
        .discard = false,
    };
    m_pending_barriers.emplace_back(decay_resource(buffer));
}

void Resource_State_Tracker::use_resource(
    const Image& image,
    rhi::Barrier_Pipeline_Stage stage,
    rhi::Barrier_Access access,
    rhi::Barrier_Image_Layout layout,
    bool discard)
{
    auto& resource_state = get_resource_state(decay_resource(image));
    resource_state = {
        .image = image,
        .stage_before = resource_state.stage_after,
        .stage_after = stage,
        .access_before = resource_state.access_after,
        .access_after = access,
        .image_layout_before = resource_state.image_layout_after,
        .image_layout_after = layout,
        .discard = discard,
    };
    m_pending_barriers.emplace_back(decay_resource(image));
}

void Resource_State_Tracker::set_resource_state(
    const Buffer& buffer,
    rhi::Barrier_Pipeline_Stage stage,
    rhi::Barrier_Access access)
{
    auto& resource_state = get_resource_state(decay_resource(buffer));
    resource_state = {
        .buffer = buffer,
        .stage_before = stage,
        .stage_after = rhi::Barrier_Pipeline_Stage::None,
        .access_before = access,
        .access_after = rhi::Barrier_Access::None,
        .image_layout_before = rhi::Barrier_Image_Layout::Undefined,
        .image_layout_after = rhi::Barrier_Image_Layout::Undefined,
        .discard = false,
    };
}

void Resource_State_Tracker::set_resource_state(
    const Image& image,
    rhi::Barrier_Pipeline_Stage stage,
    rhi::Barrier_Access access,
    rhi::Barrier_Image_Layout layout)
{
    auto& resource_state = get_resource_state(decay_resource(image));
    resource_state = {
        .image = image,
        .stage_before = stage,
        .stage_after = rhi::Barrier_Pipeline_Stage::None,
        .access_before = access,
        .access_after = rhi::Barrier_Access::None,
        .image_layout_before = layout,
        .image_layout_after = rhi::Barrier_Image_Layout::Undefined,
        .discard = false,
    };
}

void Resource_State_Tracker::flush_barriers(rhi::Command_List* cmd)
{
    std::vector<rhi::Image_Barrier_Info> image_barriers;
    image_barriers.reserve(m_pending_barriers.size());
    std::vector<rhi::Buffer_Barrier_Info> buffer_barriers;
    buffer_barriers.reserve(m_pending_barriers.size());
    for (auto* identifier : m_pending_barriers)
    {
        auto& resource_state = get_resource_state(identifier);
        if (resource_state.buffer)
        {
            buffer_barriers.emplace_back(rhi::Buffer_Barrier_Info{
                .stage_before = resource_state.stage_before,
                .stage_after = resource_state.stage_after,
                .access_before = resource_state.access_before,
                .access_after = resource_state.access_after,
                .buffer = resource_state.buffer,
            });
        }
        else
        {
            image_barriers.emplace_back(rhi::Image_Barrier_Info{
                .stage_before = resource_state.stage_before,
                .stage_after = resource_state.stage_after,
                .access_before = resource_state.access_before,
                .access_after = resource_state.access_after,
                .layout_before = resource_state.image_layout_before,
                .layout_after = resource_state.image_layout_after,
                .queue_type_ownership_transfer_target_queue = rhi::Queue_Type::Graphics,
                .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
                .image = resource_state.image,
                .subresource_range = {
                    .first_mip_level = 0,
                    .mip_count = 0,
                    .first_array_index = 0,
                    .array_size = 0,
                    .first_plane = 0,
                    .plane_count = 0
                },
                .discard = resource_state.discard,
            });
        }

        resource_state = {
            .buffer = resource_state.buffer,
            .image = resource_state.image,
            .stage_before = resource_state.stage_after,
            .stage_after = rhi::Barrier_Pipeline_Stage::None,
            .access_before = resource_state.access_after,
            .access_after = rhi::Barrier_Access::None,
            .image_layout_before = resource_state.image_layout_after,
            .image_layout_after = rhi::Barrier_Image_Layout::Undefined,
            .discard = false,
        };
    }

    rhi::Barrier_Info barriers = {
        .buffer_barriers = buffer_barriers,
        .image_barriers = image_barriers,
        .memory_barriers = {},
    };
    cmd->barrier(barriers);

    m_pending_barriers.clear();
}

Resource_State_Tracker::Resource_State& Resource_State_Tracker::get_resource_state(void* identifier)
{
    if (m_resource_states.contains(identifier))
        return m_resource_states[identifier];
    m_resource_states[identifier] = {
        .stage_before = rhi::Barrier_Pipeline_Stage::None,
        .stage_after = rhi::Barrier_Pipeline_Stage::None,
        .access_before = rhi::Barrier_Access::None,
        .access_after = rhi::Barrier_Access::None,
        .image_layout_before = rhi::Barrier_Image_Layout::Undefined,
        .image_layout_after = rhi::Barrier_Image_Layout::Undefined,
        .discard = false
    };
    return m_resource_states[identifier];
}
}
