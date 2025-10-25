#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ren
{
std::vector<uint8_t> load_file_binary_unsafe(const char* path);

std::string load_file_as_string_unsafe(const char* path);
}
