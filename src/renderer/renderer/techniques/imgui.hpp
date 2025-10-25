#pragma once

#include "renderer/render_resource_blackboard.hpp"

namespace rhi
{
class Command_List;
}

namespace ren
{
class Asset_Repository;
class GPU_Transfer_Context;
class Resource_State_Tracker;

namespace techniques
{
class Imgui
{
public:
    constexpr static auto VERTEX_BUFFER_NAME = "imgui::vertex_buffer";
    constexpr static auto INDEX_BUFFER_NAME = "imgui::index_buffer";
    constexpr static auto FONT_TEXTURE_NAME = "imgui::font_texture";

    Imgui(Asset_Repository& asset_repository,
        GPU_Transfer_Context& gpu_transfer_context,
        Render_Resource_Blackboard& render_resource_blackboard);
    ~Imgui();

    Imgui(const Imgui&) = delete;
    Imgui& operator=(const Imgui&) = delete;
    Imgui(Imgui&&) = delete;
    Imgui& operator=(Imgui&&) = delete;

    void render(rhi::Command_List* cmd, const Image& target);

private:
    Asset_Repository& m_asset_repository;
    GPU_Transfer_Context& m_gpu_transfer_context;
    Render_Resource_Blackboard& m_render_resource_blackboard;

    Buffer m_vertex_buffer;
    Buffer m_index_buffer;
    Image m_font_texture;

private:
    void setup_render_state(rhi::Command_List* cmd, const Image& image) const;
    void setup_style();
};
}
}
