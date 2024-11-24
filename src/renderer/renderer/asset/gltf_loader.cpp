#include "renderer/asset/gltf_loader.hpp"

#include <fastgltf/core.hpp>

namespace ren
{
fastgltf::Asset load_gltf_asset_from_file(const std::filesystem::path& path)
{
    constexpr static auto supported_exts =
        fastgltf::Extensions::KHR_mesh_quantization;

    fastgltf::Parser parser(supported_exts);

    constexpr auto gltf_options =
        fastgltf::Options::DontRequireValidAssetMember |
        fastgltf::Options::AllowDouble |
        fastgltf::Options::LoadGLBBuffers |
        fastgltf::Options::LoadExternalBuffers |
        fastgltf::Options::LoadExternalImages |
        fastgltf::Options::GenerateMeshIndices;

    auto gltf_file = fastgltf::MappedGltfFile::FromPath(path);
    if (!gltf_file)
    {
        // TODO: log error
        return fastgltf::Asset();
    }

    auto gltf_asset = parser.loadGltf(gltf_file.get(), path.parent_path(), gltf_options);
    if (gltf_asset.error() != fastgltf::Error::None)
    {
        // TODO: log error
        return fastgltf::Asset();
    }

    return std::move(gltf_asset.get());
}

Model_Data load_gltf_from_file(const std::filesystem::path& path)
{
    Model_Data result = {};

    auto asset = load_gltf_asset_from_file(path);

    const auto mesh_count = asset.meshes.size();
    result.meshes.reserve(mesh_count);
    result.transforms.reserve(mesh_count);
    result.parent_indices.reserve(mesh_count);

    for (const auto& gltf_mesh : asset.meshes)
    {
        auto& mesh = result.meshes.emplace_back();
        auto& transform = result.transforms.emplace_back();
        auto& parent_index = result.parent_indices.emplace_back();
    }

    return result;
}
}
