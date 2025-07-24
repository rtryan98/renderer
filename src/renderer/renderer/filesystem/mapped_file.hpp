#pragma once

namespace ren
{
struct Mapped_File
{
    void* data;
    void* handle_file;
    void* handle_map;

    void map(const char* path);
    void unmap();
};
}
