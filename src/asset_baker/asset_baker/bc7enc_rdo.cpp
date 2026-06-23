#include "asset_baker/bc7enc_rdo.hpp"

#include <rhi/image_format.hpp>
#include <rdo_bc_encoder.h>

#include <cassert>

namespace asset_baker::bc7enc_rdo
{
// bc7enc_rdo doesn't differentiate between several BCn formats.
static DXGI_FORMAT to_encoder_dxgi_format(rhi::Image_Format image_format)
{
    switch (image_format)
    {
    case rhi::Image_Format::BC1_RGB_UNORM_BLOCK:
    case rhi::Image_Format::BC1_RGB_SRGB_BLOCK:
    case rhi::Image_Format::BC1_RGBA_UNORM_BLOCK:
    case rhi::Image_Format::BC1_RGBA_SRGB_BLOCK:
        return DXGI_FORMAT_BC1_UNORM;
    case rhi::Image_Format::BC3_UNORM_BLOCK:
    case rhi::Image_Format::BC3_SRGB_BLOCK:
        return DXGI_FORMAT_BC3_UNORM;
    case rhi::Image_Format::BC4_UNORM_BLOCK:
        return DXGI_FORMAT_BC4_UNORM;
    case rhi::Image_Format::BC5_UNORM_BLOCK:
        return DXGI_FORMAT_BC5_UNORM;
    case rhi::Image_Format::BC7_UNORM_BLOCK:
    case rhi::Image_Format::BC7_SRGB_BLOCK:
        return DXGI_FORMAT_BC7_UNORM;
    default:
        assert(false && "Unsupported BCn target format for bc7enc_rdo");
        return DXGI_FORMAT_UNKNOWN;
    }
}

std::vector<uint8_t> encode_mip(const uint8_t* rgba_data, uint32_t width, uint32_t height, rhi::Image_Format image_format)
{
    utils::image_u8 src_image(width, height);
    memcpy(src_image.get_pixels().data(), rgba_data, size_t(width) * height * 4);

    rdo_bc::rdo_bc_params bc_params;
    bc_params.m_dxgi_format = to_encoder_dxgi_format(image_format);
    // bc_params.m_bc7_uber_level = 0;
    bc_params.m_rdo_lambda = 0.f;

    rdo_bc::rdo_bc_encoder encoder;
    if (!encoder.init(src_image, bc_params))
    {
        assert(false && "bc7enc_rdo init failed (unsupported format or bad image)");
        return {};
    }
    if (!encoder.encode())
    {
        assert(false && "bc7enc_rdo encode failed");
        return {};
    }

    const void* blocks = encoder.get_blocks();
    const uint32_t byte_count = encoder.get_total_blocks_size_in_bytes();
    std::vector<uint8_t> bytes(byte_count);
    memcpy(bytes.data(), blocks, byte_count);

    return bytes;
}
}
