#include "Pch.h"

#include "Controllers/ControllerSelectionPolicy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Evaluate
//
//  The whole selection rule in one pass over what is attached, which the
//  caller lists LONGEST-ATTACHED FIRST.
//
//  With nothing selected, the first controller listed takes the game port and
//  the arrow keys and paddle give it up, so plugging a controller in is all a
//  user has to do (FR-032). With the selected controller attached, nothing
//  changes: the selection stays on the controller in use, and one arriving
//  does not take it.
//
//  With the selected controller gone, the selection moves (FR-008a). A
//  DirectInput unit that moved to another port comes back under a different
//  identity, so exactly one attached unit of its model is taken to be it;
//  with two of that model, which one moved is a coin flip, and they count as
//  any other controller. Otherwise the longest-attached controller takes over,
//  and with none attached the selection is cleared. A controller returning
//  later is only a controller arriving.
//
//  A CONTROLLER A PLAYER IS HOLDING IS CHOSEN LAST. While multiplayer is on it
//  is already playing that player's paddles, so a free controller is the
//  better stand-in; and the selection drives nothing in that mode anyway, so
//  the player still at the machine does not move (SC-012).
//
////////////////////////////////////////////////////////////////////////////////

ControllerSelectionPolicy::Decision ControllerSelectionPolicy::Evaluate (
    const std::optional<ControllerUnitKey> &   current,
    const std::vector<ControllerDeviceInfo> &  devices,
    bool                                       hasGamePort,
    const MultiplayerSetup &                   multiplayer)
{
    Decision                      decision;
    const ControllerDeviceInfo *  found     = nullptr;
    const ControllerDeviceInfo *  preferred = nullptr;



    decision.selection = current;

    // A machine with no game port keeps the selection and does nothing with
    // it, so switching back to one that has a port finds the choice intact.
    if (!hasGamePort)
    {
        return decision;
    }

    if (!current.has_value())
    {
        if (devices.empty())
        {
            return decision;
        }

        preferred                      = &GetPreferredDevice (devices, multiplayer);
        decision.selection             = preferred->unit;
        decision.description           = preferred->description;
        decision.reason                = SelectionChangeReason::AutomaticSelection;
        decision.hasChanged            = true;
        decision.clearsOtherInputModes = true;

        return decision;
    }

    if (FindUnit (devices, current.value()) != nullptr)
    {
        return decision;
    }

    // Xbox-class controllers are recognized by model alone (FR-018a), so a
    // selection of one already matched above against whichever unit is
    // attached. Only DirectInput units can go missing while their replacement
    // is present under another identity.
    if (current.value().model.kind == ControllerKind::DirectInput)
    {
        found = FindSoleSameModel (devices, current.value().model);
    }

    decision.hasChanged = true;

    if (found != nullptr)
    {
        decision.selection   = found->unit;
        decision.description = found->description;
        decision.reason      = SelectionChangeReason::Adoption;

        return decision;
    }

    if (devices.empty())
    {
        decision.selection = std::nullopt;
        decision.reason    = SelectionChangeReason::Cleared;

        return decision;
    }

    preferred            = &GetPreferredDevice (devices, multiplayer);
    decision.selection   = preferred->unit;
    decision.description = preferred->description;
    decision.reason      = SelectionChangeReason::Replacement;

    return decision;
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
//  GetPreferredDevice
//
//  The first listed controller no player is holding, or the first listed when
//  both players hold one. The caller guarantees the list is not empty.
//
////////////////////////////////////////////////////////////////////////////////

const ControllerDeviceInfo & ControllerSelectionPolicy::GetPreferredDevice (
    const std::vector<ControllerDeviceInfo> &  devices,
    const MultiplayerSetup &                   multiplayer)
{
    for (const ControllerDeviceInfo & device : devices)
    {
        if (!FindPlayer (multiplayer, device.unit).has_value())
        {
            return device;
        }
    }

    return devices.front();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSelectedAttached
//
////////////////////////////////////////////////////////////////////////////////

bool ControllerSelectionPolicy::IsSelectedAttached (
    const std::optional<ControllerUnitKey> &   selection,
    const std::vector<ControllerDeviceInfo> &  devices)
{
    if (!selection.has_value())
    {
        return false;
    }

    return FindUnit (devices, selection.value()) != nullptr;
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
