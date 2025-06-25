#include "renderer/asset/asset_repository.hpp"
#include <rhi_dxc_lib/shader_compiler.hpp>
#include <nlohmann/json.hpp>
#include <fstream>

namespace ren
{
class Asset_Repository::Shader_Compiler
{
public:
    Shader_Compiler() = default;
    ~Shader_Compiler() = default;

    Shader_Compiler(const Shader_Compiler&) = delete;
    Shader_Compiler& operator=(const Shader_Compiler&) = delete;
    Shader_Compiler(Shader_Compiler&&) = delete;
    Shader_Compiler& operator=(Shader_Compiler&&) = delete;

    rhi::dxc::Shader compile_from_memory(const rhi::dxc::Shader_Compiler_Settings& settings, const rhi::dxc::Shader_Compile_Info& compile_info)
    {
        return m_compiler.compile_from_memory(settings, compile_info);
    }

private:
    rhi::dxc::Shader_Compiler m_compiler;
};

Asset_Repository::Asset_Repository(const std::initializer_list<const std::string_view>& paths)
    : m_shader_compiler(std::make_unique<Shader_Compiler>())
{
}

void Asset_Repository::compile_and_add_shader_library(std::string_view hlsl_path, std::string_view json_path)
{
    // parse the json file
    auto shader_json = nlohmann::json::parse(std::ifstream(std::string(json_path)));
    
}
}
