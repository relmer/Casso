#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeadzoneShaper
//
//  Turns a normalized axis reading into a paddle position: the rest area
//  around center reads as center, and what is left is rescaled so the edge of
//  that area is center and full deflection still reaches the ends. Without
//  the rescale a deadzone would cost travel at both ends.
//
//  A pair of axes from one stick is shaped together, so the rest area is a
//  circle rather than a square. Shaped separately, a diagonal push would
//  leave one axis inside its deadzone and stick to a cardinal direction.
//
////////////////////////////////////////////////////////////////////////////////

class DeadzoneShaper
{
public:

    // XInput publishes this for its sticks; DirectInput publishes nothing, so
    // the second value is Casso's own choice (research R8).
    static constexpr float  kXInputStickDeadzone  = 7849.0f / 32767.0f;
    static constexpr float  kDirectInputDeadzone  = 0.12f;
    static constexpr float  kMaxDeadzone          = 0.9f;

    static float  GetDefaultDeadzone (ControllerKind kind);
    static float  ShapeAxis          (float value, float deadzone);
    static void   ShapeStick         (float x, float y, float deadzone, float & outX, float & outY);
    static Byte   ToPaddle           (float value);
};
