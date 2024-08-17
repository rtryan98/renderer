#include "shared/ocean_shared_types.h"
#include "shared/camera_shared_types.h"
#include "shaders/ocean/debug_render_types.hlsli"

PS_Out main(PS_In ps_in)
{
    PS_Out result = { ps_in.color };
    return result;
}
