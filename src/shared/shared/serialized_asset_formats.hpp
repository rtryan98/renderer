#ifndef SERIALIZED_ASSET_FORMATS_HPP
#define SERIALIZED_ASSET_FORMATS_HPP
#ifdef __cplusplus

#include <cstdint>
#include "rhi/resource.hpp"

namespace serialization
{
enum class Attribute_Flags : uint32_t
{
    None = 0x0,
    Color = 0x1,
    Normal = 0x2,
    Tangent = 0x4,
    Tex_Coords = 0x8,
    Joints = 0x10,
    Weights = 0x20,
};
}

template<>
constexpr static bool RHI_ENABLE_BIT_OPERATORS<serialization::Attribute_Flags> = true;

namespace serialization
{
constexpr static auto NAME_MAX_SIZE = 159ull;
constexpr static auto NAME_FIELD_SIZE = NAME_MAX_SIZE + 1ull;
constexpr static auto HASH_IDENTIFIER_FIELD_SIZE = 32ull;
constexpr static auto TEXTURE_MAX_MIP_LEVELS = 14;

constexpr static auto MODEL_FILE_EXTENSION = ".renmdl"; // renderer model container
constexpr static auto TEXTURE_FILE_EXTENSION = ".rentex"; // renderer texture container

struct Image_Mip_Data
{
    uint32_t width;
    uint32_t height;
    void* data;
};

struct Image_Header
{
    constexpr static uint32_t MAGIC = 0x58455452u; // RTEX

    // can't directly set value, otherwise no longer trivial type
    uint32_t magic;
    uint32_t version;

    bool validate()
    {
        return magic == MAGIC && version == 1;
    }
};

struct Image_Mip_Metadata
{
    uint32_t width;
    uint32_t height;
};

struct Image_Data_00
{
    Image_Header header;
    uint32_t mip_count;
    rhi::Image_Format format;
    char name[NAME_FIELD_SIZE];
    char hash_identifier[HASH_IDENTIFIER_FIELD_SIZE];
    Image_Mip_Metadata mips[TEXTURE_MAX_MIP_LEVELS];

    char* get_mip_data(const uint32_t mip_level)
    {
        auto ptr = reinterpret_cast<char*>(this);
        ptr += sizeof(Image_Data_00);
        for (uint32_t i = 0; i < mip_level; ++i)
        {
            ptr += rhi::get_image_format_info(format).bytes * mips[i].width * mips[i].height;
        }
        return ptr;
    }
};

struct Vertex_Attributes
{
    std::array<float, 3> normal;
    std::array<float, 4> tangent;
    std::array<float, 2> tex_coords;
    std::array<uint8_t, 4> color;
};

struct Vertex_Skin_Attributes
{
    std::array<uint32_t, 4> joints;
    std::array<float, 4> weights;
};

static std::size_t calculate_total_attribute_size(Attribute_Flags flags)
{
    const auto is_same = [&](const Attribute_Flags rhs)
    {
        return (flags & rhs) == rhs;
    };

    std::size_t result = 0;
    result += is_same(Attribute_Flags::Color) * sizeof(uint32_t);
    result += is_same(Attribute_Flags::Normal) * sizeof(float) * 3;
    result += is_same(Attribute_Flags::Tangent) * sizeof(float) * 3;
    result += is_same(Attribute_Flags::Tex_Coords) * sizeof(float) * 2;
    result += is_same(Attribute_Flags::Joints) * sizeof(uint32_t) * 4;
    result += is_same(Attribute_Flags::Weights) * sizeof(float) * 4;
    return result;
};

struct URI_Reference_00
{
    char value[NAME_FIELD_SIZE];
};

struct Mesh_Material_00
{
    constexpr static auto URI_NO_REFERENCE = ~0u;

    float base_color_factor[4];
    float pbr_roughness;
    float pbr_metallic;
    float emissive_color[3];
    float emissive_strength;
    uint32_t albedo_uri_index;
    uint32_t normal_uri_index;
    uint32_t metallic_roughness_uri_index;
    uint32_t emissive_uri_index;
};

struct Submesh_Data_Ranges_00
{
    uint32_t attribute_flags;
    uint32_t material_index;
    uint32_t vertex_position_range_start;
    uint32_t vertex_position_range_end;
    uint32_t vertex_attribute_range_start;
    uint32_t vertex_attribute_range_end;
    uint32_t vertex_skin_attribute_range_start;
    uint32_t vertex_skin_attribute_range_end;
    uint32_t index_range_start;
    uint32_t index_range_end;
};

struct Mesh_Instance_00
{
    uint32_t submeshes_range_start;
    uint32_t submeshes_range_end;
    uint32_t parent_index;
    float translation[3];
    float rotation[4];
    float scale[3];
};

struct Model_Header
{
    constexpr static uint32_t MAGIC = 0x4C444D52u; // RMDL

    // can't directly set value, otherwise no longer trivial type
    uint32_t magic;
    uint32_t version;

    bool validate()
    {
        return magic == MAGIC && version == 1;
    }
};

struct Model_Header_00
{
    Model_Header header;
    char name[NAME_FIELD_SIZE];
    uint32_t referenced_uri_count;          // URI_Reference_00
    uint32_t material_count;                // Mesh_Material_00
    uint32_t submesh_count;                 // Submesh_Data_Ranges_00
    uint32_t instance_count;                // Mesh_Instance_00
    uint32_t vertex_position_count;         // std::array<float, 3>
    uint32_t vertex_attribute_count;        // Vertex_Attributes
    uint32_t vertex_skin_attribute_count;   // Vertex_Skin_Attributes
    uint32_t index_count;                   // uint32_t

    // Data is ordered in the way it was declared.
    // That means first all referenced URIs are listed, then all materials, and so on.

    static std::size_t get_referenced_uris_offset()
    {
        return sizeof(Model_Header_00);
    }

    URI_Reference_00* get_referenced_uris()
    {
        auto ptr = reinterpret_cast<char*>(this);
        ptr += get_referenced_uris_offset();
        return reinterpret_cast<URI_Reference_00*>(ptr);
    }

    std::size_t get_materials_offset() const
    {
        return get_referenced_uris_offset()
            + referenced_uri_count * sizeof(URI_Reference_00);
    }

    Mesh_Material_00* get_materials()
    {
        auto ptr = reinterpret_cast<char*>(this);
        ptr += get_materials_offset();
        return reinterpret_cast<Mesh_Material_00*>(ptr);
    }

    std::size_t get_submeshes_offset() const
    {
        return get_materials_offset()
            + material_count * sizeof(Mesh_Material_00);
    }

    Submesh_Data_Ranges_00* get_submeshes()
    {
        auto ptr = reinterpret_cast<char*>(this);
        ptr += get_submeshes_offset();
        return reinterpret_cast<Submesh_Data_Ranges_00*>(ptr);
    }

    std::size_t get_instances_offset() const
    {
        return get_submeshes_offset()
            + submesh_count * sizeof(Submesh_Data_Ranges_00);
    }

    Mesh_Instance_00* get_instances()
    {
        auto ptr = reinterpret_cast<char*>(this);
        ptr += get_instances_offset();
        return reinterpret_cast<Mesh_Instance_00*>(ptr);
    }

    std::size_t get_vertex_positions_offset() const
    {
        return get_instances_offset()
            + instance_count * sizeof(Mesh_Instance_00);
    }

    float* get_vertex_positions()
    {
        auto ptr = reinterpret_cast<char*>(this);
        ptr += get_vertex_positions_offset();
        return reinterpret_cast<float*>(ptr);
    }

    std::size_t get_vertex_attributes_offset() const
    {
        return get_vertex_positions_offset()
            + vertex_position_count * 12; // sizeof(float[3]);
    }

    float* get_vertex_attributes()
    {
        auto ptr = reinterpret_cast<char*>(this);
        ptr += get_vertex_attributes_offset();
        return reinterpret_cast<float*>(ptr);
    }

    std::size_t get_vertex_skin_attributes_offset() const
    {
        return get_vertex_attributes_offset()
            + vertex_attribute_count * sizeof(Vertex_Attributes);
    }

    Vertex_Skin_Attributes* get_vertex_skin_attributes()
    {
        auto ptr = reinterpret_cast<char*>(this);
        ptr += get_vertex_skin_attributes_offset();
        return reinterpret_cast<Vertex_Skin_Attributes*>(ptr);
    }

    std::size_t get_indices_offset() const
    {
        return get_vertex_skin_attributes_offset()
            + vertex_skin_attribute_count * sizeof(Vertex_Skin_Attributes);
    }

    uint32_t* get_indices()
    {
        auto ptr = reinterpret_cast<char*>(this);
        ptr += get_indices_offset();
        return reinterpret_cast<uint32_t*>(ptr);
    }

    std::size_t get_size() const
    {
        auto size = sizeof(Model_Header_00);
        size += (referenced_uri_count * sizeof(URI_Reference_00));
        size += (material_count * sizeof(Mesh_Material_00));
        size += (submesh_count * sizeof(Submesh_Data_Ranges_00));
        size += (instance_count * sizeof(Mesh_Instance_00));
        size += (vertex_position_count * 12); //sizeof(float[3]));
        size += (vertex_attribute_count * sizeof(Vertex_Attributes));
        size += (vertex_skin_attribute_count * sizeof(Vertex_Skin_Attributes));
        size += (index_count * sizeof(uint32_t));
        return size;
    }
};
}

#endif
#endif
