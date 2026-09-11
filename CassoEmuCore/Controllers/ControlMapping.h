#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"





enum class AxisResponse
{
    Absolute,
    Rate,
};





enum class AxisBindingKind
{
    Analog,
    DigitalPair,
};





////////////////////////////////////////////////////////////////////////////////
//
//  AxisBinding
//
//  One control driving one paddle axis. An analog control gives position
//  directly, or with Rate moves the paddle while it is deflected and leaves it
//  where it was on release, which is how a self-centering stick plays paddle
//  games. A digital pair drives the axis to one end or the other while held.
//
////////////////////////////////////////////////////////////////////////////////

struct AxisBinding
{
    static constexpr float  kDefaultMaxSpeed = 256.0f;

    AxisBindingKind  kind      = AxisBindingKind::Analog;
    ControlId        analog;
    bool             inverted  = false;
    AxisResponse     response  = AxisResponse::Absolute;
    float            maxSpeed  = kDefaultMaxSpeed;
    ControlId        negative;
    ControlId        positive;

    bool operator== (const AxisBinding &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ButtonBinding
//
//  One control driving one pushbutton. An analog control counts as pressed
//  past its threshold, so a trigger can be a fire button.
//
////////////////////////////////////////////////////////////////////////////////

struct ButtonBinding
{
    static constexpr float  kDefaultThreshold = 0.5f;
    static constexpr float  kTriggerThreshold = 30.0f / 255.0f;

    ControlId  control;
    float      threshold         = kDefaultThreshold;
    bool       negativeDirection = false;

    bool operator== (const ButtonBinding &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControlMapping
//
//  What each game-port target is driven by. A target with nothing assigned
//  rests: an axis at center, a button released.
//
////////////////////////////////////////////////////////////////////////////////

struct ControlMapping
{
    std::vector<AxisBinding>    pdl0;
    std::vector<AxisBinding>    pdl1;
    std::vector<ButtonBinding>  pb0;
    std::vector<ButtonBinding>  pb1;
    std::vector<ButtonBinding>  pb2;

    bool operator== (const ControlMapping &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DefaultMapping
//
//  The mapping a controller gets before anyone edits one.
//
////////////////////////////////////////////////////////////////////////////////

class DefaultMapping
{
public:

    static ControlMapping  For (const ControllerModelKey & model, const std::vector<ControlId> & controls);

private:

    static bool  HasControl (const std::vector<ControlId> & controls, const ControlId & control);
};
