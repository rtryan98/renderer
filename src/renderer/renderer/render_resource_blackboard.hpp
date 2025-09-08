#pragma once

#include <rhi/resource.hpp>
#include <ankerl/unordered_dense.h>
#include <plf_colony.h>

namespace rhi
{
class Graphics_Device;
class Swapchain;
}

namespace ren
{
class Render_Resource_Blackboard;

class Buffer
{
public:
    Buffer() = default;
    Buffer(Render_Resource_Blackboard& blackboard, rhi::Buffer** buffer, const std::string& name);

    rhi::Buffer_Create_Info get_create_info() const;
    void recreate(const rhi::Buffer_Create_Info& create_info);

    uint64_t size() const noexcept;

    operator uint32_t() const; //NOLINT
    operator rhi::Buffer*() const; //NOLINT
    operator void*() const; //NOLINT
    operator const std::string&() const; //NOLINT

private:
    Render_Resource_Blackboard* m_blackboard;
    rhi::Buffer** m_buffer;
    std::string m_name;
};

struct Image_View_Subresource_Info
{
    uint16_t mip_level;
    uint16_t first_array_level;
    uint16_t array_levels;
    rhi::Image_View_Type view_type;
};

class Image_View
{
public:
    Image_View() = default;
    Image_View(rhi::Image_View** image_view);

    operator uint32_t() const; //NOLINT
    operator rhi::Image*() const; //NOLINT
    operator rhi::Image_View*() const; //NOLINT

private:
    rhi::Image_View** m_image_view;
};

class Image
{
public:
    Image() = default;
    Image(Render_Resource_Blackboard& blackboard, rhi::Image** image, const std::string& name);
    Image(rhi::Swapchain& swapchain);

    rhi::Image_Create_Info get_create_info() const;
    void recreate(const rhi::Image_Create_Info& create_info);
    [[nodiscard]] Image_View create_image_view(const Image_View_Subresource_Info& subresource);

    operator uint32_t() const; //NOLINT
    operator rhi::Image*() const; //NOLINT
    operator rhi::Image_View*() const; //NOLINT
    operator const std::string&() const; //NOLINT

private:
    static constexpr auto MAX_IMAGE_VIEWS = 16;

    Render_Resource_Blackboard* m_blackboard;
    rhi::Image** m_image;
    std::string m_name;
    std::array<std::pair<Image_View_Subresource_Info, rhi::Image_View*>, MAX_IMAGE_VIEWS> m_image_views;

private:
    rhi::Image_View* create_image_view_internal(const Image_View_Subresource_Info& subresource) const;
};

class Sampler
{
public:
    Sampler() = default;
    Sampler(rhi::Sampler* sampler);

    operator uint32_t() const; //NOLINT
    operator rhi::Sampler*() const; //NOLINT

private:
    rhi::Sampler* m_sampler;
};

class Render_Resource_Blackboard
{
public:
    Render_Resource_Blackboard(rhi::Graphics_Device* device);
    ~Render_Resource_Blackboard();

    Render_Resource_Blackboard(const Render_Resource_Blackboard&) = delete;
    Render_Resource_Blackboard& operator=(const Render_Resource_Blackboard&) = delete;
    Render_Resource_Blackboard(Render_Resource_Blackboard&&) = delete;
    Render_Resource_Blackboard& operator=(Render_Resource_Blackboard&&) = delete;

    Sampler get_sampler(const rhi::Sampler_Create_Info& create_info);

    Buffer create_buffer(const std::string& name, const rhi::Buffer_Create_Info& create_info, uint32_t index = rhi::NO_RESOURCE_INDEX);
    Buffer get_buffer(const std::string& name);
    bool has_buffer(const std::string& name);
    void destroy_buffer(const std::string& name);

    Image create_image(const std::string& name, const rhi::Image_Create_Info& create_info, uint32_t index = rhi::NO_RESOURCE_INDEX);
    Image get_image(const std::string& name);
    bool has_image(const std::string& name);
    void destroy_image(const std::string& name);

    void garbage_collect(uint64_t frame);

    // TODO: should this be public?
    [[nodiscard]] rhi::Graphics_Device* get_graphics_device() const { return m_device; }

private:
    void delete_resource(rhi::Buffer* buffer);
    void delete_resource(rhi::Image* image);

private:
    friend class Buffer;
    friend class Image;

    struct Buffer_Wrapper { rhi::Buffer* buffer; };
    struct Image_Wrapper { rhi::Image* image; };
    struct Sampler_Hash
    {
        [[nodiscard]] uint64_t operator()(const rhi::Sampler_Create_Info& create_info) const noexcept
        {
            return ankerl::unordered_dense::detail::wyhash::hash(&create_info, sizeof(rhi::Sampler_Create_Info));
        }
    };

    rhi::Graphics_Device* m_device = nullptr;

    ankerl::unordered_dense::map<rhi::Sampler_Create_Info, Sampler, Sampler_Hash> m_samplers;

    ankerl::unordered_dense::map<std::string, Buffer_Wrapper*> m_buffer_wrapper_ptrs;
    plf::colony<Buffer_Wrapper> m_buffers;
    ankerl::unordered_dense::map<std::string, Image_Wrapper*> m_image_wrapper_ptrs;
    plf::colony<Image_Wrapper> m_images;

    struct Deleted_Resource
    {
        rhi::Buffer* buffer;
        rhi::Image* image;
        uint64_t frame;
    };
    std::vector<Deleted_Resource> m_deletion_queue;
    uint64_t m_current_garbage_frame;
};
}
