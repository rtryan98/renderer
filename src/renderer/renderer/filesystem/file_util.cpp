#include "renderer/filesystem/file_util.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace ren
{
std::vector<uint8_t> load_file_binary_unsafe(const char* path)
{
    std::vector<uint8_t> result;
    std::ifstream file(path, std::ios::binary);
    file.unsetf(std::ios::skipws);
    file.seekg(0, std::ios::end);
    std::streampos file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    result.reserve(file_size);
    result.insert(
        result.begin(),
        std::istream_iterator<uint8_t>(file),
        std::istream_iterator<uint8_t>());
    file.close();
    return result;
}

std::string load_file_as_string_unsafe(const char* path)
{
    auto text_length = std::filesystem::file_size(path);
    std::string result(text_length, '\0');
    std::ifstream file(path);
    file.read(&result[0], text_length);
    file.close();
    return result;
}
}
