#pragma once

#include <filesystem>
#include <memory>
#include <optional>

namespace ren
{
enum class File_Notification_Type
{
    Rename = 0,
    Remove,
    Create,
    Modify
};

struct File_Watch_Notification
{
    File_Notification_Type type;
    std::filesystem::path path;
};

class File_Watch
{
public:
    static std::unique_ptr<File_Watch> create(const std::filesystem::path& path);
    virtual ~File_Watch() = default;

    virtual std::optional<File_Watch_Notification> poll_for_changes() = 0;
};
}
