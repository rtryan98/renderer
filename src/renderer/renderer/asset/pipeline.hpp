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

class Compute_Pipeline
{
public:
    explicit Compute_Pipeline(Compute_Library* compute_library);
    operator rhi::Pipeline*() const; //NOLINT

    Compute_Pipeline& set_variant(std::string_view name);

private:
    Compute_Library* m_compute_library;
    Compute_Pipeline_Wrapper* m_active_pipeline;
};
}
