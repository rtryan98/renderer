#include "renderer/scene/scene.hpp"

#include <numeric>
#include <shared/serialized_asset_formats.hpp>
#include "renderer/application.hpp"

namespace ren
{
XMMATRIX TRS::to_mat() const noexcept
{
    return XMMatrixAffineTransformation(
        XMLoadFloat3(&scale),
        XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
        XMLoadFloat4(&rotation),
        XMLoadFloat3(&translation));
}

XMFLOAT3X4 TRS::to_transform(const XMMATRIX& parent) const noexcept
{
    XMFLOAT3X4 result;
    XMStoreFloat3x4(&result, XMMatrixMultiplyTranspose(parent, to_mat()));
    return result;
}

XMFLOAT3X3 TRS::to_transposed_adjugate(const XMMATRIX& parent) const noexcept
{
    const auto intermediate = XMMatrixMultiply(parent, to_mat());
    XMFLOAT3 r0 = {};
    XMStoreFloat3(&r0, XMVector3Cross(intermediate.r[1], intermediate.r[2]));
    XMFLOAT3 r1 = {};
    XMStoreFloat3(&r1, XMVector3Cross(intermediate.r[2], intermediate.r[0]));
    XMFLOAT3 r2 = {};
    XMStoreFloat3(&r2, XMVector3Cross(intermediate.r[0], intermediate.r[1]));
    return { // already transposed
        r0.x, r1.x, r2.x,
        r0.y, r1.y, r2.y,
        r0.z, r1.z, r2.z,
    };
}

void Static_Scene_Data::add_model(const Model_Descriptor& model_descriptor)
{
    auto* model = m_models.acquire();
    auto* loadable_model = static_cast<serialization::Model_Header_00*>(
        m_asset_repository.get_model(model_descriptor.name)->data);
    m_logger->info("Loading model '{}'", model_descriptor.name);

    // create buffers and upload the data
    {
        rhi::Buffer_Create_Info buffer_create_info = {
            .size = loadable_model->vertex_position_count * sizeof(std::array<float, 3>),
            .heap = rhi::Memory_Heap_Type::GPU
        };
        model->vertex_positions = m_graphics_device->create_buffer(buffer_create_info).value_or(nullptr);
        m_graphics_device->name_resource(model->vertex_positions, (std::string("gltf:") + model_descriptor.name + ":position").c_str());
        buffer_create_info.size = loadable_model->vertex_attribute_count * sizeof(serialization::Vertex_Attributes);
        model->vertex_attributes = m_graphics_device->create_buffer(buffer_create_info).value_or(nullptr);
        m_graphics_device->name_resource(model->vertex_attributes, (std::string("gltf:") + model_descriptor.name + ":attributes").c_str());
        model->index_buffer_allocation = m_index_buffer_allocator.allocate(loadable_model->index_count);

        auto* positions = loadable_model->get_vertex_positions();
        m_app.upload_buffer_data_immediate(
            model->vertex_positions,
            positions,
            loadable_model->vertex_position_count * sizeof(std::array<float, 3>),
            0);

        auto* attributes = loadable_model->get_vertex_attributes();
        m_app.upload_buffer_data_immediate(
            model->vertex_attributes,
            attributes,
            loadable_model->vertex_attribute_count * sizeof(serialization::Vertex_Attributes),
            0);

        auto* indices = loadable_model->get_indices();
        m_app.upload_buffer_data_immediate(
            m_global_index_buffer,
            indices,
            loadable_model->index_count * sizeof(std::uint32_t),
            model->index_buffer_allocation.offset * sizeof(std::uint32_t));
    }

    model->materials.resize(loadable_model->material_count);
    for (auto i = 0; i < loadable_model->material_count; ++i)
    {
        const auto& loadable_material = loadable_model->get_materials()[i];
        model->materials[i] = m_materials.acquire();
        auto& material = *model->materials[i];
        material.unused = 42;
    }

    model->submeshes.resize(loadable_model->submesh_count);
    for (auto i = 0; i < loadable_model->submesh_count; ++i)
    {
        const auto& loadable_submesh = loadable_model->get_submeshes()[i];
        auto& submesh = model->submeshes[i];
        submesh.first_index = loadable_submesh.index_range_start;
        submesh.index_count = loadable_submesh.index_range_end - loadable_submesh.index_range_start;
        submesh.first_vertex = loadable_submesh.vertex_position_range_start;
        submesh.aabb_min = {};
        submesh.aabb_max = {};
        submesh.default_material = loadable_submesh.material_index != MESH_PARENT_INDEX_NO_PARENT
            ? model->materials[loadable_submesh.material_index]
            : nullptr;
    }

    model->meshes.resize(loadable_model->instance_count);
    for (auto i = 0; i < loadable_model->instance_count; ++i)
    {
        const auto& loadable_mesh_instance = loadable_model->get_instances()[i];
        auto& mesh = model->meshes[i];
        mesh.parent = loadable_mesh_instance.parent_index != MESH_PARENT_INDEX_NO_PARENT
            ? &model->meshes[loadable_mesh_instance.parent_index]
            : nullptr;
        mesh.trs = {
            .translation = {
                loadable_mesh_instance.translation[0],
                loadable_mesh_instance.translation[1],
                loadable_mesh_instance.translation[2]
            },
            .rotation = {
                loadable_mesh_instance.rotation[0],
                loadable_mesh_instance.rotation[1],
                loadable_mesh_instance.rotation[2],
                loadable_mesh_instance.rotation[3]
            },
            .scale = {
                loadable_mesh_instance.scale[0],
                loadable_mesh_instance.scale[1],
                loadable_mesh_instance.scale[2]
            }
        };
        const auto start = loadable_mesh_instance.submeshes_range_start;
        const auto end = loadable_mesh_instance.submeshes_range_end;
        mesh.submeshes.reserve(end - start);
        for (auto j = start; j < end; ++j)
        {
            mesh.submeshes.emplace_back(&model->submeshes[j]);
        }
    }

    for (auto& model_instance_descriptor : model_descriptor.instances)
    {
        auto* model_instance = m_model_Instances.acquire();
        model_instance->model = model;
        model_instance->trs = model_instance_descriptor;
        model_instance->mesh_instances.resize(model->meshes.size());
        for (auto i = 0; i < model->meshes.size(); ++i)
        {
            auto& mesh = model->meshes[i];
            auto& mesh_instance = model_instance->mesh_instances[i];
            mesh_instance.mesh = &mesh;
            mesh_instance.parent = nullptr;
            if (mesh.parent)
            {
                const auto index = mesh.parent - &model->meshes[0];
                mesh_instance.parent = &model_instance->mesh_instances[index];
            }
            mesh_instance.trs = mesh.trs;
            mesh_instance.instance_transform_index = acquire_instance_transform_index();
            mesh_instance.submesh_instances.reserve(mesh.submeshes.size());
            for (auto* submesh : mesh.submeshes)
            {
                auto& submesh_instance = mesh_instance.submesh_instances.emplace_back();
                submesh_instance.submesh = submesh;
                submesh_instance.material = submesh->default_material;
            }
        }
    }
}

uint32_t Static_Scene_Data::acquire_instance_transform_index()
{
    const auto val = m_instance_freelist.back();
    m_instance_freelist.pop_back();
    return val;
}

Static_Scene_Data::Static_Scene_Data(Application& app, std::shared_ptr<Logger> logger,
    Asset_Repository& asset_repository, rhi::Graphics_Device* graphics_device)
    : m_app(app)
    , m_logger(std::move(logger))
    , m_asset_repository(asset_repository)
    , m_graphics_device(graphics_device)
    , m_index_buffer_allocator(MAX_INDICES)
{
    m_instance_freelist.resize(MAX_INSTANCES);
    std::iota(m_instance_freelist.rbegin(), m_instance_freelist.rend(), 0);

    rhi::Buffer_Create_Info buffer_create_info = {
        .size = INDEX_BUFFER_SIZE,
        .heap = rhi::Memory_Heap_Type::GPU
    };
    m_global_index_buffer = graphics_device->create_buffer(buffer_create_info).value_or(nullptr);
    m_graphics_device->name_resource(m_global_index_buffer, "scene:global_index_buffer");
    buffer_create_info.size = INSTANCE_TRANSFORM_BUFFER_SIZE;
    m_global_instance_transform_buffer = graphics_device->create_buffer(buffer_create_info).value_or(nullptr);
    m_graphics_device->name_resource(m_global_instance_transform_buffer, "scene:global_instance_transform_buffer");
}

Static_Scene_Data::~Static_Scene_Data()
{
    m_graphics_device->wait_idle();
    m_graphics_device->destroy_buffer(m_global_index_buffer);
    m_graphics_device->destroy_buffer(m_global_instance_transform_buffer);
    for (auto& model : m_models)
    {
        m_graphics_device->destroy_buffer(model.vertex_positions);
        m_graphics_device->destroy_buffer(model.vertex_attributes);
    }
}
}
