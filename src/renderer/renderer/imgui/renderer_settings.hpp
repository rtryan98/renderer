#pragma once

#include <string>
#include <vector>

namespace ren
{
class Settings_Base
{
public:
    static void set_pad_scale(float scale);
    static float get_pad_scale() { return s_content_negative_pad; }

    virtual ~Settings_Base() noexcept = default;

    const [[nodiscard]] std::string& get_name() noexcept;
    virtual void process_gui() = 0;

protected:
    Settings_Base(const std::string_view name) noexcept;

private:
    std::string m_name;
    static float s_content_negative_pad;
};

class Renderer_Settings
{
public:
    void process_gui(bool* active);
    void add_settings(Settings_Base* settings) noexcept;

private:
    float m_listbox_width = 150.f;
    uint32_t m_selected = 0;
    std::vector<Settings_Base*> m_settings = {};
};
}
