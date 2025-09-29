#pragma once

#include <rhi/command_list.hpp>
#include <rhi/resource.hpp>

namespace rhi
{
class Graphics_Device;
}

namespace ren
{
struct BLAS_Build_Request
{
    rhi::Acceleration_Structure* acceleration_structure;
    rhi::Acceleration_Structure_Build_Sizes build_sizes;
    rhi::Acceleration_Structure_Flags flags;
    rhi::Acceleration_Structure_Geometry_Data geometry_data;
};

struct TLAS_Build_Request
{
    rhi::Acceleration_Structure* acceleration_structure;
    rhi::Acceleration_Structure_Build_Sizes build_sizes;
    uint32_t instance_count;
    bool array_of_pointers;
    uint64_t instances_gpu_address;
};

class Acceleration_Structure_Builder
{
public:
    Acceleration_Structure_Builder(rhi::Graphics_Device* graphics_device);
    ~Acceleration_Structure_Builder();

    Acceleration_Structure_Builder(const Acceleration_Structure_Builder& other) = delete;
    Acceleration_Structure_Builder operator=(const Acceleration_Structure_Builder& other) = delete;
    Acceleration_Structure_Builder(Acceleration_Structure_Builder&& other) = delete;
    Acceleration_Structure_Builder operator=(Acceleration_Structure_Builder&& other) = delete;

    void build_acceleration_structures(rhi::Command_List* cmd);

    void add_blas_build_request(const BLAS_Build_Request& request);
    void add_tlas_build_request(const TLAS_Build_Request& request);

private:
    rhi::Graphics_Device* m_graphics_device;

    rhi::Buffer* m_scratch_buffer;
    std::vector<BLAS_Build_Request> m_blas_build_requests;
    std::vector<TLAS_Build_Request> m_tlas_build_requests;
};
}
