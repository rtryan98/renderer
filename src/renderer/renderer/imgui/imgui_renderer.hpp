#pragma once

#include <rhi/graphics_device.hpp>
#include <imgui.h>

namespace ren
{
struct Imgui_Renderer_Create_Info
{
    rhi::Graphics_Device* device;
    uint32_t frames_in_flight;
    rhi::Image_Format swapchain_image_format;
};

class Imgui_Renderer
{
public:
    Imgui_Renderer(const Imgui_Renderer_Create_Info& create_info);
    ~Imgui_Renderer() noexcept;

    Imgui_Renderer(const Imgui_Renderer& other) = delete;
    Imgui_Renderer(Imgui_Renderer&& other) = delete;
    Imgui_Renderer& operator=(const Imgui_Renderer& other) = delete;
    Imgui_Renderer& operator=(Imgui_Renderer&& other) = delete;

    void next_frame() noexcept;
    void render(rhi::Command_List* cmd) noexcept;
    void create_fonts_texture() noexcept;

private:
    void setup_render_state(
        rhi::Command_List* cmd,
        rhi::Buffer* index_buffer,
        ImDrawData* draw_data) noexcept;

private:
    rhi::Graphics_Device* m_device;
    std::vector<rhi::Buffer*> m_vertex_buffers;
    std::vector<rhi::Buffer*> m_index_buffers;
    std::vector<rhi::Image*> m_images;
    rhi::Shader_Blob* m_vertex_shader;
    rhi::Shader_Blob* m_pixel_shader;
    rhi::Pipeline* m_pipeline;
    rhi::Sampler* m_sampler;
    uint32_t m_frames_in_flight;
    uint32_t m_frame_index;
};
}
