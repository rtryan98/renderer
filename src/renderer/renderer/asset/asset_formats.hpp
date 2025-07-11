#pragma once

#include <vector>
#include <rhi/resource.hpp>
#include <DirectXMath.h>

namespace ren
{
constexpr static uint32_t MESH_PARENT_INDEX_NO_PARENT = ~0u;

struct Material
{
    uint32_t unused;
};

struct Submesh
{
    std::array<uint32_t, 2> vertex_position_range;
    std::array<uint32_t, 2> vertex_attribute_range;
    std::array<uint32_t, 2> index_range;
    uint32_t material_index;
};

struct Instance
{
    uint32_t submeshes_range_start;
    uint32_t submeshes_range_end;
    uint32_t parent_index;
    DirectX::XMFLOAT3 translation;
    DirectX::XMFLOAT4 rotation;
    DirectX::XMFLOAT3 scale;
};

struct Model
{
    std::vector<Material> materials;
    std::vector<Submesh> submeshes;
    std::vector<Instance> instances;
    rhi::Buffer* vertex_positions;
    rhi::Buffer* vertex_attributes;
    rhi::Buffer* indices;
};

}
