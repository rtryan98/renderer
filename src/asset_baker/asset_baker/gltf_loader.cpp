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
constexpr static auto GLTF_ATTRIBUTE_COLOR_0 = "COLOR_0";
constexpr static auto GLTF_ATTRIBUTE_NORMAL = "NORMAL";
constexpr static auto GLTF_ATTRIBUTE_TANGENT = "TANGENT";
constexpr static auto GLTF_ATTRIBUTE_TEXCOORD_0 = "TEXCOORD_0";
constexpr static auto GLTF_ATTRIBUTE_JOINTS_0 = "JOINTS_0";
constexpr static auto GLTF_ATTRIBUTE_WEIGHTS_0 = "WEIGHTS_0";

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
                    spdlog::debug("GLTF file '{}' uses unsupported embedded image data (Array).", path.string());
                    return std::string("");
                },
                [&](const fastgltf::sources::BufferView& buffer_view)
                {
                    spdlog::debug("GLTF file '{}' uses unsupported embedded image data (BufferView).", path.string());
                    return std::string("");
                },
                [&](const auto&)
                {
                    spdlog::debug("GLTF file '{}' uses unknown unsupported embedded image data (BufferView).", path.string());
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
            }

            { // Attributes
                if (auto color_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_COLOR_0))
                {
                    if (color_attribute != primitive.attributes.end())
                    {
                        spdlog::error("SKIPPING PROCESSING - NOT YET SUPPORTED COLORS.");
                        return std::unexpected(GLTF_Error::Non_Supported_Accessor);

                        auto& colors = asset->accessors.at(color_attribute->accessorIndex);
                        mesh.colors.resize(colors.count);
                        fastgltf::copyFromAccessor<fastgltf::math::fvec4>(
                            asset.get(),
                            colors,
                            mesh.indices.data());
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
                        std::vector<std::array<float, 4>> tangents_vec4{};
                        tangents_vec4.resize(tangents.count);
                        fastgltf::copyFromAccessor<fastgltf::math::fvec4>(
                            asset.get(),
                            tangents,
                            tangents_vec4.data());
                        mesh.tangents.reserve(tangents_vec4.size());
                        for (auto i = 0; i < tangents_vec4.size(); ++i)
                        {
                            auto& mesh_tangent = mesh.tangents.emplace_back();
                            mesh_tangent = {
                                tangents_vec4[i][0] * tangents_vec4[i][3],
                                tangents_vec4[i][1] * tangents_vec4[i][3],
                                tangents_vec4[i][2] * tangents_vec4[i][3],
                            };
                        }
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
                        spdlog::error("SKIPPING PROCESSING - NOT YET SUPPORTED JOINTS.");
                        return std::unexpected(GLTF_Error::Non_Supported_Accessor);

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
                        spdlog::error("SKIPPING PROCESSING - NOT YET SUPPORTED WEIGHTS.");
                        return std::unexpected(GLTF_Error::Non_Supported_Accessor);

                        auto& weights = asset->accessors.at(weights_attribute->accessorIndex);
                        mesh.weights.resize(weights.count);
                        fastgltf::copyFromAccessor<fastgltf::math::fvec4>(
                            asset.get(),
                            weights,
                            mesh.weights.data());
                    }
                }

                if (mesh.tangents.size() == 0 && mesh.normals.size() > 0 && mesh.tex_coords.size() > 0)
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
                        std::vector<std::array<float, 3>>& tangents;
                        std::vector<uint32_t>& indices;
                    } user_data = {
                        .positions = mesh.positions,
                        .normals = mesh.normals,
                        .tex_coords = mesh.tex_coords,
                        .tangents = mesh.tangents,
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
                            tangent[0] = sign * fv_tangent[0]; // TODO: handedness?
                            tangent[1] = sign * fv_tangent[1];
                            tangent[2] = sign * fv_tangent[2];
                        },
                        .m_setTSpace = nullptr,
                    };
                    SMikkTSpaceContext mikktspace_context = {
                        .m_pInterface = &mikktspace_interface,
                        .m_pUserData = &user_data
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
    spdlog::trace("Serializing submeshes and ranges.");
    std::vector<serialization::Submesh_Data_Ranges_00> mesh_data_ranges;
    std::vector<std::array<float, 3>> mesh_positions;
    std::vector<uint32_t> mesh_indices;
    std::vector<serialization::Vertex_Attributes> mesh_attributes;
    std::vector<serialization::Vertex_Skin_Attributes> mesh_skin_attributes;
    // std::vector<uint32_t> mesh_attribute_data;

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

        /*

        spdlog::trace("copying positions");
        auto mesh_position_data = reinterpret_cast<char*>(&mesh_positions.data()[current_mesh_position_count]);
        memcpy(mesh_position_data,
            submesh.positions.data(),
            submesh.positions.size() * sizeof(float) * 3);

        spdlog::trace("copying indices");
        auto mesh_indices_data = reinterpret_cast<char*>(&mesh_indices.data()[current_mesh_indices_count]);
        memcpy(mesh_indices_data,
            submesh.indices.data(),
            submesh.indices.size() * sizeof(uint32_t));

        serialization::Attribute_Flags attribute_flags = {};
        auto add_flag = [&](const auto& data, auto flag)
        {
            attribute_flags = attribute_flags | (data.size() > 0 ? flag : serialization::Attribute_Flags::None);
        };
        add_flag(submesh.colors, serialization::Attribute_Flags::Color);
        add_flag(submesh.normals, serialization::Attribute_Flags::Normal);
        add_flag(submesh.tangents, serialization::Attribute_Flags::Tangent);
        add_flag(submesh.tex_coords, serialization::Attribute_Flags::Tex_Coords);
        add_flag(submesh.joints, serialization::Attribute_Flags::Joints);
        add_flag(submesh.weights, serialization::Attribute_Flags::Weights);

        auto current_mesh_attribute_start = mesh_attribute_data.size();
        std::size_t current_mesh_attribute_count = serialization::calculate_total_attribute_size(attribute_flags);
        auto current_mesh_attribute_end = current_mesh_attribute_start + current_mesh_attribute_count * submesh.positions.size();
        mesh_attribute_data.resize(current_mesh_attribute_end);

        spdlog::trace("copying attributes");
        if (current_mesh_attribute_count > 0)
        {
            spdlog::trace("pos: {}", submesh.positions.size());
            spdlog::trace("col: {}", submesh.colors.size());
            spdlog::trace("nrm: {}", submesh.normals.size());
            spdlog::trace("tan: {}", submesh.tangents.size());
            spdlog::trace("uvs: {}", submesh.tex_coords.size());
            spdlog::trace("jnt: {}", submesh.joints.size());
            spdlog::trace("wgt: {}", submesh.weights.size());

            auto* ptr = &mesh_attribute_data[current_mesh_attribute_start];
            for (auto i = 0; i < submesh.positions.size(); ++i)
            {
                auto add_element = [&]<typename T, std::size_t N>(const std::vector<std::array<T, N>>& data)
                {
                    if (data.size() > 0)
                    {
                        memcpy(ptr, &data[i], N * sizeof(uint32_t));
                        ptr += N;
                    }
                };
                add_element(submesh.colors);
                add_element(submesh.normals);
                add_element(submesh.tangents);
                add_element(submesh.tex_coords);
                add_element(submesh.joints);
                add_element(submesh.weights);
            }
        }

        mesh_data_ranges.emplace_back( serialization::Submesh_Data_Ranges_00 {
            .material_index = static_cast<uint32_t>(submesh.material_index),
            .vertex_position_range_start = static_cast<uint32_t>(current_mesh_position_count),
            .vertex_position_range_end = static_cast<uint32_t>(new_mesh_position_count),
            .vertex_attribute_range_start = static_cast<uint32_t>(current_mesh_attribute_start),
            .vertex_attribute_range_end = static_cast<uint32_t>(current_mesh_attribute_end),
            .index_range_start = static_cast<uint32_t>(current_mesh_indices_count),
            .index_range_end = static_cast<uint32_t>(new_mesh_indices_count),
        });
        */
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
