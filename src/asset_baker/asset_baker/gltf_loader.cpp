#include "asset_baker/gltf_loader.hpp"

#include <spdlog/spdlog.h>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <shared/serialized_asset_formats.hpp>
#include <ankerl/unordered_dense.h>
#include <mikktspace.h>
#include <ranges>
#include <stb_image.h>
#include <stb_image_resize2.h>
#include <vector>
#include <xxhash.h>
#include <glm/gtc/quaternion.hpp>

#include "asset_baker/gltf_accessor.hpp"

namespace asset_baker
{
constexpr static auto NO_INDEX = ~0ull;

auto gltf_to_renderer_permutation_matrix()
{
    return glm::mat4(
        -1.0f,  0.0f,  0.0f,  0.0f,
         0.0f,  0.0f,  1.0f,  0.0f,
         0.0f,  1.0f,  0.0f,  0.0f,
         0.0f,  0.0f,  0.0f,  1.0f
    );
}

template<typename T>
auto gltf_to_renderer(const T& t) = delete;

template<>
auto gltf_to_renderer(const glm::vec3& vec3)
{
    return glm::vec3(gltf_to_renderer_permutation_matrix() * glm::vec4(vec3, 0.0f));
}

template<>
auto gltf_to_renderer(const glm::vec4& vec4)
{
    return gltf_to_renderer_permutation_matrix() * vec4;
}

template<>
auto gltf_to_renderer(const glm::quat& gltf_rotation)
{
    auto gltf_angle = glm::angle(gltf_rotation);
    auto gltf_axis = glm::axis(gltf_rotation);

    if (glm::length(gltf_axis) < glm::epsilon<float>())
    {
        return glm::identity<glm::quat>();
    }

    auto renderer_axis = glm::normalize(gltf_to_renderer(gltf_axis));
    auto rotation = glm::angleAxis(gltf_angle, renderer_axis);
    return rotation;
}

template <typename T>
std::string base_16_string(const T& input)
{
    constexpr static char TABLE[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
    };
    std::string result;
    result.reserve(sizeof(T) * 2);
    for (auto i = 0; i < sizeof(T); ++i)
    {
        const char data = reinterpret_cast<const char*>(&input)[i];
        const char lower = (data & 0x0F) >> 0;
        const char upper = (data & 0xF0) >> 4;
        result += TABLE[upper];
        result += TABLE[lower];
    }
    return result;
}

glm::u8vec4 pack_4x8u(float a, float b, float c, float d)
{
    return {
        static_cast<uint8_t>(a * 255.f),
        static_cast<uint8_t>(b * 255.f),
        static_cast<uint8_t>(c * 255.f),
        static_cast<uint8_t>(d * 255.f)
    };
}

void generate_tangents_for_submesh(GLTF_Submesh& submesh, const std::vector<GLTF_Material>& materials)
{
    // Generate tangents with non-zero length but zero-sign.
    submesh.tangents.resize(submesh.normals.size(), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));

    if (submesh.tangents.size() == 0 &&                         // Mesh has no tangents
        submesh.normals.size() > 0 &&                           // and it has normals
        submesh.tex_coords.size() > 0 &&                        // and texture coordinates
        submesh.material_index != NO_INDEX &&                   // and a material
        !materials[submesh.material_index].normal_uri.empty())  // that contains a normal map
    {
        if (!(submesh.normals.size() == submesh.tangents.size() &&
            submesh.tangents.size() == submesh.tex_coords.size()))
        {
            // spdlog::error("GLTF file '{}' has no tangents and they failed to generate.", path.string());
            // return std::unexpected(GLTF_Error::Tangent_Generation_Failed);
        }

        struct Mikkt_Space_User_Data
        {
            std::vector<glm::vec3>& positions;
            std::vector<glm::vec3>& normals;
            std::vector<glm::vec2>& tex_coords;
            std::vector<glm::vec4>& tangents;
            std::vector<uint32_t>& indices;
        } mikkt_space_user_data = {
                .positions = submesh.positions,
                .normals = submesh.normals,
                .tex_coords = submesh.tex_coords,
                .tangents = submesh.tangents,
                .indices = submesh.indices,
            };

        auto get_num_faces = [](
            const SMikkTSpaceContext* mikktspace_context) -> int32_t
        {
            const auto& user_data = *static_cast<Mikkt_Space_User_Data*>(mikktspace_context->m_pUserData);
            return static_cast<int32_t>(user_data.indices.size() / 3);
        };
        auto get_num_vertices_of_face = [](
            const SMikkTSpaceContext* mikktspace_context,
            const int face_idx) -> int32_t
        {
            (void)mikktspace_context;
            (void)face_idx;
            return 3;
        };
        auto get_position = [](
            const SMikkTSpaceContext* mikktspace_context,
            float fv_pos_out[],
            const int face_idx,
            const int vert_idx) -> void
        {
            const auto& user_data = *static_cast<Mikkt_Space_User_Data*>(mikktspace_context->m_pUserData);
            const auto& position = user_data.positions[user_data.indices[face_idx * 3 + vert_idx]];
            fv_pos_out[0] = position[0];
            fv_pos_out[1] = position[1];
            fv_pos_out[2] = position[2];
        };
        auto get_normal = [](
            const SMikkTSpaceContext* mikktspace_context,
            float fv_norm_out[],
            const int face_idx,
            const int vert_idx) -> void
        {
            const auto& user_data = *static_cast<Mikkt_Space_User_Data*>(mikktspace_context->m_pUserData);
            const auto& normal = user_data.normals[user_data.indices[face_idx * 3 + vert_idx]];
            fv_norm_out[0] = normal[0];
            fv_norm_out[1] = normal[1];
            fv_norm_out[2] = normal[2];
        };
        auto get_tex_coord = [](
            const SMikkTSpaceContext* mikktspace_context,
            float fv_tex_coord_out[],
            const int face_idx,
            const int vert_idx) -> void
        {
            const auto& user_data = *static_cast<Mikkt_Space_User_Data*>(mikktspace_context->m_pUserData);
            const auto& tex_coord = user_data.tex_coords[user_data.indices[face_idx * 3 + vert_idx]];
            fv_tex_coord_out[0] = tex_coord[0];
            fv_tex_coord_out[1] = tex_coord[1];
        };
        auto set_tangent_space = [](
            const SMikkTSpaceContext* mikktspace_context,
            const float fv_tangent[],
            const float sign,
            const int face_idx,
            const int vert_idx) -> void
        {
            const auto& data = *static_cast<Mikkt_Space_User_Data*>(mikktspace_context->m_pUserData);
            auto& tangent = data.tangents[data.indices[face_idx * 3 + vert_idx]];
            tangent[0] = fv_tangent[0];
            tangent[1] = fv_tangent[1];
            tangent[2] = fv_tangent[2];
            tangent[3] = sign;
        };

        SMikkTSpaceInterface mikktspace_interface = {
            .m_getNumFaces = get_num_faces,
            .m_getNumVerticesOfFace = get_num_vertices_of_face,
            .m_getPosition = get_position,
            .m_getNormal = get_normal,
            .m_getTexCoord = +get_tex_coord,
            .m_setTSpaceBasic = set_tangent_space,
            .m_setTSpace = nullptr,
        };
        SMikkTSpaceContext mikktspace_context = {
            .m_pInterface = &mikktspace_interface,
            .m_pUserData = &mikkt_space_user_data
        };
        genTangSpaceDefault(&mikktspace_context);
    }
}

std::expected<GLTF_Model, GLTF_Error> process_gltf_from_file(const std::filesystem::path& path)
{
    using fastgltf::Extensions;
    constexpr auto extensions = Extensions::KHR_materials_emissive_strength;
    auto parser = fastgltf::Parser(extensions);

    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None)
    {
        return std::unexpected(GLTF_Error::File_Load_Failed);
    }

    constexpr auto options = fastgltf::Options::LoadExternalBuffers
                           | fastgltf::Options::LoadExternalImages
                           | fastgltf::Options::GenerateMeshIndices
                           | fastgltf::Options::DecomposeNodeMatrices
                           | fastgltf::Options::DontRequireValidAssetMember;
    auto asset = parser.loadGltf(data.get(), path.parent_path(), options);
    if (asset.error() != fastgltf::Error::None)
    {
        switch (asset.error())
        {
        case fastgltf::Error::MissingExtensions:
            [[fallthrough]];
        case fastgltf::Error::UnknownRequiredExtension:
            return std::unexpected(GLTF_Error::Unsupported_Extension);
        default:
            return std::unexpected(GLTF_Error::Parse_Failed);
        }
    }

    GLTF_Model result = {};

    result.materials.reserve(asset->materials.size());
    for (const auto& material : asset->materials)
    {
        const auto get_uri = [&]<typename T>(const fastgltf::Optional<T>& texture_info_opt,
            bool squash,
            rhi::Image_Format target_format) -> std::string
        {
            if (!texture_info_opt.has_value()) return "";
            const auto& texture_info = texture_info_opt.value();
            const auto texture_index = texture_info.textureIndex;
            const auto& texture = asset->textures.at(texture_index);
            const auto image_index = texture.imageIndex.value_or(NO_INDEX);
            if (image_index == NO_INDEX) return "";
            const auto& image = asset->images.at(image_index);

            auto texture_name = path.stem().string() + ":" + std::string(image.name);

            auto request = GLTF_Texture_Load_Request {
                .squash_gb_to_rg = squash,
                .name = texture_name,
                .target_format = target_format,
            };
            request.data = std::visit(fastgltf::visitor{
                [&](auto&)
                {
                    spdlog::warn("GLTF file '{}' has unsupported embedded image (.:unknown).", path.string());
                    return std::vector<char>{};
                },
                [&](const fastgltf::sources::BufferView& buffer_view_ref)
                {
                    const auto& buffer_view = asset->bufferViews.at(buffer_view_ref.bufferViewIndex);
                    const auto& buffer = asset->buffers.at(buffer_view.bufferIndex);
                    return std::visit(fastgltf::visitor{
                        [&](auto&)
                        {
                            spdlog::warn("GLTF file '{}' has unsupported embedded image (BufferView:unknown).", path.string());
                            return std::vector<char>{};
                        },
                        [&](const fastgltf::sources::Array& vector)
                        {
                            std::vector<char> request_image_data;
                            request_image_data.resize(vector.bytes.size());
                            memcpy(request_image_data.data(), vector.bytes.data(), vector.bytes.size());
                            return request_image_data;
                        }
                    }, buffer.data);
                },
                [&](const fastgltf::sources::URI&)
                {
                    spdlog::warn("GLTF file '{}' has unsupported embedded image (.:URI).", path.string());
                    return std::vector<char>{};
                },
                [&](const fastgltf::sources::Array& vector)
                {
                    std::vector<char> request_image_data;
                    request_image_data.resize(vector.bytes.size());
                    memcpy(request_image_data.data(), vector.bytes.data(), vector.bytes.size());
                    return request_image_data;
                }
            }, image.data);

            if (request.data.empty())
            {
                spdlog::debug("GLTF file '{}' - texture '{}' has no data.", path.string(), texture_name);
                return "";
            }

            const auto hash = XXH3_128bits(request.data.data(), request.data.size());
            request.hash_identifier = base_16_string(hash);

            spdlog::trace("Mangled name of texture '{}' to '{}'",
                texture_name,
                std::string(request.hash_identifier, serialization::HASH_IDENTIFIER_FIELD_SIZE));

            result.texture_load_requests.emplace_back(request);
            return request.hash_identifier + serialization::TEXTURE_FILE_EXTENSION;
        };

        result.materials.emplace_back( GLTF_Material {
            .base_color_factor = pack_4x8u(
                material.pbrData.baseColorFactor[0], material.pbrData.baseColorFactor[1],
                material.pbrData.baseColorFactor[2], material.pbrData.baseColorFactor[3]
            ),
            .pbr_roughness = material.pbrData.roughnessFactor,
            .pbr_metallic = material.pbrData.metallicFactor,
            .emissive_color = {{
                material.emissiveFactor[0], material.emissiveFactor[1],
                material.emissiveFactor[2]
            }},
            .emissive_strength = material.emissiveStrength,
            .albedo_uri = get_uri.template operator()<fastgltf::TextureInfo>(
                material.pbrData.baseColorTexture,
                false,
                rhi::Image_Format::R8G8B8A8_SRGB),
            .normal_uri = get_uri.template operator()<fastgltf::NormalTextureInfo>(
                material.normalTexture,
                false,
                rhi::Image_Format::R8G8B8A8_UNORM),
            .metallic_roughness_uri = get_uri.template operator()<fastgltf::TextureInfo>(
                material.pbrData.metallicRoughnessTexture,
                true,
                rhi::Image_Format::R8G8_UNORM),
            .emissive_uri = get_uri.template operator()<fastgltf::TextureInfo>(
                material.emissiveTexture,
                false,
                rhi::Image_Format::R8G8B8A8_SRGB),
            .alpha_mode = std::bit_cast<GLTF_Alpha_Mode>(material.alphaMode),
            .double_sided = material.doubleSided
        });
    }

    result.submeshes.reserve(asset->meshes.size());
    ankerl::unordered_dense::map<fastgltf::Mesh*, std::pair<std::size_t, std::size_t>> submesh_ranges;
    for (auto& gltf_mesh : asset->meshes)
    {
        if (!gltf_mesh.name.empty())
            spdlog::debug("Processing mesh '{}'.", gltf_mesh.name);
        else
            spdlog::debug("Processing unnamed mesh.");

        auto submesh_range_start = result.submeshes.size();

        for (auto& primitive : gltf_mesh.primitives)
        {
            auto& mesh = result.submeshes.emplace_back();

            if (primitive.type != fastgltf::PrimitiveType::Triangles)
            {
                spdlog::error("GLTF file '{}' has unsupported primitive type.", path.string());
                return std::unexpected(GLTF_Error::Non_Supported_Primitive);
            }

            mesh.material_index = primitive.materialIndex.value_or(NO_INDEX);

            get_indices(asset.get(), primitive, mesh.indices);
            get_positions(asset.get(), primitive, mesh.positions);
            get_colors(asset.get(), primitive, mesh.colors);
            get_normals(asset.get(), primitive, mesh.normals);
            get_tangents(asset.get(), primitive, mesh.tangents);
            get_tex_coords(asset.get(), primitive, mesh.tex_coords);
            get_joints(asset.get(), primitive, mesh.joints);
            get_weights(asset.get(), primitive, mesh.weights);

            generate_tangents_for_submesh(mesh, result.materials);

            for (auto& position : mesh.positions)
            {
                position = gltf_to_renderer(position);
            }
            for (auto& normal : mesh.normals)
            {
                normal = glm::normalize(gltf_to_renderer(normal));
            }
            for (auto& tangent : mesh.tangents)
            {
                tangent = glm::vec4(glm::normalize(gltf_to_renderer(glm::vec3(tangent))), tangent.w);
            }
        }

        submesh_ranges[&gltf_mesh] = std::make_pair(submesh_range_start, result.submeshes.size());
    }

    spdlog::debug("Iterating scenes.");
    for (const auto& scene : asset->scenes)
    {
        spdlog::trace("Processing scene '{}'.", scene.name);
        auto process_node_bfs = [&](this const auto& self, const auto relative_node_idx, const auto node_idx, const auto parent_index) -> void
        {
            auto& node = asset->nodes[node_idx];
            spdlog::trace("Processing node '{}'.", node.name);

            if (node.cameraIndex.has_value())
            {
                spdlog::debug("Node is camera node, skipping node and children.");
                return;
            };

            auto mesh_idx = node.meshIndex.value_or(NO_INDEX);
            auto trs = std::get<fastgltf::TRS>(node.transform);
            std::size_t submesh_range_start = 0;
            std::size_t submesh_range_end = 0;

            if (mesh_idx != NO_INDEX)
            {
                auto& range = submesh_ranges[&asset->meshes[mesh_idx]];
                spdlog::trace("Submesh range for mesh index '{}': {} - {}", mesh_idx, range.first, range.second);
                submesh_range_start = range.first;
                submesh_range_end = range.second;
            }

            glm::quat rotation = gltf_to_renderer(glm::quat(trs.rotation[3], trs.rotation[0], trs.rotation[1], trs.rotation[2]));
            glm::vec3 translation = gltf_to_renderer(glm::vec3(trs.translation[0], trs.translation[1], trs.translation[2]));

            result.instances.emplace_back( GLTF_Mesh_Instance {
                .submesh_range_start = submesh_range_start,
                .submesh_range_end = submesh_range_end,
                .parent_index = parent_index,
                .translation = { translation.x, translation.y, translation.z },
                .rotation = { rotation.w, rotation.x, rotation.y, rotation.z },
                .scale = { trs.scale[0], trs.scale[2], trs.scale[1] },
            } );

            spdlog::trace("Iterating children of node {}", node_idx);
            for (const auto child_idx : node.children)
            {
                self(result.instances.size(), child_idx, relative_node_idx);
            }
        };
        for (const auto node_idx : scene.nodeIndices)
        {
            process_node_bfs(result.instances.size(), node_idx, NO_INDEX);
        }
    }

    return result;
}

std::vector<char> process_and_serialize_gltf_texture(const GLTF_Texture_Load_Request& request)
{
    int32_t x = 0, y = 0, comp = 0;
    auto original_data = stbi_load_from_memory(
        reinterpret_cast<const uint8_t*>(request.data.data()),
        static_cast<int>(request.data.size()),
        &x, &y, &comp,
        STBI_rgb_alpha);
    if (!original_data)
    {
        spdlog::error("Failed to load texture.");
        return {};
    }

    const auto max_mips_x = std::countr_zero(static_cast<uint32_t>(x));
    const auto max_mips_y = std::countr_zero(static_cast<uint32_t>(y));
    const auto mip_level_count = std::max(std::min(max_mips_x, max_mips_y) + 1 - 2, 1);

    serialization::Image_Data_00 image_data = {
        .header = {
            .magic = serialization::Image_Header::MAGIC,
            .version = 1,
        },
        .mip_count = static_cast<uint32_t>(mip_level_count),
        .format = request.target_format,
    };
    request.name.copy(image_data.name, std::min(request.name.size(), serialization::NAME_MAX_SIZE));
    request.hash_identifier.copy(image_data.hash_identifier, serialization::HASH_IDENTIFIER_FIELD_SIZE);

    std::vector<uint8_t> squashed_data;
    if (request.squash_gb_to_rg)
    {
        squashed_data.resize(rhi::get_image_format_info(image_data.format).bytes * x * y);
        spdlog::trace("squashing texture");
        for (auto i = 0; i < y; ++i)
        {
            for (auto j = 0; j < x; ++j)
            {
                squashed_data[(i * x + j) * 2 + 0] = original_data[(i * x + j) * 4 + 1];
                squashed_data[(i * x + j) * 2 + 1] = original_data[(i * x + j) * 4 + 2];
            }
        }
    }

    std::vector<std::vector<uint8_t>> mip_image_data;
    uint32_t image_data_size = 0;
    for (auto i = 0; i < mip_level_count; ++i)
    {
        const uint32_t size_x = x >> i;
        const uint32_t size_y = y >> i;

        image_data.mips[i] = {
            .width = size_x,
            .height = size_y,
        };

        spdlog::trace("Generating mip {} with size w:{}, h:{}", i, size_x, size_y);

        auto& mip_data = mip_image_data.emplace_back();

        const auto data_size = size_x * size_y * rhi::get_image_format_info(image_data.format).bytes;
        mip_data.resize(data_size);

        if (i > 0)
        {
            auto& last_mip_data = mip_image_data[i - 1];
            if (request.squash_gb_to_rg)
            {
                stbir_resize_uint8_linear(
                    last_mip_data.data(), size_x << 1, size_y << 1, 0,
                    mip_data.data(), size_x, size_y, 0,
                    STBIR_2CHANNEL);
            }
            else
            {
                stbir_resize_uint8_srgb(
                    last_mip_data.data(), size_x << 1, size_y << 1, 0,
                    mip_data.data(), size_x, size_y, 0,
                    STBIR_RGBA);
            }
        }
        else
        {
            if (request.squash_gb_to_rg)
            {
                mip_data.assign(squashed_data.begin(), squashed_data.end());
            }
            else
            {
                memcpy(mip_data.data(), original_data, data_size);
            }
        }
        image_data_size += data_size;
    }

    stbi_image_free(original_data);

    std::vector<char> result;
    result.resize(
        sizeof(serialization::Image_Data_00)
        + image_data_size);

    spdlog::trace("Saving results. Image data size: {}, Total size: {}", image_data_size, result.size());

    auto ptr = result.data();
    memcpy(ptr, &image_data, sizeof(serialization::Image_Data_00));
    auto* image_data_ptr = reinterpret_cast<serialization::Image_Data_00*>(ptr);
    for (auto i = 0; i < image_data.mip_count; ++i)
    {
        memcpy(image_data_ptr->get_mip_data(i), mip_image_data[i].data(), mip_image_data[i].size());
    }

    return result;
}

std::vector<char> serialize_gltf_model(const std::string& name, GLTF_Model& gltf_model)
{
    spdlog::debug("Serializing GLTF model '{}'.", name);

    serialization::Model_Header_00 serialized_model = {
        .header = {
            .magic = serialization::Model_Header::MAGIC,
            .version = 1,
        }
    };

    // Name
    name.copy(serialized_model.name, std::min(name.length(), serialization::NAME_MAX_SIZE));

    // URI references
    std::vector<serialization::URI_Reference_00> uri_references;
    ankerl::unordered_dense::map<std::string, uint32_t> mapped_uris;
    {
        ankerl::unordered_dense::set<std::string> unique_uris;
        for (const auto& material : gltf_model.materials)
        {
            const auto add_if_not_empty = [&](const std::string& value)
            {
                if (!value.empty()) unique_uris.emplace(value);
            };
            add_if_not_empty(material.albedo_uri);
            add_if_not_empty(material.normal_uri);
            add_if_not_empty(material.metallic_roughness_uri);
            add_if_not_empty(material.emissive_uri);
        }
        {
            uri_references.reserve(unique_uris.size());
            uint32_t uri_count = 0;
            for (const auto& uri : unique_uris)
            {
                mapped_uris.emplace(uri, uri_count);
                auto& [serialized_uri] = uri_references.emplace_back();
                uri.copy(serialized_uri, std::min(uri.size(), serialization::NAME_MAX_SIZE));
                uri_count = uri_count + 1;
            }
        }
    }
    serialized_model.referenced_uri_count = static_cast<uint32_t>(uri_references.size());

    // materials
    std::vector<serialization::Material_00> materials;
    materials.reserve(gltf_model.materials.size());
    for (const auto& material : gltf_model.materials)
    {
        const auto get_uri = [&](const std::string& value)
        {
            if (mapped_uris.contains(value))
                return mapped_uris.at(value);
            return serialization::Material_00::URI_NO_REFERENCE;
        };

        materials.emplace_back( serialization::Material_00 {
            .base_color_factor = {
                material.base_color_factor[0], material.base_color_factor[1],
                material.base_color_factor[2], material.base_color_factor[3]
            },
            .pbr_roughness = material.pbr_roughness,
            .pbr_metallic = material.pbr_metallic,
            .emissive_color = {
                material.emissive_color[0], material.emissive_color[1],
                material.emissive_color[2]
            },
            .emissive_strength = material.emissive_strength,
            .albedo_uri_index = get_uri(material.albedo_uri),
            .normal_uri_index = get_uri(material.normal_uri),
            .metallic_roughness_uri_index = get_uri(material.metallic_roughness_uri),
            .emissive_uri_index = get_uri(material.emissive_uri),
            .alpha_mode = static_cast<serialization::Material_Alpha_Mode>(material.alpha_mode),
            .double_sided = material.double_sided,
        });
    }
    serialized_model.material_count = static_cast<uint32_t>(materials.size());

    // instances
    std::vector<serialization::Mesh_Instance_00> instances;
    instances.reserve(gltf_model.instances.size());
    for (const auto& instance : gltf_model.instances)
    {
        instances.emplace_back( serialization::Mesh_Instance_00 {
            .submeshes_range_start = static_cast<uint32_t>(instance.submesh_range_start),
            .submeshes_range_end = static_cast<uint32_t>(instance.submesh_range_end),
            .parent_index = static_cast<uint32_t>(instance.parent_index),
            .translation = {
                instance.translation[0], instance.translation[1],
                instance.translation[2]
            },
            .rotation = {
                instance.rotation[0], instance.rotation[1],
                instance.rotation[2], instance.rotation[3]
            },
            .scale = {
                instance.scale[0], instance.scale[1],
                instance.scale[2]
            }
        });
    }
    serialized_model.instance_count = static_cast<uint32_t>(instances.size());

    // submeshes and ranges
    std::vector<serialization::Submesh_Data_Ranges_00> mesh_data_ranges;
    std::vector<std::array<float, 3>> mesh_positions;
    std::vector<uint32_t> mesh_indices;
    std::vector<serialization::Vertex_Attributes> mesh_attributes;
    std::vector<serialization::Vertex_Skin_Attributes> mesh_skin_attributes;

    for (const auto& submesh : gltf_model.submeshes)
    {
        auto current_mesh_position_count = mesh_positions.size();
        auto new_mesh_position_count = submesh.positions.size() + current_mesh_position_count;
        auto current_mesh_indices_count = mesh_indices.size();
        auto new_mesh_indices_count = submesh.indices.size() + current_mesh_indices_count;
        auto current_mesh_attributes_count = mesh_attributes.size();
        auto new_mesh_attributes_count = submesh.normals.size() + current_mesh_attributes_count;
        auto current_mesh_skin_attributes_count = mesh_skin_attributes.size();
        auto new_mesh_skin_attributes_count = submesh.weights.size() + current_mesh_skin_attributes_count;

        mesh_positions.reserve(new_mesh_position_count);
        for (auto& position : submesh.positions)
        {
            mesh_positions.emplace_back(std::to_array({ position[0], position[1], position[2] }));
        }

        mesh_indices.reserve(new_mesh_indices_count);
        for (auto& indices : submesh.indices)
        {
            mesh_indices.emplace_back(indices);
        }

        mesh_attributes.reserve(new_mesh_attributes_count);
        for (auto i = 0; i < submesh.positions.size(); ++i)
        {
            auto& attributes = mesh_attributes.emplace_back();
            if (submesh.normals.size() > i)
            {
                attributes.normal = { submesh.normals[i][0], submesh.normals[i][1], submesh.normals[i][2] };
            }
            else
            {
                attributes.normal = {};
            }
            if (submesh.tangents.size() > i)
            {
                attributes.tangent = { submesh.tangents[i][0], submesh.tangents[i][1], submesh.tangents[i][2], submesh.tangents[i][3] };
            }
            else
            {
                attributes.tangent = {};
            }
            if (submesh.tex_coords.size() > i)
            {
                attributes.tex_coords = { submesh.tex_coords[i][0], submesh.tex_coords[i][1] };
            }
            else
            {
                attributes.tex_coords = {};
            }
            if (submesh.colors.size() > i)
            {
                attributes.color = {
                    static_cast<uint8_t>(submesh.colors[i][0] * 255.f),
                    static_cast<uint8_t>(submesh.colors[i][1] * 255.f),
                    static_cast<uint8_t>(submesh.colors[i][2] * 255.f),
                    static_cast<uint8_t>(submesh.colors[i][3] * 255.f)
                };
            }
            else
            {
                attributes.color = { 255, 255, 255, 255 };
            }
        }

        if (submesh.weights.size() > 0 && submesh.joints.size() > 0)
        {
            mesh_skin_attributes.reserve(new_mesh_skin_attributes_count);
            for (auto i = 0; i < submesh.positions.size(); ++i)
            {
                auto& attributes = mesh_skin_attributes.emplace_back();

                if (submesh.joints.size() > i)
                {
                    attributes.joints = { submesh.joints[i][0], submesh.joints[i][1], submesh.joints[i][2], submesh.joints[i][3] };
                }
                else
                {
                    attributes.joints = {};
                }

                if (submesh.weights.size() > i)
                {
                    attributes.weights = { submesh.weights[i][0], submesh.weights[i][1], submesh.weights[i][2], submesh.weights[i][3] };
                }
                else
                {
                    attributes.weights = {};
                }
            }
        }

        mesh_data_ranges.emplace_back( serialization::Submesh_Data_Ranges_00 {
            .material_index = static_cast<uint32_t>(submesh.material_index),
            .vertex_position_range_start = static_cast<uint32_t>(current_mesh_position_count),
            .vertex_position_range_end = static_cast<uint32_t>(new_mesh_position_count),
            .vertex_attribute_range_start = static_cast<uint32_t>(current_mesh_attributes_count),
            .vertex_attribute_range_end = static_cast<uint32_t>(new_mesh_attributes_count),
            .vertex_skin_attribute_range_start = static_cast<uint32_t>(current_mesh_skin_attributes_count),
            .vertex_skin_attribute_range_end = static_cast<uint32_t>(new_mesh_skin_attributes_count),
            .index_range_start = static_cast<uint32_t>(current_mesh_indices_count),
            .index_range_end = static_cast<uint32_t>(new_mesh_indices_count),
        });
    }
    serialized_model.submesh_count = static_cast<uint32_t>(mesh_data_ranges.size());
    serialized_model.vertex_position_count = static_cast<uint32_t>(mesh_positions.size());
    serialized_model.vertex_attribute_count = static_cast<uint32_t>(mesh_attributes.size());
    serialized_model.vertex_skin_attribute_count = static_cast<uint32_t>(mesh_skin_attributes.size());
    serialized_model.index_count = static_cast<uint32_t>(mesh_indices.size());

    std::vector<char> result;
    result.resize(serialized_model.get_size());
    auto data = result.data();
    spdlog::trace("Saving results. Total size: {}", serialized_model.get_size());

    spdlog::trace("Copying header. Offset: {}, Size: {}",
        0, sizeof(serialization::Model_Header_00));
    memcpy(data, &serialized_model, sizeof(serialization::Model_Header_00));

    data = &(result.data()[serialized_model.get_referenced_uris_offset()]);
    spdlog::trace("Copying URIs. Offset: {}, Size: {}",
        serialized_model.get_referenced_uris_offset(), uri_references.size() * sizeof(serialization::URI_Reference_00));
    memcpy(data, uri_references.data(), uri_references.size() * sizeof(serialization::URI_Reference_00));

    data = &(result.data()[serialized_model.get_materials_offset()]);
    spdlog::trace("Copying materials. Offset: {}, Size: {}",
        serialized_model.get_materials_offset(), materials.size() * sizeof(serialization::Material_00));
    memcpy(data, materials.data(), materials.size() * sizeof(serialization::Material_00));

    data = &(result.data()[serialized_model.get_submeshes_offset()]);
    spdlog::trace("Copying submesh data ranges. Offset: {}, Size: {}",
        serialized_model.get_submeshes_offset(), mesh_data_ranges.size() * sizeof(serialization::Submesh_Data_Ranges_00));
    memcpy(data, mesh_data_ranges.data(), mesh_data_ranges.size() * sizeof(serialization::Submesh_Data_Ranges_00));

    data = &(result.data()[serialized_model.get_instances_offset()]);
    spdlog::trace("Copying instances. Offset: {}, Size: {}",
        serialized_model.get_instances_offset(), instances.size() * sizeof(serialization::Mesh_Instance_00));
    memcpy(data, instances.data(), instances.size() * sizeof(serialization::Mesh_Instance_00));

    data = &(result.data()[serialized_model.get_vertex_positions_offset()]);
    spdlog::trace("Copying positions. Offset: {}, Size: {}",
        serialized_model.get_vertex_positions_offset(), mesh_positions.size() * 12);
    memcpy(data, mesh_positions.data(), mesh_positions.size() * 12);

    data = &(result.data()[serialized_model.get_vertex_attributes_offset()]);
    spdlog::trace("Copying attributes. Offset: {}, Size: {}",
        serialized_model.get_vertex_attributes_offset(), mesh_attributes.size() * sizeof(serialization::Vertex_Attributes));
    memcpy(data, mesh_attributes.data(), mesh_attributes.size() * sizeof(serialization::Vertex_Attributes));

    data = &(result.data()[serialized_model.get_vertex_skin_attributes_offset()]);
    spdlog::trace("Copying skin attributes. Offset: {}, Size: {}",
        serialized_model.get_vertex_skin_attributes_offset(), mesh_skin_attributes.size() * sizeof(serialization::Vertex_Skin_Attributes));
    memcpy(data, mesh_skin_attributes.data(), mesh_skin_attributes.size() * sizeof(serialization::Vertex_Skin_Attributes));

    data = &(result.data()[serialized_model.get_indices_offset()]);
    spdlog::trace("Copying indices. Offset: {}, Size: {}",
        serialized_model.get_indices_offset(), mesh_indices.size() * sizeof(uint32_t));
    memcpy(data, mesh_indices.data(), mesh_indices.size() * sizeof(uint32_t));

    return result;
}
}
