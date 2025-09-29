#pragma once

#include <plf_colony.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <rhi/resource.hpp>
#include <shared/draw_shared_types.h>
#include <shared/scene_shared_types.h>
#include <offsetAllocator.hpp>

#include "ankerl/unordered_dense.h"
#include "glm/ext/matrix_transform.hpp"
#include "renderer/logger.hpp"

#include <array>

namespace rhi
{
class Graphics_Device;
}

namespace ren
{
class Asset_Repository;
class GPU_Transfer_Context;
class Acceleration_Structure_Builder;

enum class Material_Alpha_Mode
{
    Opaque,
    Mask,
    Blend
};

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
    Material_Alpha_Mode alpha_mode;
    bool double_sided;
};

struct TRS
{
    glm::vec3 translation;
    glm::quat rotation;
    glm::vec3 scale;

    [[nodiscard]] glm::mat4 to_mat() const noexcept;
    [[nodiscard]] glm::mat4 to_transform(const glm::mat4& parent = glm::identity<glm::mat4>()) const noexcept;
    [[nodiscard]] glm::mat3 adjugate(const glm::mat4& parent = glm::identity<glm::mat4>()) const noexcept;
};

struct Submesh
{
    uint32_t first_index;
    uint32_t index_count;
    uint32_t first_vertex;
    glm::vec3 aabb_min;
    glm::vec3 aabb_max;
    Material* material;
    rhi::Acceleration_Structure* blas;
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
    rhi::Buffer* blas_allocation;
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
    constexpr static auto MAX_LIGHTS = 1 << 10; // 1024 lights

    constexpr static auto INSTANCE_TRANSFORM_BUFFER_SIZE = sizeof(GPU_Instance_Transform_Data) * MAX_TRANSFORMS;
    constexpr static auto MATERIAL_INSTANCE_BUFFER_SIZE = sizeof(GPU_Material) * MAX_MATERIALS;
    constexpr static auto INSTANCE_INDICES_BUFFER_SIZE = sizeof(GPU_Instance_Indices) * MAX_INSTANCES;
    constexpr static auto LIGHT_BUFFER_SIZE = sizeof(Punctual_Light) * MAX_LIGHTS;

    Static_Scene_Data(
        rhi::Graphics_Device* graphics_device,
        std::shared_ptr<Logger> logger,
        GPU_Transfer_Context& gpu_transfer_context,
        Asset_Repository& asset_repository,
        Render_Resource_Blackboard& render_resource_blackboard,
        Acceleration_Structure_Builder& acceleration_structure_builder);
    ~Static_Scene_Data();

    Static_Scene_Data(const Static_Scene_Data&) = delete;
    Static_Scene_Data& operator=(const Static_Scene_Data&) = delete;
    Static_Scene_Data(Static_Scene_Data&&) = delete;
    Static_Scene_Data& operator=(Static_Scene_Data&&) = delete;

    void add_model(const Model_Descriptor& model_descriptor);

    [[nodiscard]] auto& get_models() const noexcept { return m_models; }
    [[nodiscard]] auto& get_instances() const noexcept { return m_model_Instances; }
    [[nodiscard]] auto* get_index_buffer() const noexcept { return m_global_index_buffer; }

    void update_lights();
    void update_tlas();

private:
    uint32_t acquire_instance_index();
    uint32_t acquire_material_index();
    uint32_t acquire_transform_index();

    rhi::Image* get_or_create_image(const std::string& uri, rhi::Image* replacement);

    void create_default_images();

private:
    rhi::Graphics_Device* m_graphics_device;
    std::shared_ptr<Logger> m_logger;
    GPU_Transfer_Context& m_gpu_transfer_context;
    Asset_Repository& m_asset_repository;
    Render_Resource_Blackboard& m_render_resource_blackboard;
    Acceleration_Structure_Builder& m_acceleration_structure_builder;

    OffsetAllocator::Allocator m_index_buffer_allocator;

    std::vector<uint32_t> m_instance_freelist = {};
    std::vector<uint32_t> m_material_freelist = {};
    std::vector<uint32_t> m_transform_freelist = {};

    std::vector<Material> m_materials = {};

    plf::colony<Model> m_models = {};
    plf::colony<Model_Instance> m_model_Instances = {};
    std::vector<Punctual_Light> m_punctual_lights = {};

    ankerl::unordered_dense::map<std::string, rhi::Image*> m_images = {};
    rhi::Buffer* m_global_index_buffer = nullptr;
    rhi::Buffer* m_transform_buffer = nullptr;
    rhi::Buffer* m_material_buffer = nullptr;
    rhi::Buffer* m_instance_buffer = nullptr;
    rhi::Buffer* m_light_buffer = nullptr;
    rhi::Buffer* m_scene_info_buffer = nullptr;
    std::array<rhi::Buffer*, REN_MAX_FRAMES_IN_FLIGHT> m_tlas_instance_buffers = {};
    rhi::Buffer* m_tlas_buffer = nullptr;

    rhi::Acceleration_Structure* m_tlas = nullptr;

    rhi::Image* m_default_albedo_tex = nullptr;
    rhi::Image* m_default_normal_tex = nullptr;
    rhi::Image* m_default_metallic_roughness_tex = nullptr;
    rhi::Image* m_default_emissive_tex = nullptr;
    Material m_default_material = {};
};
}
