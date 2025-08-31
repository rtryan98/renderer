#pragma once

#include <vector>
#include <filesystem>

namespace asset_baker
{
std::vector<char> load_radiance_hdr(const std::filesystem::path& path);
}
