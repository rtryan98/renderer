#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace ren
{
enum class File_Notification_Type
{
    Rename = 0,
    Remove,
    Create,
    Modify,
    Invalid
};

struct File_Watch_Notification
{
    File_Notification_Type type;
    std::filesystem::path old_path;
    std::filesystem::path path;
};

class File_Watch
{
public:
    static std::unique_ptr<File_Watch> create(const std::filesystem::path& path);
    virtual ~File_Watch() = default;

    virtual std::optional<std::vector<File_Watch_Notification>> poll_for_changes() = 0;
};
}
