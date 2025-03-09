#include "renderer/filesystem/file_watch.hpp"

#include <array>
#include <Windows.h>

namespace ren
{
constexpr static auto FILTERS =
    FILE_NOTIFY_CHANGE_FILE_NAME  | FILE_NOTIFY_CHANGE_DIR_NAME |
    FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;

File_Notification_Type translate_notification_type(DWORD action)
{
    switch (action)
    {
    case FILE_ACTION_ADDED:
        return File_Notification_Type::Create;
    case FILE_ACTION_REMOVED:
        return File_Notification_Type::Remove;
    case FILE_ACTION_MODIFIED:
        return File_Notification_Type::Modify;
    case FILE_ACTION_RENAMED_OLD_NAME:
        return File_Notification_Type::Invalid;
    case FILE_ACTION_RENAMED_NEW_NAME:
        return File_Notification_Type::Rename;
    default:
        return File_Notification_Type::Invalid;
    }
}

class File_Watch_Win32 final : public File_Watch
{
public:
    File_Watch_Win32(const std::filesystem::path& path)
    {
        auto path_str = path.string();

        m_dir_handle = CreateFileA(path_str.c_str(),
            FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr);
        if (m_dir_handle == INVALID_HANDLE_VALUE) return; // TODO: should this silently fail?

        m_overlapped.hEvent = CreateEvent(nullptr, false, false, nullptr);

        if (m_overlapped.hEvent == INVALID_HANDLE_VALUE) return; // TODO: should this silently fail?

        ReadDirectoryChangesW(m_dir_handle, m_buffer.data(), m_buffer.size(),
            true, FILTERS, nullptr, &m_overlapped, nullptr);
    }

    ~File_Watch_Win32() override
    {
        if (m_overlapped.hEvent != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_overlapped.hEvent);
        }
        if (m_dir_handle != INVALID_HANDLE_VALUE)
        {
            CancelIo(m_dir_handle);
            CloseHandle(m_dir_handle);
        }
    }

    virtual std::vector<File_Watch_Notification> poll_for_changes() override
    {
        if (m_dir_handle == INVALID_HANDLE_VALUE) return {};

        if (WaitForSingleObject(m_overlapped.hEvent, 0) == WAIT_OBJECT_0)
        {
            DWORD bytes_returned = 0;
            bool success = GetOverlappedResult(m_dir_handle, &m_overlapped, &bytes_returned, true);
            if (success && bytes_returned > 0)
            {
                std::vector<File_Watch_Notification> notifications;
                FILE_NOTIFY_INFORMATION* notify_info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(m_buffer.data());
                std::filesystem::path old_path;
                bool process_next = true;
                while (process_next) {
                    process_next = notify_info->NextEntryOffset != 0;
                    if (notify_info->Action != FILE_ACTION_RENAMED_OLD_NAME)
                    {
                        notifications.push_back({
                            .type = translate_notification_type(notify_info->Action),
                            .old_path = notify_info->Action == FILE_ACTION_RENAMED_NEW_NAME ? old_path : "",
                            .path = std::wstring(notify_info->FileName, notify_info->FileNameLength / sizeof(wchar_t)) });
                    }
                    else
                    {
                        old_path = std::wstring(notify_info->FileName, notify_info->FileNameLength / sizeof(wchar_t));
                    }
                    notify_info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<BYTE*>(notify_info)
                        + notify_info->NextEntryOffset);
                };
                ReadDirectoryChangesW(m_dir_handle, m_buffer.data(), m_buffer.size(),
                    true, FILTERS, nullptr, &m_overlapped, nullptr);
                return notifications;
            }
        }
    }

private:
    HANDLE m_dir_handle = NULL;
    OVERLAPPED m_overlapped = {};
    alignas(DWORD) std::array<uint8_t, 8192> m_buffer = {};
};

std::unique_ptr<File_Watch> File_Watch::create(const std::filesystem::path& path)
{
    return std::make_unique<File_Watch_Win32>(path);
}
}
