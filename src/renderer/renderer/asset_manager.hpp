#pragma once

#include "renderer/asset.hpp"
#include "renderer/logger.hpp"

#include <rhi/graphics_device.hpp>
#include <unordered_map>
#include <unordered_set>

namespace ren
{
class Asset_Manager
{
public:
    Asset_Manager(std::shared_ptr<Logger> logger, rhi::Graphics_Device* device, uint64_t frames_in_flight) noexcept;

    [[nodiscard]] rhi::Buffer* create_buffer(const rhi::Buffer_Create_Info& create_info) noexcept;
    void destroy_buffer(rhi::Buffer* buffer) noexcept;

    [[nodiscard]] rhi::Image* create_image(const rhi::Image_Create_Info& create_info) noexcept;
    void destroy_image(rhi::Image* image) noexcept;

    [[nodiscard]] rhi::Pipeline* create_pipeline(const rhi::Graphics_Pipeline_Create_Info& create_info) noexcept;
    [[nodiscard]] rhi::Pipeline* create_pipeline(const rhi::Mesh_Shading_Pipeline_Create_Info& create_info) noexcept;
    [[nodiscard]] rhi::Pipeline* create_pipeline(const rhi::Compute_Pipeline_Create_Info& create_info) noexcept;

    // void recreate_dirty_pipelines();
    void flush_deletion_queue(uint64_t frame);
    void next_frame() noexcept;

private:
    uint64_t get_deletion_frame();

private:
    enum class Asset_Deletion_Type
    {
        Buffer,
        Image,
        Pipeline,
    };

    struct Deletion_Queue_Entry
    {
        uint64_t frame;
        Asset_Deletion_Type type;
        union
        {
            rhi::Buffer* buffer;
            rhi::Image* image;
            rhi::Pipeline* pipeline;
        };
    };

    std::shared_ptr<Logger> m_logger;
    rhi::Graphics_Device* m_device;
    std::vector<Deletion_Queue_Entry> m_deletion_queue;
    uint64_t m_current_frame = 0;
    uint64_t m_frames_in_flight;
};
}
