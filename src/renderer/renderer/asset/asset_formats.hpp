#pragma once

#include <DirectXMath.h>
#include <string>
#include <vector>

namespace ren
{
using namespace DirectX;

constexpr static uint32_t MESH_PARENT_INDEX_NO_PARENT = ~0u;

struct Vertex_Data
{
    XMFLOAT4 normal;
    XMFLOAT4 tangent;
    XMFLOAT2 uv;
};

struct Mesh_Data
{
    std::vector<XMFLOAT4> vertex_positions;
    std::vector<Vertex_Data> vertices;
    std::vector<uint32_t> indices;
    uint32_t albedo_uri_index;
    uint32_t normal_uri_index;
    uint32_t metal_roughness_uri_index;
    uint32_t ao_uri_index;
};

struct Model_Data
{
    std::vector<Mesh_Data> meshes;
    std::vector<uint32_t> parent_indices;
    std::vector<XMFLOAT4X4> transforms;
    std::vector<std::string> texture_uris;
};
}
