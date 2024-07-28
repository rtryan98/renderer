#pragma once

#include <rhi/resource.hpp>
#include <string>
#include <vector>

namespace ren
{
enum class Render_Attachment_Scaling_Mode
{
    Ratio,
    Absolute
};

struct Render_Attachment_Create_Info
{
    std::string name;
    rhi::Image_Format format;
    Render_Attachment_Scaling_Mode scaling_mode;
    float scaling_factor;
    uint32_t absolute_width;
    uint32_t absolute_height;
    uint32_t layers;
    bool create_srv;
};

struct Render_Attachment
{
    std::string name;
    Render_Attachment_Scaling_Mode scaling_mode;
    float scaling_factor;
    bool create_srv;
    rhi::Image* image;
};
}
