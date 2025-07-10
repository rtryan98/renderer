#include "asset_baker/gltf_loader.hpp"

#include <spdlog/spdlog.h>
#include <DirectXPackedVector.h>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <shared/serialized_asset_formats.hpp>
#include <ankerl/unordered_dense.h>
#include <mikktspace.h>
#include <ranges>

namespace asset_baker
{
constexpr static auto GLTF_ATTRIBUTE_POSITION = "POSITION";
constexpr static auto GLTF_ATTRIBUTE_COLOR = "COLOR_0";
constexpr static auto GLTF_ATTRIBUTE_NORMAL = "NORMAL";
constexpr static auto GLTF_ATTRIBUTE_TANGENT = "TANGENT";
constexpr static auto GLTF_ATTRIBUTE_TEXCOORD_0 = "TEXCOORD_0";
constexpr static auto GLTF_ATTRIBUTE_JOINTS = "JOINTS";
constexpr static auto GLTF_ATTRIBUTE_WEIGHTS = "WEIGHTS";

constexpr static auto NO_INDEX = ~0ull;

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
                           | fastgltf::Options::GenerateMeshIndices
                           | fastgltf::Options::DecomposeNodeMatrices
                           | fastgltf::Options::DontRequireValidAssetMember;
    auto asset = parser.loadGltf(data.get(), path.parent_path(), options);
    if (asset.error() != fastgltf::Error::None)
    {
        return std::unexpected(GLTF_Error::Parse_Failed);
    }

    GLTF_Model result = {};

    result.materials.reserve(asset->materials.size());
    for (const auto& material : asset->materials)
    {
        const auto mangle_uri = [&](const std::string& uri) -> std::string
        {
            if (uri.empty()) return "";
            std::filesystem::path uri_path(uri);
            return uri_path.stem().string() + serialization::TEXTURE_FILE_EXTENSION;
        };
        const auto get_uri = [&]<typename T>(const fastgltf::Optional<T>& texture_info_opt) -> std::string
        {
            if (!texture_info_opt.has_value()) return "";
            const auto& texture_info = texture_info_opt.value();
            const auto texture_index = texture_info.textureIndex;
            const auto& texture = asset->textures.at(texture_index);
            const auto image_index = texture.imageIndex.value_or(NO_INDEX);
            if (image_index == NO_INDEX) return "";
            const auto& image = asset->images.at(image_index);
            return std::visit(fastgltf::visitor{
                [&](const fastgltf::sources::URI& file_path)
                {
                    auto mangled_uri = mangle_uri(std::string(file_path.uri.string()));
                    spdlog::trace("URI '{}' mangled to '{}'", file_path.uri.string(), mangled_uri);
                    return mangled_uri;
                },
                [&](const fastgltf::sources::Array& vector)
                {
                    spdlog::warn("GLTF file '{}' uses unsupported embedded image data (Array).", path.string());
                    return std::string("");
                },
                [&](const fastgltf::sources::BufferView& buffer_view)
                {
                    spdlog::warn("GLTF file '{}' uses unsupported embedded image data (BufferView).", path.string());
                    return std::string("");
                },
                [&](const auto&)
                {
                    spdlog::warn("GLTF file '{}' uses unknown unsupported embedded image data (BufferView).", path.string());
                    return std::string("");
                } // no data
            }, image.data);
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
            .albedo_uri = get_uri.template operator()<fastgltf::TextureInfo>(material.pbrData.baseColorTexture),
            .normal_uri = get_uri.template operator()<fastgltf::NormalTextureInfo>(material.normalTexture),
            .metallic_roughness_uri = get_uri.template operator()<fastgltf::TextureInfo>(material.pbrData.metallicRoughnessTexture),
            .emissive_uri = get_uri.template operator()<fastgltf::TextureInfo>(material.emissiveTexture),
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
            auto position_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_POSITION);
            auto normal_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_NORMAL);
            auto tangent_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_TANGENT);
            auto tex_coord_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_TEXCOORD_0);

            auto load_accessor = [&]<typename T>(const fastgltf::Accessor& accessor, std::vector<T>& target)
            {
                if (!accessor.bufferViewIndex.has_value())
                {
                    spdlog::error("GLTF file '{}' has unsupported accessor (no buffer view).", path.string());
                    return GLTF_Error::No_Buffer_View;
                }
                const auto& view = asset->bufferViews.at(accessor.bufferViewIndex.value());
                const auto& buffer = asset->buffers.at(view.bufferIndex);
                const auto* array = std::get_if<fastgltf::sources::Array>(&buffer.data);

                if (!array)
                {
                    spdlog::error("GLTF file '{}' has unsupported accessor (array).", path.string());
                    return GLTF_Error::Non_Supported_Accessor;
                }
                if (accessor.sparse.has_value())
                {
                    spdlog::error("GLTF file '{}' has unsupported sparse accessor.", path.string());
                    return GLTF_Error::Non_Supported_Accessor;
                }
                const auto offset = view.byteOffset + accessor.byteOffset;
                const auto values = array->bytes.data() + offset;

                const auto count = accessor.count;
                const auto stride = view.byteStride.value_or(0);

                const auto element_count = [](const fastgltf::AccessorType type)
                {
                    switch (type)
                    {
                    case fastgltf::AccessorType::Invalid:
                        return 0;
                    case fastgltf::AccessorType::Scalar:
                        return 1;
                    case fastgltf::AccessorType::Vec2:
                        return 2;
                    case fastgltf::AccessorType::Vec3:
                        return 3;
                    case fastgltf::AccessorType::Vec4:
                        return 4;
                    case fastgltf::AccessorType::Mat2:
                        return 4;
                    case fastgltf::AccessorType::Mat3:
                        return 9;
                    case fastgltf::AccessorType::Mat4:
                        return 16;
                    }
                    return 0;
                }(accessor.type);
                const auto component_type_size = [](const fastgltf::ComponentType type)
                {
                    switch (type)
                    {
                    case fastgltf::ComponentType::Invalid:
                        return 0ull;
                    case fastgltf::ComponentType::Byte:
                        return sizeof(int8_t);
                    case fastgltf::ComponentType::UnsignedByte:
                        return sizeof(uint8_t);
                    case fastgltf::ComponentType::Short:
                        return sizeof(int16_t);
                    case fastgltf::ComponentType::UnsignedShort:
                        return sizeof(uint16_t);
                    case fastgltf::ComponentType::Int:
                        return sizeof(int32_t);
                    case fastgltf::ComponentType::UnsignedInt:
                        return sizeof(uint32_t);
                    case fastgltf::ComponentType::Float:
                        return sizeof(float);
                    case fastgltf::ComponentType::Double:
                        return sizeof(double);
                    }
                    return 0ull;
                }(accessor.componentType);

                if (element_count == 0)
                {
                    spdlog::error("GLTF file '{}' has unsupported sparse accessor (element count).", path.string());
                    return GLTF_Error::Non_Supported_Accessor;
                }

                if (component_type_size == 0)
                {
                    spdlog::error("GLTF file '{}' has unsupported sparse accessor (element type size).", path.string());
                    return GLTF_Error::Non_Supported_Accessor;
                }

                const auto element_size = element_count * component_type_size;

                if (element_size != sizeof(T))
                {
                    spdlog::error("GLTF file '{}' has unsupported sparse accessor (element size).", path.string());
                    return GLTF_Error::Non_Supported_Accessor;
                }

                target.reserve(element_size * count);
                for (auto i = 0; i < count; ++i)
                {
                    auto& value = target.emplace_back();
                    if (stride)
                    {
                        memcpy(&value, &values[i * stride], element_size);
                    }
                    else
                    {
                        memcpy(&value, &values[i * element_size], element_size);
                    }
                }

                return GLTF_Error::No_Error;
            };

            if (position_attribute)
            {
                spdlog::trace("Gathering positions.");
                auto& positions = asset->accessors.at(position_attribute->accessorIndex);
                auto error = load_accessor(positions, mesh.positions);
                if (error != GLTF_Error::No_Error) return std::unexpected(error);
            }

            if (primitive.indicesAccessor.has_value())
            {
                spdlog::trace("Gathering indices.");
                auto& indices = asset->accessors.at(primitive.indicesAccessor.value());
                switch (indices.componentType)
                {
                case fastgltf::ComponentType::Byte:
                    [[fallthrough]];
                case fastgltf::ComponentType::UnsignedByte:
                    {
                        std::vector<uint8_t> indices8{};
                        load_accessor(indices, indices8);
                        mesh.indices.assign(indices8.begin(), indices8.end());
                        break;
                    }
                case fastgltf::ComponentType::Short:
                    [[fallthrough]];
                case fastgltf::ComponentType::UnsignedShort:
                    {
                        std::vector<uint16_t> indices16{};
                        load_accessor(indices, indices16);
                        mesh.indices.assign(indices16.begin(), indices16.end());
                        break;
                    }
                case fastgltf::ComponentType::Int:
                    [[fallthrough]];
                case fastgltf::ComponentType::UnsignedInt:
                    {
                        load_accessor(indices, mesh.indices);
                        break;
                    }
                default:
                    spdlog::error("GLTF file '{}' has unsupported indices type.", path.string());
                    return std::unexpected(GLTF_Error::Non_Supported_Indices);
                }
            }

            { // Attributes
                spdlog::debug("Processing attributes.");

                std::vector<std::array<float, 3>> normals_deinterleaved;
                std::vector<std::array<float, 4>> tangents_deinterleaved;
                std::vector<std::array<float, 2>> tex_coords_deinterleaved;

                if (normal_attribute && normal_attribute != primitive.attributes.end())
                {
                    spdlog::trace("Gathering normals.");
                    auto& normals = asset->accessors.at(normal_attribute->accessorIndex);
                    auto error = load_accessor(normals, normals_deinterleaved);
                    if (error != GLTF_Error::No_Error) return std::unexpected(error);
                }
                if (normals_deinterleaved.size() == 0)
                {
                    spdlog::error("GLTF file '{}' has no normals.", path.string());
                    return std::unexpected(GLTF_Error::Missing_Normals);
                }

                if (tangent_attribute && tangent_attribute != primitive.attributes.end())
                {
                    spdlog::trace("Gathering tangents.");
                    auto& tangents = asset->accessors.at(tangent_attribute->accessorIndex);
                    auto error = load_accessor(tangents, tangents_deinterleaved);
                    if (error != GLTF_Error::No_Error) return std::unexpected(error);
                }

                if (tex_coord_attribute && tex_coord_attribute != primitive.attributes.end())
                {
                    spdlog::trace("Gathering uvs.");
                    auto& tex_coords = asset->accessors.at(tex_coord_attribute->accessorIndex);
                    auto error = load_accessor(tex_coords, tex_coords_deinterleaved);
                    if (error != GLTF_Error::No_Error) return std::unexpected(error);
                }
                if (tex_coords_deinterleaved.size() == 0)
                {
                    spdlog::error("GLTF file '{}' has no texture coordinates.", path.string());
                    return std::unexpected(GLTF_Error::Missing_Texcoords);
                }

                auto has_tangents = tangents_deinterleaved.size() > 0;
                auto requires_tangents = !result.materials[mesh.material_index].normal_uri.empty();

                if (!has_tangents && requires_tangents)
                {
                    spdlog::debug("GLTF file '{}' has no tangents and they need to be generated.", path.string());

                    tangents_deinterleaved.resize(normals_deinterleaved.size());

                    if (!(normals_deinterleaved.size() == tangents_deinterleaved.size() &&
                        tangents_deinterleaved.size() == tex_coords_deinterleaved.size()))
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
                    } user_data = {
                        .positions = mesh.positions,
                        .normals = normals_deinterleaved,
                        .tex_coords = tex_coords_deinterleaved,
                        .tangents = tangents_deinterleaved,
                        .indices = mesh.indices,
                    };

                    SMikkTSpaceInterface mikktspace_interface = {
                        .m_getNumFaces = [](
                            const SMikkTSpaceContext* mikktspace_context) -> int32_t
                        {
                            const auto& data = *static_cast<Mikkt_Space_User_Data*>(mikktspace_context->m_pUserData);
                            return static_cast<int32_t>(data.indices.size() / 3);
                        },
                        .m_getNumVerticesOfFace = [](
                            const SMikkTSpaceContext* mikktspace_context,
                            const int face_idx) -> int32_t
                        {
                            (void)mikktspace_context;
                            (void)face_idx;
                            return 3;
                        },
                        .m_getPosition = [](
                            const SMikkTSpaceContext* mikktspace_context,
                            float fv_pos_out[],
                            const int face_idx,
                            const int vert_idx) -> void
                        {
                            const auto& data = *static_cast<Mikkt_Space_User_Data*>(mikktspace_context->m_pUserData);
                            auto& position = data.positions[data.indices[face_idx * 3 + vert_idx]];
                            fv_pos_out[0] = position[0];
                            fv_pos_out[1] = position[1];
                            fv_pos_out[2] = position[2];
                        },
                        .m_getNormal = [](
                            const SMikkTSpaceContext* mikktspace_context,
                            float fv_norm_out[],
                            const int face_idx,
                            const int vert_idx) -> void
                        {
                            const auto& data = *static_cast<Mikkt_Space_User_Data*>(mikktspace_context->m_pUserData);
                            auto& normal = data.normals[data.indices[face_idx * 3 + vert_idx]];
                            fv_norm_out[0] = normal[0];
                            fv_norm_out[1] = normal[1];
                            fv_norm_out[2] = normal[2];
                        },
                        .m_getTexCoord = [](
                            const SMikkTSpaceContext* mikktspace_context,
                            float fv_tex_coord_out[],
                            const int face_idx,
                            const int vert_idx) -> void
                        {
                            const auto& data = *static_cast<Mikkt_Space_User_Data*>(mikktspace_context->m_pUserData);
                            auto& tex_coord = data.tex_coords[data.indices[face_idx * 3 + vert_idx]];
                            fv_tex_coord_out[0] = tex_coord[0];
                            fv_tex_coord_out[1] = tex_coord[1];
                        },
                        .m_setTSpaceBasic = [](
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
                            tangent[3] = sign; // TODO: handedness?
                        },
                        .m_setTSpace = nullptr,
                    };
                    SMikkTSpaceContext mikktspace_context = {
                        .m_pInterface = &mikktspace_interface,
                        .m_pUserData = &user_data
                    };
                    has_tangents = genTangSpaceDefault(&mikktspace_context);

                    if (!has_tangents)
                    {
                        spdlog::warn("GLTF file '{}' failed to generate mikktspace tangents.", path.string());
                    }
                    else
                    {
                        spdlog::debug("Generated mikktspace tangents.");
                    }
                }
                else if (!has_tangents && !requires_tangents)
                {
                    spdlog::debug("Mesh has no normal map and requires no tangents.");
                    tangents_deinterleaved.resize(normals_deinterleaved.size());
                    return std::unexpected(GLTF_Error::Missing_Normals);
                }

                auto equal_attribute_count = normals_deinterleaved.size() == tangents_deinterleaved.size()
                                          && tangents_deinterleaved.size() == tex_coords_deinterleaved.size()
                                          && tex_coords_deinterleaved.size() == mesh.positions.size();

                if (has_tangents && equal_attribute_count)
                {
                    mesh.vertex_attributes.reserve(normals_deinterleaved.size());
                    for (auto i = 0; i < normals_deinterleaved.size(); ++i)
                    {
                        auto tangent_w = tangents_deinterleaved[i][3];
                        mesh.vertex_attributes.emplace_back( GLTF_Default_Vertex_Attributes {
                            .normal = {
                                normals_deinterleaved[i][0],
                                normals_deinterleaved[i][1],
                                normals_deinterleaved[i][2]
                            },
                            .tangent = {
                                tangent_w * tangents_deinterleaved[i][0],
                                tangent_w * tangents_deinterleaved[i][1],
                                tangent_w * tangents_deinterleaved[i][2]
                            },
                            .uv = {
                                tex_coords_deinterleaved[i][0],
                                tex_coords_deinterleaved[i][1]
                            }
                        });
                    }
                }
                else if (!equal_attribute_count)
                {
                    spdlog::error("GLTF file '{}' has varying attribute sizes.", path.string());
                    spdlog::debug("Position count: {}", mesh.positions.size());
                    spdlog::debug("Normal count: {}", normals_deinterleaved.size());
                    spdlog::debug("Tangent count: {}", tangents_deinterleaved.size());
                    spdlog::debug("UVs count: {}", tex_coords_deinterleaved.size());
                    return std::unexpected(GLTF_Error::Varying_Attribute_Size);
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
                submesh_range_start = range.first;
                submesh_range_end = range.second;
            }

            result.instances.emplace_back( GLTF_Mesh_Instance {
                .submesh_range_start = submesh_range_start,
                .submesh_range_end = submesh_range_end,
                .mesh_index = mesh_idx,
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
    spdlog::trace("Serializing name.");
    name.copy(serialized_model.name, std::min(name.length(), serialization::NAME_MAX_SIZE));

    // URI references
    spdlog::trace("Serializing URI references.");
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
    spdlog::trace("Serializing materials.");
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
    spdlog::trace("Serializing instances.");
    std::vector<serialization::Mesh_Instance_00> instances;
    instances.reserve(gltf_model.instances.size());
    for (const auto& instance : gltf_model.instances)
    {
        instances.emplace_back( serialization::Mesh_Instance_00 {
            .mesh_index = static_cast<uint32_t>(instance.mesh_index),
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
    spdlog::trace("Serializing submeshes and ranges.");
    std::vector<serialization::Submesh_Data_Ranges_00> mesh_data_ranges;
    std::vector<std::array<float, 3>> mesh_positions;
    std::vector<std::array<float, 8>> mesh_attributes;
    std::vector<uint32_t> mesh_indices;
    mesh_data_ranges.reserve(gltf_model.submeshes.size());
    for (const auto& mesh : gltf_model.submeshes)
    {
        auto current_mesh_position_count = mesh_positions.size();
        auto current_mesh_attribute_count = mesh_attributes.size();
        auto current_mesh_indices_count = mesh_indices.size();

        auto new_mesh_position_count = mesh.positions.size() + current_mesh_attribute_count;
        auto new_mesh_attribute_count = mesh.vertex_attributes.size() + current_mesh_attribute_count;
        auto new_mesh_indices_count = mesh.indices.size() + current_mesh_indices_count;

        mesh_positions.resize(new_mesh_position_count, {});
        mesh_attributes.resize(new_mesh_attribute_count, {});
        mesh_indices.resize(new_mesh_indices_count, {});

        auto mesh_position_data = reinterpret_cast<char*>(&mesh_positions.data()[current_mesh_position_count]);
        memcpy(mesh_position_data,
            mesh.positions.data(),
            mesh.positions.size() * sizeof(float[3]));
        auto mesh_attribute_data = reinterpret_cast<char*>(&mesh_attributes.data()[current_mesh_attribute_count]);
        memcpy(mesh_attribute_data,
            mesh.vertex_attributes.data(),
            mesh.vertex_attributes.size() * sizeof(float[8]));
        auto mesh_indices_data = reinterpret_cast<char*>(&mesh_indices.data()[current_mesh_indices_count]);
        memcpy(mesh_indices_data,
            mesh.indices.data(),
            mesh.indices.size() * sizeof(uint32_t));

        mesh_data_ranges.emplace_back( serialization::Submesh_Data_Ranges_00 {
            .material_index = static_cast<uint32_t>(mesh.material_index),
            .vertex_position_range_start = static_cast<uint32_t>(current_mesh_position_count),
            .vertex_position_range_end = static_cast<uint32_t>(new_mesh_position_count),
            .vertex_attribute_range_start = static_cast<uint32_t>(current_mesh_attribute_count),
            .vertex_attribute_range_end = static_cast<uint32_t>(current_mesh_attribute_count),
            .index_range_start = static_cast<uint32_t>(current_mesh_indices_count),
            .index_range_end = static_cast<uint32_t>(current_mesh_indices_count),
        });
    }
    serialized_model.submesh_count = static_cast<uint32_t>(mesh_data_ranges.size());
    serialized_model.vertex_position_count = static_cast<uint32_t>(mesh_positions.size());
    serialized_model.vertex_attribute_count = static_cast<uint32_t>(mesh_attributes.size());
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
        serialized_model.get_vertex_attributes_offset(), mesh_attributes.size() * 32);
    memcpy(data, mesh_attributes.data(), mesh_attributes.size() * 32);

    data = &(result.data()[serialized_model.get_indices_offset()]);
    spdlog::trace("Copying indices. Offset: {}, Size: {}",
        serialized_model.get_indices_offset(), mesh_indices.size() * sizeof(uint32_t));
    memcpy(data, mesh_indices.data(), mesh_indices.size() * sizeof(uint32_t));

    return result;
}
}
