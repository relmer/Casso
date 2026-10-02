#pragma once

#include "Pch.h"

#include "Controllers/ControlMapping.h"
#include "Controllers/ControllerProfileStore.h"
#include "Controllers/ControllerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AxisResponseRules
//
//  Which responses an analog binding may have in a profile of each kind. A
//  Joystick profile's axes always give position. A Paddle profile's axes move
//  their paddle at a paddle speed, and give position only when the control is
//  a real knob: a DirectInput axis on a controller that is not a gamepad, such
//  as Atari paddles on a USB adapter, or a wheel. Such an axis that stays
//  where it is left (AxisRoleRules), a throttle or a slider, starts at
//  Position; one that springs back starts at Paddle speed. A Joyport
//  profile's responses are left as they are.
//
////////////////////////////////////////////////////////////////////////////////

class AxisResponseRules
{
public:

    // Whether a Paddle profile offers Position for this binding beside
    // Paddle speed.
    static bool          IsPositionOffered (ProfileMode              mode,
                                            const AxisBinding      & binding,
                                            ControllerKind           kind,
                                            ControllerFormFactor     formFactor);

    // The response a binding plays with, given the one it asks for.
    static AxisResponse  GetAllowedResponse (ProfileMode              mode,
                                             const AxisBinding      & binding,
                                             ControllerKind           kind,
                                             ControllerFormFactor     formFactor);

    // The response a binding newly added to a profile of this kind starts
    // with: Position in a Joystick profile; in a Paddle profile, Position for
    // an axis that stays where it is left and is offered Position, and Paddle
    // speed otherwise; and the one it was given in a Joyport profile. `role`
    // is the bound control's, from its device's list.
    static AxisResponse  GetNewResponse     (ProfileMode              mode,
                                             const AxisBinding      & binding,
                                             ControllerKind           kind,
                                             ControllerFormFactor     formFactor,
                                             AxisRole                 role);

    // Every analog binding in the mapping set to its allowed response, as a
    // saved profile is loaded.
    static void          Normalize          (ControlMapping         & mapping,
                                             ProfileMode              mode,
                                             ControllerKind           kind,
                                             ControllerFormFactor     formFactor);
};
