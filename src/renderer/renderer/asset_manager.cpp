#include "renderer/asset_manager.hpp"
#include "renderer/window.hpp"

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
    uint64_t frames_in_flight,
    Window& window) noexcept
    : m_logger(logger)
    , m_device(device)
    , m_window(window)
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
        if (name != nullptr) m_logger->critical("\tBuffer name: {}", name);
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

rhi::Sampler* Asset_Manager::create_sampler(const rhi::Sampler_Create_Info& create_info) noexcept
{
    auto result = m_device->create_sampler(create_info);
    if (!result.has_value())
    {
        m_logger->critical("Failed to create sampler! Reason: {}", result_to_string(result.error()));
        return nullptr;
    }
    return result.value();
}

void Asset_Manager::destroy_sampler(rhi::Sampler* sampler) noexcept
{
    m_deletion_queue.push_back({
        .frame = get_deletion_frame(),
        .type = Asset_Deletion_Type::Sampler,
        .sampler = sampler
        });
}

rhi::Image* Asset_Manager::create_image(const rhi::Image_Create_Info& create_info, const char* name) noexcept
{
    auto result = m_device->create_image(create_info);
    if (!result.has_value())
    {
        m_logger->critical("Failed to create image! Reason: {}", result_to_string(result.error()));
        if (name != nullptr) m_logger->critical("\tImage name: {}", name);
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

Render_Attachment* Asset_Manager::create_render_attachment(const Render_Attachment_Create_Info& create_info) noexcept
{
    Render_Attachment_Image_Create_Info render_attachment_image_create_info = {
        .format = create_info.format,
        .scaling_mode = create_info.scaling_mode,
        .scaling_factor = create_info.scaling_factor,
        .layers = create_info.layers,
        .create_srv = create_info.create_srv
    };
    auto image_create_info = make_render_attachment_image_create_info(render_attachment_image_create_info);
    auto render_attachment = m_render_attachments.acquire();
    render_attachment->image = create_image(image_create_info, create_info.name.c_str());
    return render_attachment;
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

void Asset_Manager::recreate_size_dependent_render_attachment_images()
{
    for (auto& render_attachment : m_render_attachments)
    {
        if (render_attachment.scaling_mode != Render_Attachment_Scaling_Mode::Ratio)
        {
            continue;
        }
        Render_Attachment_Image_Create_Info render_attachment_image_create_info = {
            .format = render_attachment.image->format,
            .scaling_mode = Render_Attachment_Scaling_Mode::Ratio,
            .scaling_factor = render_attachment.scaling_factor,
            .layers = render_attachment.image->array_size,
            .create_srv = render_attachment.create_srv
        };
        auto image_create_info = make_render_attachment_image_create_info(render_attachment_image_create_info);
        destroy_image(render_attachment.image);
        render_attachment.image = create_image(image_create_info, render_attachment.name.c_str());
    }
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

rhi::Image_Create_Info Asset_Manager::make_render_attachment_image_create_info(
    const Render_Attachment_Image_Create_Info& create_info) noexcept
{
    rhi::Image_Usage sampled = create_info.create_srv
        ? rhi::Image_Usage::Sampled
        : static_cast<rhi::Image_Usage>(0);
    auto is_depth_format = [](rhi::Image_Format format)
        {
            switch (format)
            {
            case rhi::Image_Format::D16_UNORM:
                [[fallthrough]];
            case rhi::Image_Format::D32_SFLOAT:
                [[fallthrough]];
            case rhi::Image_Format::D24_UNORM_S8_UINT:
                [[fallthrough]];
            case rhi::Image_Format::D32_SFLOAT_S8_UINT:
                return true;
            default:
                return false;
            }
        };
    const auto& window_data = m_window.get_window_data();
    rhi::Image_Create_Info result = {
        .format = create_info.format,
        .width = create_info.scaling_mode == Render_Attachment_Scaling_Mode::Absolute
            ? create_info.width
            : uint32_t(float(window_data.width) * create_info.scaling_factor),
        .height = create_info.scaling_mode == Render_Attachment_Scaling_Mode::Absolute
            ? create_info.height
            : uint32_t(float(window_data.height) * create_info.scaling_factor),
        .depth = 1,
        .array_size = uint16_t(create_info.layers),
        .mip_levels = 1,
        .usage = sampled | (is_depth_format(create_info.format)
            ? rhi::Image_Usage::Depth_Stencil_Attachment
            : rhi::Image_Usage::Color_Attachment),
        .primary_view_type = create_info.layers > 1
            ? rhi::Image_View_Type::Texture_2D_Array
            : rhi::Image_View_Type::Texture_2D
    };
    return result;
}
}
