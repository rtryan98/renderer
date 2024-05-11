#include "renderer/shader_manager.hpp"

namespace ren
{
Shader_Library::Shader_Library(std::shared_ptr<Logger> logger)
    : m_logger(logger)
{
    m_logger->info("Loading shaders.");
    m_logger->trace("Constructing dependency trees.");
    std::unordered_set<std::string> shader_sources;
    for (auto i = 0; i < shader_metadata.size(); ++i)
    {
        const auto& shader = shader_metadata[i];
        shader_sources.insert(shader.source_path);
    }
    for (const auto& shader_source : shader_sources)
    {
        m_logger->trace("Processing '{}'.", shader_source);
        // TODO: parse includes recursively
    }
    for (auto i = 0; i < shader_metadata.size(); ++i)
    {
        const auto& shader = shader_metadata[i];
        m_logger->info("Loading shader '{}'", shader.name);

        bool shader_binary_outdated = true;
        bool shader_binary_does_not_exist = true;

        if (shader_binary_does_not_exist)
        {
            m_logger->trace("Cached binary '{}' does not exist. Compile required.", shader.binary_path);
        }
        else if (shader_binary_outdated)
        {
            m_logger->trace("Cached binary '{}' is outdated. Recompile required.", shader.binary_path);
        }
        if (shader_binary_does_not_exist || shader_binary_outdated)
        {
            m_logger->trace("Compiling shader '{}' from source '{}'", shader.name, shader.source_path);
        }
        else if (!shader_binary_does_not_exist || !shader_binary_outdated)
        {
            m_logger->trace("Loading shader from cached binary '{}'", shader.binary_path);
        }
    }
    m_logger->info("Finished loading {} shaders.", shader_metadata.size());
}

rhi::Shader_Blob* Shader_Library::get_shader(Shaders shader) const
{
    auto shader_idx = std::to_underlying(shader);
    return m_predefined_shaders[shader_idx];
}
}
