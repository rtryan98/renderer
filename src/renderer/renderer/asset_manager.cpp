#include "renderer/asset_manager.hpp"

namespace ren
{
constexpr const char* result_to_string(rhi::Result result)
{
    switch (result)
    {
    case rhi::Result::Success:
        return "Success";
    case rhi::Result::Wait_Timeout:
        return "Wait timed out";
    case rhi::Result::Error_Wait_Failed:
        return "Wait failed";
    case rhi::Result::Error_Out_Of_Memory:
        return "Out of memory";
    case rhi::Result::Error_Invalid_Parameters:
        return "Invalid parameters";
    case rhi::Result::Error_Device_Lost:
        return "Device lost";
    case rhi::Result::Error_No_Resource:
        return "No resource";
    case rhi::Result::Error_Unknown:
        return "Unknown Error";
    default:
        return "";
    }
}

Asset_Manager::Asset_Manager(
    std::shared_ptr<Logger> logger,
    rhi::Graphics_Device* device,
    uint64_t frames_in_flight) noexcept
    : m_logger(logger)
    , m_device(device)
    , m_deletion_queue()
    , m_frames_in_flight(frames_in_flight)
{}

// TODO: abort or keep going on error?
rhi::Buffer* Asset_Manager::create_buffer(const rhi::Buffer_Create_Info& create_info, const char* name) noexcept
{
    auto result = m_device->create_buffer(create_info);
    if (!result.has_value())
    {
        m_logger->critical("Failed to create buffer! Reason: {}", result_to_string(result.error()));
        return nullptr;
    }
    if (name != nullptr) m_device->name_resource(result.value(), name);
    return result.value();
}

void Asset_Manager::destroy_buffer(rhi::Buffer* buffer) noexcept
{
    m_deletion_queue.push_back({
        .frame = get_deletion_frame(),
        .type = Asset_Deletion_Type::Buffer,
        .buffer = buffer
        });
}

rhi::Image* Asset_Manager::create_image(const rhi::Image_Create_Info& create_info, const char* name) noexcept
{
    auto result = m_device->create_image(create_info);
    if (!result.has_value())
    {
        m_logger->critical("Failed to create image! Reason: {}", result_to_string(result.error()));
        return nullptr;
    }
    if (name != nullptr) m_device->name_resource(result.value(), name);
    return result.value();
}

void Asset_Manager::destroy_image(rhi::Image* image) noexcept
{
    m_deletion_queue.push_back({
        .frame = get_deletion_frame(),
        .type = Asset_Deletion_Type::Image,
        .image = image
        });
}

rhi::Pipeline* Asset_Manager::create_pipeline(const rhi::Graphics_Pipeline_Create_Info& create_info) noexcept
{
    auto result = m_device->create_pipeline(create_info);
    if (!result.has_value())
    {
        m_logger->critical("Failed to create pipeline! Reason: {}", result_to_string(result.error()));
        return nullptr;
    }
    return result.value();
}

rhi::Pipeline* Asset_Manager::create_pipeline(const rhi::Mesh_Shading_Pipeline_Create_Info& create_info) noexcept
{
    auto result = m_device->create_pipeline(create_info);
    if (!result.has_value())
    {
        m_logger->critical("Failed to create pipeline! Reason: {}", result_to_string(result.error()));
        return nullptr;
    }
    return result.value();
}

rhi::Pipeline* Asset_Manager::create_pipeline(const rhi::Compute_Pipeline_Create_Info& create_info) noexcept
{
    auto result = m_device->create_pipeline(create_info);
    if (!result.has_value())
    {
        m_logger->critical("Failed to create pipeline! Reason: {}", result_to_string(result.error()));
        return nullptr;
    }
    return result.value();
}

void Asset_Manager::destroy_pipeline(rhi::Pipeline* pipeline) noexcept
{
    m_deletion_queue.push_back({
        .frame = get_deletion_frame(),
        .type = Asset_Deletion_Type::Pipeline,
        .pipeline = pipeline
        });
}

void Asset_Manager::flush_deletion_queue(uint64_t frame)
{
    std::vector<Deletion_Queue_Entry> kept_entries;
    kept_entries.reserve(m_deletion_queue.size());
    for (auto& entry : m_deletion_queue)
    {
        if (frame >= entry.frame)
        {
            switch (entry.type)
            {
            case Asset_Deletion_Type::Buffer:
                m_device->destroy_buffer(entry.buffer);
                break;
            case Asset_Deletion_Type::Image:
                m_device->destroy_image(entry.image);
                break;
            case Asset_Deletion_Type::Pipeline:
                m_device->destroy_pipeline(entry.pipeline);
                break;
            default:
                break;
            }
        }
        else
        {
            kept_entries.emplace_back(entry);
        }
    }

    m_deletion_queue = kept_entries;
}

void Asset_Manager::next_frame() noexcept
{
    m_current_frame += 1;
}

uint64_t Asset_Manager::get_deletion_frame()
{
    return m_current_frame + m_frames_in_flight;
}

}
