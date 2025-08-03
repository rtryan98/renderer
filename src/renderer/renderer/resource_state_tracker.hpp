#pragma once

#include <ankerl/unordered_dense.h>
#include <rhi/command_list.hpp>

namespace ren
{
class Buffer;
class Image;

class Resource_State_Tracker
{
public:
    void use_resource(
        const Buffer& buffer,
        rhi::Barrier_Pipeline_Stage stage,
        rhi::Barrier_Access access);
    void use_resource(
        const Image& image,
        rhi::Barrier_Pipeline_Stage stage,
        rhi::Barrier_Access access,
        rhi::Barrier_Image_Layout layout,
        bool discard = false);
    void set_resource_state(
        const Buffer& buffer,
        rhi::Barrier_Pipeline_Stage stage,
        rhi::Barrier_Access access);
    void set_resource_state(
        const Image& image,
        rhi::Barrier_Pipeline_Stage stage,
        rhi::Barrier_Access access,
        rhi::Barrier_Image_Layout layout);

    void flush_barriers(rhi::Command_List* cmd);

private:
    using Identifier = void*;

    struct Resource_State
    {
        rhi::Buffer* buffer;
        rhi::Image* image;

        rhi::Barrier_Pipeline_Stage stage_before;
        rhi::Barrier_Pipeline_Stage stage_after;
        rhi::Barrier_Access access_before;
        rhi::Barrier_Access access_after;
        rhi::Barrier_Image_Layout image_layout_before;
        rhi::Barrier_Image_Layout image_layout_after;
        bool discard;

        auto operator<=>(const Resource_State& other) const;
    };

    Resource_State& get_resource_state(Identifier identifier);

    static Identifier decay_resource(const Image& image);
    static Identifier decay_resource(const Buffer& buffer);

    ankerl::unordered_dense::map<Identifier, Resource_State> m_resource_states;
    std::vector<Identifier> m_pending_barriers;
};
}
