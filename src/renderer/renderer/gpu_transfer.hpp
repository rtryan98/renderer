#pragma once

#include <array>
#include <vector>

namespace rhi
{
class Graphics_Device;
class Command_List;
struct Buffer;
struct Image;
}

namespace ren
{
class Buffer;
class Image;

class GPU_Transfer_Context
{
public:
    GPU_Transfer_Context(rhi::Graphics_Device* graphics_device);
    ~GPU_Transfer_Context();

    GPU_Transfer_Context(const GPU_Transfer_Context&) = delete;
    GPU_Transfer_Context& operator=(const GPU_Transfer_Context&) = delete;
    GPU_Transfer_Context(GPU_Transfer_Context&&) = delete;
    GPU_Transfer_Context& operator=(GPU_Transfer_Context&&) = delete;

    // Buffer upload functions

    void enqueue_immediate_upload(const Buffer& buffer, void* data, std::size_t size, std::size_t dst_offset);
    void enqueue_immediate_upload(rhi::Buffer* dst, void* data, std::size_t size, std::size_t dst_offset);

    template<typename T>
    void enqueue_immediate_upload(rhi::Buffer* buffer, T& data, std::size_t offset = 0)
    {
        enqueue_immediate_upload(buffer, static_cast<void*>(&data), sizeof(T), offset);
    }

    // Image upload functions.

    // void** data is a pointer to arrays of mipmap data
    void enqueue_immediate_upload(rhi::Image* image, void** data);


    // Upload processing

    void process_immediate_uploads_on_graphics_queue(rhi::Command_List* cmd);

    // Bookkeeping

    void garbage_collect();

private:
    struct Buffer_Staging_Info
    {
        rhi::Buffer* src;
        rhi::Buffer* dst;
        uint64_t offset;
        uint64_t size;
    };

    struct Image_Staging_Info
    {
        rhi::Buffer* src;
        rhi::Image* dst;
    };

    rhi::Graphics_Device* m_graphics_device;
    std::array<std::vector<rhi::Buffer*>, REN_MAX_FRAMES_IN_FLIGHT> m_staging_buffers;
    std::array<std::vector<Buffer_Staging_Info>, REN_MAX_FRAMES_IN_FLIGHT> m_buffer_staging_infos;
    std::array<std::vector<Image_Staging_Info>, REN_MAX_FRAMES_IN_FLIGHT> m_image_staging_infos;

    std::size_t m_current_frame = 0;
};
}
