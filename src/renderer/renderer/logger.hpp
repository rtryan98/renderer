#pragma once

#include <spdlog/sinks/sink.h>
#include <spdlog/logger.h>

namespace ren
{
class Logger
{
public:
    Logger() noexcept;

    template<typename... Args>
    void trace(fmt::format_string<Args...> fmt, Args&&... args)
    {
        m_log->trace(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void debug(fmt::format_string<Args...> fmt, Args&&... args)
    {
        m_log->debug(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(fmt::format_string<Args...> fmt, Args&&... args)
    {
        m_log->info(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(fmt::format_string<Args...> fmt, Args&&... args)
    {
        m_log->warn(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(fmt::format_string<Args...> fmt, Args&&... args)
    {
        m_log->error(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void critical(fmt::format_string<Args...> fmt, Args&&... args)
    {
        m_log->critical(fmt, std::forward<Args>(args)...);
    }

private:
    std::shared_ptr<spdlog::sinks::sink> m_stdout_sink;
    std::unique_ptr<spdlog::logger> m_log;
};
}
