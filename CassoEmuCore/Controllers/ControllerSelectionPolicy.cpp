#include "Pch.h"

#include "Controllers/ControllerSelectionPolicy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FindAdoptedUnit
//
//  The pick when it is attached; otherwise the one attached unit of its model,
//  which is taken to be the picked controller back under another identity.
//  With none of that model attached, or two, there is nothing to adopt.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<ControllerUnitKey> ControllerSelectionPolicy::FindAdoptedUnit (
    const ControllerUnitKey &                  pick,
    const std::vector<ControllerDeviceInfo> &  devices)
{
    const ControllerDeviceInfo *  found = FindUnit (devices, pick);



    if (found == nullptr)
    {
        found = FindSoleSameModel (devices, pick.model);
    }

    if (found == nullptr)
    {
        return std::nullopt;
    }

    return found->unit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AdoptSlotKeyedPlayers
//
//  A slot is the best guess such a setup can offer about which controller it
//  meant, and it is used exactly once: the unit found there is what the
//  caller saves, so from then on the player follows that controller whatever
//  slot it connects into. A slot with nothing in it now is left as it is, to
//  be adopted when a controller arrives there.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerSelectionPolicy::AdoptSlotKeyedPlayers (
    MultiplayerSetup &                         setup,
    const std::vector<ControllerDeviceInfo> &  devices)
{
    bool  hasMoved = false;
    int   slot     = 0;



    for (MultiplayerSlot & player : setup.players)
    {
        if (!player.unit.has_value() || player.unit->source != ControllerUnitSource::XInputSlot)
        {
            continue;
        }

        if (std::from_chars (player.unit->unitId.data(), player.unit->unitId.data() + player.unit->unitId.size(), slot).ec != std::errc())
        {
            continue;
        }

        for (const ControllerDeviceInfo & device : devices)
        {
            if (device.unit.model.kind == ControllerKind::XInput &&
                device.xinputSlot == slot &&
                !(device.unit == player.unit.value()))
            {
                player.unit = device.unit;
                hasMoved    = true;
                break;
            }
        }
    }

    return hasMoved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTargetAxes
//
//  A joystick is two paddles wired to one stick; a paddle is one. Paddles the
//  machine does not have are left out here rather than removed from the slot,
//  so a //c plays what it can of a setup saved on a //e and the //e plays all
//  of it again (FR-035).
//
////////////////////////////////////////////////////////////////////////////////

MultiplayerSetup::AxisSet ControllerSelectionPolicy::GetTargetAxes (
    PlayerAxisTarget  target,
    size_t            axisCount)
{
    MultiplayerSetup::AxisSet  axes;
    size_t                     axis  = 0;



    switch (target)
    {
        case PlayerAxisTarget::Joystick0:  axes.set (0); axes.set (1); break;
        case PlayerAxisTarget::Joystick1:  axes.set (2); axes.set (3); break;
        case PlayerAxisTarget::Paddle0:    axes.set (0);               break;
        case PlayerAxisTarget::Paddle1:    axes.set (1);               break;
        case PlayerAxisTarget::Paddle2:    axes.set (2);               break;
        case PlayerAxisTarget::Paddle3:    axes.set (3);               break;

        default:                                                       break;
    }

    for (axis = axisCount; axis < axes.size(); axis++)
    {
        axes.reset (axis);
    }

    return axes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAxesForPlayer
//
////////////////////////////////////////////////////////////////////////////////

MultiplayerSetup::AxisSet ControllerSelectionPolicy::GetAxesForPlayer (
    const MultiplayerSetup &  setup,
    size_t                    player,
    size_t                    axisCount)
{
    MultiplayerSetup::AxisSet  axes;
    bool                       isPlaying = setup.isEnabled
                                           && player < MultiplayerSetup::kPlayerCount
                                           && setup.players[player].unit.has_value();



    if (!isPlaying)
    {
        return axes;
    }

    return GetTargetAxes (setup.players[player].target, axisCount);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindPlayer
//
////////////////////////////////////////////////////////////////////////////////

std::optional<size_t> ControllerSelectionPolicy::FindPlayer (
    const MultiplayerSetup &   setup,
    const ControllerUnitKey &  unit)
{
    size_t  player = 0;



    if (!setup.isEnabled)
    {
        return std::nullopt;
    }

    for (player = 0; player < MultiplayerSetup::kPlayerCount; player++)
    {
        if (setup.players[player].unit.has_value() && setup.players[player].unit.value() == unit)
        {
            return player;
        }
    }

    return std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Normalize
//
//  THE LATER SLOT GIVES WAY. A setup that repeats a controller or claims a
//  paddle the first slot already holds cannot be played as written, and
//  emptying the second slot is the answer the user can see: the player whose
//  choice was refused has an empty slot to fill rather than a paddle that
//  quietly does nothing.
//
//  The paddles are compared across the whole port, not the machine's count, so
//  a //c cannot accept an overlap a //e would refuse.
//
////////////////////////////////////////////////////////////////////////////////

MultiplayerSetup ControllerSelectionPolicy::Normalize (MultiplayerSetup setup)
{
    MultiplayerSetup::AxisSet  first    = GetTargetAxes (setup.players[0].target, GamePortContribution::kAxisCount);
    MultiplayerSetup::AxisSet  second   = GetTargetAxes (setup.players[1].target, GamePortContribution::kAxisCount);
    bool                       isFilled = setup.players[0].unit.has_value() && setup.players[1].unit.has_value();
    bool                       isSame   = false;
    bool                       overlaps = false;



    if (!isFilled)
    {
        return setup;
    }

    isSame   = setup.players[0].unit.value() == setup.players[1].unit.value();
    overlaps = (first & second).any();

    if (isSame || overlaps)
    {
        setup.players[1].unit.reset();
    }

    return setup;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTargetChoices
//
//  What the settings page offers one slot: every target the machine has the
//  paddles for, less the ones the other player is already holding. A slot with
//  no controller in the other player's hands is offered everything the machine
//  can play.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<PlayerAxisTarget> ControllerSelectionPolicy::GetTargetChoices (
    const MultiplayerSetup &  setup,
    size_t                    player,
    size_t                    axisCount)
{
    static constexpr PlayerAxisTarget  kAllTargets[] = { PlayerAxisTarget::Joystick0,
                                                         PlayerAxisTarget::Joystick1,
                                                         PlayerAxisTarget::Paddle0,
                                                         PlayerAxisTarget::Paddle1,
                                                         PlayerAxisTarget::Paddle2,
                                                         PlayerAxisTarget::Paddle3 };
    std::vector<PlayerAxisTarget>      choices;
    MultiplayerSetup::AxisSet          taken;
    size_t                             other   = 0;



    if (player >= MultiplayerSetup::kPlayerCount)
    {
        return choices;
    }

    other = (player == 0) ? 1 : 0;

    if (setup.players[other].unit.has_value())
    {
        taken = GetTargetAxes (setup.players[other].target, axisCount);
    }

    for (PlayerAxisTarget target : kAllTargets)
    {
        MultiplayerSetup::AxisSet  axes = GetTargetAxes (target, axisCount);

        // A target the machine cannot play in full is not offered: half a
        // joystick is not a choice the user made.
        if (axes != GetTargetAxes (target, GamePortContribution::kAxisCount) || (axes & taken).any())
        {
            continue;
        }

        choices.push_back (target);
    }

    return choices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindUnit
//
////////////////////////////////////////////////////////////////////////////////

const ControllerDeviceInfo * ControllerSelectionPolicy::FindUnit (
    const std::vector<ControllerDeviceInfo> &  devices,
    const ControllerUnitKey &                  unit)
{
    for (const ControllerDeviceInfo & device : devices)
    {
        if (device.unit == unit)
        {
            return &device;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSoleSameModel
//
//  The one attached unit of this model, or null when there is none or more
//  than one.
//
////////////////////////////////////////////////////////////////////////////////

const ControllerDeviceInfo * ControllerSelectionPolicy::FindSoleSameModel (
    const std::vector<ControllerDeviceInfo> &  devices,
    const ControllerModelKey &                 model)
{
    const ControllerDeviceInfo *  found = nullptr;



    for (const ControllerDeviceInfo & device : devices)
    {
        if (!(device.unit.model == model))
        {
            continue;
        }

        if (found != nullptr)
        {
            return nullptr;
        }

        found = &device;
    }

    return found;
}
