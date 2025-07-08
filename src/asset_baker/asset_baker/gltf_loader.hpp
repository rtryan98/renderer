#pragma once

#include <array>
#include <filesystem>
#include <vector>
#include <expected>

namespace ren
{
enum class GLTF_Error
{
    File_Load_Failed,
    Parse_Failed,
    Non_Supported_Primitive
};

struct GLTF_Vertex_Attributes
{
    std::vector<std::array<float, 3>> vertex_positions;
    std::vector<std::array<float, 3>> normals;
    std::vector<std::array<float, 3>> tangents;
    std::vector<std::array<float, 2>> uvs;
};

struct GLTF_Mesh_Data
{
    std::array<std::size_t, 2> vertex_position_range;
    std::array<std::size_t, 2> vertex_attribute_range;
    std::array<std::size_t, 2> index_range;
};

struct GLTF_Mesh_Instance
{
    std::size_t material_index;
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
};

struct GLTF_Model
{
    std::vector<GLTF_Material> materials;
    std::vector<GLTF_Mesh_Data> meshes;
    std::vector<GLTF_Mesh_Instance> instances;
    std::vector<GLTF_Vertex_Attributes> vertex_attributes;
    std::vector<uint32_t> indices;
};

std::expected<GLTF_Model, GLTF_Error> load_gltf_from_file(const std::filesystem::path& path);
}
