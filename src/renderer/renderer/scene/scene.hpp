#pragma once

#include <plf_colony.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <rhi/resource.hpp>
#include <shared/draw_shared_types.h>
#include <offsetAllocator.hpp>

#include "ankerl/unordered_dense.h"
#include "glm/ext/matrix_transform.hpp"
#include "renderer/logger.hpp"

namespace rhi
{
class Graphics_Device;
}

namespace ren
{
class Application;
class Asset_Repository;

struct Material
{
    uint32_t material_index;
    glm::u8vec4 base_color_factor;
    float pbr_roughness;
    float pbr_metallic;
    glm::vec3 emissive_color;
    float emissive_strength;
    rhi::Image* albedo;
    rhi::Image* normal;
    rhi::Image* metallic_roughness;
    rhi::Image* emissive;
    rhi::Sampler* sampler;
};

struct TRS
{
    glm::vec3 translation;
    glm::quat rotation;
    glm::vec3 scale;

    [[nodiscard]] glm::mat4 to_mat() const noexcept;
    [[nodiscard]] glm::mat4 to_transform(const glm::mat4& parent = glm::identity<glm::mat4>()) const noexcept;
    [[nodiscard]] glm::mat3 to_transposed_adjugate(const glm::mat4& parent = glm::identity<glm::mat4>()) const noexcept;
};

struct Submesh
{
    uint32_t first_index;
    uint32_t index_count;
    uint32_t first_vertex;
    glm::vec3 aabb_min;
    glm::vec3 aabb_max;
    Material* default_material;
};

struct Submesh_Instance
{
    Submesh* submesh;
    Material* material;
    uint32_t instance_index; // Points to both transform and material
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
    uint32_t transform_index;
    TRS trs;
    glm::mat4 mesh_to_world;
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
    glm::mat4 model_to_world;
    std::vector<Mesh_Instance> mesh_instances;
};

struct Model_Descriptor
{
    std::string name;
    std::vector<TRS> instances;
};

class Render_Resource_Blackboard;

class Static_Scene_Data
{
public:
    constexpr static auto MAX_INDICES = 1 << 25; // ~32M indices
    constexpr static auto INDEX_BUFFER_SIZE = sizeof(uint32_t) * MAX_INDICES;
    constexpr static auto MAX_TRANSFORMS = 1 << 20; // ~1M transforms (meshes)
    constexpr static auto MAX_MATERIALS = 1 << 17; // 128k Materials
    constexpr static auto MAX_INSTANCES = 1 << 22; // ~4M instances

    constexpr static auto INSTANCE_TRANSFORM_BUFFER_SIZE = sizeof(GPU_Instance_Transform_Data) * MAX_TRANSFORMS;
    constexpr static auto MATERIAL_INSTANCE_BUFFER_SIZE = sizeof(GPU_Material) * MAX_MATERIALS;
    constexpr static auto INSTANCE_INDICES_BUFFER_SIZE = sizeof(GPU_Instance_Indices) * MAX_INSTANCES;

    Static_Scene_Data(Application& app, std::shared_ptr<Logger> logger,
        Asset_Repository& asset_repository,
        Render_Resource_Blackboard& render_resource_blackboard,
        rhi::Graphics_Device* graphics_device);
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
    [[nodiscard]] auto* get_transform_buffer() const noexcept { return m_transform_buffer; }
    [[nodiscard]] auto* get_material_buffer() const noexcept { return m_material_buffer; }
    [[nodiscard]] auto* get_instance_buffer() const noexcept { return m_instance_buffer; }

private:
    uint32_t acquire_instance_index();
    uint32_t acquire_material_index();
    uint32_t acquire_transform_index();

    rhi::Image* get_or_create_image(const std::string& uri);

private:
    Application& m_app;
    std::shared_ptr<Logger> m_logger;
    Asset_Repository& m_asset_repository;
    Render_Resource_Blackboard& m_render_resource_blackboard;
    rhi::Graphics_Device* m_graphics_device;

    OffsetAllocator::Allocator m_index_buffer_allocator;

    std::vector<uint32_t> m_instance_freelist = {};
    std::vector<uint32_t> m_material_freelist = {};
    std::vector<uint32_t> m_transform_freelist = {};

    std::vector<Material> m_materials = {};

    plf::colony<Model> m_models = {};
    plf::colony<Model_Instance> m_model_Instances = {};

    ankerl::unordered_dense::map<std::string, rhi::Image*> m_images = {};
    rhi::Buffer* m_global_index_buffer = nullptr;
    rhi::Buffer* m_transform_buffer = nullptr;
    rhi::Buffer* m_material_buffer = nullptr;
    rhi::Buffer* m_instance_buffer = nullptr;
};
}
