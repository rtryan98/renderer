#pragma once

#include <vector>

namespace rhi
{
enum class Image_Format;
}

namespace asset_baker::bc7enc_rdo
{
// IMPORTANT: Source `rgba_data` must be 4-channel 8bpc.
std::vector<uint8_t> encode_mip(const uint8_t* rgba_data, uint32_t width, uint32_t height, rhi::Image_Format image_format);
}
