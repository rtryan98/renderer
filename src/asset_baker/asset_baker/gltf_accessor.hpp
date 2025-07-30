#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <fastgltf/core.hpp>

namespace asset_baker
{
void get_indices(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, std::vector<uint32_t>& indices_out);
void get_positions(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, std::vector<glm::vec3>& positions_out);
void get_colors(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, std::vector<glm::vec4>& colors_out);
void get_normals(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, std::vector<glm::vec3>& normals_out);
void get_tangents(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, std::vector<glm::vec4>& tangents_out);
void get_tex_coords(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, std::vector<glm::vec2>& tex_coords_out);
void get_joints(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, std::vector<glm::uvec4>& joints_out);
void get_weights(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, std::vector<glm::vec4>& weights_out);
}
