#include "renderer/filesystem/file_watch.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <WinBase.h>

namespace ren
{
class File_Watch_Win32 final : public File_Watch
{
public:
    File_Watch_Win32()
    {

    }

    ~File_Watch_Win32() override
    {

    }

    virtual std::optional<File_Watch_Notification> poll_for_changes() override
    {

    }

private:
    HANDLE m_handle = NULL;
};

std::unique_ptr<File_Watch> File_Watch::create(const std::filesystem::path& path)
{
    return std::make_unique<File_Watch_Win32>();
}
}
