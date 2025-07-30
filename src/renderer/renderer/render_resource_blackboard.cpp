#include "renderer/render_resource_blackboard.hpp"

#include <rhi/graphics_device.hpp>
#include "renderer/application.hpp"

namespace ren
{
Buffer::Buffer(Render_Resource_Blackboard& blackboard, rhi::Buffer** buffer, const std::string& name)
    : m_blackboard(&blackboard)
    , m_buffer(buffer)
    , m_name(name)
{}

rhi::Buffer_Create_Info Buffer::get_create_info() const
{
    if (!m_buffer)
        return rhi::Buffer_Create_Info();
    return {
        .size = (*m_buffer)->size,
        .heap = (*m_buffer)->heap_type,
    };
}

void Buffer::recreate(const rhi::Buffer_Create_Info& create_info)
{
    if (!m_buffer) return;

    m_blackboard->delete_resource(*m_buffer);
    auto device = m_blackboard->m_device;
    *m_buffer = device->create_buffer(create_info).value_or(nullptr);
    device->name_resource(*m_buffer, m_name.c_str());
}

uint64_t Buffer::size() const noexcept
{
    if (!m_buffer) return 0ull;
    return (*m_buffer)->size;
}

Buffer::operator unsigned int() const
{
    if (!m_buffer) return 0u;
    return (*m_buffer)->buffer_view->bindless_index;
}

Buffer::operator rhi::Buffer*() const
{
    if (!m_buffer) return nullptr;
    return *m_buffer;
}

Buffer::operator void*() const
{
    if (!m_buffer) return nullptr;
    return (*m_buffer)->data;
}

Buffer::operator const std::string&() const
{
    return m_name;
}

Image::Image(Render_Resource_Blackboard& blackboard, rhi::Image** image, const std::string& name)
    : m_blackboard(&blackboard)
    , m_image(image)
    , m_name(name)
{}

rhi::Image_Create_Info Image::get_create_info() const
{
    if (!m_image)
        return rhi::Image_Create_Info{};
    return {
        .format = (*m_image)->format,
        .width = (*m_image)->width,
        .height = (*m_image)->height,
        .depth = (*m_image)->depth,
        .array_size = (*m_image)->array_size,
        .mip_levels = (*m_image)->mip_levels,
        .usage = (*m_image)->usage,
        .primary_view_type = (*m_image)->primary_view_type,
    };
}

void Image::recreate(const rhi::Image_Create_Info& create_info)
{
    if (!m_image) return;

    m_blackboard->delete_resource(*m_image);
    auto device = m_blackboard->m_device;
    *m_image = device->create_image(create_info).value_or(nullptr);
    device->name_resource(*m_image, m_name.c_str());
}

Image::operator unsigned int() const
{
    if (!m_image) return 0u;
    return (*m_image)->image_view->bindless_index;
}

Image::operator rhi::Image*() const
{
    if (!m_image) return nullptr;
    return *m_image;
}

Image::operator rhi::Image_View*() const
{
    if (!m_image) return nullptr;
    return (*m_image)->image_view;
}

Image::operator const std::string&() const
{
    return m_name;
}

Sampler::Sampler(rhi::Sampler* sampler)
    : m_sampler(sampler)
{}

Sampler::operator unsigned int() const
{
    if (!m_sampler) return 0u;
    return m_sampler->bindless_index;
}

Sampler::operator rhi::Sampler*() const
{
    return m_sampler;
}

Render_Resource_Blackboard::Render_Resource_Blackboard(rhi::Graphics_Device* device)
    : m_device(device)
    , m_deletion_queue()
    , m_current_garbage_frame(FRAME_IN_FLIGHT_COUNT)
{}

Render_Resource_Blackboard::~Render_Resource_Blackboard()
{
    m_device->wait_idle();
    garbage_collect(~0ull);
    // remove remaining resources
    for (auto& buffer : m_buffers)
        delete_resource(buffer.buffer);
    for (auto& image : m_images)
        delete_resource(image.image);
    for (auto& sampler : m_samplers)
        m_device->destroy_sampler(sampler.second);
    garbage_collect(~0ull);
}

Sampler Render_Resource_Blackboard::get_sampler(const rhi::Sampler_Create_Info& create_info)
{
    rhi::Sampler* sampler = nullptr;
    if (!m_samplers.contains(create_info))
    {
        sampler = m_device->create_sampler(create_info).value_or(nullptr);
        m_samplers[create_info] = sampler;
    }
    return m_samplers[create_info];
}

Buffer Render_Resource_Blackboard::create_buffer(const std::string& name, const rhi::Buffer_Create_Info& create_info)
{
    if (has_buffer(name)) return Buffer(*this, &(m_buffer_wrapper_ptrs[name]->buffer), name);
    const auto buffer = m_buffers.acquire();
    m_buffer_wrapper_ptrs[name] = buffer;
    buffer->buffer = m_device->create_buffer(create_info).value_or(nullptr);
    m_device->name_resource(buffer->buffer, name.c_str());
    return Buffer(*this, &(m_buffer_wrapper_ptrs[name]->buffer), name);
}

Buffer Render_Resource_Blackboard::get_buffer(const std::string& name)
{
    if (!has_buffer(name)) return Buffer();
    return Buffer(*this, &(m_buffer_wrapper_ptrs[name]->buffer), name);
}

bool Render_Resource_Blackboard::has_buffer(const std::string& name)
{
    return m_buffer_wrapper_ptrs.contains(name);
}

void Render_Resource_Blackboard::destroy_buffer(const std::string& name)
{
    if (!has_buffer(name))
        return;
    m_buffers.release(m_buffer_wrapper_ptrs[name]);
    delete_resource(m_buffer_wrapper_ptrs[name]->buffer);
    m_buffer_wrapper_ptrs.erase(name);
}

Image Render_Resource_Blackboard::create_image(const std::string& name, const rhi::Image_Create_Info& create_info)
{
    if (has_image(name)) return Image(*this, &(m_image_wrapper_ptrs[name]->image), name);
    const auto image = m_images.acquire();
    m_image_wrapper_ptrs[name] = image;
    image->image = m_device->create_image(create_info).value_or(nullptr);
    m_device->name_resource(image->image, name.c_str());
    return Image(*this, &(m_image_wrapper_ptrs[name]->image), name);
}

Image Render_Resource_Blackboard::get_image(const std::string& name)
{
    if (!has_image(name)) return Image();
    return Image(*this, &(m_image_wrapper_ptrs[name]->image), name);
}

bool Render_Resource_Blackboard::has_image(const std::string& name)
{
    return m_image_wrapper_ptrs.contains(name);
}

void Render_Resource_Blackboard::destroy_image(const std::string& name)
{
    if (!has_image(name))
        return;
    m_images.release(m_image_wrapper_ptrs[name]);
    delete_resource(m_image_wrapper_ptrs[name]->image);
    m_image_wrapper_ptrs.erase(name);
}

void Render_Resource_Blackboard::garbage_collect(const uint64_t frame)
{
    auto remover = [frame, this](const Deleted_Resource& resource) noexcept -> bool
    {
        if (frame > resource.frame)
        {
            m_device->destroy_buffer(resource.buffer);
            m_device->destroy_image(resource.image);
            return true;
        }
        return false;
    };
    std::vector<Deleted_Resource> survivors;
    survivors.reserve(m_deletion_queue.size());
    for (const auto& deleted_resource : m_deletion_queue)
    {
        if (!remover(deleted_resource))
        {
            survivors.emplace_back(deleted_resource);
        }
    }
    std::swap(m_deletion_queue, survivors);
    m_current_garbage_frame = frame + FRAME_IN_FLIGHT_COUNT;
}

void Render_Resource_Blackboard::delete_resource(rhi::Buffer* buffer)
{
    m_deletion_queue.push_back({ .buffer = buffer, .image = nullptr, .frame = m_current_garbage_frame });
}

void Render_Resource_Blackboard::delete_resource(rhi::Image* image)
{
    m_deletion_queue.push_back({ .buffer = nullptr, .image = image, .frame = m_current_garbage_frame });
}
}
