#include "asset_baker/gltf_loader.hpp"

#include <DirectXPackedVector.h>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

namespace ren
{
constexpr static auto GLTF_ATTRIBUTE_POSITION = "POSITION";
constexpr static auto GLTF_ATTRIBUTE_COLOR = "COLOR_0";
constexpr static auto GLTF_ATTRIBUTE_NORMAL = "NORMAL";
constexpr static auto GLTF_ATTRIBUTE_TANGENT = "TANGENT";
constexpr static auto GLTF_ATTRIBUTE_TEXCOORD_0 = "TEXCOORD_0";
constexpr static auto GLTF_ATTRIBUTE_JOINTS = "JOINTS";
constexpr static auto GLTF_ATTRIBUTE_WEIGHTS = "WEIGHTS";

constexpr static auto NO_INDEX = ~0ull;

std::expected<GLTF_Model, GLTF_Error> load_gltf_from_file(const std::filesystem::path& path)
{
    using fastgltf::Extensions;
    constexpr auto extensions = Extensions::EXT_mesh_gpu_instancing
                              | Extensions::KHR_materials_emissive_strength;
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
        return std::unexpected(GLTF_Error::Parse_Failed);
    }

    GLTF_Model result = {};

    result.materials.reserve(asset->materials.size());
    for (const auto& material : asset->materials)
    {
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
            .emissive_strength = material.emissiveStrength
        });
    }

    for (const auto& mesh : asset->meshes)
    {
        for (const auto& primitive : mesh.primitives)
        {
            if (primitive.type != fastgltf::PrimitiveType::Triangles)
            {
                return std::unexpected(GLTF_Error::Non_Supported_Primitive);
            }

        }
    }

    for (const auto& scene : asset->scenes)
    {
        auto process_node_bfs = [&](this const auto& self, const auto node_idx, const auto parent_index) -> void
        {
            auto& node = asset->nodes[node_idx];
            auto mesh_idx = node.meshIndex.has_value() ? node.meshIndex.value() : NO_INDEX;
            auto trs = std::get<fastgltf::TRS>(node.transform);

            if (mesh_idx != NO_INDEX)
            {

            }
            else
            {
                // TODO: find out how to handle empty node
            }
            for (const auto child_idx : node.children)
            {
                self(child_idx, node_idx);
            }
        };
        for (const auto node_idx : scene.nodeIndices)
        {
            process_node_bfs(node_idx, NO_INDEX);
        }
    }

    return result;
}
}
