#pragma once

#include <array>
#include <filesystem>
#include <vector>
#include <expected>

namespace asset_baker
{
enum class GLTF_Error
{
    No_Error = 0,
    File_Load_Failed,
    Parse_Failed,
    Non_Supported_Primitive,
    Non_Supported_Accessor,
    No_Buffer_View,
    Varying_Attribute_Size,
    Missing_Normals,
    Missing_Texcoords,
    Tangent_Generation_Failed
};

struct GLTF_Submesh
{
    std::size_t material_index;
    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 4>> colors;
    std::vector<std::array<float, 3>> normals;
    std::vector<std::array<float, 3>> tangents;
    std::vector<std::array<float, 2>> tex_coords;
    std::vector<std::array<uint32_t, 4>> joints;
    std::vector<std::array<float, 4>> weights;
    std::vector<uint32_t> indices;
};

struct GLTF_Mesh_Instance
{
    std::size_t submesh_range_start;
    std::size_t submesh_range_end;
    std::size_t parent_index;
    std::array<float, 3> translation;
    std::array<float, 4> rotation;
    std::array<float, 3> scale;
};

struct GLTF_Material
{
    std::array<float, 4> base_color_factor;
    float pbr_roughness;
    float pbr_metallic;
    std::array<float, 3> emissive_color;
    float emissive_strength;
    std::string albedo_uri;
    std::string normal_uri;
    std::string metallic_roughness_uri;
    std::string emissive_uri;
};

struct GLTF_Model
{
    std::vector<GLTF_Material> materials;
    std::vector<GLTF_Submesh> submeshes;
    std::vector<GLTF_Mesh_Instance> instances;
};

std::expected<GLTF_Model, GLTF_Error> process_gltf_from_file(const std::filesystem::path& path);
std::vector<char> serialize_gltf_model(const std::string& name, GLTF_Model& gltf_model);
}
