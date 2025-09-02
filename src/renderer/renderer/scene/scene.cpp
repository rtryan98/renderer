#include "renderer/scene/scene.hpp"

#include "renderer/gpu_transfer.hpp"
#include "renderer/asset/asset_formats.hpp"
#include "renderer/asset/asset_repository.hpp"

#include <numeric>
#include <ranges>
#include <shared/serialized_asset_formats.hpp>
#include <rhi/graphics_device.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/quaternion.hpp>

#include "renderer/render_resource_blackboard.hpp"

#include <shared/shared_resources.h>

namespace ren
{
constexpr static auto DEFAULT_SAMPLER_CREATE_INFO = rhi::Sampler_Create_Info {
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

glm::mat4 TRS::to_mat() const noexcept
{
    const auto s = glm::scale(glm::identity<glm::mat4>(), scale);
    const auto r = glm::mat4_cast(rotation);
    const auto t = glm::translate(glm::identity<glm::mat4>(), translation);
    return t * r * s;
}

glm::mat4 TRS::to_transform(const glm::mat4& parent) const noexcept
{
    return parent * to_mat();
}

glm::mat3 TRS::adjugate(const glm::mat4& parent) const noexcept
{
    const glm::mat3 m = to_transform(parent);
    return {
        glm::cross(m[1], m[2]),
        glm::cross(m[2], m[0]),
        glm::cross(m[0], m[1])
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
        m_gpu_transfer_context.enqueue_immediate_upload(
            model.vertex_positions,
            positions,
            loadable_model->vertex_position_count * sizeof(std::array<float, 3>),
            0);

        auto* attributes = loadable_model->get_vertex_attributes();
        m_gpu_transfer_context.enqueue_immediate_upload(
            model.vertex_attributes,
            attributes,
            loadable_model->vertex_attribute_count * sizeof(serialization::Vertex_Attributes),
            0);

        auto* indices = loadable_model->get_indices();
        m_gpu_transfer_context.enqueue_immediate_upload(
            m_global_index_buffer,
            indices,
            loadable_model->index_count * sizeof(std::uint32_t),
            model.index_buffer_allocation.offset * sizeof(std::uint32_t));
    }

    model.materials.resize(loadable_model->material_count);
    for (auto i = 0; i < loadable_model->material_count; ++i)
    {
        const auto& loadable_material = loadable_model->get_materials()[i];
        auto material_index = acquire_material_index();
        model.materials[i] = &m_materials[material_index];
        auto& material = *model.materials[i];

        auto get_material_texture = [&](uint32_t index, rhi::Image* replacement) -> rhi::Image* {
            const auto* uris = loadable_model->get_referenced_uris();
            if (index == ~0u)
            {
                return replacement;
            }
            return get_or_create_image(uris[index].value, replacement);
        };

        material = {
            .material_index = material_index,
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
            .albedo = get_material_texture(loadable_material.albedo_uri_index, m_default_albedo_tex),
            .normal = get_material_texture(loadable_material.normal_uri_index, m_default_normal_tex),
            .metallic_roughness = get_material_texture(loadable_material.metallic_roughness_uri_index, m_default_metallic_roughness_tex),
            .emissive = get_material_texture(loadable_material.emissive_uri_index, m_default_emissive_tex),
            .sampler = m_render_resource_blackboard.get_sampler(DEFAULT_SAMPLER_CREATE_INFO),
            .alpha_mode = static_cast<Material_Alpha_Mode>(loadable_material.alpha_mode),
            .double_sided = static_cast<bool>(loadable_material.double_sided),
        };

        auto get_image_index = [&](const rhi::Image* image)
        {
            if (!image) return ~0u;
            return image->image_view->bindless_index;
        };

        GPU_Material gpu_material = {
            .base_color_factor = glm::packUint4x8(material.base_color_factor),
            .pbr_roughness = material.pbr_roughness,
            .pbr_metallic = material.pbr_metallic,
            .emissive_color = material.emissive_color,
            .emissive_strength = material.emissive_strength,
            .albedo = get_image_index(material.albedo),
            .normal = get_image_index(material.normal),
            .metallic_roughness = get_image_index(material.metallic_roughness),
            .emissive = get_image_index(material.emissive),
            .sampler_id = material.sampler->bindless_index
        };

        m_gpu_transfer_context.enqueue_immediate_upload(
            m_material_buffer,
            &gpu_material,
            sizeof(GPU_Material),
            material.material_index * sizeof(GPU_Material));
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
        submesh.material = loadable_submesh.material_index != MESH_PARENT_INDEX_NO_PARENT
            ? model.materials[loadable_submesh.material_index]
            : &m_default_material;
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
            mesh_instance.transform_index = acquire_transform_index();
            mesh_instance.trs = mesh.trs;
            mesh_instance.submesh_instances.reserve(mesh.submeshes.size());
            for (auto* submesh : mesh.submeshes)
            {
                auto& submesh_instance = mesh_instance.submesh_instances.emplace_back();
                submesh_instance.submesh = submesh;
                submesh_instance.material = submesh->material;
                submesh_instance.instance_index = acquire_instance_index();
            }
        }
        for (auto& mesh_instance : model_instance.mesh_instances)
        {
            if (mesh_instance.parent)
                mesh_instance.mesh_to_world = mesh_instance.trs.to_transform(mesh_instance.parent->mesh_to_world);
            else
                mesh_instance.mesh_to_world = mesh_instance.trs.to_mat();
        }
        for (const auto& mesh_instance : model_instance.mesh_instances)
        {
            GPU_Instance_Transform_Data instance_transform_data = {
                .mesh_to_world = mesh_instance.mesh_to_world,
                .normal_to_world = mesh_instance.trs.adjugate(
                    mesh_instance.parent != nullptr
                    ? mesh_instance.parent->mesh_to_world
                    : glm::identity<glm::mat4>())
            };
            m_gpu_transfer_context.enqueue_immediate_upload(
                m_transform_buffer,
                &instance_transform_data,
                sizeof(GPU_Instance_Transform_Data),
                mesh_instance.transform_index * sizeof(GPU_Instance_Transform_Data));
            for (const auto& submesh_instance : mesh_instance.submesh_instances)
            {
                GPU_Instance_Indices instance_indices = {
                    .transform_index = mesh_instance.transform_index,
                    .material_index = submesh_instance.material->material_index
                };
                m_gpu_transfer_context.enqueue_immediate_upload(
                    m_instance_buffer,
                    &instance_indices,
                    sizeof(instance_indices),
                    submesh_instance.instance_index * sizeof(GPU_Instance_Indices));
            }
        }
    }
}

uint32_t Static_Scene_Data::acquire_instance_index()
{
    const auto val = m_instance_freelist.back();
    m_instance_freelist.pop_back();
    return val;
}

uint32_t Static_Scene_Data::acquire_material_index()
{
    const auto val = m_material_freelist.back();
    m_material_freelist.pop_back();
    return val;
}

uint32_t Static_Scene_Data::acquire_transform_index()
{
    const auto val = m_transform_freelist.back();
    m_transform_freelist.pop_back();
    return val;
}

rhi::Image* Static_Scene_Data::get_or_create_image(const std::string& uri, rhi::Image* replacement)
{
    if (m_images.contains(uri))
    {
        return m_images[uri];
    }

    const auto texture_file = m_asset_repository.get_texture_safe(uri);
    if (!texture_file)
        return replacement;

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
    m_graphics_device->name_resource(image, (std::string("gltf:") + loadable_image->name).c_str());
    std::array<void*, 14> mip_data{};
    for (auto mip = 0; mip < loadable_image->mip_count; ++mip)
    {
        mip_data[mip] = loadable_image->get_mip_data(mip);
    }
    m_gpu_transfer_context.enqueue_immediate_upload(image, mip_data.data());
    m_images[uri] = image;

    return m_images[uri];
}

void Static_Scene_Data::create_default_images()
{
    rhi::Image_Create_Info default_texture_create_info = {
        .format = rhi::Image_Format::R8G8B8A8_SRGB,
        .width = 2,
        .height = 2,
        .depth = 1,
        .array_size = 1,
        .mip_levels = 1,
        .usage = rhi::Image_Usage::Sampled,
        .primary_view_type = rhi::Image_View_Type::Texture_2D
    };

    m_default_albedo_tex = m_graphics_device->create_image(default_texture_create_info).value_or(nullptr);
    m_graphics_device->name_resource(m_default_albedo_tex, "scene:default_albedo_texture");
    m_default_emissive_tex = m_graphics_device->create_image(default_texture_create_info).value_or(nullptr);
    m_graphics_device->name_resource(m_default_emissive_tex, "scene:default_emissive_texture");

    default_texture_create_info.format = rhi::Image_Format::R8G8B8A8_UNORM;
    m_default_normal_tex = m_graphics_device->create_image(default_texture_create_info).value_or(nullptr);
    m_graphics_device->name_resource(m_default_normal_tex, "scene:default_normal_texture");
    m_default_metallic_roughness_tex = m_graphics_device->create_image(default_texture_create_info).value_or(nullptr);
    m_graphics_device->name_resource(m_default_metallic_roughness_tex, "scene:default_metallic_roughness_texture");

    std::array<uint8_t, 16> default_missing_texture_data = {
        255, 255, 255, 255,
        255, 255, 255, 255,
        255, 255, 255, 255,
        255, 255, 255, 255,
    };

    std::array<uint8_t, 16> default_normal_data = {
        127, 127, 255, 0,
        127, 127, 255, 0,
        127, 127, 255, 0,
        127, 127, 255, 0,
    };

    std::array<uint8_t, 16> default_empty_attributes_data = {
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    };

    void* data = default_missing_texture_data.data();
    m_gpu_transfer_context.enqueue_immediate_upload(m_default_albedo_tex, &data);
    data = default_normal_data.data();
    m_gpu_transfer_context.enqueue_immediate_upload(m_default_normal_tex, &data);
    data = default_empty_attributes_data.data();
    m_gpu_transfer_context.enqueue_immediate_upload(m_default_metallic_roughness_tex, &data);
    m_gpu_transfer_context.enqueue_immediate_upload(m_default_emissive_tex, &data);
}

Static_Scene_Data::Static_Scene_Data(
    rhi::Graphics_Device* graphics_device,
    std::shared_ptr<Logger> logger,
    GPU_Transfer_Context& gpu_transfer_context,
    Asset_Repository& asset_repository,
    Render_Resource_Blackboard& render_resource_blackboard)
    : m_graphics_device(graphics_device)
    , m_logger(std::move(logger))
    , m_gpu_transfer_context(gpu_transfer_context)
    , m_asset_repository(asset_repository)
    , m_render_resource_blackboard(render_resource_blackboard)
    , m_index_buffer_allocator(MAX_INDICES)
{
    m_instance_freelist.resize(MAX_INSTANCES);
    std::iota(m_instance_freelist.rbegin(), m_instance_freelist.rend(), 0);

    m_material_freelist.resize(MAX_INSTANCES);
    std::iota(m_material_freelist.rbegin(), m_material_freelist.rend(), 0);
    m_materials.resize(MAX_MATERIALS);

    m_transform_freelist.resize(MAX_INSTANCES);
    std::iota(m_transform_freelist.rbegin(), m_transform_freelist.rend(), 0);

    rhi::Buffer_Create_Info buffer_create_info = {
        .size = INDEX_BUFFER_SIZE,
        .heap = rhi::Memory_Heap_Type::GPU
    };
    m_global_index_buffer = graphics_device->create_buffer(buffer_create_info, REN_GLOBAL_INDEX_BUFFER).value_or(nullptr);
    m_graphics_device->name_resource(m_global_index_buffer, "scene:global_index_buffer");
    buffer_create_info.size = INSTANCE_TRANSFORM_BUFFER_SIZE;
    m_transform_buffer = graphics_device->create_buffer(buffer_create_info, REN_GLOBAL_INSTANCE_TRANSFORM_BUFFER).value_or(nullptr);
    m_graphics_device->name_resource(m_transform_buffer, "scene:instance_transform_buffer");
    buffer_create_info.size = MATERIAL_INSTANCE_BUFFER_SIZE;
    m_material_buffer = graphics_device->create_buffer(buffer_create_info, REN_GLOBAL_MATERIAL_INSTANCE_BUFFER).value_or(nullptr);
    m_graphics_device->name_resource(m_material_buffer, "scene:material_instance_buffer");
    buffer_create_info.size = INSTANCE_INDICES_BUFFER_SIZE;
    m_instance_buffer = graphics_device->create_buffer(buffer_create_info, REN_GLOBAL_INSTANCE_INDICES_BUFFER).value_or(nullptr);
    m_graphics_device->name_resource(m_instance_buffer, "scene:instance_indices_buffer");

    create_default_images();

    m_default_material = {
        .material_index = acquire_material_index(),
        .base_color_factor = glm::u8vec4(255, 255, 255, 255),
        .pbr_roughness = 1.0f,
        .pbr_metallic = 0.0f,
        .emissive_color = glm::vec3(0.0f, 0.0f, 0.0f),
        .emissive_strength = 0.0f,
        .albedo = m_default_albedo_tex,
        .normal = m_default_normal_tex,
        .metallic_roughness = m_default_metallic_roughness_tex,
        .emissive = m_default_emissive_tex,
        .sampler = m_render_resource_blackboard.get_sampler(DEFAULT_SAMPLER_CREATE_INFO),
        .alpha_mode = Material_Alpha_Mode::Opaque,
        .double_sided = false
    };
}

Static_Scene_Data::~Static_Scene_Data()
{
    m_graphics_device->wait_idle();
    m_graphics_device->destroy_buffer(m_global_index_buffer);
    m_graphics_device->destroy_buffer(m_transform_buffer);
    m_graphics_device->destroy_buffer(m_material_buffer);
    m_graphics_device->destroy_buffer(m_instance_buffer);
    m_graphics_device->destroy_image(m_default_albedo_tex);
    m_graphics_device->destroy_image(m_default_normal_tex);
    m_graphics_device->destroy_image(m_default_metallic_roughness_tex);
    m_graphics_device->destroy_image(m_default_emissive_tex);
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
