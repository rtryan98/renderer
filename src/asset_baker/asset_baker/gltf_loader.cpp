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
            auto position_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_POSITION);
            auto color_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_COLOR_0);
            auto normal_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_NORMAL);
            auto tangent_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_TANGENT);
            auto tex_coord_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_TEXCOORD_0);
            auto joint_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_JOINTS_0);
            auto weights_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_WEIGHTS_0);

            auto load_accessor = [&]<typename T>(const fastgltf::Accessor& accessor, std::vector<T>& target, const std::string& semantic)
            {
                if (!accessor.bufferViewIndex.has_value())
                {
                    spdlog::error("GLTF file '{}' has unsupported accessor (no buffer view) '{}'. Semantic: '{}'",
                        path.string(), accessor.name, semantic);
                    return GLTF_Error::No_Buffer_View;
                }
                const auto& view = asset->bufferViews.at(accessor.bufferViewIndex.value());
                const auto& buffer = asset->buffers.at(view.bufferIndex);
                const auto* array = std::get_if<fastgltf::sources::Array>(&buffer.data);

                if (!array)
                {
                    spdlog::error("GLTF file '{}' has unsupported accessor (array) '{}'. Semantic: '{}'",
                        path.string(), accessor.name, semantic);
                    return GLTF_Error::Non_Supported_Accessor;
                }
                if (accessor.sparse.has_value())
                {
                    spdlog::error("GLTF file '{}' has unsupported sparse accessor '{}'. Semantic: '{}'",
                        path.string(), accessor.name, semantic);
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
                    spdlog::error("GLTF file '{}' has unsupported accessor (element count) '{}'. Semantic: '{}'",
                        path.string(), accessor.name, semantic);
                    return GLTF_Error::Non_Supported_Accessor;
                }

                if (component_type_size == 0)
                {
                    spdlog::error("GLTF file '{}' has unsupported accessor (element type size) '{}'. Semantic: '{}'",
                        path.string(), accessor.name,semantic);
                    return GLTF_Error::Non_Supported_Accessor;
                }

                const auto element_size = element_count * component_type_size;

                if (element_size != sizeof(T))
                // if (component_type_size != sizeof(T))
                {
                    spdlog::error("GLTF file '{}' has unsupported accessor (element size) '{}'. Semantic: '{}'",
                        path.string(), accessor.name,semantic);
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
                auto error = load_accessor(positions, mesh.positions, GLTF_ATTRIBUTE_POSITION);
                if (error != GLTF_Error::No_Error) return std::unexpected(error);
            }

            if (primitive.indicesAccessor.has_value())
            {
                spdlog::trace("Gathering indices.");
                auto& indices = asset->accessors.at(primitive.indicesAccessor.value());
                switch (indices.componentType)
                {
                case fastgltf::ComponentType::UnsignedByte:
                    {
                        std::vector<uint8_t> indices8{};
                        load_accessor(indices, indices8, "INDICES");
                        mesh.indices.assign(indices8.begin(), indices8.end());
                        break;
                    }
                case fastgltf::ComponentType::UnsignedShort:
                    {
                        std::vector<uint16_t> indices16{};
                        load_accessor(indices, indices16, "INDICES");
                        mesh.indices.assign(indices16.begin(), indices16.end());
                        break;
                    }
                case fastgltf::ComponentType::UnsignedInt:
                    {
                        load_accessor(indices, mesh.indices, "INDICES");
                        break;
                    }
                default:
                    break;
                }
            }

            { // Attributes
                spdlog::trace("Processing attributes.");

                if (color_attribute && color_attribute != primitive.attributes.end())
                {
                    spdlog::error("SKIPPING PROCESSING - NOT YET SUPPORTED COLORS.");
                    return std::unexpected(GLTF_Error::Non_Supported_Accessor);

                    spdlog::trace("Gathering colors.");
                    auto& colors = asset->accessors.at(color_attribute->accessorIndex);
                    switch (colors.componentType)
                    {
                    case fastgltf::ComponentType::UnsignedByte:
                        {
                            if (colors.type == fastgltf::AccessorType::Vec3)
                            {
                                std::vector<std::array<uint8_t, 3>> colors8{};
                                auto error = load_accessor(colors, colors8, GLTF_ATTRIBUTE_COLOR_0);
                                if (error != GLTF_Error::No_Error) return std::unexpected(error);
                                mesh.colors.reserve(colors8.size());
                                for (auto i = 0; i < colors8.size(); ++i)
                                {
                                    auto& mesh_colors = mesh.colors.emplace_back();
                                    mesh_colors[0] |= colors8[i][0] << 24;
                                    mesh_colors[0] |= colors8[i][1] << 16;
                                    mesh_colors[0] |= colors8[i][2] <<  8;
                                }
                            }
                            else
                            {
                                std::vector<std::array<uint8_t, 4>> colors8{};
                                auto error = load_accessor(colors, colors8, GLTF_ATTRIBUTE_COLOR_0);
                                if (error != GLTF_Error::No_Error) return std::unexpected(error);
                                mesh.colors.reserve(colors8.size());
                                for (auto i = 0; i < colors8.size(); ++i)
                                {
                                    auto& mesh_colors = mesh.colors.emplace_back();
                                    mesh_colors[0] |= colors8[i][0] << 24;
                                    mesh_colors[0] |= colors8[i][1] << 16;
                                    mesh_colors[0] |= colors8[i][2] <<  8;
                                    mesh_colors[0] |= colors8[i][3] <<  0;
                                }
                            }
                            break;
                        }
                    case fastgltf::ComponentType::Float:
                        {
                            if (colors.type == fastgltf::AccessorType::Vec3)
                            {
                                std::vector<std::array<float, 3>> colors_f32{};
                                auto error = load_accessor(colors, colors_f32, GLTF_ATTRIBUTE_COLOR_0);
                                if (error != GLTF_Error::No_Error) return std::unexpected(error);
                                mesh.colors.reserve(colors_f32.size());
                                for (auto i = 0; i < colors_f32.size(); ++i)
                                {
                                    auto& mesh_colors = mesh.colors.emplace_back();
                                    mesh_colors[0] |= static_cast<uint8_t>(colors_f32[i][0] * 255.f) << 24;
                                    mesh_colors[0] |= static_cast<uint8_t>(colors_f32[i][1] * 255.f) << 16;
                                    mesh_colors[0] |= static_cast<uint8_t>(colors_f32[i][2] * 255.f) <<  8;
                                }
                            }
                            else
                            {
                                std::vector<std::array<float, 4>> colors_f32{};
                                auto error = load_accessor(colors, colors_f32, GLTF_ATTRIBUTE_COLOR_0);
                                if (error != GLTF_Error::No_Error) return std::unexpected(error);
                                mesh.colors.reserve(colors_f32.size());
                                for (auto i = 0; i < colors_f32.size(); ++i)
                                {
                                    auto& mesh_colors = mesh.colors.emplace_back();
                                    mesh_colors[0] |= static_cast<uint8_t>(colors_f32[i][0] * 255.f) << 24;
                                    mesh_colors[0] |= static_cast<uint8_t>(colors_f32[i][1] * 255.f) << 16;
                                    mesh_colors[0] |= static_cast<uint8_t>(colors_f32[i][2] * 255.f) <<  8;
                                    mesh_colors[0] |= static_cast<uint8_t>(colors_f32[i][3] * 255.f) <<  0;
                                }
                            }
                            break;
                        }
                    default:
                        break;
                    }
                }

                if (normal_attribute && normal_attribute != primitive.attributes.end())
                {
                    spdlog::trace("Gathering normals.");
                    auto& normals = asset->accessors.at(normal_attribute->accessorIndex);
                    auto error = load_accessor(normals, mesh.normals, GLTF_ATTRIBUTE_NORMAL);
                    if (error != GLTF_Error::No_Error) return std::unexpected(error);
                }

                if (tangent_attribute && tangent_attribute != primitive.attributes.end())
                {
                    spdlog::trace("Gathering tangents.");
                    auto& tangents = asset->accessors.at(tangent_attribute->accessorIndex);
                    std::vector<std::array<float, 4>> tangents_f32{};
                    auto error = load_accessor(tangents, tangents_f32, GLTF_ATTRIBUTE_TANGENT);
                    mesh.tangents.reserve(tangents_f32.size());
                    for (auto i = 0; i < tangents_f32.size(); ++i)
                    {
                        auto& mesh_tangent = mesh.tangents.emplace_back();
                        mesh_tangent = {
                            tangents_f32[i][0] * tangents_f32[i][3],
                            tangents_f32[i][1] * tangents_f32[i][3],
                            tangents_f32[i][2] * tangents_f32[i][3],
                        };
                    }
                    if (error != GLTF_Error::No_Error) return std::unexpected(error);
                }

                if (tex_coord_attribute && tex_coord_attribute != primitive.attributes.end())
                {
                    spdlog::trace("Gathering uvs.");
                    auto& tex_coords = asset->accessors.at(tex_coord_attribute->accessorIndex);
                    switch (tex_coords.componentType)
                    {
                    case fastgltf::ComponentType::UnsignedByte:
                        {
                            std::vector<std::array<uint8_t, 2>> tex_coords_u8{};
                            auto error = load_accessor(tex_coords, tex_coords_u8, GLTF_ATTRIBUTE_TEXCOORD_0);
                            if (error != GLTF_Error::No_Error) return std::unexpected(error);
                            mesh.tex_coords.reserve(tex_coords_u8.size() * tex_coords.count);
                            for (uint32_t i = 0; i < tex_coords_u8.size(); ++i)
                            {
                                auto& tex_coords_val = mesh.tex_coords.emplace_back();
                                tex_coords_val = {
                                    static_cast<float>(tex_coords_u8[i][0]) / 255.0f,
                                    static_cast<float>(tex_coords_u8[i][1]) / 255.0f
                                };
                            }
                            break;
                        }
                    case fastgltf::ComponentType::UnsignedShort:
                        {
                            std::vector<std::array<uint16_t, 2>> tex_coords_u16{};
                            auto error = load_accessor(tex_coords, tex_coords_u16, GLTF_ATTRIBUTE_TEXCOORD_0);
                            if (error != GLTF_Error::No_Error) return std::unexpected(error);
                            mesh.tex_coords.reserve(tex_coords_u16.size() * tex_coords.count);
                            for (uint32_t i = 0; i < tex_coords_u16.size(); ++i)
                            {
                                auto& tex_coords_val = mesh.tex_coords.emplace_back();
                                tex_coords_val = {
                                    static_cast<float>(tex_coords_u16[i][0]) / 65535.0f,
                                    static_cast<float>(tex_coords_u16[i][1]) / 65535.0f
                                };
                            }
                            break;
                        }
                    case fastgltf::ComponentType::Float:
                        {
                            auto error = load_accessor(tex_coords, mesh.tex_coords, GLTF_ATTRIBUTE_TEXCOORD_0);
                            if (error != GLTF_Error::No_Error) return std::unexpected(error);
                            break;
                        }
                    default:
                        break;
                    }
                }

                if (joint_attribute && joint_attribute != primitive.attributes.end())
                {
                    spdlog::error("SKIPPING PROCESSING - NOT YET SUPPORTED JOINTS.");
                    return std::unexpected(GLTF_Error::Non_Supported_Accessor);

                    spdlog::trace("Gathering joints.");
                    auto& joints = asset->accessors.at(joint_attribute->accessorIndex);
                    switch (joints.componentType)
                    {
                    case fastgltf::ComponentType::UnsignedByte:
                        {
                            std::vector<std::array<uint8_t, 4>> joints8{};
                            auto error = load_accessor(joints, joints8, GLTF_ATTRIBUTE_JOINTS_0);
                            if (error != GLTF_Error::No_Error) return std::unexpected(error);
                            mesh.joints.reserve(joints8.size());
                            for (auto i = 0; i < joints8.size(); ++i)
                            {
                                auto& joint = mesh.joints.emplace_back();
                                joint = {
                                    static_cast<uint32_t>(joints8[i][0]),
                                    static_cast<uint32_t>(joints8[i][1]),
                                    static_cast<uint32_t>(joints8[i][2]),
                                    static_cast<uint32_t>(joints8[i][3])
                                };
                            }
                            break;
                        }
                    case fastgltf::ComponentType::UnsignedShort:
                        {
                            std::vector<std::array<uint16_t, 4>> joints16{};
                            auto error = load_accessor(joints, joints16, GLTF_ATTRIBUTE_JOINTS_0);
                            if (error != GLTF_Error::No_Error) return std::unexpected(error);
                            mesh.joints.reserve(joints16.size());
                            for (auto i = 0; i < joints16.size(); i += 4)
                            {
                                auto& joint = mesh.joints.emplace_back();
                                joint = {
                                    static_cast<uint32_t>(joints16[i][0]),
                                    static_cast<uint32_t>(joints16[i][1]),
                                    static_cast<uint32_t>(joints16[i][2]),
                                    static_cast<uint32_t>(joints16[i][3])
                                };
                            }
                            break;
                        }
                    default:
                        break;
                    }
                }

                if (weights_attribute && weights_attribute != primitive.attributes.end())
                {
                    spdlog::error("SKIPPING PROCESSING - NOT YET SUPPORTED WEIGHTS.");
                    return std::unexpected(GLTF_Error::Non_Supported_Accessor);

                    spdlog::trace("Gathering weights.");
                    auto& weights = asset->accessors.at(weights_attribute->accessorIndex);
                    switch (weights.componentType)
                    {
                    case fastgltf::ComponentType::UnsignedByte:
                        {
                            std::vector<uint8_t> weights8{};
                            auto error = load_accessor(weights, weights8, GLTF_ATTRIBUTE_WEIGHTS_0);
                            if (error != GLTF_Error::No_Error) return std::unexpected(error);
                            mesh.weights.reserve(weights8.size() / 4);
                            for (auto i = 0; i < weights8.size(); i += 4)
                            {
                                auto& weight = mesh.weights.emplace_back();
                                weight = {
                                    static_cast<float>(weights8[i + 0]) / 255.f,
                                    static_cast<float>(weights8[i + 1]) / 255.f,
                                    static_cast<float>(weights8[i + 2]) / 255.f,
                                    static_cast<float>(weights8[i + 3]) / 255.f
                                };
                            }
                            break;
                        }
                    case fastgltf::ComponentType::UnsignedShort:
                        {
                            std::vector<uint16_t> weights16{};
                            auto error = load_accessor(weights, weights16, GLTF_ATTRIBUTE_WEIGHTS_0);
                            if (error != GLTF_Error::No_Error) return std::unexpected(error);
                            mesh.weights.reserve(weights16.size() / 4);
                            for (auto i = 0; i < weights16.size(); i += 4)
                            {
                                auto& weight = mesh.weights.emplace_back();
                                weight = {
                                    static_cast<float>(weights16[i + 0]) / 65535.f,
                                    static_cast<float>(weights16[i + 1]) / 65535.f,
                                    static_cast<float>(weights16[i + 2]) / 65535.f,
                                    static_cast<float>(weights16[i + 3]) / 65535.f
                                };
                            }
                            break;
                        }
                    case fastgltf::ComponentType::Float:
                        {
                            auto error = load_accessor(weights, mesh.weights, GLTF_ATTRIBUTE_WEIGHTS_0);
                            if (error != GLTF_Error::No_Error) return std::unexpected(error);
                            break;
                        }
                    default:
                        break;
                    }
                }

                auto has_tangents = mesh.tangents.size() > 0;
                auto has_material = mesh.material_index != NO_INDEX;
                auto requires_tangents = has_material && !result.materials[mesh.material_index].normal_uri.empty();

                if (!has_tangents && requires_tangents)
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
                    spdlog::trace("Mesh has no normal map and requires no tangents.");
                }
                spdlog::trace("Attributes processed");
                spdlog::trace("Swizzling axes");
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
        spdlog::trace("Submesh range for mesh '{}': {} - {}", static_cast<void*>(&gltf_mesh), range.first, range.second);
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
    std::vector<uint32_t> mesh_attribute_data;

    for (const auto& submesh : gltf_model.submeshes)
    {
        auto current_mesh_position_count = mesh_positions.size();
        auto new_mesh_position_count = submesh.positions.size() + current_mesh_position_count;
        auto current_mesh_indices_count = mesh_indices.size();
        auto new_mesh_indices_count = submesh.indices.size() + current_mesh_indices_count;

        mesh_positions.resize(new_mesh_position_count, {});
        mesh_indices.resize(new_mesh_indices_count, {});


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
    }
    serialized_model.submesh_count = static_cast<uint32_t>(mesh_data_ranges.size());
    serialized_model.vertex_position_count = static_cast<uint32_t>(mesh_positions.size());
    serialized_model.vertex_attribute_count = static_cast<uint32_t>(mesh_attribute_data.size());
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
        serialized_model.get_vertex_attributes_offset(), mesh_attribute_data.size() * sizeof(uint32_t));
    memcpy(data, mesh_attribute_data.data(), mesh_attribute_data.size() * sizeof(uint32_t));

    data = &(result.data()[serialized_model.get_indices_offset()]);
    spdlog::trace("Copying indices. Offset: {}, Size: {}",
        serialized_model.get_indices_offset(), mesh_indices.size() * sizeof(uint32_t));
    memcpy(data, mesh_indices.data(), mesh_indices.size() * sizeof(uint32_t));

    return result;
}
}
