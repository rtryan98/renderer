#pragma once

#include "renderer/asset.hpp"
#include "renderer/logger.hpp"

#include <unordered_map>
#include <rhi/graphics_device.hpp>
#include <rhi/common/array_vector.hpp>

namespace ren
{
class Window;

class Asset_Manager
{
private:
    struct Render_Attachment_Image_Create_Info;
public:
    Asset_Manager(std::shared_ptr<Logger> logger, rhi::Graphics_Device* device, uint64_t frames_in_flight, Window& window) noexcept;

    [[nodiscard]] rhi::Buffer* create_buffer(const rhi::Buffer_Create_Info& create_info, const char* name = nullptr) noexcept;
    void destroy_buffer(rhi::Buffer* buffer) noexcept;

    [[nodiscard]] rhi::Sampler* create_sampler(const rhi::Sampler_Create_Info& create_info) noexcept;
    void destroy_sampler(rhi::Sampler* sampler) noexcept;

    [[nodiscard]] rhi::Image* create_image(const rhi::Image_Create_Info& create_info, const char* name = nullptr) noexcept;
    void destroy_image(rhi::Image* image) noexcept;

    Render_Attachment* create_render_attachment(const Render_Attachment_Create_Info& create_info) noexcept;
    void destroy_render_attachment(Render_Attachment* attachment) noexcept;

    [[nodiscard]] rhi::Pipeline* create_pipeline(const rhi::Graphics_Pipeline_Create_Info& create_info) noexcept;
    [[nodiscard]] rhi::Pipeline* create_pipeline(const rhi::Mesh_Shading_Pipeline_Create_Info& create_info) noexcept;
    [[nodiscard]] rhi::Pipeline* create_pipeline(const rhi::Compute_Pipeline_Create_Info& create_info) noexcept;
    void destroy_pipeline(rhi::Pipeline* pipeline) noexcept;

    // void recreate_dirty_pipelines();
    void recreate_size_dependent_render_attachment_images();
    void flush_deletion_queue(uint64_t frame);
    void next_frame() noexcept;

private:
    uint64_t get_deletion_frame();
    rhi::Image_Create_Info make_render_attachment_image_create_info(const Render_Attachment_Image_Create_Info& create_info) noexcept;

private:
    enum class Asset_Deletion_Type
    {
        Buffer,
        Sampler,
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
            rhi::Sampler* sampler;
            rhi::Image* image;
            rhi::Pipeline* pipeline;
        };
    };

    struct Render_Attachment_Image_Create_Info
    {
        rhi::Image_Format format;
        Render_Attachment_Scaling_Mode scaling_mode;
        float scaling_factor;
        uint32_t width;
        uint32_t height;
        uint32_t layers;
        bool create_srv;
    };

    std::shared_ptr<Logger> m_logger;
    rhi::Graphics_Device* m_device;
    Window& m_window;
    std::vector<Deletion_Queue_Entry> m_deletion_queue;
    uint64_t m_current_frame = 0;
    uint64_t m_frames_in_flight;
    rhi::Array_Vector<Render_Attachment, 64> m_render_attachments;
};
}
