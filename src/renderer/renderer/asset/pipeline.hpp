#pragma once
#include <string_view>

namespace rhi
{
struct Pipeline;
}

namespace ren
{
struct Compute_Library;
struct Compute_Pipeline_Wrapper;
struct Graphics_Pipeline_Library;

class Compute_Pipeline
{
public:
    Compute_Pipeline() = default;
    explicit Compute_Pipeline(Compute_Library* compute_library);
    operator rhi::Pipeline*() const; //NOLINT

    Compute_Pipeline& set_variant(std::string_view name);

private:
    Compute_Library* m_compute_library;
    Compute_Pipeline_Wrapper* m_active_pipeline;
};

class Graphics_Pipeline
{
public:
    Graphics_Pipeline() = default;
    explicit Graphics_Pipeline(Graphics_Pipeline_Library* graphics_pipeline_library);
    operator rhi::Pipeline*() const; // NOLINT

private:
    Graphics_Pipeline_Library* m_graphics_pipeline_library;
    rhi::Pipeline* m_active_pipeline;
};
}
