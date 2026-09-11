#include "Pch.h"

#include "Controllers/DeadzoneShaper.h"

#include "Controllers/GamePortInputMixer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GetDefaultDeadzone
//
////////////////////////////////////////////////////////////////////////////////

float DeadzoneShaper::GetDefaultDeadzone (ControllerKind kind)
{
    return kind == ControllerKind::XInput ? kXInputStickDeadzone : kDirectInputDeadzone;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShapeAxis
//
//  One axis on its own. Inside the deadzone reads exactly center; outside it,
//  the remaining travel is stretched back over the full range.
//
////////////////////////////////////////////////////////////////////////////////

float DeadzoneShaper::ShapeAxis (float value, float deadzone)
{
    float  limited   = std::clamp (deadzone, 0.0f, kMaxDeadzone);
    float  magnitude = std::abs (value);
    float  shaped    = 0.0f;



    if (magnitude <= limited)
    {
        return 0.0f;
    }

    shaped = (magnitude - limited) / (1.0f - limited);
    shaped = std::min (shaped, 1.0f);

    return value < 0.0f ? -shaped : shaped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShapeStick
//
//  Both axes of one stick. The deadzone is a circle around center: how far
//  the stick is pushed decides whether it is inside it, not how far each axis
//  happens to be on its own.
//
////////////////////////////////////////////////////////////////////////////////

void DeadzoneShaper::ShapeStick (float x, float y, float deadzone, float & outX, float & outY)
{
    float  limited   = std::clamp (deadzone, 0.0f, kMaxDeadzone);
    float  magnitude = std::sqrt (x * x + y * y);
    float  shaped    = 0.0f;
    float  scale     = 0.0f;



    outX = 0.0f;
    outY = 0.0f;

    if (magnitude <= limited || magnitude <= 0.0f)
    {
        return;
    }

    // A stick cannot be pushed further than its own rim, so the rim is full
    // deflection however the push is divided between the axes: at the
    // diagonal limit each axis reads about 0.707, and that is as far over as
    // the stick goes. Measuring against the raw magnitude instead would make
    // a diagonal push read short of the ends, which is the corner a game
    // never reaches.
    magnitude = std::min (magnitude, 1.0f);
    shaped    = std::min ((magnitude - limited) / (1.0f - limited), 1.0f);
    scale     = shaped / magnitude;

    outX = std::clamp (x * scale, -1.0f, 1.0f);
    outY = std::clamp (y * scale, -1.0f, 1.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToPaddle
//
//  [-1, 1] to the game port's 0-255, with center on the same value every
//  other input source rests at.
//
////////////////////////////////////////////////////////////////////////////////

Byte DeadzoneShaper::ToPaddle (float value)
{
    constexpr float  kBelowCenter = 127.0f;
    constexpr float  kAboveCenter = 128.0f;
    float            clamped      = std::clamp (value, -1.0f, 1.0f);
    float            scaled       = 0.0f;



    scaled = clamped < 0.0f ? GamePortState::kPaddleCenter + clamped * kBelowCenter
                            : GamePortState::kPaddleCenter + clamped * kAboveCenter;

    return (Byte) std::lround (std::clamp (scaled, 0.0f, 255.0f));
}
