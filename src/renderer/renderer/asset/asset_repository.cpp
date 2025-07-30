#include "renderer/asset/asset_repository.hpp"
#include <rhi_dxc_lib/shader_compiler.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <shared/serialized_asset_formats.hpp>
#include "renderer/application.hpp"
#include "renderer/filesystem/mapped_file.hpp"
#include "renderer/filesystem/file_util.hpp"

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

Mapped_File* Asset_Repository::get_texture(const std::string_view& name) const
{
    return m_texture_ptrs.at(std::string(name));
}

Mapped_File* Asset_Repository::get_texture_safe(const std::string_view& name) const
{
    if (!m_texture_ptrs.contains(std::string(name)))
        return nullptr;
    return m_texture_ptrs.at(std::string(name));
}

std::vector<std::string> Asset_Repository::get_model_files() const
{
    std::vector<std::string> result;
    result.reserve(m_model_ptrs.size());
    for (const auto& key : m_model_ptrs.values() | std::views::keys)
    {
        result.push_back(key);
    }
    return result;
}

rhi::dxc::Shader_Type shader_type_from_string(std::string_view type)
{
    if (type == "vs")
        return rhi::dxc::Shader_Type::Vertex;
    if (type == "ps")
        return rhi::dxc::Shader_Type::Pixel;
    if (type == "cs")
        return rhi::dxc::Shader_Type::Compute;
    if (type == "ts")
        return rhi::dxc::Shader_Type::Task;
    if (type == "ms")
        return rhi::dxc::Shader_Type::Mesh;
    if (type == "rgen")
        return rhi::dxc::Shader_Type::Ray_Gen;
    if (type == "rahit")
        return rhi::dxc::Shader_Type::Ray_Any_Hit;
    if (type == "rchit")
        return rhi::dxc::Shader_Type::Ray_Closest_Hit;
    if (type == "rmiss")
        return rhi::dxc::Shader_Type::Ray_Miss;
    if (type == "rint")
        return rhi::dxc::Shader_Type::Ray_Intersection;
    if (type == "rcall")
        return rhi::dxc::Shader_Type::Ray_Callable;
    std::unreachable();
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
            .matrix_majorness = rhi::dxc::Matrix_Majorness::Column_Major,
            .shader_type = shader_type,
            .version = rhi::dxc::Shader_Version::SM6_8,
            .embed_debug = true
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

    if (shader_type == rhi::dxc::Shader_Type::Compute)
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
                ? rhi::blend_factor_from_string(color_attachment_json["color_src_blend"].get<std::string>())
                : rhi::Blend_Factor::Zero;
            ca_blend.color_dst_blend = color_attachment_json.contains("color_dst_blend")
                ? rhi::blend_factor_from_string(color_attachment_json["color_dst_blend"].get<std::string>())
                : rhi::Blend_Factor::Zero;
            ca_blend.color_blend_op = color_attachment_json.contains("color_blend_op")
                ? rhi::blend_op_from_string(color_attachment_json["color_blend_op"].get<std::string>())
                : rhi::Blend_Op::Add;
            ca_blend.alpha_src_blend = color_attachment_json.contains("alpha_src_blend")
                ? rhi::blend_factor_from_string(color_attachment_json["alpha_src_blend"].get<std::string>())
                : rhi::Blend_Factor::Zero;
            ca_blend.alpha_dst_blend = color_attachment_json.contains("alpha_dst_blend")
                ? rhi::blend_factor_from_string(color_attachment_json["alpha_dst_blend"].get<std::string>())
                : rhi::Blend_Factor::Zero;
            ca_blend.alpha_blend_op = color_attachment_json.contains("alpha_blend_op")
                ? rhi::blend_op_from_string(color_attachment_json["alpha_blend_op"].get<std::string>())
                : rhi::Blend_Op::Add;
            ca_blend.logic_op = color_attachment_json.contains("logic_op")
                ? rhi::logic_op_from_string(color_attachment_json["logic_op"].get<std::string>())
                : rhi::Logic_Op::Clear;
            ca_blend.color_write_mask = color_attachment_json.contains("color_write_mask")
                ? rhi::color_component_from_string(color_attachment_json["color_write_mask"].get<std::string>())
                : rhi::Color_Component::Enable_All;

            auto& ca_format = color_attachments[color_attachment_count];
            ca_format = color_attachment_json.contains("format")
                ? rhi::get_image_format_info(color_attachment_json["format"].get<std::string>()).format
                : rhi::Image_Format::Undefined;

            color_attachment_count += 1;
        }
    }

    if (pipeline_json.contains("depth_stencil"))
    {
        auto& depth_stencil_json = pipeline_json["depth_stencil"];

        depth_stencil_format = depth_stencil_json.contains("format")
            ? rhi::get_image_format_info(depth_stencil_json["format"].get<std::string>()).format
            : rhi::Image_Format::Undefined;

        depth_stencil_info.depth_enable = depth_stencil_json.contains("depth_enable")
            ? depth_stencil_json["depth_enable"].get<bool>()
            : false;
        depth_stencil_info.depth_write_enable = depth_stencil_json.contains("depth_write_enable")
            ? depth_stencil_json["depth_write_enable"].get<bool>()
            : false;
        depth_stencil_info.comparison_func = depth_stencil_json.contains("comparison_func")
            ? rhi::comparison_func_from_string(depth_stencil_json["comparison_func"].get<std::string>())
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
                ? rhi::stencil_op_from_string(stencil_json["fail"].get<std::string>())
                : rhi::Stencil_Op::Keep;
            stencil_face_info.depth_fail = depth_stencil_json.contains("depth_fail")
                ? rhi::stencil_op_from_string(stencil_json["depth_fail"].get<std::string>())
                : rhi::Stencil_Op::Keep;
            stencil_face_info.pass = depth_stencil_json.contains("pass")
                ? rhi::stencil_op_from_string(stencil_json["pass"].get<std::string>())
                : rhi::Stencil_Op::Keep;
            stencil_face_info.comparison_func = depth_stencil_json.contains("comparison_func")
                ? rhi::comparison_func_from_string(depth_stencil_json["comparison_func"].get<std::string>())
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
            ? rhi::depth_bounds_test_mode_from_string(depth_stencil_json["depth_bounds_test_mode"].get<std::string>())
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
            ? rhi::cull_mode_from_string(rasterizer_json["cull_mode"].get<std::string>())
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
        ? rhi::primitive_topology_from_string(pipeline_json["primitive_topology"].get<std::string>())
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
}
}
