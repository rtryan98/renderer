#include "asset_baker/gltf_accessor.hpp"

#include <fastgltf/tools.hpp>

namespace asset_baker
{
constexpr static auto GLTF_ATTRIBUTE_POSITION = "POSITION";
constexpr static auto GLTF_ATTRIBUTE_COLOR_0 = "COLOR_0";
constexpr static auto GLTF_ATTRIBUTE_NORMAL = "NORMAL";
constexpr static auto GLTF_ATTRIBUTE_TANGENT = "TANGENT";
constexpr static auto GLTF_ATTRIBUTE_TEXCOORD_0 = "TEXCOORD_0";
constexpr static auto GLTF_ATTRIBUTE_JOINTS_0 = "JOINTS_0";
constexpr static auto GLTF_ATTRIBUTE_WEIGHTS_0 = "WEIGHTS_0";

void get_indices(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, std::vector<uint32_t>& indices_out)
{
    if (primitive.indicesAccessor.has_value())
    {
        auto& indices_accessor = asset.accessors.at(primitive.indicesAccessor.value());
        indices_out.resize(indices_accessor.count);
        fastgltf::copyFromAccessor<uint32_t>(
            asset,
            indices_accessor,
            indices_out.data());
    }
}

void get_positions(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive,
                   std::vector<glm::vec3>& positions_out)
{
    if (const auto position_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_POSITION))
    {
        auto& positions_accessor = asset.accessors.at(position_attribute->accessorIndex);
        positions_out.resize(positions_accessor.count);
        fastgltf::copyFromAccessor<fastgltf::math::fvec3>(
            asset,
            positions_accessor,
            positions_out.data());
    }
}

void get_colors(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, std::vector<glm::vec4>& colors_out)
{
    if (const auto color_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_COLOR_0))
    {
        if (color_attribute != primitive.attributes.end())
        {
            auto& colors_accessor = asset.accessors.at(color_attribute->accessorIndex);
            colors_out.resize(colors_accessor.count);
            if (colors_accessor.type == fastgltf::AccessorType::Vec4)
            {
                fastgltf::copyFromAccessor<fastgltf::math::fvec4>(
                    asset,
                    colors_accessor,
                    colors_out.data());
            }
            else
            {
                std::vector<std::array<float, 3>> colors_rgb;
                colors_rgb.resize(colors_accessor.count);
                fastgltf::copyFromAccessor<fastgltf::math::fvec3>(
                    asset,
                    colors_accessor,
                    colors_rgb.data());
                for (auto i = 0; i < colors_accessor.count; ++i)
                {
                    colors_out[i] = {
                        colors_rgb[i][0],
                        colors_rgb[i][1],
                        colors_rgb[i][2],
                        1.f,
                    };
                }
            }
        }
    }
}

void get_normals(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive,
    std::vector<glm::vec3>& normals_out)
{
    if (const auto normal_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_NORMAL))
    {
        if (normal_attribute != primitive.attributes.end())
        {
            auto& normals_accessor = asset.accessors.at(normal_attribute->accessorIndex);
            normals_out.resize(normals_accessor.count);
            fastgltf::copyFromAccessor<fastgltf::math::fvec3>(
                asset,
                normals_accessor,
                normals_out.data());
        }
    }
}

void get_tangents(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive,
    std::vector<glm::vec4>& tangents_out)
{
    if (const auto tangent_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_TANGENT))
    {
        if (tangent_attribute != primitive.attributes.end())
        {
            auto& tangents_accessor = asset.accessors.at(tangent_attribute->accessorIndex);
            tangents_out.resize(tangents_accessor.count);
            fastgltf::copyFromAccessor<fastgltf::math::fvec4>(
                asset,
                tangents_accessor,
                tangents_out.data());
        }
    }
}

void get_tex_coords(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive,
    std::vector<glm::vec2>& tex_coords_out)
{
    if (auto tex_coord_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_TEXCOORD_0))
    {
        if (tex_coord_attribute != primitive.attributes.end())
        {
            auto& tex_coords_accessor = asset.accessors.at(tex_coord_attribute->accessorIndex);
            tex_coords_out.resize(tex_coords_accessor.count);
            fastgltf::copyFromAccessor<fastgltf::math::fvec2>(
                asset,
                tex_coords_accessor,
                tex_coords_out.data());
        }
    }
}

void get_joints(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, std::vector<glm::uvec4>& joints_out)
{
    if (auto joint_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_JOINTS_0))
    {
        if (joint_attribute != primitive.attributes.end())
        {
            auto& joints_accessor = asset.accessors.at(joint_attribute->accessorIndex);
            joints_out.resize(joints_accessor.count);
            fastgltf::copyFromAccessor<fastgltf::math::uvec4>(
                asset,
                joints_accessor,
                joints_out.data());
        }
    }
}

void get_weights(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive,
    std::vector<glm::vec4>& weights_out)
{
    if (auto weights_attribute = primitive.findAttribute(GLTF_ATTRIBUTE_WEIGHTS_0))
    {
        if (weights_attribute != primitive.attributes.end())
        {
            auto& weights_accessor = asset.accessors.at(weights_attribute->accessorIndex);
            weights_out.resize(weights_accessor.count);
            fastgltf::copyFromAccessor<fastgltf::math::fvec4>(
                asset,
                weights_accessor,
                weights_out.data());
        }
    }
}
}
