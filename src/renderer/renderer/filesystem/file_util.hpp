#pragma once

#include <cstdint>
#include <vector>

namespace ren
{
std::vector<uint8_t> load_file_binary_unsafe(const char* path);
}
