#pragma once

#include <rhi/common/array_vector.hpp>
#include <DirectXMath.h>
#include <string>
#include <rhi/resource.hpp>
#include <shared/draw_shared_types.h>
#include <offsetAllocator.hpp>

namespace rhi
{
class Graphics_Device;
}

namespace ren
{
class Application;
class Asset_Repository;

constexpr static auto SCENE_SMALL_ARRAY_VEC_SIZE = 32;
constexpr static auto SCENE_MEDIUM_ARRAY_VEC_SIZE = 256;

struct Material
{
    uint32_t unused;
};

struct TRS
{
    DirectX::XMFLOAT3 translation;
    DirectX::XMFLOAT4 rotation;
    DirectX::XMFLOAT3 scale;

    [[nodiscard]] DirectX::XMMATRIX to_mat() const noexcept;
    [[nodiscard]] DirectX::XMFLOAT3X3 to_transposed_adjugate(
        const DirectX::XMMATRIX& parent = DirectX::XMMatrixIdentity()) const noexcept;
};

struct Submesh
{
    uint32_t first_index;
    uint32_t index_count;
    uint32_t first_vertex;
    DirectX::XMFLOAT3 aabb_min;
    DirectX::XMFLOAT3 aabb_max;
    Material* default_material;
};

struct Submesh_Instance
{
    Submesh* submesh;
    Material* material;
};

struct Mesh
{
    Mesh* parent;
    TRS trs;
    std::vector<Submesh*> submeshes;
};

struct Mesh_Instance
{
    Mesh* mesh;
    Mesh_Instance* parent;
    TRS trs;
    uint32_t instance_transform_index;
    std::vector<Submesh_Instance> submesh_instances;
};

struct Model
{
    std::vector<Material*> materials;
    std::vector<Mesh> meshes;
    std::vector<Submesh> submeshes;
    rhi::Buffer* vertex_positions;
    rhi::Buffer* vertex_attributes;
    OffsetAllocator::Allocation index_buffer_allocation;
};

struct Model_Instance
{
    Model* model;
    TRS trs;
    std::vector<Mesh_Instance> mesh_instances;
};

struct Model_Descriptor
{
    std::string name;
    std::vector<TRS> instances;
};

class Static_Scene_Data
{
public:
    constexpr static auto MAX_INDICES = 1 << 20; // ~1M indices
    constexpr static auto INDEX_BUFFER_SIZE = sizeof(uint32_t) * MAX_INDICES; // 4 MiB
    constexpr static auto MAX_INSTANCES = 1 << 21; // ~2M instances
    constexpr static auto INSTANCE_TRANSFORM_BUFFER_SIZE = sizeof(GPU_Instance) * MAX_INSTANCES; // ~168 MiB

    Static_Scene_Data(Application& app, Asset_Repository& asset_repository, rhi::Graphics_Device* graphics_device);
    ~Static_Scene_Data();

    Static_Scene_Data(const Static_Scene_Data&) = delete;
    Static_Scene_Data& operator=(const Static_Scene_Data&) = delete;
    Static_Scene_Data(Static_Scene_Data&&) = delete;
    Static_Scene_Data& operator=(Static_Scene_Data&&) = delete;

    void add_model(const Model_Descriptor& model_descriptor);

    // TODO: those should be const but rhi::Array_Vector currently has no const iterator
    [[nodiscard]] auto& get_models() { return m_models; }
    [[nodiscard]] auto& get_instances() { return m_model_Instances; }
    [[nodiscard]] auto* get_index_buffer() const noexcept { return m_global_index_buffer; }
    [[nodiscard]] auto* get_instance_transform_buffer() const noexcept { return m_global_instance_transform_buffer; }

private:
    uint32_t acquire_instance_transform_index();

private:
    Application& m_app;
    Asset_Repository& m_asset_repository;
    rhi::Graphics_Device* m_graphics_device;
    OffsetAllocator::Allocator m_index_buffer_allocator;

    std::vector<uint32_t> m_instance_freelist = {};
    rhi::Array_Vector<Material, SCENE_MEDIUM_ARRAY_VEC_SIZE> m_materials = {};
    rhi::Array_Vector<Model, SCENE_MEDIUM_ARRAY_VEC_SIZE> m_models = {};
    rhi::Array_Vector<Model_Instance, SCENE_MEDIUM_ARRAY_VEC_SIZE> m_model_Instances = {};
    rhi::Buffer* m_global_index_buffer = nullptr;
    rhi::Buffer* m_global_instance_transform_buffer = nullptr;
};
}
