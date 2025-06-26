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

Asset_Repository::Asset_Repository(std::shared_ptr<Logger> logger, const Asset_Repository_Paths& paths)
    : m_logger(std::move(logger))
    , m_paths(paths)
    , m_shader_compiler(std::make_unique<Shader_Compiler>())
{
    // TODO: FOR TESTING ONLY
    {
        auto hlsl_path = m_paths.shaders + "shaders/fft.cs.hlsl";
        auto json_path = m_paths.shaders + "shaders/fft.cs.json";

        m_logger->debug("Current working directory: '{}'", std::filesystem::current_path().string());
        m_logger->debug("hlsl_path '{}'", hlsl_path);
        m_logger->debug("json_path '{}'", json_path);

        compile_shader_library(hlsl_path, json_path);
    }
}

Asset_Repository::~Asset_Repository()
{}

enum class Shader_Type
{
    Vertex,
    Pixel,
    Compute,
    Task,
    Mesh,
    Library,
    Unknown
};

Shader_Type shader_type_from_string(std::string_view type)
{
    if (type == "vs")
        return Shader_Type::Vertex;
    else if (type == "ps")
        return Shader_Type::Pixel;
    else if (type == "cs")
        return Shader_Type::Compute;
    else if (type == "ts")
        return Shader_Type::Task;
    else if (type == "ms")
        return Shader_Type::Mesh;
    else if (type == "lib")
        return Shader_Type::Library;
    return Shader_Type::Unknown;
}

void Asset_Repository::compile_shader_library(std::string_view hlsl_path, std::string_view json_path)
{
    // parse the json file
    auto shader_json = nlohmann::json::parse(std::ifstream(std::string(json_path)));

    if (!shader_json.contains("name"))
    {
        m_logger->warn("Shader metadata '{}' does not contain mandatory 'name' field.", json_path);
        return;
    }
    if (!shader_json.contains("shader_type"))
    {
        m_logger->warn("Shader metadata '{}' does not contain mandatory 'shader_type' field.", json_path);
        return;
    }
    if (!shader_json.contains("entry_point"))
    {
        m_logger->warn("Shader metadata '{}' does not contain mandatory 'entry_point' field.", json_path);
        return;
    }

    m_logger->debug("Parsing shader library '{}'", json_path);

    auto name = shader_json["name"].get<std::string>();
    auto shader_type = shader_type_from_string(shader_json["shader_type"].get<std::string>());
    auto entry_point = shader_json["entry_point"].get<std::string>();

    struct Shader_Permutation_Group
    {
        std::string name;
        std::vector<std::string> define_names;
        std::vector<std::vector<std::string>> define_values;
        bool is_bool;
    };

    std::vector<std::pair<std::string, std::vector<std::wstring>>> define_lists;

    // Process permutations if they exist
    {
        std::vector<Shader_Permutation_Group> permutation_groups;

        if (shader_json.contains("permutation_groups"))
        {
            m_logger->debug("Parsing shader permutations.");

            permutation_groups.reserve(shader_json["permutation_groups"].size());

            for (const auto& permutation_group_json : shader_json["permutation_groups"])
            {
                auto& permutation_group = permutation_groups.emplace_back();
                // TODO: remove "swizzle_define_values" as just removing the name should be enough
                if (permutation_group_json.contains("name") && !permutation_group_json.contains("swizzle_define_values"))
                {
                    permutation_group.name = permutation_group_json["name"].get<std::string>();
                }
                if (permutation_group_json.contains("define_names"))
                {
                    permutation_group.define_names.reserve(permutation_group_json["define_names"].size());
                    for (const auto& define_name_json : permutation_group_json["define_names"])
                    {
                        permutation_group.define_names.push_back(define_name_json.get<std::string>());
                    }
                }
                else if (permutation_group_json.contains("define_name"))
                {
                    permutation_group.define_names.emplace_back(permutation_group_json["define_name"].get<std::string>());
                }
                if (permutation_group_json.contains("type"))
                {
                    if (permutation_group_json["type"].get<std::string>() == "bool")
                    {
                        permutation_group.is_bool = true;
                    }
                }
                if (permutation_group_json.contains("define_values"))
                {
                    permutation_group.define_values.reserve(permutation_group_json["define_values"].size());
                    for (const auto& define_values_list_json : permutation_group_json["define_values"])
                    {
                        auto& define_values = permutation_group.define_values.emplace_back();
                        define_values.reserve(permutation_group_json.size());
                        for (const auto& define_value_json : define_values_list_json)
                        {
                            switch (define_value_json.type())
                            {
                            case nlohmann::detail::value_t::boolean:
                                define_values.push_back(std::to_string(define_value_json.get<bool>()));
                                break;
                            case nlohmann::detail::value_t::number_integer:
                                define_values.push_back(std::to_string(define_value_json.get<int64_t>()));
                                break;
                            case nlohmann::detail::value_t::number_unsigned:
                                define_values.push_back(std::to_string(define_value_json.get<uint64_t>()));
                                break;
                            case nlohmann::detail::value_t::number_float:
                                define_values.push_back(std::to_string(define_value_json.get<double>()));
                                break;
                            default:
                                break;
                            }
                        }
                    }
                }
                else // default assume bool
                {
                    permutation_group.define_values.emplace_back(std::vector<std::string>({"0", "1"}));
                }
            }
        }

        m_logger->debug("Enumerating shader permutations.");
        // index, divisor
        // std::vector<std::tuple<std::size_t, std::size_t>> permutation_iter_indices;
        // permutation_iter_indices.reserve(permutation_groups.size());
        std::vector<std::size_t> permutation_value_indices;
        permutation_value_indices.reserve(permutation_groups.size());
        auto permutation_count = 1ull;
        for (const auto& permutation_group : permutation_groups)
        {
            auto current_size = permutation_group.define_values.at(0).size();
            permutation_count *= current_size;
            permutation_value_indices.emplace_back(0);
        }

        m_logger->debug("Generating shader permutations.");

        for (auto i = 0; i < permutation_count; ++i)
        {
            auto& [permutation_name, defines] = define_lists.emplace_back();
            std::string postfix;

            // Calculate separate counters for every permutation group.
            for (auto j = 0; j < permutation_groups.size(); ++j)
            {
                auto divisor = 1ull;
                for (auto k = 0; k < j; ++k)
                {
                    divisor *= permutation_groups[k].define_values[0].size();
                }
                permutation_value_indices[j] = i / divisor;
            }

            for (auto j = 0; j < permutation_groups.size(); ++j)
            {
                auto& permutation_group = permutation_groups[j];
                // auto& [index, divisor] = permutation_iter_indices[j];
                auto index = permutation_value_indices[j] % permutation_group.define_values[0].size();

                // Add the permutation defines for the given permutation group and index
                for (auto k = 0; k < permutation_group.define_names.size(); ++k)
                {
                    auto& define_name = permutation_group.define_names[k];
                    auto& define_value_list = permutation_group.define_values[k];
                    auto& define_value = define_value_list[index];

                    auto define_str = define_name;
                    define_str.append("=");
                    define_str.append(define_value);
                    defines.emplace_back(define_str.begin(), define_str.end());
                }

                // Construct the permutation nape postfix for the given permutation group
                if (permutation_group.name.empty())
                {
                    postfix.append("_");
                    postfix.append(permutation_group.define_values[0][index]);
                }
                else
                {
                    if (permutation_group.is_bool)
                    {
                        if (index > 0)
                        {
                            postfix.append("_");
                            postfix.append(permutation_group.name);
                        }
                    }
                    else
                    {
                        postfix.append("_");
                        postfix.append(permutation_group.name);
                    }
                }
            }

            permutation_name = name;
            permutation_name.append(postfix);
        }
    }

    rhi::dxc::Shader_Compile_Info compile_info = {};
    rhi::dxc::Shader_Compiler_Settings settings = {};
    if (!define_lists.empty())
    {
        for (auto& [name, define_list] : define_lists)
        {
            // compile using defines
            settings.defines = std::move(define_list);

            // auto shader = m_shader_compiler->compile_from_memory(settings, compile_info);
            m_logger->info("Listed permutation '{}'", name);
        }
    }
    else // no defines specified
    {

    }

    if (!m_shader_library_ptrs.contains(name))
    {
        m_shader_library_ptrs.insert(std::make_pair(name, m_shader_libraries.acquire()));
    }
    auto& shader_library = m_shader_library_ptrs[name];
}
}
