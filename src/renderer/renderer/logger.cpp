#include "renderer/logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace ren
{
Logger::Logger() noexcept
    : m_stdout_sink(std::make_shared<spdlog::sinks::stderr_color_sink_mt>())
    , m_log(std::make_unique<spdlog::logger>("Renderer", m_stdout_sink))
{
    m_stdout_sink->set_pattern("%^[%T] [%l]%$ : %v");
}
}
