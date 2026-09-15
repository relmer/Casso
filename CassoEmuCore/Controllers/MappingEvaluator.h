#pragma once

#include "Pch.h"

#include "Controllers/ControlMapping.h"
#include "Controllers/ControllerTypes.h"
#include "Controllers/GamePortInputMixer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MappingEvaluator
//
//  One controller reading plus one mapping becomes what the controller asks
//  of the game port. Buttons read pressed if any of their controls is held;
//  an axis takes whichever of its controls is furthest from center, so a
//  mapping with both a stick and a D-pad on one axis follows whichever is
//  being used.
//
////////////////////////////////////////////////////////////////////////////////

class MappingEvaluator
{
public:

    // Where a rate binding's paddle starts, and returns to on a reset.
    static constexpr float  kRateCenter  = 127.5f;

    // The longest time one reading may move a rate binding's paddle. A reading
    // that arrives after a long wait -- the controller was idle, or Casso was
    // inactive -- would otherwise jump the paddle across the screen at once.
    static constexpr float  kMaxRateStep = 0.05f;

    // axisCount limits which axis targets are evaluated: the ones past it are
    // left absent in the result (FR-035).
    GamePortContribution  Evaluate     (const ControllerSample & sample,
                                        const ControlMapping   & mapping,
                                        float                    deadzone,
                                        float                    elapsedSeconds = 0.0f,
                                        size_t                   axisCount      = GamePortContribution::kAxisCount);

    // Rate bindings' paddles back to center: on a change of selection,
    // profile or machine (FR-021a).
    void                  ResetRate    ();

    // Whether the last evaluation moved a rate binding's paddle. The
    // controller thread keeps reading while one is moving, since a stick held
    // still sends no change events and the paddle must go on moving.
    bool                  IsRateMoving () const;

private:

    static float  ReadAnalog          (const ControllerSample & sample, const ControlId & control);
    static bool   IsHeld              (const ControllerSample & sample, const ControlId & control);
    static bool   IsButtonHeld        (const ControllerSample & sample, const ButtonBinding & binding);
    static bool   IsButtonListHeld    (const ControllerSample & sample, const std::vector<ButtonBinding> & bindings);
    static float  EvaluateAxisBinding (const ControllerSample & sample, const AxisBinding & binding);
    static float  EvaluateAxis        (const ControllerSample & sample, const std::vector<AxisBinding> & bindings, const AxisBinding *& outWinner);
    static bool   IsOneStick          (const std::vector<AxisBinding> & xBindings, const std::vector<AxisBinding> & yBindings);

    void          EvaluatePair        (const ControllerSample         & sample,
                                       const std::vector<AxisBinding> & xBindings,
                                       const std::vector<AxisBinding> & yBindings,
                                       size_t                           firstAxis,
                                       size_t                           axisCount,
                                       float                            deadzone,
                                       float                            step,
                                       GamePortContribution           & contribution);
    Byte          ToAxisPaddle        (size_t axis, float shaped, const AxisBinding * winner, float elapsedSeconds);

    // Each axis's rate-binding paddle, in paddle units.
    std::array<float, GamePortContribution::kAxisCount>  m_rateValue    = { kRateCenter, kRateCenter, kRateCenter, kRateCenter };
    bool                                                 m_isRateMoving = false;
};
