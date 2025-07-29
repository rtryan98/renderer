#pragma once

#include <cstdint>
#include <SDL3/SDL_scancode.h>

namespace ren
{
typedef SDL_Scancode Key_Code;

enum class Mouse_Button : uint8_t
{
    Mouse_Left = 1,
    Mouse_Middle = 2,
    Mouse_Right = 3,
    Mouse_X1 = 4,
    Mouse_X2 = 5
};
}
