#include "renderer/techniques/tone_map.hpp"

#include "renderer/asset/asset_repository.hpp"
#include "renderer/resource_state_tracker.hpp"
#include "renderer/gpu_transfer.hpp"

#include <rhi/command_list.hpp>
#include <shared/tone_map_shared_types.h>
#include <imgui.h>

namespace ren::techniques
{
Tone_Map::Tone_Map(Asset_Repository& asset_repository,
    GPU_Transfer_Context& gpu_transfer_context,
    Render_Resource_Blackboard& render_resource_blackboard,
    const bool hdr, const float display_peak_luminance_nits)
    : m_asset_repository(asset_repository)
    , m_gpu_transfer_context(gpu_transfer_context)
    , m_render_resource_blackboard(render_resource_blackboard)
    , m_is_hdr(hdr)
    , m_peak_intensity(display_peak_luminance_nits)
    , m_luminance_target(physical_luminance_to_reference_luminance(m_peak_intensity))
{

    constexpr rhi::Buffer_Create_Info tone_map_buffer_create_info = {
        .size = sizeof(GT7_Tone_Mapping_Data),
        .heap = rhi::Memory_Heap_Type::GPU
    };
    m_tone_map_parameters_buffer = m_render_resource_blackboard.create_buffer(
        TONE_MAP_PARAMETERS_BUFFER_NAME,
        tone_map_buffer_create_info);
}

Tone_Map::~Tone_Map()
{
    m_render_resource_blackboard.destroy_buffer(m_tone_map_parameters_buffer);
}

void Tone_Map::blit_apply(
    rhi::Command_List* cmd,
    Resource_State_Tracker& tracker,
    const Image& source_image,
    const Image& swapchain_image)
{
    cmd->begin_debug_region("tonemap", .75f, 0.f, .25f);

    initialize();

    tracker.use_resource(
        swapchain_image,
        rhi::Barrier_Pipeline_Stage::Color_Attachment_Output,
        rhi::Barrier_Access::Color_Attachment_Write,
        rhi::Barrier_Image_Layout::Color_Attachment,
        true);
    tracker.use_resource(
        source_image,
        rhi::Barrier_Pipeline_Stage::Pixel_Shader,
        rhi::Barrier_Access::Shader_Sampled_Read,
        rhi::Barrier_Image_Layout::Shader_Read_Only);
    tracker.flush_barriers(cmd);
    rhi::Render_Pass_Color_Attachment_Info swapchain_attachment_info = {
        .attachment = swapchain_image,
        .load_op = rhi::Render_Pass_Attachment_Load_Op::Discard,
        .store_op = rhi::Render_Pass_Attachment_Store_Op::Store
    };
    const rhi::Render_Pass_Begin_Info render_pass_info = {
        .color_attachments = { &swapchain_attachment_info, 1 },
        .depth_stencil_attachment = {}
    };

    const auto width = swapchain_image.get_create_info().width;
    const auto height = swapchain_image.get_create_info().height;

    cmd->begin_render_pass(render_pass_info);
    cmd->set_viewport(0.f, 0.f, static_cast<float>(width), static_cast<float>(height), 0.f, 1.f);
    cmd->set_scissor(0, 0, width, height);

    if (m_is_hdr)
        cmd->set_pipeline(m_asset_repository.get_graphics_pipeline("tone_map_hdr"));
    else
        cmd->set_pipeline(m_asset_repository.get_graphics_pipeline("tone_map"));
    cmd->set_push_constants<Tone_Map_Push_Constants>({
            .source_texture = source_image,
            .texture_sampler = m_render_resource_blackboard.get_sampler({
                .filter_min = rhi::Sampler_Filter::Nearest,
                .filter_mag = rhi::Sampler_Filter::Nearest,
                .filter_mip = rhi::Sampler_Filter::Nearest,
                .address_mode_u = rhi::Image_Sample_Address_Mode::Clamp,
                .address_mode_v = rhi::Image_Sample_Address_Mode::Clamp,
                .address_mode_w = rhi::Image_Sample_Address_Mode::Clamp,
                .mip_lod_bias = 0.f,
                .max_anisotropy = 0,
                .comparison_func = rhi::Comparison_Func::None,
                .reduction = rhi::Sampler_Reduction_Type::Standard,
                .border_color = {},
                .min_lod = .0f,
                .max_lod = .0f,
                .anisotropy_enable = false}),
        .tone_map_parameters_buffer = m_tone_map_parameters_buffer,
        .is_enabled = static_cast<uint32_t>(m_is_enabled)
        }, rhi::Pipeline_Bind_Point::Graphics);
    cmd->draw(3, 1, 0, 0);

    cmd->end_render_pass();
    cmd->end_debug_region();
}

void Tone_Map::render_debug(rhi::Command_List* cmd,
    Resource_State_Tracker& tracker,
    const Image& render_target,
    const Buffer& camera)
{
    if (!m_render_debug)
        return;

    cmd->begin_debug_region("tonemap:debug", .75f, 0.f, .25f);
    tracker.use_resource(
        render_target,
        rhi::Barrier_Pipeline_Stage::Color_Attachment_Output,
        rhi::Barrier_Access::Color_Attachment_Write,
        rhi::Barrier_Image_Layout::Color_Attachment);
    tracker.flush_barriers(cmd);

    auto color_attachment_infos = std::to_array<rhi::Render_Pass_Color_Attachment_Info>(
        {{
            .attachment = render_target,
            .load_op = rhi::Render_Pass_Attachment_Load_Op::Load,
            .store_op = rhi::Render_Pass_Attachment_Store_Op::Store,
            .clear_value = {}
        }});
    const rhi::Render_Pass_Begin_Info render_pass_info = {
        .color_attachments = color_attachment_infos
    };
    cmd->begin_render_pass(render_pass_info);

    const uint32_t width = static_cast<rhi::Image*>(render_target)->width;
    const uint32_t height = static_cast<rhi::Image*>(render_target)->height;

    cmd->set_viewport(0.f, 0.f,
        static_cast<float>(width), static_cast<float>(height), 0.f, 1.f);
    cmd->set_scissor(0, 0, width, height);

    cmd->set_pipeline(m_asset_repository.get_graphics_pipeline("tone_map_debug"));

    cmd->set_push_constants<Tone_Map_Debug_Quads_Push_Constants>({
        .aspect = static_cast<float>(width) / static_cast<float>(height)
        }, rhi::Pipeline_Bind_Point::Graphics);
    cmd->draw(6, 1, 0, 0);
    cmd->end_render_pass();
    cmd->end_debug_region();
}

void Tone_Map::set_hdr_state(const bool hdr, const float display_peak_luminance_nits)
{
    m_is_hdr = hdr;
    m_peak_intensity = display_peak_luminance_nits;
}

void Tone_Map::initialize()
{
    if (m_is_hdr)
    {
        m_luminance_target = physical_luminance_to_reference_luminance(m_peak_intensity);
        m_sdr_correction_factor = 1.0f;
    }
    else
    {
        m_luminance_target = physical_luminance_to_reference_luminance(m_sdr_paper_white);
        m_sdr_correction_factor = 1.0f / m_luminance_target;
    }

    m_luminance_target_ictcp = calculate_peak_luminance_ictcp(m_luminance_target);
    m_luminance_target_jzazbz = calculate_peak_luminance_jzazbz(m_luminance_target);

    const float k = (m_linear_section - 1.0f) / (m_alpha - 1.0f);
    m_ka = m_luminance_target * m_linear_section + m_luminance_target * k;
    m_kb = -m_luminance_target * k * glm::exp(m_linear_section / k);
    m_kc = -1.0f / (k * m_luminance_target);

    GT7_Tone_Mapping_Data tone_map_data = {
        .is_hdr = m_is_hdr,
        .reference_luminance = IMAGE_REFERENCE_LUMINANCE,
        .alpha = m_alpha,
        .mid_point = m_mid_point,
        .linear_section = m_linear_section,
        .toe_strength = m_toe_strength,
        .kA = m_ka,
        .kB = m_kb,
        .kC = m_kc,
        .sdr_correction_factor = m_sdr_correction_factor,
        .luminance_target = m_luminance_target,
        .luminance_target_ICtCp = m_luminance_target_ictcp,
        .luminance_target_Jzazbz = m_luminance_target_jzazbz,
        .blend_ratio = m_blend_ratio,
        .fade_start = m_fade_start,
        .fade_end = m_fade_end,
    };
    m_gpu_transfer_context.enqueue_immediate_upload(m_tone_map_parameters_buffer, tone_map_data);
}

void Tone_Map::process_gui()
{
    if (ImGui::CollapsingHeader("Tone Mapping"))
    {
        ImGui::SeparatorText("General##TM");
        ImGui::Checkbox("Enabled", &m_is_enabled);
        ImGui::SliderFloat("SDR paper white", &m_sdr_paper_white, 0.0f, 500.0f);
        ImGui::SeparatorText("Curve##TM");
        ImGui::SliderFloat("Alpha", &m_alpha, 0.f, 1.f);
        ImGui::SliderFloat("Midpoint", &m_mid_point, 0.f, 1.f);
        ImGui::SliderFloat("Linear section", &m_linear_section, 0.f, 1.f);
        ImGui::SliderFloat("Toe strength", &m_toe_strength, 0.f, 5.f);
        ImGui::SeparatorText("Evaluation##TM");
        ImGui::SliderFloat("Blend ratio", &m_blend_ratio, 0.f, 1.f);
        ImGui::SliderFloat("Fade start", &m_fade_start, 0.f, 2.f);
        ImGui::SliderFloat("Fade end", &m_fade_end, 0.f, 2.f);
        ImGui::SeparatorText("Debug##TM");
        ImGui::Checkbox("Display debug colors##TM", &m_render_debug);
    }
}

float Tone_Map::pq_ieotf(const float value, const float exponent_scale)
{
    constexpr static float PQ_m1 = 0.1593017578125f;
    constexpr static float PQ_m2 = 78.84375f;
    constexpr static float PQ_c1 = 0.8359375f;
    constexpr static float PQ_c2 = 18.8515625f;
    constexpr static float PQ_c3 = 18.6875f;
    constexpr static float PQ_PEAK_LUMINANCE = 10000.0f;
    const float ym1 = glm::pow(reference_luminance_to_physical_luminance(value) / PQ_PEAK_LUMINANCE, PQ_m1);
    return glm::pow((PQ_c1 + PQ_c2 * ym1) / (1.0f + PQ_c3 * ym1), PQ_m2 * exponent_scale);
}

float Tone_Map::calculate_peak_luminance_ictcp(const float peak_luminance)
{
    const glm::vec3 peak_luminance_vec = { peak_luminance, peak_luminance, peak_luminance };
    glm::mat3 rgb_to_lms = {
        1688.0f, 2146.0f, 262.0f,
        683.0f, 2951.0f, 462.0f,
        99.0f, 309.0f, 3688.0f
    };
    rgb_to_lms /= 4096.0f;
    rgb_to_lms = glm::transpose(rgb_to_lms);
    const glm::vec3 lms = rgb_to_lms * peak_luminance_vec;
    return 0.5f * pq_ieotf(lms.x, 1.0f) + 0.5f * pq_ieotf(lms.y, 1.0f);
}

float Tone_Map::calculate_peak_luminance_jzazbz(const float peak_luminance)
{
    const glm::vec3 peak_luminance_vec = { peak_luminance, peak_luminance, peak_luminance };
    glm::mat3 rgb_to_lms = {
        0.530004f, 0.355704f, 0.086090f,
        0.289388f, 0.525395f, 0.157481f,
        0.091098f, 0.147588f, 0.734234f
    };
    rgb_to_lms = glm::transpose(rgb_to_lms);
    const glm::vec3 lms = rgb_to_lms * peak_luminance_vec;
    float iz = 0.5f * pq_ieotf(lms.x, 1.7f) + 0.5f * pq_ieotf(lms.y, 1.7f);
    return (0.44f * iz) / (1.0f - 0.56f * iz) - 1.6295499532821566e-11f;
}

float Tone_Map::physical_luminance_to_reference_luminance(const float luminance)
{
    return luminance / IMAGE_REFERENCE_LUMINANCE;
}

float Tone_Map::reference_luminance_to_physical_luminance(const float luminance)
{
    return luminance * IMAGE_REFERENCE_LUMINANCE;
}
}
