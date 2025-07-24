#include "renderer/filesystem/mapped_file.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace ren
{
void Mapped_File::map(const char* path)
{
    handle_file = CreateFile(
        path,
        GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
    nullptr);
    handle_map = CreateFileMapping(
        handle_file,
        nullptr,
        PAGE_READONLY,
        0,
        0,
        nullptr);
    data = MapViewOfFile(
        handle_map,
        FILE_MAP_READ,
        0, 0, 0);
}

void Mapped_File::unmap()
{
    CloseHandle(handle_map);
    handle_map = nullptr;
    CloseHandle(handle_file);
    handle_file = nullptr;
    data = nullptr;
}
}
