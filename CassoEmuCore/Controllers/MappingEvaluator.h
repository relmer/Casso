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

    GamePortContribution  Evaluate (const ControllerSample & sample, const ControlMapping & mapping, float deadzone);

private:

    static float  ReadAnalog          (const ControllerSample & sample, const ControlId & control);
    static bool   IsHeld              (const ControllerSample & sample, const ControlId & control);
    static bool   IsButtonHeld        (const ControllerSample & sample, const ButtonBinding & binding);
    static bool   IsButtonListHeld    (const ControllerSample & sample, const std::vector<ButtonBinding> & bindings);
    static float  EvaluateAxisBinding (const ControllerSample & sample, const AxisBinding & binding);
    static float  EvaluateAxis        (const ControllerSample & sample, const std::vector<AxisBinding> & bindings);
    static bool   IsOneStick          (const std::vector<AxisBinding> & xBindings, const std::vector<AxisBinding> & yBindings);
};
