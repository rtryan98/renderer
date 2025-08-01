#pragma once

#include <array>
#include <filesystem>
#include <vector>
#include <expected>
#include <rhi/resource.hpp>
#include <glm/glm.hpp>

namespace asset_baker
{
enum class GLTF_Error
{
    No_Error = 0,
    File_Load_Failed,
    Parse_Failed,
    Unsupported_Extension,
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
    std::vector<glm::vec3> positions;
    std::vector<glm::vec4> colors;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec4> tangents;
    std::vector<glm::vec2> tex_coords;
    std::vector<glm::uvec4> joints;
    std::vector<glm::vec4> weights;
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
    glm::u8vec4 base_color_factor;
    float pbr_roughness;
    float pbr_metallic;
    std::array<float, 3> emissive_color;
    float emissive_strength;
    std::string albedo_uri;
    std::string normal_uri;
    std::string metallic_roughness_uri;
    std::string emissive_uri;
};

struct GLTF_Texture_Load_Request
{
    std::vector<char> data;
    bool squash_gb_to_rg;
    std::string name;
    std::string hash_identifier;
    rhi::Image_Format target_format;
};

struct GLTF_Model
{
    std::vector<GLTF_Material> materials;
    std::vector<GLTF_Submesh> submeshes;
    std::vector<GLTF_Mesh_Instance> instances;
    std::vector<GLTF_Texture_Load_Request> texture_load_requests;
};

std::expected<GLTF_Model, GLTF_Error> process_gltf_from_file(const std::filesystem::path& path);
std::vector<char> process_and_serialize_gltf_texture(const GLTF_Texture_Load_Request& request);
std::vector<char> serialize_gltf_model(const std::string& name, GLTF_Model& gltf_model);
}
