#include "renderer/logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace ren
{
Logger::Logger() noexcept
    : m_stdout_sink(std::make_shared<spdlog::sinks::stderr_color_sink_mt>())
    , m_log(std::make_unique<spdlog::logger>("Renderer", m_stdout_sink))
{
    m_stdout_sink->set_pattern("%^[%T] [%l]%$: %v");
    m_log->set_level(spdlog::level::trace);
}

Logger::Logger(int32_t level) noexcept
    : m_stdout_sink(std::make_shared<spdlog::sinks::stderr_color_sink_mt>())
    , m_log(std::make_unique<spdlog::logger>("Renderer", m_stdout_sink))
{
    m_stdout_sink->set_pattern("%^[%T] [%l]%$: %v");
    m_log->set_level(static_cast<spdlog::level::level_enum>(std::min(std::max(0, level), 5)));
}
}
