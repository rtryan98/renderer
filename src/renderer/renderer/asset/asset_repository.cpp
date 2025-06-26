#include "renderer/asset/asset_repository.hpp"
#include <rhi_dxc_lib/shader_compiler.hpp>
#include <nlohmann/json.hpp>
#include <fstream>

#include "rhi/graphics_device.hpp"

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

Asset_Repository::Asset_Repository(
    std::shared_ptr<Logger> logger, rhi::Graphics_Device* graphics_device,
    Asset_Repository_Paths&& paths)
    : m_logger(std::move(logger))
    , m_graphics_device(graphics_device)
    , m_paths(std::move(paths))
    , m_shader_compiler(std::make_unique<Shader_Compiler>())
{
    std::vector<std::wstring> shader_include_dirs;
    shader_include_dirs.emplace_back(m_paths.shaders.begin(), m_paths.shaders.end());
    shader_include_dirs.emplace_back(shader_include_dirs[0] + L"/../shared/");
    shader_include_dirs.emplace_back(shader_include_dirs[0] + L"/../../thirdparty/rhi/src/shaders/");
    ankerl::unordered_dense::set<std::string> shader_set;
    for (const auto& shader_path : std::filesystem::recursive_directory_iterator(std::filesystem::path(m_paths.shaders)))
    {
        const auto& path = shader_path.path();
        auto extension = path.extension();
        if (extension == ".hlsl" || extension == ".json")
        {
            auto full_path = (path.parent_path() / path.stem()).string();
            shader_set.insert(full_path);
        }
    }
    for (const auto& shader : shader_set)
    {
        m_logger->debug("Processing shader {}", shader);
        auto hlsl_path = std::string(shader) + ".hlsl";
        auto json_path = std::string(shader) + ".json";

        if (!std::filesystem::exists(hlsl_path) || !std::filesystem::exists(json_path))
        {
            continue;
        }
        compile_shader_library(hlsl_path, json_path, shader_include_dirs);
    }
}

Asset_Repository::~Asset_Repository()
{}

Compute_Pipeline Asset_Repository::get_compute_pipeline(const std::string_view& name) const
{
    return Compute_Pipeline(m_compute_library_ptrs.at(std::string(name)));
}

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

std::wstring shader_model_from_shader_type(Shader_Type type)
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

std::vector<uint8_t> load_file_binary_unsafe(const char* path)
{
    std::vector<uint8_t> result;
    std::ifstream file(path, std::ios::binary);
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

void Asset_Repository::compile_shader_library(
    std::string_view hlsl_path,
    std::string_view json_path,
    const std::vector<std::wstring>& include_dirs)
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
    auto shader_type_string = shader_json["shader_type"].get<std::string>();
    auto shader_type = shader_type_from_string(shader_type_string);
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

            m_logger->info("Created shader variant: '{}'", permutation_name);
        }
    }

    std::vector<Named_Shader> named_shaders;
    // Compile all shader permutations
    {
        auto file = load_file_binary_unsafe(std::string(hlsl_path).c_str());
        rhi::dxc::Shader_Compile_Info compile_info = {
            .data = file.data(),
            .data_size = file.size(),
            .entrypoint = std::wstring(entry_point.begin(), entry_point.end()),
            .shader_model = shader_model_from_shader_type(shader_type)
        };
        rhi::dxc::Shader_Compiler_Settings settings = {
            .include_dirs = include_dirs
        };

        if (define_lists.empty())
        {
            define_lists.emplace_back( name, std::vector<std::wstring>{} );
        }
        auto is_dx12 = m_graphics_device->get_graphics_api() == rhi::Graphics_API::D3D12;
        for (auto& [name, define_list] : define_lists)
        {
            m_logger->info("Compiling shader: '{}'", name);
            // compile using defines
            settings.defines = std::move(define_list);
            auto shader = m_shader_compiler->compile_from_memory(settings, compile_info);
            rhi::Shader_Blob_Create_Info create_info = {
                .data = is_dx12 ? shader.dxil.data() : shader.spirv.data(),
                .data_size = is_dx12 ? shader.dxil.size() : shader.spirv.size(),
                .groups_x = shader.reflection.workgroups_x,
                .groups_y = shader.reflection.workgroups_y,
                .groups_z = shader.reflection.workgroups_z
            };
            named_shaders.emplace_back( name, m_graphics_device->create_shader_blob(create_info).value_or(nullptr) );
        }
    }

    auto shader_library_lookup_name = name + "." + shader_type_string;
    if (!m_shader_library_ptrs.contains(shader_library_lookup_name))
    {
        m_shader_library_ptrs.insert(std::make_pair(shader_library_lookup_name, m_shader_libraries.acquire()));
    }
    auto* shader_library = m_shader_library_ptrs[shader_library_lookup_name];
    shader_library->shaders = std::move(named_shaders);
    m_logger->info("Successfully created shader library '{}'", shader_library_lookup_name);

    if (shader_type == Shader_Type::Compute)
    {
        m_logger->debug("Creating or updating associated compute library.");
        if (!m_compute_library_ptrs.contains(name))
        {
            m_compute_library_ptrs.insert(std::make_pair(name, m_compute_libraries.acquire()));
        }
        auto* compute_library = m_compute_library_ptrs[name];
        shader_library->referenced_compute_library = compute_library;
        compute_library->create_pipelines(m_graphics_device, shader_library);
        m_logger->info("Successfully created compute library '{}'", name);
    }
}
}
