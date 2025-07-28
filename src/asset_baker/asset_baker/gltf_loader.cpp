#include "asset_baker/gltf_loader.hpp"

#include <spdlog/spdlog.h>
#include <DirectXPackedVector.h>
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

namespace asset_baker
{
constexpr static auto GLTF_ATTRIBUTE_POSITION = "POSITION";
constexpr static auto GLTF_ATTRIBUTE_COLOR_0 = "COLOR_0";
constexpr static auto GLTF_ATTRIBUTE_NORMAL = "NORMAL";
constexpr static auto GLTF_ATTRIBUTE_TANGENT = "TANGENT";
constexpr static auto GLTF_ATTRIBUTE_TEXCOORD_0 = "TEXCOORD_0";
constexpr static auto GLTF_ATTRIBUTE_JOINTS_0 = "JOINTS_0";
constexpr static auto GLTF_ATTRIBUTE_WEIGHTS_0 = "WEIGHTS_0";

constexpr static auto NO_INDEX = ~0ull;

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
};

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
            .base_color_factor = {{
                material.pbrData.baseColorFactor[0], material.pbrData.baseColorFactor[1],
                material.pbrData.baseColorFactor[2], material.pbrData.baseColorFactor[3]
            }},
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

        auto range = std::make_pair(result.submeshes.size(), result.submeshes.size());

        for (auto& primitive : gltf_mesh.primitives)
        {
            auto& mesh = result.submeshes.emplace_back();

            if (primitive.type != fastgltf::PrimitiveType::Triangles)
            {
                spdlog::error("GLTF file '{}' has unsupported primitive type.", path.string());
                return std::unexpected(GLTF_Error::Non_Supported_Primitive);
            }

            mesh.material_index = primitive.materialIndex.value_or(NO_INDEX);

            if (auto position_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_POSITION))
            {
                auto& positions = asset->accessors.at(position_attribute->accessorIndex);
                mesh.positions.resize(positions.count);
                fastgltf::copyFromAccessor<fastgltf::math::fvec3>(
                    asset.get(),
                    positions,
                    mesh.positions.data());
            }

            if (primitive.indicesAccessor.has_value())
            {
                auto& indices = asset->accessors.at(primitive.indicesAccessor.value());
                mesh.indices.resize(indices.count);
                fastgltf::copyFromAccessor<uint32_t>(
                    asset.get(),
                    indices,
                    mesh.indices.data());
                for (auto i = 0; i < indices.count; i += 3)
                {
                    auto a = mesh.indices[i + 0];
                    auto b = mesh.indices[i + 1];
                    auto c = mesh.indices[i + 2];
                    mesh.indices[i + 0] = c;
                    mesh.indices[i + 1] = b;
                    mesh.indices[i + 2] = a;
                }
            }

            { // Attributes
                if (auto color_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_COLOR_0))
                {
                    if (color_attribute != primitive.attributes.end())
                    {
                        auto& colors = asset->accessors.at(color_attribute->accessorIndex);
                        mesh.colors.resize(colors.count);
                        if (colors.type == fastgltf::AccessorType::Vec4)
                        {
                            fastgltf::copyFromAccessor<fastgltf::math::fvec4>(
                                asset.get(),
                                colors,
                                mesh.colors.data());
                        }
                        else
                        {
                            std::vector<std::array<float, 3>> colors_rgb;
                            colors_rgb.resize(colors.count);
                            fastgltf::copyFromAccessor<fastgltf::math::fvec3>(
                                asset.get(),
                                colors,
                                colors_rgb.data());
                            for (auto i = 0; i < colors.count; ++i)
                            {
                                mesh.colors[i] = {
                                    colors_rgb[i][0],
                                    colors_rgb[i][1],
                                    colors_rgb[i][2],
                                    1.f,
                                };
                            }
                        }
                    }
                }

                if (auto normal_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_NORMAL))
                {
                    if (normal_attribute != primitive.attributes.end())
                    {
                        auto& normals = asset->accessors.at(normal_attribute->accessorIndex);
                        mesh.normals.resize(normals.count);
                        fastgltf::copyFromAccessor<fastgltf::math::fvec3>(
                            asset.get(),
                            normals,
                            mesh.normals.data());
                    }
                }

                if (auto tangent_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_TANGENT))
                {
                    if (tangent_attribute != primitive.attributes.end())
                    {
                        auto& tangents = asset->accessors.at(tangent_attribute->accessorIndex);
                        mesh.tangents.resize(tangents.count);
                        fastgltf::copyFromAccessor<fastgltf::math::fvec4>(
                            asset.get(),
                            tangents,
                            mesh.tangents.data());
                    }
                }

                if (auto tex_coord_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_TEXCOORD_0))
                {
                    if (tex_coord_attribute != primitive.attributes.end())
                    {
                        auto& tex_coords = asset->accessors.at(tex_coord_attribute->accessorIndex);
                        mesh.tex_coords.resize(tex_coords.count);
                        fastgltf::copyFromAccessor<fastgltf::math::fvec2>(
                            asset.get(),
                            tex_coords,
                            mesh.tex_coords.data());
                    }
                }

                if (auto joint_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_JOINTS_0))
                {
                    if (joint_attribute != primitive.attributes.end())
                    {
                        auto& joints = asset->accessors.at(joint_attribute->accessorIndex);
                        mesh.joints.resize(joints.count);
                        fastgltf::copyFromAccessor<fastgltf::math::uvec4>(
                            asset.get(),
                            joints,
                            mesh.joints.data());
                    }
                }

                if (auto weights_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_WEIGHTS_0))
                {
                    if (weights_attribute != primitive.attributes.end())
                    {
                        auto& weights = asset->accessors.at(weights_attribute->accessorIndex);
                        mesh.weights.resize(weights.count);
                        fastgltf::copyFromAccessor<fastgltf::math::fvec4>(
                            asset.get(),
                            weights,
                            mesh.weights.data());
                    }
                }

                if (mesh.tangents.size() == 0 &&                                // Mesh has no tangents
                    mesh.normals.size() > 0 &&                                  // and it has normals
                    mesh.tex_coords.size() > 0 &&                               // and texture coordinates
                    mesh.material_index != NO_INDEX &&                          // and a material
                    !result.materials[mesh.material_index].normal_uri.empty())  // that contains a normal map
                {
                    spdlog::debug("GLTF file '{}' has no tangents and they need to be generated.", path.string());

                    mesh.tangents.resize(mesh.normals.size());

                    if (!(mesh.normals.size() == mesh.tangents.size() &&
                        mesh.tangents.size() == mesh.tex_coords.size()))
                    {
                        spdlog::error("GLTF file '{}' has no tangents and they failed to generate.", path.string());
                        return std::unexpected(GLTF_Error::Tangent_Generation_Failed);
                    }

                    struct Mikkt_Space_User_Data
                    {
                        std::vector<std::array<float, 3>>& positions;
                        std::vector<std::array<float, 3>>& normals;
                        std::vector<std::array<float, 2>>& tex_coords;
                        std::vector<std::array<float, 4>>& tangents;
                        std::vector<uint32_t>& indices;
                    } mikkt_space_user_data = {
                        .positions = mesh.positions,
                        .normals = mesh.normals,
                        .tex_coords = mesh.tex_coords,
                        .tangents = mesh.tangents,
                        .indices = mesh.indices,
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
                    auto has_tangents = genTangSpaceDefault(&mikktspace_context);

                    if (!has_tangents)
                    {
                        spdlog::warn("GLTF file '{}' failed to generate mikktspace tangents.", path.string());
                    }
                    else
                    {
                        spdlog::debug("Generated mikktspace tangents.");
                    }
                }
                else
                {
                    spdlog::debug("Mesh requires no tangents or cannot generate them.");
                }

                for (auto& position : mesh.positions)
                {
                    position = { position[0], position[2], position[1] };
                }
                for (auto& normal : mesh.normals)
                {
                    normal = { normal[0], normal[2], normal[1] };
                }
                for (auto& tangent : mesh.tangents)
                {
                    tangent = { tangent[0], tangent[2], tangent[1] };
                }
            }
        }

        range.second = result.submeshes.size();
        submesh_ranges[&gltf_mesh] = range;
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

            result.instances.emplace_back( GLTF_Mesh_Instance {
                .submesh_range_start = submesh_range_start,
                .submesh_range_end = submesh_range_end,
                .parent_index = parent_index,
                .translation = { trs.translation[0], trs.translation[1], trs.translation[2] },
                .rotation = { trs.rotation[0], trs.rotation[1], trs.rotation[2], trs.rotation[3] },
                .scale = { trs.scale[0], trs.scale[1], trs.scale[2] },
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
    std::vector<serialization::Mesh_Material_00> materials;
    materials.reserve(gltf_model.materials.size());
    for (const auto& material : gltf_model.materials)
    {
        const auto get_uri = [&](const std::string& value)
        {
            if (mapped_uris.contains(value))
                return mapped_uris.at(value);
            return serialization::Mesh_Material_00::URI_NO_REFERENCE;
        };

        materials.emplace_back( serialization::Mesh_Material_00 {
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
            .emissive_uri_index = get_uri(material.emissive_uri)
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
            mesh_positions.emplace_back(position);
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
                attributes.normal = submesh.normals[i];
            }
            else
            {
                attributes.normal = {};
            }
            if (submesh.tangents.size() > i)
            {
                attributes.tangent = submesh.tangents[i];
            }
            else
            {
                attributes.tangent = {};
            }
            if (submesh.tex_coords.size() > i)
            {
                attributes.tex_coords = submesh.tex_coords[i];
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
                    attributes.joints = submesh.joints[i];
                }
                else
                {
                    attributes.joints = {};
                }

                if (submesh.weights.size() > i)
                {
                    attributes.weights = submesh.weights[i];
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
        serialized_model.get_materials_offset(), materials.size() * sizeof(serialization::Mesh_Material_00));
    memcpy(data, materials.data(), materials.size() * sizeof(serialization::Mesh_Material_00));

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
