#pragma once
#include <filesystem>
#include <ankerl/unordered_dense.h>
#include <plf_colony.h>

#include "renderer/asset/shader_library.hpp"
#include "renderer/asset/compute_library.hpp"
#include "renderer/asset/graphics_pipeline_library.hpp"
#include "renderer/logger.hpp"
#include "renderer/asset/pipeline.hpp"
#include "renderer/filesystem/mapped_file.hpp"

namespace rhi
{
    class Graphics_Device;
}

namespace ren
{
struct Asset_Repository_Paths
{
    std::string shaders;
    std::string pipelines;
    std::vector<std::string> shader_include_paths;
    std::string models;
};

class Application;

class Asset_Repository
{
public:
    Asset_Repository(std::shared_ptr<Logger> logger, rhi::Graphics_Device* graphics_device,
        Asset_Repository_Paths&& paths, Application& app);
    ~Asset_Repository();

    [[nodiscard]] rhi::Shader_Blob* get_shader_blob(const std::string_view& name, const std::string_view& variant) const;
    [[nodiscard]] rhi::Shader_Blob* get_shader_blob(const std::string_view& name) const;
    [[nodiscard]] Compute_Pipeline get_compute_pipeline(const std::string_view& name) const;
    [[nodiscard]] Graphics_Pipeline get_graphics_pipeline(const std::string_view& name) const;
    [[nodiscard]] Mapped_File* get_model(const std::string_view& name) const;
    [[nodiscard]] Mapped_File* get_texture(const std::string_view& name) const;
    [[nodiscard]] Mapped_File* get_texture_safe(const std::string_view& name) const;
    [[nodiscard]] std::vector<std::string> get_model_files() const;

    void recompile_shaders();

private:
    void compile_shader_library(
        std::string_view hlsl_path,
        std::string_view json_path,
        const std::vector<std::wstring>& include_dirs);

    void compile_graphics_pipeline_library(const std::string_view& json_path);

    void create_shader_and_compute_libraries();
    void create_graphics_pipeline_libraries();

    void register_textures();
    void register_texture(const std::filesystem::path& path);

    void register_models();
    void register_model(const std::filesystem::path& path);

private:
    std::shared_ptr<Logger> m_logger;
    rhi::Graphics_Device* m_graphics_device;
    Asset_Repository_Paths m_paths;
    Application& m_app;

    class Shader_Compiler;
    std::unique_ptr<Shader_Compiler> m_shader_compiler;

    template<typename T>
    using String_Map = ankerl::unordered_dense::map<std::string, T>;

    // ankerl is fast but has unstable pointers on insert
    // Because of that the maps are not directly storing the data
    String_Map<Shader_Library*> m_shader_library_ptrs = {};
    plf::colony<Shader_Library> m_shader_libraries = {};

    String_Map<Compute_Library*> m_compute_library_ptrs = {};
    plf::colony<Compute_Library> m_compute_libraries = {};

    String_Map<Graphics_Pipeline_Library*> m_pipeline_library_ptrs = {};
    plf::colony<Graphics_Pipeline_Library> m_pipeline_libraries = {};

    String_Map<Mapped_File*> m_model_ptrs = {};
    String_Map<Mapped_File*> m_texture_ptrs = {};
    plf::colony<Mapped_File> m_files = {};
};
}
