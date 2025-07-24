#include "renderer/asset/asset_repository.hpp"
#include <rhi_dxc_lib/shader_compiler.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <shared/serialized_asset_formats.hpp>
#include "renderer/application.hpp"
#include "renderer/filesystem/mapped_file.hpp"

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
    Asset_Repository_Paths&& paths, Application& app)
    : m_logger(std::move(logger))
    , m_graphics_device(graphics_device)
    , m_paths(std::move(paths))
    , m_app(app)
    , m_shader_compiler(std::make_unique<Shader_Compiler>())
{
    m_logger->info("Asset repository created with the following asset paths:");
    m_logger->info("Shaders: '{}'", m_paths.shaders);
    m_logger->info("Pipelines: '{}'", m_paths.pipelines);
    m_logger->info("Asset repository uses the following include dirs for shader compilation:");
    for (auto& path : m_paths.shader_include_paths)
    {
        m_logger->info("Include path: '{}'", path);
    }

    create_shader_and_compute_libraries();
    create_graphics_pipeline_libraries();
    register_textures();
    register_models();
}

Asset_Repository::~Asset_Repository()
{
    // for (auto& model : m_models)
    // {
    //     m_graphics_device->destroy_buffer(model.indices);
    //     m_graphics_device->destroy_buffer(model.vertex_positions);
    //     m_graphics_device->destroy_buffer(model.vertex_attributes);
    // }
    for (auto& file : m_files)
    {
        file.unmap();
    }
}

rhi::Shader_Blob* Asset_Repository::get_shader_blob(const std::string_view& name, const std::string_view& variant) const
{
    if (!m_shader_library_ptrs.contains(std::string(name)))
    {
        m_logger->error("Asset repository does not contain shader blob '{}'", name);
        return nullptr;
    }
    return m_shader_library_ptrs.at(std::string(name))->get_shader(variant);
}

rhi::Shader_Blob* Asset_Repository::get_shader_blob(const std::string_view& name) const
{
    if (!m_shader_library_ptrs.contains(std::string(name)))
    {
        m_logger->error("Asset repository does not contain shader blob '{}'", name);
        return nullptr;
    }
    return m_shader_library_ptrs.at(std::string(name))->shaders.at(0).blob;
}

Compute_Pipeline Asset_Repository::get_compute_pipeline(const std::string_view& name) const
{
    return Compute_Pipeline(m_compute_library_ptrs.at(std::string(name)));
}

Graphics_Pipeline Asset_Repository::get_graphics_pipeline(const std::string_view& name) const
{
    return Graphics_Pipeline(m_pipeline_library_ptrs.at(std::string(name)));
}

Mapped_File* Asset_Repository::get_model(const std::string_view& name) const
{
    return m_model_ptrs.at(std::string(name));
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
    shader_library->hlsl_path = hlsl_path;
    shader_library->json_path = json_path;
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

rhi::Image_Format image_format_from_string(std::string_view str)
{
    constexpr static auto IMAGE_FORMATS =
        std::to_array<std::pair<const std::string_view, const rhi::Image_Format>>(
        {
            {"Undefined", rhi::Image_Format::Undefined},
            {"R8_UNORM", rhi::Image_Format::R8_UNORM},
            {"R8_SNORM",rhi::Image_Format::R8_SNORM},
            {"R8_UINT", rhi::Image_Format::R8_UINT},
            {"R8_SINT", rhi::Image_Format::R8_SINT},
            {"R8G8_UNORM", rhi::Image_Format::R8G8_UNORM},
            {"R8G8_SNORM", rhi::Image_Format::R8G8_SNORM},
            {"R8G8_UINT", rhi::Image_Format::R8G8_UINT},
            {"R8G8_SINT", rhi::Image_Format::R8G8_SINT},
            {"R8G8B8A8_UNORM", rhi::Image_Format::R8G8B8A8_UNORM},
            {"R8G8B8A8_SNORM", rhi::Image_Format::R8G8B8A8_SNORM},
            {"R8G8B8A8_UINT", rhi::Image_Format::R8G8B8A8_UINT},
            {"R8G8B8A8_SINT", rhi::Image_Format::R8G8B8A8_SINT},
            {"R8G8B8A8_SRGB", rhi::Image_Format::R8G8B8A8_SRGB},
            {"B8G8R8A8_UNORM", rhi::Image_Format::B8G8R8A8_UNORM},
            {"B8G8R8A8_SNORM", rhi::Image_Format::B8G8R8A8_SNORM},
            {"B8G8R8A8_UINT", rhi::Image_Format::B8G8R8A8_UINT},
            {"B8G8R8A8_SINT", rhi::Image_Format::B8G8R8A8_SINT},
            {"B8G8R8A8_SRGB", rhi::Image_Format::B8G8R8A8_SRGB},
            {"A2R10G10B10_UNORM_PACK32", rhi::Image_Format::A2R10G10B10_UNORM_PACK32},
            {"R16_UNORM", rhi::Image_Format::R16_UNORM},
            {"R16_SNORM", rhi::Image_Format::R16_SNORM},
            {"R16_UINT", rhi::Image_Format::R16_UINT},
            {"R16_SINT", rhi::Image_Format::R16_SINT},
            {"R16_SFLOAT", rhi::Image_Format::R16_SFLOAT},
            {"R16G16_UNORM", rhi::Image_Format::R16G16_UNORM},
            {"R16G16_SNORM", rhi::Image_Format::R16G16_SNORM},
            {"R16G16_UINT", rhi::Image_Format::R16G16_UINT},
            {"R16G16_SINT", rhi::Image_Format::R16G16_SINT},
            {"R16G16_SFLOAT", rhi::Image_Format::R16G16_SFLOAT},
            {"R16G16B16A16_UNORM", rhi::Image_Format::R16G16B16A16_UNORM},
            {"R16G16B16A16_SNORM", rhi::Image_Format::R16G16B16A16_SNORM},
            {"R16G16B16A16_UINT", rhi::Image_Format::R16G16B16A16_UINT},
            {"R16G16B16A16_SINT", rhi::Image_Format::R16G16B16A16_SINT},
            {"R16G16B16A16_SFLOAT", rhi::Image_Format::R16G16B16A16_SFLOAT},
            {"R32_UINT", rhi::Image_Format::R32_UINT},
            {"R32_SINT", rhi::Image_Format::R32_SINT},
            {"R32_SFLOAT", rhi::Image_Format::R32_SFLOAT},
            {"R32G32_UINT", rhi::Image_Format::R32G32_UINT},
            {"R32G32_SINT", rhi::Image_Format::R32G32_SINT},
            {"R32G32_SFLOAT", rhi::Image_Format::R32G32_SFLOAT},
            {"R32G32B32A32_UINT", rhi::Image_Format::R32G32B32A32_UINT},
            {"R32G32B32A32_SINT", rhi::Image_Format::R32G32B32A32_SINT},
            {"R32G32B32A32_SFLOAT", rhi::Image_Format::R32G32B32A32_SFLOAT},
            {"B10G11R11_UFLOAT_PACK32", rhi::Image_Format::B10G11R11_UFLOAT_PACK32},
            {"E5B9G9R9_UFLOAT_PACK32", rhi::Image_Format::E5B9G9R9_UFLOAT_PACK32},
            {"D16_UNORM", rhi::Image_Format::D16_UNORM},
            {"D32_SFLOAT", rhi::Image_Format::D32_SFLOAT},
            {"D24_UNORM_S8_UINT", rhi::Image_Format::D24_UNORM_S8_UINT},
            {"D32_SFLOAT_S8_UINT", rhi::Image_Format::D32_SFLOAT_S8_UINT},
            {"BC1_RGB_UNORM_BLOCK", rhi::Image_Format::BC1_RGB_UNORM_BLOCK},
            {"BC1_RGB_SRGB_BLOCK", rhi::Image_Format::BC1_RGB_SRGB_BLOCK},
            {"BC1_RGBA_UNORM_BLOCK", rhi::Image_Format::BC1_RGBA_UNORM_BLOCK},
            {"BC1_RGBA_SRGB_BLOCK", rhi::Image_Format::BC1_RGBA_SRGB_BLOCK},
            {"BC2_UNORM_BLOCK", rhi::Image_Format::BC2_UNORM_BLOCK},
            {"BC2_SRGB_BLOCK", rhi::Image_Format::BC2_SRGB_BLOCK},
            {"BC3_UNORM_BLOCK", rhi::Image_Format::BC3_UNORM_BLOCK},
            {"BC3_SRGB_BLOCK", rhi::Image_Format::BC3_SRGB_BLOCK},
            {"BC4_UNORM_BLOCK", rhi::Image_Format::BC4_UNORM_BLOCK},
            {"BC4_SNORM_BLOCK", rhi::Image_Format::BC4_SNORM_BLOCK},
            {"BC5_UNORM_BLOCK", rhi::Image_Format::BC5_UNORM_BLOCK},
            {"BC5_SNORM_BLOCK", rhi::Image_Format::BC5_SNORM_BLOCK},
            {"BC6H_UFLOAT_BLOCK", rhi::Image_Format::BC6H_UFLOAT_BLOCK},
            {"BC6H_SFLOAT_BLOCK", rhi::Image_Format::BC6H_SFLOAT_BLOCK},
            {"BC7_UNORM_BLOCK", rhi::Image_Format::BC7_UNORM_BLOCK},
            {"BC7_SRGB_BLOCK", rhi::Image_Format::BC7_SRGB_BLOCK}
        });
        for (const auto& [string_val, enum_val] : IMAGE_FORMATS)
        {
            if (string_val == str)
                return enum_val;
        }
        return rhi::Image_Format::Undefined;
}

rhi::Blend_Factor blend_factor_from_string(std::string_view str)
{
    constexpr static auto BLEND_FACTORS =
        std::to_array<std::pair<const std::string_view, const rhi::Blend_Factor>>(
        {
            {"Zero", rhi::Blend_Factor::Zero},
            {"One", rhi::Blend_Factor::One},
            {"Src_Color", rhi::Blend_Factor::Src_Color},
            {"One_Minus_Src_Color", rhi::Blend_Factor::One_Minus_Src_Color},
            {"Dst_Color", rhi::Blend_Factor::Dst_Color},
            {"One_Minus_Dst_Color", rhi::Blend_Factor::One_Minus_Dst_Color},
            {"Src_Alpha", rhi::Blend_Factor::Src_Alpha},
            {"One_Minus_Src_Alpha", rhi::Blend_Factor::One_Minus_Src_Alpha},
            {"Dst_Alpha", rhi::Blend_Factor::Dst_Alpha},
            {"One_Minus_Dst_Alpha", rhi::Blend_Factor::One_Minus_Dst_Alpha},
            {"Constant_Color", rhi::Blend_Factor::Constant_Color},
            {"One_Minus_Constant_Color", rhi::Blend_Factor::One_Minus_Constant_Color},
            {"Constant_Alpha", rhi::Blend_Factor::Constant_Alpha},
            {"One_Minus_Constant_Alpha", rhi::Blend_Factor::One_Minus_Constant_Alpha},
            {"Src1_Color", rhi::Blend_Factor::Src1_Color},
            {"One_Minus_Src1_Color", rhi::Blend_Factor::One_Minus_Src1_Color},
            {"Src1_Alpha", rhi::Blend_Factor::Src1_Alpha},
            {"One_Minus_Src1_Alpha", rhi::Blend_Factor::One_Minus_Src1_Alpha}
        });
    for (const auto& [string_val, enum_val] : BLEND_FACTORS)
    {
        if (string_val == str)
            return enum_val;
    }
    return rhi::Blend_Factor::Zero;
}

rhi::Blend_Op blend_op_from_string(std::string_view str)
{
    constexpr static auto BLEND_OPS =
        std::to_array<std::pair<const std::string_view, const rhi::Blend_Op>>(
        {
            {"Add", rhi::Blend_Op::Add},
            {"Sub", rhi::Blend_Op::Sub},
            {"Reverse_Sub", rhi::Blend_Op::Reverse_Sub},
            {"Min", rhi::Blend_Op::Min},
            {"Max", rhi::Blend_Op::Max},
        });
    for (const auto& [string_val, enum_val] : BLEND_OPS)
    {
        if (string_val == str)
            return enum_val;
    }
    return rhi::Blend_Op::Add;
}

rhi::Logic_Op logic_op_from_string(std::string_view str)
{
    constexpr static auto LOGIC_OPS =
        std::to_array<std::pair<const std::string_view, const rhi::Logic_Op>>(
        {
            {"Clear", rhi::Logic_Op::Clear},
            {"Set", rhi::Logic_Op::Set},
            {"Copy", rhi::Logic_Op::Copy},
            {"Copy_Inverted", rhi::Logic_Op::Copy_Inverted},
            {"Noop", rhi::Logic_Op::Noop},
            {"Invert", rhi::Logic_Op::Invert},
            {"AND", rhi::Logic_Op::AND},
            {"NAND", rhi::Logic_Op::NAND},
            {"OR", rhi::Logic_Op::OR},
            {"NOR", rhi::Logic_Op::NOR},
            {"XOR", rhi::Logic_Op::XOR},
            {"Equiv", rhi::Logic_Op::Equiv},
            {"AND_Reverse", rhi::Logic_Op::AND_Reverse},
            {"AND_Inverted", rhi::Logic_Op::AND_Inverted},
            {"OR_Reverse", rhi::Logic_Op::OR_Reverse},
            {"OR_Inverted", rhi::Logic_Op::OR_Inverted},
        });
    for (const auto& [string_val, enum_val] : LOGIC_OPS)
    {
        if (string_val == str)
            return enum_val;
    }
    return rhi::Logic_Op::Clear;
}

rhi::Color_Component color_component_from_string(std::string_view str)
{
    rhi::Color_Component components{0};
    if (str.contains('R') || str.contains('r') ) components = components | rhi::Color_Component::R_Bit;
    if (str.contains('G') || str.contains('g') ) components = components | rhi::Color_Component::G_Bit;
    if (str.contains('B') || str.contains('b') ) components = components | rhi::Color_Component::B_Bit;
    if (str.contains('A') || str.contains('a') ) components = components | rhi::Color_Component::A_Bit;
    return components;
}

rhi::Comparison_Func comparison_func_from_string(const std::string_view str)
{
    constexpr static auto COMPARISON_FUNCS =
        std::to_array<std::pair<const std::string_view, const rhi::Comparison_Func>>(
        {
            {"None", rhi::Comparison_Func::None},
            {"Never", rhi::Comparison_Func::Never},
            {"Less", rhi::Comparison_Func::Less},
            {"Equal", rhi::Comparison_Func::Equal},
            {"Less_Equal", rhi::Comparison_Func::Less_Equal},
            {"Greater", rhi::Comparison_Func::Greater},
            {"Not_Equal", rhi::Comparison_Func::Not_Equal},
            {"Greater_Equal", rhi::Comparison_Func::Greater_Equal},
            {"Always", rhi::Comparison_Func::Always},
        });
    for (const auto& [string_val, enum_val] : COMPARISON_FUNCS)
    {
        if (string_val == str)
            return enum_val;
    }
    return rhi::Comparison_Func::None;
}

rhi::Stencil_Op stencil_op_from_string(const std::string_view str)
{
    constexpr static auto STENCIL_OPS =
        std::to_array<std::pair<const std::string_view, const rhi::Stencil_Op>>(
        {
            {"Keep", rhi::Stencil_Op::Keep},
            {"Zero", rhi::Stencil_Op::Zero},
            {"Replace", rhi::Stencil_Op::Replace},
            {"Incr_Sat", rhi::Stencil_Op::Incr_Sat},
            {"Decr_Sat", rhi::Stencil_Op::Decr_Sat},
            {"Invert", rhi::Stencil_Op::Invert},
            {"Incr", rhi::Stencil_Op::Incr},
            {"Decr", rhi::Stencil_Op::Decr},
        });
    for (const auto& [string_val, enum_val] : STENCIL_OPS)
    {
        if (string_val == str)
            return enum_val;
    }
    return rhi::Stencil_Op::Keep;
}

rhi::Depth_Bounds_Test_Mode depth_bounds_test_mode_from_string(const std::string_view str)
{
    constexpr static auto DEPTH_BOUNDS_TEST_MODES =
        std::to_array<std::pair<const std::string_view, const rhi::Depth_Bounds_Test_Mode>>(
        {
            {"Disabled", rhi::Depth_Bounds_Test_Mode::Disabled},
            {"Static", rhi::Depth_Bounds_Test_Mode::Static},
            {"Dynamic", rhi::Depth_Bounds_Test_Mode::Dynamic},
        });
    for (const auto& [string_val, enum_val] : DEPTH_BOUNDS_TEST_MODES)
    {
        if (string_val == str)
            return enum_val;
    }
    return rhi::Depth_Bounds_Test_Mode::Disabled;
}

rhi::Cull_Mode cull_mode_from_string(const std::string_view str)
{
    constexpr static auto CULL_MODES =
        std::to_array<std::pair<const std::string_view, const rhi::Cull_Mode>>(
        {
            {"None", rhi::Cull_Mode::None},
            {"Front", rhi::Cull_Mode::Front},
            {"Back", rhi::Cull_Mode::Back},
        });
    for (const auto& [string_val, enum_val] : CULL_MODES)
    {
        if (string_val == str)
            return enum_val;
    }
    return rhi::Cull_Mode::None;
}

rhi::Primitive_Topology_Type primitive_topology_from_string(const std::string_view str)
{
    constexpr static auto PRIMITIVE_TOPOLOGY_TYPES =
        std::to_array<std::pair<const std::string_view, const rhi::Primitive_Topology_Type>>(
        {
            {"Point", rhi::Primitive_Topology_Type::Point},
            {"Line", rhi::Primitive_Topology_Type::Line},
            {"Triangle", rhi::Primitive_Topology_Type::Triangle},
            {"Patch", rhi::Primitive_Topology_Type::Patch},
        });
    for (const auto& [string_val, enum_val] : PRIMITIVE_TOPOLOGY_TYPES)
    {
        if (string_val == str)
            return enum_val;
    }
    return rhi::Primitive_Topology_Type::Triangle;
}

void Asset_Repository::compile_graphics_pipeline_library(const std::string_view& json_path)
{
    // parse the json file
    auto pipeline_json = nlohmann::json::parse(std::ifstream(std::string(json_path)));

    // TODO: add permutations?
    bool is_mesh_shading = false;

    rhi::Shader_Blob* ts;
    rhi::Shader_Blob* ms;
    rhi::Shader_Blob* vs;
    rhi::Shader_Blob* ps;
    rhi::Pipeline_Blend_State_Info blend_state_info{};
    rhi::Primitive_Topology_Type primitive_topology;
    rhi::Pipeline_Rasterization_State_Info rasterizer_state_info{};
    rhi::Pipeline_Depth_Stencil_State_Info depth_stencil_info{};
    std::array<rhi::Image_Format, rhi::PIPELINE_COLOR_ATTACHMENTS_MAX> color_attachments{};
    uint32_t color_attachment_count = 0;
    rhi::Image_Format depth_stencil_format;

    auto set_shader = [&](auto& shader, const std::string_view type)
    {
        if (pipeline_json.contains(type))
        {
            if (pipeline_json[type].contains("name"))
            {
                const auto name = pipeline_json[type]["name"].get<std::string>();
                auto variant_name = pipeline_json[type].contains("variant")
                    ? pipeline_json[type]["name"]["variant"].get<std::string>()
                    : "";
                if (!m_shader_library_ptrs.contains(name))
                {
                    m_logger->error("Shader library '{}' does not exist.", name);
                }
                const auto shader_lib = m_shader_library_ptrs[name];
                if (!variant_name.empty())
                {
                    shader = shader_lib->get_shader(variant_name);
                }
                else
                {
                    shader = shader_lib->shaders[0].blob;
                    variant_name = shader_lib->shaders[0].name;
                }
                return std::make_pair(m_shader_library_ptrs[name], variant_name);
            }
        }
        return std::make_pair(static_cast<Shader_Library*>(nullptr), std::string());
    };

    auto [ts_lib, ts_variant] = set_shader(ts, "ts");
    auto [ms_lib, ms_variant] = set_shader(ms, "ms");
    auto [vs_lib, vs_variant] = set_shader(vs, "vs");
    auto [ps_lib, ps_variant] = set_shader(ps, "ps");

    blend_state_info.independent_blend_enable = pipeline_json.contains("independent_blend_enable")
            ? pipeline_json["independent_blend_enable"].get<bool>()
            : false;

    if (pipeline_json.contains("color_attachments"))
    {
        for (const auto& color_attachment_json : pipeline_json["color_attachments"])
        {
            auto& ca_blend = blend_state_info.color_attachments[color_attachment_count];
            ca_blend.blend_enable = color_attachment_json.contains("blend_enable")
                ? color_attachment_json["blend_enable"].get<bool>()
                : false;
            ca_blend.logic_op_enable = color_attachment_json.contains("logic_op_enable")
                ? color_attachment_json["logic_op_enable"].get<bool>()
                : false;
            ca_blend.color_src_blend = color_attachment_json.contains("color_src_blend")
                ? blend_factor_from_string(color_attachment_json["color_src_blend"].get<std::string>())
                : rhi::Blend_Factor::Zero;
            ca_blend.color_dst_blend = color_attachment_json.contains("color_dst_blend")
                ? blend_factor_from_string(color_attachment_json["color_dst_blend"].get<std::string>())
                : rhi::Blend_Factor::Zero;
            ca_blend.color_blend_op = color_attachment_json.contains("color_blend_op")
                ? blend_op_from_string(color_attachment_json["color_blend_op"].get<std::string>())
                : rhi::Blend_Op::Add;
            ca_blend.alpha_src_blend = color_attachment_json.contains("alpha_src_blend")
                ? blend_factor_from_string(color_attachment_json["alpha_src_blend"].get<std::string>())
                : rhi::Blend_Factor::Zero;
            ca_blend.alpha_dst_blend = color_attachment_json.contains("alpha_dst_blend")
                ? blend_factor_from_string(color_attachment_json["alpha_dst_blend"].get<std::string>())
                : rhi::Blend_Factor::Zero;
            ca_blend.alpha_blend_op = color_attachment_json.contains("alpha_blend_op")
                ? blend_op_from_string(color_attachment_json["alpha_blend_op"].get<std::string>())
                : rhi::Blend_Op::Add;
            ca_blend.logic_op = color_attachment_json.contains("logic_op")
                ? logic_op_from_string(color_attachment_json["logic_op"].get<std::string>())
                : rhi::Logic_Op::Clear;
            ca_blend.color_write_mask = color_attachment_json.contains("color_write_mask")
                ? color_component_from_string(color_attachment_json["color_write_mask"].get<std::string>())
                : rhi::Color_Component::Enable_All;

            auto& ca_format = color_attachments[color_attachment_count];
            ca_format = color_attachment_json.contains("format")
                ? image_format_from_string(color_attachment_json["format"].get<std::string>())
                : rhi::Image_Format::Undefined;

            color_attachment_count += 1;
        }
    }

    if (pipeline_json.contains("depth_stencil"))
    {
        auto& depth_stencil_json = pipeline_json["depth_stencil"];

        depth_stencil_format = depth_stencil_json.contains("format")
            ? image_format_from_string(depth_stencil_json["format"].get<std::string>())
            : rhi::Image_Format::Undefined;

        depth_stencil_info.depth_enable = depth_stencil_json.contains("depth_enable")
            ? depth_stencil_json["depth_enable"].get<bool>()
            : false;
        depth_stencil_info.depth_write_enable = depth_stencil_json.contains("depth_write_enable")
            ? depth_stencil_json["depth_write_enable"].get<bool>()
            : false;
        depth_stencil_info.comparison_func = depth_stencil_json.contains("comparison_func")
            ? comparison_func_from_string(depth_stencil_json["comparison_func"].get<std::string>())
            : rhi::Comparison_Func::None;
        depth_stencil_info.stencil_enable = depth_stencil_json.contains("stencil_enable")
            ? depth_stencil_json["stencil_enable"].get<bool>()
            : false;

        auto set_stencil_info = [&](auto& stencil_face_info, const std::string_view stencil_info_str)
        {
            if (!depth_stencil_json.contains(stencil_info_str))
                return;
            auto& stencil_json = depth_stencil_json[stencil_info_str];
            stencil_face_info.fail = depth_stencil_json.contains("fail")
                ? stencil_op_from_string(stencil_json["fail"].get<std::string>())
                : rhi::Stencil_Op::Keep;
            stencil_face_info.depth_fail = depth_stencil_json.contains("depth_fail")
                ? stencil_op_from_string(stencil_json["depth_fail"].get<std::string>())
                : rhi::Stencil_Op::Keep;
            stencil_face_info.pass = depth_stencil_json.contains("pass")
                ? stencil_op_from_string(stencil_json["pass"].get<std::string>())
                : rhi::Stencil_Op::Keep;
            stencil_face_info.comparison_func = depth_stencil_json.contains("comparison_func")
                ? comparison_func_from_string(depth_stencil_json["comparison_func"].get<std::string>())
                : rhi::Comparison_Func::None;
            stencil_face_info.stencil_read_mask = depth_stencil_json.contains("stencil_read_mask")
                ? uint8_t(stencil_json["stencil_read_mask"].get<uint32_t>())
                : 0;
            stencil_face_info.stencil_write_mask = depth_stencil_json.contains("stencil_write_mask")
                ? uint8_t(stencil_json["stencil_write_mask"].get<uint32_t>())
                : 0;
        };
        set_stencil_info(depth_stencil_info.stencil_front_face, "stencil_front_face");
        set_stencil_info(depth_stencil_info.stencil_back_face, "stencil_back_face");

        depth_stencil_info.depth_bounds_test_mode = depth_stencil_json.contains("depth_bounds_test_mode")
            ? depth_bounds_test_mode_from_string(depth_stencil_json["depth_bounds_test_mode"].get<std::string>())
            : rhi::Depth_Bounds_Test_Mode::Disabled;
        depth_stencil_info.depth_bounds_min = depth_stencil_json.contains("depth_bounds_min")
            ? depth_stencil_json["depth_bounds_min"].get<float>()
            : 0.f;
        depth_stencil_info.depth_bounds_max = depth_stencil_json.contains("depth_bounds_max")
            ? depth_stencil_json["depth_bounds_max"].get<float>()
            : 0.f;
    }

    if (pipeline_json.contains("rasterizer_state"))
    {
        auto& rasterizer_json = pipeline_json["rasterizer_state"];;
        rasterizer_state_info.fill_mode = rasterizer_json.contains("wireframe")
            ? static_cast<rhi::Fill_Mode>(rasterizer_json["wireframe"].get<bool>())
            : rhi::Fill_Mode::Solid;
        rasterizer_state_info.cull_mode = rasterizer_json.contains("cull_mode")
            ? cull_mode_from_string(rasterizer_json["cull_mode"].get<std::string>())
            : rhi::Cull_Mode::None;
        rasterizer_state_info.winding_order = rasterizer_json.contains("front_face_cw")
            ? static_cast<rhi::Winding_Order>(rasterizer_json["front_face_cw"].get<bool>())
            : rhi::Winding_Order::Front_Face_CCW;
        rasterizer_state_info.depth_bias = rasterizer_json.contains("depth_bias")
            ? rasterizer_json["depth_bias"].get<float>()
            : 0.f;
        rasterizer_state_info.depth_bias_clamp = rasterizer_json.contains("depth_bias_clamp")
            ? rasterizer_json["depth_bias_clamp"].get<float>()
            : 0.f;
        rasterizer_state_info.depth_bias_slope_scale = rasterizer_json.contains("depth_bias_slope_scale")
            ? rasterizer_json["depth_bias_slope_scale"].get<float>()
            : 0.f;
        rasterizer_state_info.depth_clip_enable = rasterizer_json.contains("depth_clip_enable")
            ? rasterizer_json["depth_clip_enable"].get<bool>()
            : true;
    }

    primitive_topology = pipeline_json.contains("primitive_topology")
        ? primitive_topology_from_string(pipeline_json["primitive_topology"].get<std::string>())
        : rhi::Primitive_Topology_Type::Triangle;

    rhi::Pipeline* pipeline = nullptr;
    if (is_mesh_shading)
    {
        rhi::Mesh_Shading_Pipeline_Create_Info create_info =
        {
            .ts = ts,
            .ms = ms,
            .ps = ps,
            .blend_state_info = blend_state_info,
            .rasterizer_state_info = rasterizer_state_info,
            .depth_stencil_info = depth_stencil_info,
            .primitive_topology = primitive_topology,
            .color_attachment_count = color_attachment_count,
            .color_attachment_formats = color_attachments,
            .depth_stencil_format = depth_stencil_format
        };
        auto pipeline_result = m_graphics_device->create_pipeline(create_info);
        pipeline = pipeline_result.value_or(nullptr);
        if (!pipeline_result.has_value())
        {
            m_logger->error("Failed to create graphics pipeline '{}'.", std::string(json_path));
            switch (pipeline_result.error())
            {
            case rhi::Result::Error_Out_Of_Memory:
                m_logger->error("Out of memory.");
                break;
            case rhi::Result::Error_Invalid_Parameters:
                m_logger->error("Invalid parameters.");
                break;
            case rhi::Result::Error_No_Resource:
                m_logger->error("No resource.");
                break;
            default:
                break;
            }
        }
    }
    else
    {
        rhi::Graphics_Pipeline_Create_Info create_info =
        {
            .vs = vs,
            .ps = ps,
            .blend_state_info = blend_state_info,
            .rasterizer_state_info = rasterizer_state_info,
            .depth_stencil_info = depth_stencil_info,
            .primitive_topology = primitive_topology,
            .color_attachment_count = color_attachment_count,
            .color_attachment_formats = color_attachments,
            .depth_stencil_format = depth_stencil_format
        };
        auto pipeline_result = m_graphics_device->create_pipeline(create_info);
        pipeline = pipeline_result.value_or(nullptr);
        if (!pipeline_result.has_value())
        {
            m_logger->error("Failed to create graphics pipeline '{}'.", std::string(json_path));
            switch (pipeline_result.error())
            {
            case rhi::Result::Error_Out_Of_Memory:
                m_logger->error("Out of memory.");
                break;
            case rhi::Result::Error_Invalid_Parameters:
                m_logger->error("Invalid parameters.");
                break;
            case rhi::Result::Error_No_Resource:
                m_logger->error("No resource.");
                break;
            default:
                break;
            }
        }
    }

    auto name = pipeline_json["name"].get<std::string>();
    if (!m_pipeline_library_ptrs.contains(name))
    {
        m_pipeline_library_ptrs[name] = m_pipeline_libraries.acquire();
    }

    auto& pipeline_library = *m_pipeline_library_ptrs[name];

    // Bookkeeping
    auto register_pipeline_to_shader_lib = [&pipeline_library](Shader_Library* shader_library)
    {
        if (shader_library)
            shader_library->referenced_pipeline_libraries.push_back(&pipeline_library);
    };

    pipeline_library.pipeline = pipeline;

    pipeline_library.ts = ts_lib;
    pipeline_library.ts_variant = ts_variant;
    register_pipeline_to_shader_lib(ts_lib);
    pipeline_library.ms = ms_lib;
    pipeline_library.ms_variant = ms_variant;
    register_pipeline_to_shader_lib(ms_lib);
    pipeline_library.vs = vs_lib;
    pipeline_library.vs_variant = vs_variant;
    register_pipeline_to_shader_lib(vs_lib);
    pipeline_library.ps = ps_lib;
    pipeline_library.ps_variant = ps_variant;
    register_pipeline_to_shader_lib(ps_lib);
    pipeline_library.blend_state_info = blend_state_info;
    pipeline_library.primitive_topology = primitive_topology;
    pipeline_library.rasterizer_state_info = rasterizer_state_info;
    pipeline_library.depth_stencil_info = depth_stencil_info;
    pipeline_library.color_attachments = color_attachments;
    pipeline_library.color_attachment_count = color_attachment_count;
    pipeline_library.depth_stencil_format = depth_stencil_format;

    m_logger->info("Created graphics pipeline library '{}'", name);
}

void Asset_Repository::create_shader_and_compute_libraries()
{
    std::vector<std::wstring> shader_include_dirs;
    shader_include_dirs.reserve(m_paths.shader_include_paths.size() + 1);
    shader_include_dirs.emplace_back(m_paths.shaders.begin(), m_paths.shaders.end());
    for (auto& shader_include_path : m_paths.shader_include_paths)
    {
        shader_include_dirs.emplace_back(
            shader_include_dirs[0] + std::wstring(shader_include_path.begin(), shader_include_path.end()));
    }
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

void Asset_Repository::create_graphics_pipeline_libraries()
{
    ankerl::unordered_dense::set<std::string> graphics_pipeline_library_set;
    for (const auto& graphics_pipeline_library_path : std::filesystem::recursive_directory_iterator(std::filesystem::path(m_paths.pipelines)))
    {
        const auto& path = graphics_pipeline_library_path.path();
        auto extension = path.extension();
        if (extension == ".json")
        {
            auto full_path = (path.parent_path() / path.filename()).string();
            graphics_pipeline_library_set.insert(full_path);
        }
    }
    for (const auto& graphics_pipeline_library : graphics_pipeline_library_set)
    {
        m_logger->debug("Processing graphics pipeline library '{}'", graphics_pipeline_library);
        compile_graphics_pipeline_library(graphics_pipeline_library);
    }
}

void Asset_Repository::register_textures()
{
    auto directory = std::filesystem::path(m_paths.models);
    for (auto& directory_entry : std::filesystem::recursive_directory_iterator(directory))
    {
        if (directory_entry.path().extension() == serialization::TEXTURE_FILE_EXTENSION)
        {
            m_logger->debug("Registering texture '{}'", directory_entry.path().string());
            register_texture(directory_entry.path());
        }
    }
}

void Asset_Repository::register_texture(const std::filesystem::path& path)
{
    Mapped_File mapped_file = {};
    mapped_file.map(path.string().c_str());
    if (!mapped_file.data)
    {
        m_logger->error("Failed to open file '{}'", path.string());
        return;
    }

    if (auto* file_header = static_cast<serialization::Image_Header*>(mapped_file.data); !file_header->validate())
    {
        m_logger->error("Failed to validate model '{}'", path.string());
        mapped_file.unmap();
        return;
    }

    const auto texture_identifier = path.filename().string();
    if (!m_texture_ptrs.contains(texture_identifier))
    {
        m_texture_ptrs[texture_identifier] = m_files.acquire();
    }
    auto& texture = *m_texture_ptrs.at(texture_identifier);
    texture = mapped_file;
    m_logger->debug("Registered texture '{}'", path.string());
    /*
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        m_logger->error("Failed to open file '{}'", path.string());
        return;
    }
    const std::streamsize stream_size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(stream_size);
    if (file.read(buffer.data(), stream_size))
    {
        auto* header = reinterpret_cast<serialization::Image_Header*>(buffer.data());
        if (!header->validate())
        {
            m_logger->error("Failed to validate image '{}'", path.string());
            file.close();
            return;
        }

        auto* data = reinterpret_cast<serialization::Image_Data_00*>(buffer.data());

        const auto texture_identifier = path.filename().string();
        if (!m_texture_ptrs.contains(texture_identifier))
        {
            m_texture_ptrs[texture_identifier] = m_textures.acquire();
        }
        auto& texture = *m_texture_ptrs.at(texture_identifier);
        rhi::Image_Create_Info create_info = {
            .format = data->format,
            .width = data->mips[0].width,
            .height = data->mips[0].height,
            .depth = 1,
            .array_size = 1,
            .mip_levels = static_cast<uint16_t>(data->mip_count),
            .usage = rhi::Image_Usage::Sampled,
            .primary_view_type = rhi::Image_View_Type::Texture_2D
        };
        texture.image = m_graphics_device->create_image(create_info).value_or(nullptr);
        m_graphics_device->name_resource(texture.image, (std::string("texture:") + data->name).c_str());
        std::array<void*, 14> mip_data_ptrs;
        for (uint32_t i = 0; i < texture.image->mip_levels; ++i)
        {
            mip_data_ptrs[i] = data->get_mip_data(i);
        }
        m_app.upload_image_data_immediate_full(texture.image, mip_data_ptrs.data());
    }
    */
}

void Asset_Repository::register_models()
{
    auto directory = std::filesystem::path(m_paths.models);
    for (auto& directory_entry : std::filesystem::recursive_directory_iterator(directory))
    {
        if (directory_entry.path().extension() == serialization::MODEL_FILE_EXTENSION)
        {
            m_logger->info("Registering model '{}'", directory_entry.path().string());
            register_model(directory_entry.path());
        }
    }
}

void Asset_Repository::register_model(const std::filesystem::path& path)
{
    Mapped_File mapped_file = {};
    mapped_file.map(path.string().c_str());
    if (!mapped_file.data)
    {
        m_logger->error("Failed to open file '{}'", path.string());
        return;
    }

    if (auto* file_header = static_cast<serialization::Model_Header*>(mapped_file.data); !file_header->validate())
    {
        m_logger->error("Failed to validate model '{}'", path.string());
        mapped_file.unmap();
        return;
    }

    const auto model_identifier = path.filename().string();
    if (!m_model_ptrs.contains(model_identifier))
    {
        m_model_ptrs[model_identifier] = m_files.acquire();
    }
    auto& model = *m_model_ptrs.at(model_identifier);
    model = mapped_file;
    m_logger->debug("Registered model '{}'", path.string());

    /*

    auto* header = static_cast<serialization::Model_Header_00*>(mapped_file.data);

    // create buffers
    {
        rhi::Buffer_Create_Info buffer_create_info = {
            .size = header->vertex_position_count * sizeof(std::array<float, 3>),
            .heap = rhi::Memory_Heap_Type::GPU
        };
        model.vertex_positions = m_graphics_device->create_buffer(buffer_create_info).value_or(nullptr);
        m_graphics_device->name_resource(model.vertex_positions, (std::string("gltf:") + model_identifier + ":position").c_str());
        buffer_create_info.size = header->vertex_attribute_count * sizeof(serialization::Vertex_Attributes);
        model.vertex_attributes = m_graphics_device->create_buffer(buffer_create_info).value_or(nullptr);
        m_graphics_device->name_resource(model.vertex_attributes, (std::string("gltf:") + model_identifier + ":attributes").c_str());
        buffer_create_info.size = header->index_count * sizeof(uint32_t);
        model.indices = m_graphics_device->create_buffer(buffer_create_info).value_or(nullptr);
        m_graphics_device->name_resource(model.indices, (std::string("gltf:") + model_identifier + ":indices").c_str());
    }

    auto* referenced_uris = header->get_referenced_uris();
    auto* materials = header->get_materials();
    model.submeshes.reserve(header->submesh_count);
    const auto* submeshes = header->get_submeshes();

    for (auto i = 0; i < header->submesh_count; ++i)
    {
        const auto& submesh_data = submeshes[i];
        model.submeshes.emplace_back( Loadable_Model_Submesh{
            .vertex_position_range = {
                submesh_data.vertex_position_range_start,
                submesh_data.vertex_position_range_end
            },
            .vertex_attribute_range = {
                submesh_data.vertex_attribute_range_start,
                submesh_data.vertex_attribute_range_end
            },
            .index_range = {
                submesh_data.index_range_start,
                submesh_data.index_range_end
            },
            .material_index = submesh_data.material_index
        } );
    }

    model.instances.reserve(header->instance_count);
    const auto* instances = header->get_instances();

    for (auto i = 0; i < header->instance_count; ++i)
    {
        const auto& instance_data = instances[i];
        model.instances.emplace_back( Loadable_Model_Mesh_Instance{
            .submeshes_range_start = instance_data.submeshes_range_start,
            .submeshes_range_end  = instance_data.submeshes_range_end,
            .parent_index = instance_data.parent_index,
            .translation = {
                instance_data.translation[0],
                instance_data.translation[1],
                instance_data.translation[2]
            },
            .rotation = {
                instance_data.rotation[0],
                instance_data.rotation[1],
                instance_data.rotation[2],
                instance_data.rotation[3]
            },
            .scale = {
                instance_data.scale[0],
                instance_data.scale[1],
                instance_data.scale[2]
            }
        } );
    }

    auto* positions = header->get_vertex_positions();
    m_app.upload_buffer_data_immediate(
        model.vertex_positions,
        positions,
        header->vertex_position_count * sizeof(std::array<float, 3>),
        0);

    auto* attributes = header->get_vertex_attributes();
    m_app.upload_buffer_data_immediate(
        model.vertex_attributes,
        attributes,
        header->vertex_attribute_count * sizeof(serialization::Vertex_Attributes),
        0);

    auto* indices = header->get_indices();
    m_app.upload_buffer_data_immediate(
        model.indices,
        indices,
        header->index_count * sizeof(uint32_t),
        0);

    m_logger->info("Successfully loaded model '{}'", header->name);
    mapped_file.unmap();
    */
}
}
