#pragma once

#include <rhi/resource.hpp>
#include <ankerl/unordered_dense.h>
#include <rhi/common/array_vector.hpp>

namespace rhi
{
class Graphics_Device;
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

    operator uint32_t() const;
    operator rhi::Buffer*() const;
    operator void*() const;
    operator const std::string&() const;

private:
    Render_Resource_Blackboard* m_blackboard;
    rhi::Buffer** m_buffer;
    std::string m_name;
};

class Image
{
public:
    Image() = default;
    Image(Render_Resource_Blackboard& blackboard, rhi::Image** image, const std::string& name);

    rhi::Image_Create_Info get_create_info() const;
    void recreate(const rhi::Image_Create_Info& create_info);

    operator uint32_t() const;
    operator rhi::Image*() const;
    operator rhi::Image_View*() const;
    operator const std::string&() const;

private:
    Render_Resource_Blackboard* m_blackboard;
    rhi::Image** m_image;
    std::string m_name;
};

class Sampler
{
public:
    Sampler() = default;
    Sampler(rhi::Sampler* sampler);

    operator uint32_t() const;
    operator rhi::Sampler*() const;

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

    Buffer create_buffer(const std::string& name, const rhi::Buffer_Create_Info& create_info);
    Buffer get_buffer(const std::string& name);
    bool has_buffer(const std::string& name);
    void destroy_buffer(const std::string& name);

    Image create_image(const std::string& name, const rhi::Image_Create_Info& create_info);
    Image get_image(const std::string& name);
    bool has_image(const std::string& name);
    void destroy_image(const std::string& name);

    void garbage_collect(uint64_t frame);

private:
    void delete_resource(rhi::Buffer* buffer);
    void delete_resource(rhi::Image* image);

private:
    friend class Buffer;
    friend class Image;

    constexpr static auto ARRAY_VEC_SIZE = 128;

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
    rhi::Array_Vector<Buffer_Wrapper, ARRAY_VEC_SIZE> m_buffers;
    ankerl::unordered_dense::map<std::string, Image_Wrapper*> m_image_wrapper_ptrs;
    rhi::Array_Vector<Image_Wrapper, ARRAY_VEC_SIZE> m_images;

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
