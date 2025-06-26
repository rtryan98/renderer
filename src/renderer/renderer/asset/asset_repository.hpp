#pragma once
#include <ankerl/unordered_dense.h>
#include "renderer/asset/shader_library.hpp"
#include "renderer/asset/compute_library.hpp"
#include "renderer/asset/pipeline_library.hpp"
#include <rhi/common/array_vector.hpp>
#include "renderer/logger.hpp"

namespace ren
{
struct Asset_Repository_Paths
{
    std::string shaders;
    std::string pipelines;
};

class Asset_Repository
{
public:
    Asset_Repository(std::shared_ptr<Logger> logger, const Asset_Repository_Paths& paths);
    ~Asset_Repository();

    Shader_Library* get_shader_library(std::string_view name) const;
    Compute_Library* get_compute_library(std::string_view name) const;
    Pipeline_Library* get_pipeline_library(std::string_view name) const;

private:
    void compile_shader_library(std::string_view hlsl_path, std::string_view json_path);

private:

    std::shared_ptr<Logger> m_logger;
    Asset_Repository_Paths m_paths;

    class Shader_Compiler;
    std::unique_ptr<Shader_Compiler> m_shader_compiler;

    constexpr static auto ARRAY_VECTOR_SIZE = 128u;

    template<typename T>
    using String_Map = ankerl::unordered_dense::map<std::string, T>;

    // ankerl is fast but has unstable pointers on insert
    // Because of that the maps are not directly storing the data
    String_Map<Shader_Library*> m_shader_library_ptrs = {};
    rhi::Array_Vector<Shader_Library, ARRAY_VECTOR_SIZE> m_shader_libraries = {};

    String_Map<Compute_Library*> m_compute_library_ptrs = {};
    rhi::Array_Vector<Compute_Library, ARRAY_VECTOR_SIZE> m_compute_libraries = {};

    String_Map<Pipeline_Library*> m_pipeline_library_ptrs = {};
    rhi::Array_Vector<Pipeline_Library, ARRAY_VECTOR_SIZE> m_pipeline_libraries = {};
};
}
