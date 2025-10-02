#include "renderer/acceleration_structure_builder.hpp"

#include <rhi/graphics_device.hpp>

namespace ren
{
Acceleration_Structure_Builder::Acceleration_Structure_Builder(rhi::Graphics_Device* graphics_device)
    : m_graphics_device(graphics_device)
    , m_scratch_buffer(nullptr)
    , m_blas_build_requests()
    , m_tlas_build_requests()
{
    rhi::Buffer_Create_Info scratch_buffer_create_info = {
        .size = 1 << 27, // 128MB
        .heap = rhi::Memory_Heap_Type::GPU
    };
    m_scratch_buffer = m_graphics_device->create_buffer(scratch_buffer_create_info).value_or(nullptr);
    m_graphics_device->name_resource(m_scratch_buffer, "acceleration_structure_builder:scratch_buffer");
}

Acceleration_Structure_Builder::~Acceleration_Structure_Builder()
{
    m_graphics_device->destroy_buffer(m_scratch_buffer);
}


void Acceleration_Structure_Builder::build_acceleration_structures(rhi::Command_List* cmd)
{
    if (m_blas_build_requests.empty() && m_tlas_build_requests.empty())
    {
        return;
    }

    // TODO: direct copy from Scene.cpp. Should be moved elsewhere.
    auto pow2_align = [](uint64_t value, uint64_t pow2)
    {
        auto mask = pow2 - 1;
        return (value & (~mask)) + ((value & mask) > 0) * pow2;
    };

    {
        rhi::Memory_Barrier_Info memory_barrier_info = {
            .stage_before = rhi::Barrier_Pipeline_Stage::Copy,
            .stage_after = rhi::Barrier_Pipeline_Stage::Acceleration_Structure_Build,
            .access_before = rhi::Barrier_Access::Transfer_Write,
            .access_after = rhi::Barrier_Access::Acceleration_Structure_Write,
        };
        rhi::Barrier_Info barrier_info = {
            .memory_barriers = { &memory_barrier_info, 1 }
        };
        cmd->barrier(barrier_info);
    }

    uint64_t scratch_buffer_size_usage = 0;
    for (auto& build_request : m_blas_build_requests)
    {
        if (build_request.build_sizes.acceleration_structure_scratch_build_size + scratch_buffer_size_usage > m_scratch_buffer->size)
        {
            rhi::Memory_Barrier_Info memory_barrier_info = {
                .stage_before = rhi::Barrier_Pipeline_Stage::Acceleration_Structure_Build,
                .stage_after = rhi::Barrier_Pipeline_Stage::Acceleration_Structure_Build,
                .access_before = rhi::Barrier_Access::Acceleration_Structure_Write,
                .access_after = rhi::Barrier_Access::Acceleration_Structure_Write,
            };
            rhi::Barrier_Info barrier_info = {
                .memory_barriers = { &memory_barrier_info, 1 }
            };
            cmd->barrier(barrier_info);
            scratch_buffer_size_usage = 0;
        }
        rhi::Acceleration_Structure_Build_Geometry_Info build_info = {
            .type = rhi::Acceleration_Structure_Type::Bottom_Level,
            .flags = build_request.flags,
            .geometry_or_instance_count = 1,
            .src = nullptr,
            .dst = build_request.acceleration_structure,
            .geometry = &build_request.geometry_data
        };
        cmd->build_acceleration_structure(build_info, m_scratch_buffer->gpu_address + scratch_buffer_size_usage);
        scratch_buffer_size_usage += pow2_align(build_request.build_sizes.acceleration_structure_scratch_build_size, 256);
    }

    if (!m_blas_build_requests.empty())
    {
        rhi::Memory_Barrier_Info memory_barrier_info = {
            .stage_before = rhi::Barrier_Pipeline_Stage::Acceleration_Structure_Build,
            .stage_after = rhi::Barrier_Pipeline_Stage::Acceleration_Structure_Build,
            .access_before = rhi::Barrier_Access::Acceleration_Structure_Write,
            .access_after = rhi::Barrier_Access::Acceleration_Structure_Read,
        };
        rhi::Barrier_Info barrier_info = {
            .memory_barriers = { &memory_barrier_info, 1 }
        };
        cmd->barrier(barrier_info);
        m_blas_build_requests.clear();
    }

    scratch_buffer_size_usage = 0;

    for (const auto& build_request : m_tlas_build_requests)
    {
        if (build_request.build_sizes.acceleration_structure_scratch_build_size + scratch_buffer_size_usage > m_scratch_buffer->size)
        {
            rhi::Memory_Barrier_Info memory_barrier_info = {
                .stage_before = rhi::Barrier_Pipeline_Stage::Acceleration_Structure_Build,
                .stage_after = rhi::Barrier_Pipeline_Stage::Acceleration_Structure_Build,
                .access_before = rhi::Barrier_Access::Acceleration_Structure_Write,
                .access_after = rhi::Barrier_Access::Acceleration_Structure_Write,
            };
            rhi::Barrier_Info barrier_info = {
                .memory_barriers = { &memory_barrier_info, 1 }
            };
            cmd->barrier(barrier_info);
            scratch_buffer_size_usage = 0;
        }
        rhi::Acceleration_Structure_Build_Geometry_Info build_info = {
            .type = rhi::Acceleration_Structure_Type::Top_Level,
            .flags = rhi::Acceleration_Structure_Flags::Fast_Build,
            .geometry_or_instance_count = build_request.instance_count,
            .src = nullptr,
            .dst = build_request.acceleration_structure,
            .instances = {
                .array_of_pointers = build_request.array_of_pointers,
                .instance_gpu_address = build_request.instances_gpu_address
            }
        };
        cmd->build_acceleration_structure(build_info, m_scratch_buffer->gpu_address + scratch_buffer_size_usage);
        scratch_buffer_size_usage += pow2_align(build_request.build_sizes.acceleration_structure_scratch_build_size, 256);
    }

    if (!m_tlas_build_requests.empty())
    {
        rhi::Memory_Barrier_Info memory_barrier_info = {
            .stage_before = rhi::Barrier_Pipeline_Stage::Acceleration_Structure_Build,
            .stage_after = rhi::Barrier_Pipeline_Stage::Acceleration_Structure_Build,
            .access_before = rhi::Barrier_Access::Acceleration_Structure_Write,
            .access_after = rhi::Barrier_Access::Acceleration_Structure_Read,
        };
        rhi::Barrier_Info barrier_info = {
            .memory_barriers = { &memory_barrier_info, 1 }
        };
        cmd->barrier(barrier_info);
        m_tlas_build_requests.clear();
    }
}

void Acceleration_Structure_Builder::add_blas_build_request(const BLAS_Build_Request& request)
{
    m_blas_build_requests.push_back(request);
}

void Acceleration_Structure_Builder::add_tlas_build_request(const TLAS_Build_Request& request)
{
    m_tlas_build_requests.push_back(request);
}
}
