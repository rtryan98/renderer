#pragma once

#include "renderer/logger.hpp"
#include "renderer/generated/compute_pipeline_library.hpp"
#include <rhi/resource.hpp>

#include <unordered_set>
#include <string>

namespace ren
{
class Shader_Library
{
public:
    Shader_Library(std::shared_ptr<Logger> logger);

    rhi::Shader_Blob* get_shader(Shaders shader) const;

private:
    std::shared_ptr<Logger> m_logger;
    std::array<rhi::Shader_Blob*, shader_metadata.size()> m_predefined_shaders = {};
    std::array<std::unordered_set<std::string>, shader_metadata.size()> m_predefined_shader_dependencies = {};
    std::unordered_map<std::string, std::unique_ptr<std::unordered_set<Shaders>>> m_shader_include_file_dependents = {};
};
}
