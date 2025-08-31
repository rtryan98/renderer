#include "asset_baker/hdr_image_loader.hpp"

#include <stb_image.h>
#include <shared/serialized_asset_formats.hpp>

namespace asset_baker
{
std::vector<char> load_radiance_hdr(const std::filesystem::path& path)
{
    std::vector<char> result;
    result.resize(sizeof(serialization::Image_Data_00));
    serialization::Image_Data_00 image_data = {
        .header = {
            .magic = serialization::Image_Header::MAGIC,
            .version = 1,
        },
        .mip_count = 1,
        .format = rhi::Image_Format::R32G32B32A32_SFLOAT,
        .name = {},
        .hash_identifier = {},
        .mips = {}
    };
    const auto name = path.filename().string();
    name.copy(image_data.name, std::min(serialization::NAME_FIELD_SIZE, name.size()));
    int32_t width, height, channels;
    if (float* data = stbi_loadf(path.string().c_str(), &width, &height, &channels, 4); data != nullptr)
    {
        image_data.mips[0] = {
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height)
        };

        const auto offset = result.size();
        const auto size = width * height * 4 * sizeof(float);
        result.resize(offset + size);
        memcpy(result.data() + offset, data, size);

        stbi_image_free(data);
    }
    memcpy(result.data(), &image_data, sizeof(image_data));
    return result;
}
}
