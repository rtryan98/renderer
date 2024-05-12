#include "renderer/shader_manager.hpp"

#include <rhi_dxc_lib/shader_compiler.hpp>
#include <utility>
#include <filesystem>
#include <fstream>

namespace ren
{
std::string get_shader_source_prefix_path()
{
    constexpr auto shader_source_prefix_paths = std::to_array({
       "../../src/shaders/",   // Standalone from build
       "./"                    // Standalone deploy
       "../src/shaders/",      // VS Debugger
        });

    for (const auto prefix_path : shader_source_prefix_paths)
    {
        std::filesystem::path path = std::string(prefix_path);
        if (std::filesystem::exists(path))
        {
            return prefix_path;
        }
    }
    return "";
}

struct Shader_Library::Shader_Compiler
{
    Shader_Compiler() = default;
    ~Shader_Compiler() = default;

    rhi::dxc::Shader_Compiler instance;
};

std::wstring translate_shader_model(Shader_Type type)
{
    switch (type)
    {
    case ren::Shader_Type::Vertex:
        return L"vs_6_8";
    case ren::Shader_Type::Pixel:
        return L"ps_6_8";
    case ren::Shader_Type::Compute:
        return L"cs_6_8";
    case ren::Shader_Type::Task:
        return L"as_6_8";
    case ren::Shader_Type::Mesh:
        return L"ms_6_8";
    case ren::Shader_Type::Library:
        return L"lib_6_8";
    case ren::Shader_Type::Unknown:
        std::unreachable();
    default:
        std::unreachable();
    }
}

std::wstring cstr_to_wstring(const char* cstr)
{
    // HACK: this only works with ASCII and ISO/IEC 8859-1 encodings.
    std::string tempstr = cstr;
    return std::wstring(tempstr.begin(), tempstr.end());
}

auto cstr_vec_to_wstring_vec(const auto& from)
{
    std::vector<std::wstring> result;
    result.reserve(from.size());
    for (auto cstr : from)
    {
        result.push_back(cstr_to_wstring(cstr));
    }
    return result;
}

std::vector<uint8_t> load_file_binary(const char* path, std::shared_ptr<Logger>& logger)
{
    std::vector<uint8_t> result;
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        logger->error("Critical to open {}.", path);
    }
    file.unsetf(std::ios::skipws);
    std::streampos file_size;
    file.seekg(0, std::ios::end);
    file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    result.reserve(file_size);
    result.insert(
        result.begin(),
        std::istream_iterator<uint8_t>(file),
        std::istream_iterator<uint8_t>());
    file.close();
    return result;
}

Shader_Library::Shader_Library(std::shared_ptr<Logger> logger, rhi::Graphics_Device* device)
    : m_logger(logger)
    , m_compiler(std::make_unique<Shader_Compiler>())
    , m_device(device)
{

    m_logger->info("Loading shaders.");
    m_logger->trace("Constructing dependency trees.");

    auto prefix_path = get_shader_source_prefix_path();
    m_logger->debug("Shader source prefix path is '{}'", prefix_path.c_str());

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

        bool requires_compile = false;

        if (shader_binary_does_not_exist)
        {
            m_logger->info("Cached binary '{}' does not exist. Compile required.", shader.binary_path);
            requires_compile = true;
        }
        else if (shader_binary_outdated)
        {
            m_logger->info("Cached binary '{}' is outdated. Recompile required.", shader.binary_path);
            requires_compile = true;
        }
        if (shader_binary_does_not_exist || shader_binary_outdated)
        {
            m_logger->info("Compiling shader '{}' from source '{}{}'", shader.name,
                prefix_path.c_str(), shader.source_path);
            requires_compile = true;
        }
        else if (!shader_binary_does_not_exist || !shader_binary_outdated)
        {
            m_logger->info("Loading shader from cached binary '{}'", shader.binary_path);
        }

        if (requires_compile)
        {
            auto path = prefix_path + std::string(shader.source_path);
            auto file = load_file_binary(path.c_str(), m_logger);
            rhi::dxc::Shader_Compiler_Settings settings = {
                .include_dirs = {},
                .defines = cstr_vec_to_wstring_vec(shader.defines)
            };
            rhi::dxc::Shader_Compile_Info compile_info = {
                .data = file.data(),
                .data_size = file.size(),
                .entrypoint = cstr_to_wstring(shader.entry_point),
                .shader_model = translate_shader_model(shader.type)
            };
            const auto is_d3d12 = m_device->get_graphics_api() == rhi::Graphics_API::D3D12;
            auto compile_result = m_compiler->instance.compile_from_memory(settings, compile_info);
            auto compiled_data = is_d3d12
                ? compile_result.dxil.data() : compile_result.spirv.data();
            auto compiled_size = is_d3d12
                ? compile_result.dxil.size() : compile_result.spirv.size();
            auto shader_blob_result = m_device->create_shader_blob({
                .data = compiled_data,
                .data_size = compiled_size,
                .groups_x = compile_result.reflection.workgroups_x,
                .groups_y = compile_result.reflection.workgroups_y,
                .groups_z = compile_result.reflection.workgroups_z });
            if (!shader_blob_result.has_value())
            {
                m_logger->error("Failed to create shader blob!");
            }
            m_predefined_shaders[i] = shader_blob_result.value_or(nullptr);
        }
    }
    m_logger->info("Finished loading {} shaders.", shader_metadata.size());
}

Shader_Library::~Shader_Library()
{}

rhi::Shader_Blob* Shader_Library::get_shader(Shaders shader) const
{
    auto shader_idx = std::to_underlying(shader);
    return m_predefined_shaders[shader_idx];
}
}
