#pragma once

#include "renderer/asset/asset_formats.hpp"

#include <filesystem>

namespace ren
{
Model_Data load_gltf_from_file(const std::filesystem::path& path);
}
