#include "Pch.h"

#include "Controllers/PlayerTargetRules.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IsPaddleMapping
//
//  A mapping that binds PDL0 and leaves PDL1 alone is a paddle: one knob, one
//  axis. Anything else, including a mapping that binds neither, is played as a
//  joystick.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerTargetRules::IsPaddleMapping (const ControlMapping & mapping)
{
    return !mapping.pdl0.empty() && mapping.pdl1.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAutomaticTarget
//
//  What a player's slot maps to when the user has not set it: the player's
//  active profile says joystick or paddle, and the player says which one.
//  Player 1 takes joystick 0, or paddle 0. Player 2 takes joystick 1, or the
//  lowest paddle the other player does not hold, so two paddle profiles make
//  a two-paddle game with nothing set by hand.
//
////////////////////////////////////////////////////////////////////////////////

PlayerAxisTarget PlayerTargetRules::GetAutomaticTarget (
    size_t                  player,
    const ControlMapping &  mapping,
    PlayerAxisTarget        otherTarget)
{
    static constexpr PlayerAxisTarget  kPaddles[] = { PlayerAxisTarget::Paddle0,
                                                      PlayerAxisTarget::Paddle1,
                                                      PlayerAxisTarget::Paddle2,
                                                      PlayerAxisTarget::Paddle3 };
    MultiplayerSetup::AxisSet          taken;
    bool                               isPaddle = IsPaddleMapping (mapping);



    if (player == 0)
    {
        return isPaddle ? PlayerAxisTarget::Paddle0 : PlayerAxisTarget::Joystick0;
    }

    if (!isPaddle)
    {
        return PlayerAxisTarget::Joystick1;
    }

    taken = ControllerSelectionPolicy::GetTargetAxes (otherTarget, GamePortContribution::kAxisCount);

    for (PlayerAxisTarget paddle : kPaddles)
    {
        if ((ControllerSelectionPolicy::GetTargetAxes (paddle, GamePortContribution::kAxisCount) & taken).none())
        {
            return paddle;
        }
    }

    return PlayerAxisTarget::Paddle3;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetButtonRoute
//
//  The lines each target has on the hardware. Joystick 0 is wired to PB0 and
//  PB1, joystick 1 to PB2 alone, and a single paddle to its own line: PB0 for
//  paddle 0, PB1 for paddle 1, PB2 for paddle 2, and none for paddle 3. A
//  target with one line takes it from the controller's pb0 bindings; the
//  bindings that have no line are kept in the profile and reach nothing.
//
////////////////////////////////////////////////////////////////////////////////

PlayerTargetRules::ButtonRoute PlayerTargetRules::GetButtonRoute (PlayerAxisTarget target)
{
    static constexpr size_t  kPb0  = 0;
    static constexpr size_t  kPb1  = 1;
    static constexpr size_t  kPb2  = 2;
    ButtonRoute              route;



    switch (target)
    {
        case PlayerAxisTarget::Joystick0:
            route[kPb0] = kPb0;
            route[kPb1] = kPb1;
            break;

        case PlayerAxisTarget::Joystick1:
            route[kPb0] = kPb2;
            break;

        case PlayerAxisTarget::Paddle0:
            route[kPb0] = kPb0;
            break;

        case PlayerAxisTarget::Paddle1:
            route[kPb0] = kPb1;
            break;

        case PlayerAxisTarget::Paddle2:
            route[kPb0] = kPb2;
            break;

        case PlayerAxisTarget::Paddle3:
        default:
            break;
    }

    return route;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSingleRoute
//
//  One controller playing on its own drives what a single controller always
//  has: its pdl0 and pdl1 bindings on PDL0 and PDL1, and its pb0-pb2 bindings
//  on PB0-PB2, whichever player's slot it holds.
//
////////////////////////////////////////////////////////////////////////////////

PlayerTargetRules::Route PlayerTargetRules::GetSingleRoute (size_t axisCount)
{
    Route   route = GetTargetRoute (PlayerAxisTarget::Joystick0, axisCount);
    size_t  line  = 0;



    for (line = 0; line < kButtonCount; line++)
    {
        route.buttons[line] = line;
    }

    return route;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTargetRoute
//
//  A slot's own paddles, in ascending order, are where the controller's pdl0,
//  pdl1 and on land, so a slot mapped to one paddle plays only its pdl0
//  bindings. Paddles past the machine's count are left out; the buttons are
//  the target's hardware lines.
//
////////////////////////////////////////////////////////////////////////////////

PlayerTargetRules::Route PlayerTargetRules::GetTargetRoute (PlayerAxisTarget target, size_t axisCount)
{
    Route                      route;
    MultiplayerSetup::AxisSet  axes    = ControllerSelectionPolicy::GetTargetAxes (target, axisCount);
    size_t                     axis    = 0;
    size_t                     logical = 0;



    for (axis = 0; axis < axes.size(); axis++)
    {
        if (axes.test (axis))
        {
            route.paddles[logical++] = axis;
        }
    }

    route.buttons = GetButtonRoute (target);

    return route;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CountPaddles
//
////////////////////////////////////////////////////////////////////////////////

size_t PlayerTargetRules::CountPaddles (const Route & route)
{
    return (size_t) std::count_if (route.paddles.begin(), route.paddles.end(),
                                   [] (const std::optional<size_t> & paddle) { return paddle.has_value(); });
}
