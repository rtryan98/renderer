#include "renderer/gpu_transfer.hpp"
#include "renderer/render_resource_blackboard.hpp"

#include <rhi/graphics_device.hpp>

namespace ren
{
constexpr static auto MIN_PER_FRAME_STAGING_BUFFER_SIZE = 1ull << 24; // 16 MiB

GPU_Transfer_Context::GPU_Transfer_Context(rhi::Graphics_Device* graphics_device)
    : m_graphics_device(graphics_device)
{}

GPU_Transfer_Context::~GPU_Transfer_Context()
{
    m_graphics_device->wait_idle();
    for (auto& staging_buffer_vec : m_coherent_staging_buffers)
    {
        for (auto staging_buffer : staging_buffer_vec)
        {
            m_graphics_device->destroy_buffer(staging_buffer.buffer);
        }
    }
}

void GPU_Transfer_Context::enqueue_immediate_upload(const Buffer& buffer, void* data, std::size_t size,
    std::size_t dst_offset)
{
    enqueue_immediate_upload(static_cast<rhi::Buffer*>(buffer), data, size, dst_offset);
}

void GPU_Transfer_Context::enqueue_immediate_upload(rhi::Buffer* dst, void* data, std::size_t size,
    std::size_t dst_offset)
{
    const auto frame_in_flight = m_current_frame % REN_MAX_FRAMES_IN_FLIGHT;

    auto staging_buffer = get_next_staging_buffer(size);

    memcpy(&static_cast<char*>(staging_buffer.buffer->data)[staging_buffer.offset], data, size);

    m_buffer_staging_infos[frame_in_flight].push_back({
        .src = staging_buffer.buffer,
        .src_offset = staging_buffer.offset,
        .dst = dst,
        .dst_offset = dst_offset,
        .size = size });
}

void GPU_Transfer_Context::enqueue_immediate_upload(rhi::Image* image, void** data)
{
    const auto frame_in_flight = m_current_frame % REN_MAX_FRAMES_IN_FLIGHT;

    const auto byte_size = rhi::get_image_format_info(image->format).bytes;
    std::size_t size = 0;
    for (auto i = 0; i < image->mip_levels; ++i)
    {
        size += byte_size * (image->width / (1 << i)) * (image->height / (1 << i));
    }

    auto staging_buffer = get_next_staging_buffer(size);
    auto mip_offset = 0ull;

    for (auto i = 0; i < image->mip_levels; ++i)
    {
        std::size_t current_size = byte_size * (image->width / (1 << i)) * (image->height / (1 << i));
        if (i > 0)
        {
            mip_offset += byte_size * (image->width / (1 << (i - 1))) * (image->height / (1 << (i - 1)));
        }
        memcpy(&static_cast<char*>(staging_buffer.buffer->data)[staging_buffer.offset + mip_offset], data[i], current_size);
    }

    m_image_staging_infos[frame_in_flight].push_back({
        .src = staging_buffer.buffer,
        .src_offset = staging_buffer.offset,
        .dst = image });
}

void GPU_Transfer_Context::process_immediate_uploads_on_graphics_queue(
    rhi::Command_List* cmd)
{
    const auto frame_in_flight = m_current_frame % REN_MAX_FRAMES_IN_FLIGHT;

    std::vector<rhi::Image_Barrier_Info> image_barriers_before;
    std::vector<rhi::Image_Barrier_Info> image_barriers_after;
    image_barriers_before.reserve(m_image_staging_infos[frame_in_flight].size());
    image_barriers_after.reserve(m_image_staging_infos[frame_in_flight].size());
    for (const auto& image_staging_info : m_image_staging_infos[frame_in_flight])
    {
        image_barriers_before.emplace_back( rhi::Image_Barrier_Info {
            .stage_before = rhi::Barrier_Pipeline_Stage::None,
            .stage_after = rhi::Barrier_Pipeline_Stage::Copy,
            .access_before = rhi::Barrier_Access::None,
            .access_after = rhi::Barrier_Access::Transfer_Write,
            .layout_before = rhi::Barrier_Image_Layout::Undefined,
            .layout_after = rhi::Barrier_Image_Layout::Copy_Dst,
            .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
            .image = image_staging_info.dst,
            .subresource_range = {
                .first_mip_level = 0,
                .mip_count = image_staging_info.dst->mip_levels,
                .first_array_index = 0,
                .array_size = image_staging_info.dst->array_size,
                .first_plane = 0,
                .plane_count = 1
            },
            .discard = true
        });
        image_barriers_after.emplace_back( rhi::Image_Barrier_Info {
            .stage_before = rhi::Barrier_Pipeline_Stage::Copy,
            .stage_after = rhi::Barrier_Pipeline_Stage::All_Commands,
            .access_before = rhi::Barrier_Access::Transfer_Write,
            .access_after = rhi::Barrier_Access::Shader_Read,
            .layout_before = rhi::Barrier_Image_Layout::Copy_Dst,
            .layout_after = rhi::Barrier_Image_Layout::Shader_Read_Only,
            .queue_type_ownership_transfer_mode = rhi::Queue_Type_Ownership_Transfer_Mode::None,
            .image = image_staging_info.dst,
            .subresource_range = {
                .first_mip_level = 0,
                .mip_count = image_staging_info.dst->mip_levels,
                .first_array_index = 0,
                .array_size = image_staging_info.dst->array_size,
                .first_plane = 0,
                .plane_count = 1
            },
            .discard = false
        });
    }
    if (image_barriers_before.size() > 0)
    {
        cmd->barrier({
            .image_barriers = image_barriers_before
            });
    }

    // TODO: this does not work for partial image copies.
    for (const auto& image_staging_info : m_image_staging_infos[frame_in_flight])
    {
        std::size_t offset = 0;
        for (auto i = 0; i < image_staging_info.dst->mip_levels; ++i)
        {
            const auto byte_size = rhi::get_image_format_info(image_staging_info.dst->format).bytes;
            const auto width = image_staging_info.dst->width / (1 << i);
            const auto height = image_staging_info.dst->height / (1 << i);
            if (i > 0)
            {
                offset += byte_size * (image_staging_info.dst->width / (1 << (i - 1))) * (image_staging_info.dst->height / (1 << (i - 1)));
            }
            cmd->copy_buffer_to_image(
                image_staging_info.src,
                image_staging_info.src_offset + offset,
                image_staging_info.dst,
                {},
                {
                    .x = width,
                    .y = height,
                    .z = 1
                },
                i,
                0);
        }
    }

    for (const auto& buffer_staging_info : m_buffer_staging_infos[frame_in_flight])
    {
        cmd->copy_buffer(
            buffer_staging_info.src,
            buffer_staging_info.src_offset,
            buffer_staging_info.dst,
            buffer_staging_info.dst_offset,
            buffer_staging_info.size);
    }
    rhi::Memory_Barrier_Info mem_barrier = {
        .stage_before = rhi::Barrier_Pipeline_Stage::Copy,
        .stage_after = rhi::Barrier_Pipeline_Stage::All_Commands,
        .access_before = rhi::Barrier_Access::Transfer_Write,
        .access_after = rhi::Barrier_Access::Shader_Read
    };
    rhi::Barrier_Info barrier = {
        .image_barriers = image_barriers_after,
        .memory_barriers = { &mem_barrier, 1 },
    };
    cmd->barrier(barrier);

    m_current_frame += 1;
}

void GPU_Transfer_Context::garbage_collect()
{
    if (m_current_frame < REN_MAX_FRAMES_IN_FLIGHT)
        return;

    const auto frame_in_flight = m_current_frame % REN_MAX_FRAMES_IN_FLIGHT;

    m_buffer_staging_infos[frame_in_flight].clear();
    m_image_staging_infos[frame_in_flight].clear();
    for (auto& staging_buffer : m_coherent_staging_buffers[frame_in_flight])
    {
        staging_buffer.offset = 0;
    }
}

GPU_Transfer_Context::Staging_Buffer GPU_Transfer_Context::get_next_staging_buffer(std::size_t size)
{
    const auto frame_in_flight = m_current_frame % REN_MAX_FRAMES_IN_FLIGHT;

    Staging_Buffer result = {};
    for (auto& staging_buffer : m_coherent_staging_buffers[frame_in_flight])
    {
        if (staging_buffer.offset + size <= staging_buffer.buffer->size)
        {
            result = staging_buffer;
            staging_buffer.offset += size;
            return result;
        }
    }
    auto& new_staging_buffer = m_coherent_staging_buffers[frame_in_flight].emplace_back();
    rhi::Buffer_Create_Info staging_buffer_create_info = {
        .size = std::max(MIN_PER_FRAME_STAGING_BUFFER_SIZE, size),
        .heap = rhi::Memory_Heap_Type::CPU_Upload,
        .acceleration_structure_memory = false
    };
    new_staging_buffer.buffer = m_graphics_device->create_buffer(staging_buffer_create_info).value_or(nullptr);
    std::string buffer_name = "gpu_transfer:staging_buffer:frame" + std::to_string(frame_in_flight) + ":buffer" + std::to_string(m_coherent_staging_buffers[frame_in_flight].size() - 1);
    m_graphics_device->name_resource(new_staging_buffer.buffer, buffer_name.c_str());

    result = new_staging_buffer;
    new_staging_buffer.offset += size;

    return result;
}
}
