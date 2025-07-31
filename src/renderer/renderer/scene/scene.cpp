#include "renderer/scene/scene.hpp"

#include <numeric>
#include <ranges>
#include <shared/serialized_asset_formats.hpp>

#include "glm/gtc/quaternion.hpp"
#include "renderer/application.hpp"
#include "renderer/asset/asset_formats.hpp"

namespace ren
{
glm::mat4 TRS::to_mat() const noexcept
{
    auto s = glm::scale(glm::identity<glm::mat4>(), scale);
    auto r = glm::mat4_cast(rotation);
    auto t = glm::translate(glm::identity<glm::mat4>(), translation);
    return t * r * s;
}

glm::mat4 TRS::to_transform(const glm::mat4& parent) const noexcept
{
    return parent * to_mat();
}

glm::mat3 TRS::to_transposed_adjugate(const glm::mat4& parent) const noexcept
{
    const glm::mat3 intermediate = to_transform(parent);
    glm::vec3 r0 = glm::cross(intermediate[1], intermediate[2]);
    glm::vec3 r1 = glm::cross(intermediate[2], intermediate[0]);
    glm::vec3 r2 = glm::cross(intermediate[0], intermediate[1]);
    return { // already transposed
        r0.x, r1.x, r2.x,
        r0.y, r1.y, r2.y,
        r0.z, r1.z, r2.z,
    };
}

void Static_Scene_Data::add_model(const Model_Descriptor& model_descriptor)
{
    auto& model = *m_models.emplace();
    auto* loadable_model = static_cast<serialization::Model_Header_00*>(
        m_asset_repository.get_model(model_descriptor.name)->data);
    m_logger->info("Loading model '{}'", model_descriptor.name);

    // create buffers and upload the data
    {
        rhi::Buffer_Create_Info buffer_create_info = {
            .size = loadable_model->vertex_position_count * sizeof(std::array<float, 3>),
            .heap = rhi::Memory_Heap_Type::GPU
        };
        model.vertex_positions = m_graphics_device->create_buffer(buffer_create_info).value_or(nullptr);
        m_graphics_device->name_resource(model.vertex_positions, (std::string("gltf:") + model_descriptor.name + ":position").c_str());
        buffer_create_info.size = loadable_model->vertex_attribute_count * sizeof(serialization::Vertex_Attributes);
        model.vertex_attributes = m_graphics_device->create_buffer(buffer_create_info).value_or(nullptr);
        m_graphics_device->name_resource(model.vertex_attributes, (std::string("gltf:") + model_descriptor.name + ":attributes").c_str());
        model.index_buffer_allocation = m_index_buffer_allocator.allocate(loadable_model->index_count);

        auto* positions = loadable_model->get_vertex_positions();
        m_app.upload_buffer_data_immediate(
            model.vertex_positions,
            positions,
            loadable_model->vertex_position_count * sizeof(std::array<float, 3>),
            0);

        auto* attributes = loadable_model->get_vertex_attributes();
        m_app.upload_buffer_data_immediate(
            model.vertex_attributes,
            attributes,
            loadable_model->vertex_attribute_count * sizeof(serialization::Vertex_Attributes),
            0);

        auto* indices = loadable_model->get_indices();
        m_app.upload_buffer_data_immediate(
            m_global_index_buffer,
            indices,
            loadable_model->index_count * sizeof(std::uint32_t),
            model.index_buffer_allocation.offset * sizeof(std::uint32_t));
    }

    model.materials.resize(loadable_model->material_count);
    for (auto i = 0; i < loadable_model->material_count; ++i)
    {
        const auto& loadable_material = loadable_model->get_materials()[i];
        model.materials[i] = &*m_materials.emplace();
        auto& material = *model.materials[i];

        auto get_material_texture = [&](uint32_t index) -> rhi::Image* {
            const auto* uris = loadable_model->get_referenced_uris();
            if (index == ~0u)
            {
                return nullptr;
            }
            return get_or_create_image(uris[index].value);
        };

        rhi::Sampler_Create_Info default_sampler_create_info = {
            .filter_min = rhi::Sampler_Filter::Linear,
            .filter_mag = rhi::Sampler_Filter::Linear,
            .filter_mip = rhi::Sampler_Filter::Linear,
            .address_mode_u = rhi::Image_Sample_Address_Mode::Wrap,
            .address_mode_v = rhi::Image_Sample_Address_Mode::Wrap,
            .address_mode_w = rhi::Image_Sample_Address_Mode::Wrap,
            .mip_lod_bias = 0.f,
            .max_anisotropy = 16,
            .comparison_func = rhi::Comparison_Func::None,
            .reduction = rhi::Sampler_Reduction_Type::Standard,
            .border_color = {},
            .min_lod = 0.f,
            .max_lod = 1000.f,
            .anisotropy_enable = true
        };

        material = {
            .base_color_factor = {
                loadable_material.base_color_factor[0],
                loadable_material.base_color_factor[1],
                loadable_material.base_color_factor[2],
                loadable_material.base_color_factor[3]
            },
            .pbr_roughness = loadable_material.pbr_roughness,
            .pbr_metallic = loadable_material.pbr_metallic,
            .emissive_color = {
                loadable_material.emissive_color[0],
                loadable_material.emissive_color[1],
                loadable_material.emissive_color[2]
            },
            .emissive_strength = loadable_material.emissive_strength,
            .albedo = get_material_texture(loadable_material.albedo_uri_index),
            .normal = get_material_texture(loadable_material.normal_uri_index),
            .metallic_roughness = get_material_texture(loadable_material.metallic_roughness_uri_index),
            .emissive = get_material_texture(loadable_material.emissive_uri_index),
            .sampler = m_render_resource_blackboard.get_sampler(default_sampler_create_info)
        };
    }

    model.submeshes.resize(loadable_model->submesh_count);
    for (auto i = 0; i < loadable_model->submesh_count; ++i)
    {
        const auto& loadable_submesh = loadable_model->get_submeshes()[i];
        auto& submesh = model.submeshes[i];
        submesh.first_index = loadable_submesh.index_range_start;
        submesh.index_count = loadable_submesh.index_range_end - loadable_submesh.index_range_start;
        submesh.first_vertex = loadable_submesh.vertex_position_range_start;
        submesh.aabb_min = {};
        submesh.aabb_max = {};
        submesh.default_material = loadable_submesh.material_index != MESH_PARENT_INDEX_NO_PARENT
            ? model.materials[loadable_submesh.material_index]
            : nullptr;
    }

    model.meshes.resize(loadable_model->instance_count);
    for (auto i = 0; i < loadable_model->instance_count; ++i)
    {
        const auto& loadable_mesh_instance = loadable_model->get_instances()[i];
        auto& mesh = model.meshes[i];
        mesh.parent = loadable_mesh_instance.parent_index != MESH_PARENT_INDEX_NO_PARENT
            ? &model.meshes[loadable_mesh_instance.parent_index]
            : nullptr;
        mesh.trs = {
            .translation = {
                loadable_mesh_instance.translation[0],
                loadable_mesh_instance.translation[1],
                loadable_mesh_instance.translation[2]
            },
            .rotation = glm::quat(
                loadable_mesh_instance.rotation[0],
                loadable_mesh_instance.rotation[1],
                loadable_mesh_instance.rotation[2],
                loadable_mesh_instance.rotation[3]
            ),
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
            mesh.submeshes.emplace_back(&model.submeshes[j]);
        }
    }

    for (auto& model_instance_descriptor : model_descriptor.instances)
    {
        auto& model_instance = *m_model_Instances.emplace();
        model_instance.model = &model;
        model_instance.trs = model_instance_descriptor;
        model_instance.mesh_instances.resize(model.meshes.size());
        for (auto i = 0; i < model.meshes.size(); ++i)
        {
            auto& mesh = model.meshes[i];
            auto& mesh_instance = model_instance.mesh_instances[i];
            mesh_instance.mesh = &mesh;
            mesh_instance.parent = nullptr;
            if (mesh.parent)
            {
                const auto index = mesh.parent - &model.meshes[0];
                mesh_instance.parent = &model_instance.mesh_instances[index];
            }
            mesh_instance.trs = mesh.trs;
            mesh_instance.submesh_instances.reserve(mesh.submeshes.size());
            for (auto* submesh : mesh.submeshes)
            {
                auto& submesh_instance = mesh_instance.submesh_instances.emplace_back();
                submesh_instance.submesh = submesh;
                submesh_instance.material = submesh->default_material;
                submesh_instance.instance_index = acquire_instance_index();
            }
        }
    }
}

void Static_Scene_Data::process_gui()
{
    if (ImGui::Begin("Scene Data", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        uint32_t i = 0;
        for (auto& model_instance : m_model_Instances)
        {
            ImGui::PushID(i++);
            ImGui::InputFloat3("Translation", &model_instance.trs.translation[0]);
            ImGui::PopID();
        }
        ImGui::End();
    }
}

uint32_t Static_Scene_Data::acquire_instance_index()
{
    const auto val = m_instance_freelist.back();
    m_instance_freelist.pop_back();
    return val;
}

rhi::Image* Static_Scene_Data::get_or_create_image(const std::string& uri)
{
    if (m_images.contains(uri))
    {
        return m_images[uri];
    }

    const auto texture_file = m_asset_repository.get_texture_safe(uri);
    if (!texture_file)
        return nullptr;

    m_logger->info("Loading texture {}", uri);
    auto loadable_image = static_cast<serialization::Image_Data_00*>(texture_file->data);
    rhi::Image_Create_Info texture_create_info = {
        .format = loadable_image->format,
        .width = loadable_image->mips[0].width,
        .height = loadable_image->mips[0].height,
        .depth = 1,
        .array_size = 1,
        .mip_levels = static_cast<uint16_t>(loadable_image->mip_count),
        .usage = rhi::Image_Usage::Sampled,
        .primary_view_type = rhi::Image_View_Type::Texture_2D
    };
    auto image = m_graphics_device->create_image(texture_create_info).value_or(nullptr);
    std::array<void*, 14> mip_data;
    for (auto mip = 0; mip < loadable_image->mip_count; ++mip)
    {
        mip_data[mip] = loadable_image->get_mip_data(mip);
    }
    m_app.upload_image_data_immediate_full(image, mip_data.data());
    m_images[uri] = image;

    return m_images[uri];
}

Static_Scene_Data::Static_Scene_Data(Application& app, std::shared_ptr<Logger> logger,
                                     Asset_Repository& asset_repository,
                                     Render_Resource_Blackboard& render_resource_blackboard,
                                     rhi::Graphics_Device* graphics_device)
    : m_app(app)
    , m_logger(std::move(logger))
    , m_asset_repository(asset_repository)
    , m_render_resource_blackboard(render_resource_blackboard)
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
    buffer_create_info.size = MATERIAL_INSTANCE_BUFFER_SIZE;
    m_global_material_instance_buffer = graphics_device->create_buffer(buffer_create_info).value_or(nullptr);
    m_graphics_device->name_resource(m_global_material_instance_buffer, "scene:global_material_instance_buffer");
}

Static_Scene_Data::~Static_Scene_Data()
{
    m_graphics_device->wait_idle();
    m_graphics_device->destroy_buffer(m_global_index_buffer);
    m_graphics_device->destroy_buffer(m_global_instance_transform_buffer);
    m_graphics_device->destroy_buffer(m_global_material_instance_buffer);
    for (const auto& model : m_models)
    {
        m_graphics_device->destroy_buffer(model.vertex_positions);
        m_graphics_device->destroy_buffer(model.vertex_attributes);
    }
    for (const auto image : m_images | std::views::values)
    {
        m_graphics_device->destroy_image(image);
    }
}
}
