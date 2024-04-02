#include "renderer/imgui/imgui.hlsli"

DECLARE_PUSH_CONSTANTS(Imgui_Push_Constants, pc);

PS_Out main(PS_In ps_in)
{
    PS_Out ps_out;
    ps_out.col = ps_in.col * 1.0; /* sample */
    return ps_out;
}
