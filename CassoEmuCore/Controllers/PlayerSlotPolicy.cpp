#include "Pch.h"

#include "Controllers/PlayerSlotPolicy.h"

#include "Controllers/ControllerTokens.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Context
//
//  What one evaluation reads, gathered once. `picks` is each player's picked
//  controller as it is played: the pick itself, or the one attached unit of
//  its model when the pick is absent. `rawPicks` is the pick as the entry
//  holds it, so a controller the user picked is never a candidate under
//  either identity.
//
////////////////////////////////////////////////////////////////////////////////

struct PlayerSlotPolicy::Context
{
    const PlayerEntries                                               & entries;
    const std::vector<ControllerDeviceInfo>                           & devices;
    const PlayerOrderLogs                                             & logs;
    const PlayerSlots                                                 & previous;
    const MappingsByUnit                                              & mappings;
    std::array<std::optional<ControllerUnitKey>, kPlayerCount>          picks;
    std::array<std::optional<ControllerUnitKey>, kPlayerCount>          rawPicks;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Evaluate
//
//  Both slots from scratch, in the order the rules depend on each other:
//
//    1. each slot on its own: a pick plays while attached; an Automatic
//       holder that is still attached keeps its slot, so a controller that
//       arrives later never takes a slot from one already in it
//    2. departures: a holder that left while the other player plays is held
//       for it, so the player who remains keeps only what they had
//    3. Automatic's lone Player 1 gives way to the first controller used
//    4. with nobody left playing, Automatic starts over
//    5. empty Automatic slots fill, in connection order when two or more
//       controllers have arrived while Casso runs, otherwise in the order
//       they were first used; a lone controller with nothing used yet is
//       Player 1 at once
//    6. targets, and a second slot that would overlap the first is refused
//
////////////////////////////////////////////////////////////////////////////////

PlayerSlots PlayerSlotPolicy::Evaluate (
    const PlayerEntries                      & entries,
    const std::vector<ControllerDeviceInfo>  & devices,
    PlayerOrderLogs                          & logs,
    const PlayerSlots                        & previous,
    const MappingsByUnit                     & mappings)
{
    Context                          context   = { entries, devices, logs, previous, mappings, {}, {} };
    PlayerSlots                      slots;
    std::array<bool, kPlayerCount>   isLeaving = {};
    size_t                           player    = 0;



    ResolvePicks (context);

    for (player = 0; player < kPlayerCount; player++)
    {
        EvaluateBase (context, player, slots, isLeaving);
    }

    ResolveDepartures  (context, isLeaving, slots);
    GiveWayProvisional (context, slots);

    if (ShouldStartOver (previous, slots))
    {
        StartOver (slots, logs);
    }

    FillAutomatic (context, slots);
    SetTargets    (context, slots);

    return slots;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolvePicks
//
//  A picked controller that is absent while exactly one unit of its model is
//  attached is taken to be that unit: a DirectInput device moved to another
//  port, or an Xbox-class controller in another XInput slot. Never a unit the
//  other player picked or was playing, which would take a controller out of
//  another player's hands.
//
////////////////////////////////////////////////////////////////////////////////

void PlayerSlotPolicy::ResolvePicks (Context & context)
{
    size_t                            player  = 0;
    size_t                            other   = 0;
    std::optional<ControllerUnitKey>  adopted;



    for (player = 0; player < kPlayerCount; player++)
    {
        const PlayerEntry  & entry = context.entries[player];

        if (entry.kind == PlayerEntryKind::Controller && entry.unit.has_value())
        {
            context.rawPicks[player] = entry.unit;
            context.picks[player]    = entry.unit;
        }
    }

    for (player = 0; player < kPlayerCount; player++)
    {
        other = (player == 0) ? 1 : 0;

        if (!context.picks[player].has_value() || IsAttached (context.devices, context.picks[player].value()))
        {
            continue;
        }

        adopted = ControllerSelectionPolicy::FindAdoptedUnit (context.picks[player].value(), context.devices);

        if (!adopted.has_value()                                        ||
            adopted == context.rawPicks[other]                          ||
            adopted == context.previous[other].holder)
        {
            continue;
        }

        context.picks[player] = adopted;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EvaluateBase
//
//  One slot before the other is looked at. A holder that has gone is only
//  marked here; whether its slot is held depends on the other player.
//
////////////////////////////////////////////////////////////////////////////////

void PlayerSlotPolicy::EvaluateBase (
    const Context                     & context,
    size_t                              player,
    PlayerSlots                       & slots,
    std::array<bool, kPlayerCount>    & isLeaving)
{
    const PlayerEntry  & entry      = context.entries[player];
    const PlayerSlot   & prev       = context.previous[player];
    PlayerSlot         & slot       = slots[player];
    bool                 wasInUse   = prev.state == PlayerSlotState::Playing || prev.state == PlayerSlotState::Held;
    bool                 isAttached = false;



    slot          = PlayerSlot();
    slot.isPicked = entry.kind == PlayerEntryKind::Controller;

    if (entry.kind == PlayerEntryKind::Controller)
    {
        slot.holder = context.picks[player];

        if (!slot.holder.has_value())
        {
            return;
        }

        isAttached = IsAttached (context.devices, slot.holder.value());
        slot.state = isAttached ? PlayerSlotState::Playing : PlayerSlotState::Waiting;

        isLeaving[player] = !isAttached && wasInUse && prev.holder == slot.holder;
        return;
    }

    if (entry.kind != PlayerEntryKind::Automatic || !prev.holder.has_value() || prev.isPicked ||
        IsPickedUnit (context, prev.holder.value()))
    {
        return;
    }

    isAttached = IsAttached (context.devices, prev.holder.value());

    if (isAttached && prev.state != PlayerSlotState::Empty)
    {
        slot.holder = prev.holder;

        if (IsLogged (context.logs.firstInput, prev.holder.value()))
        {
            slot.state = PlayerSlotState::Playing;
        }
        else
        {
            slot.state = (prev.state == PlayerSlotState::Provisional) ? PlayerSlotState::Provisional
                                                                        : PlayerSlotState::Waiting;
        }

        return;
    }

    if (!isAttached && wasInUse)
    {
        slot.holder       = prev.holder;
        isLeaving[player] = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveDepartures
//
//  THE PLAYER WHO REMAINS KEEPS ONLY WHAT THEY HAD. A holder that left while
//  the other player plays is held for it, and a held slot is what stops the
//  other player being widened onto the paddles and lines the leaver had. With
//  nobody else playing there is nothing to protect: an Automatic slot empties,
//  and a picked one waits for its controller, since a disconnect never
//  rewrites a pick.
//
////////////////////////////////////////////////////////////////////////////////

void PlayerSlotPolicy::ResolveDepartures (
    const Context                         & context,
    const std::array<bool, kPlayerCount>  & isLeaving,
    PlayerSlots                           & slots)
{
    size_t  player = 0;
    size_t  other  = 0;



    for (player = 0; player < kPlayerCount; player++)
    {
        if (!isLeaving[player])
        {
            continue;
        }

        other = (player == 0) ? 1 : 0;

        if (CountsAsPlaying (slots[other], context.entries[other]))
        {
            slots[player].state = PlayerSlotState::Held;
        }
        else if (slots[player].isPicked)
        {
            slots[player].state = PlayerSlotState::Waiting;
        }
        else
        {
            slots[player].state = PlayerSlotState::Empty;
            slots[player].holder.reset();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GiveWayProvisional
//
//  A lone controller plays as Player 1 before anyone has used it, and gives
//  the slot up the moment another controller is used first: from then on the
//  order controllers are used in decides who is who.
//
////////////////////////////////////////////////////////////////////////////////

void PlayerSlotPolicy::GiveWayProvisional (const Context & context, PlayerSlots & slots)
{
    PlayerSlot  & slot = slots[0];



    if (slot.state != PlayerSlotState::Provisional)
    {
        return;
    }

    for (const ControllerUnitKey & unit : context.logs.firstInput)
    {
        if (!(slot.holder == unit) && IsCandidate (context, slots, unit))
        {
            slot = PlayerSlot();
            return;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShouldStartOver
//
//  A controller was playing, or held a slot, and now nothing plays and
//  nothing is held.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerSlotPolicy::ShouldStartOver (const PlayerSlots & previous, const PlayerSlots & slots)
{
    bool    wasInUse = false;
    bool    isStill  = false;
    size_t  player   = 0;



    for (player = 0; player < kPlayerCount; player++)
    {
        wasInUse = wasInUse || IsInUse (previous[player]);
        isStill  = isStill  || IsInUse (slots[player]);
    }

    return wasInUse && !isStill;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsInUse
//
//  A controller playing in the slot, or one the slot is held for.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerSlotPolicy::IsInUse (const PlayerSlot & slot)
{
    return slot.holder.has_value()
           && (slot.state == PlayerSlotState::Playing || slot.state == PlayerSlotState::Held);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartOver
//
//  Every Automatic slot empties, and both logs forget every controller that
//  holds no slot, so the next controller used is Player 1 again. Picks are
//  never rewritten.
//
////////////////////////////////////////////////////////////////////////////////

void PlayerSlotPolicy::StartOver (PlayerSlots & slots, PlayerOrderLogs & logs)
{
    auto  holdsSlot = [&slots] (const ControllerUnitKey & unit)
    {
        return std::any_of (slots.begin(), slots.end(), [&unit] (const PlayerSlot & slot) { return slot.holder == unit; });
    };



    for (PlayerSlot & slot : slots)
    {
        if (!slot.isPicked)
        {
            slot = PlayerSlot();
        }
    }

    std::erase_if (logs.connected,  [&holdsSlot] (const ControllerUnitKey & unit) { return !holdsSlot (unit); });
    std::erase_if (logs.firstInput, [&holdsSlot] (const ControllerUnitKey & unit) { return !holdsSlot (unit); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillAutomatic
//
//  CONNECTION ORDER FIRST, WHEN THERE IS ONE. Two controllers plugged in while
//  Casso runs are Player 1 and Player 2 in the order they arrived, used or
//  not. Otherwise the order they are first used in decides, so a stick that is
//  always plugged in does not come ahead of the pad the user picks up.
//
//  A slot filled from the connection log waits for its first input before it
//  counts as playing; one filled from the input log is playing at once.
//
////////////////////////////////////////////////////////////////////////////////

void PlayerSlotPolicy::FillAutomatic (const Context & context, PlayerSlots & slots)
{
    std::vector<ControllerUnitKey>  connected;
    std::vector<ControllerUnitKey>  order;
    size_t                          candidates = 0;
    size_t                          player     = 0;



    for (const ControllerUnitKey & unit : context.logs.connected)
    {
        if (IsAttached (context.devices, unit) && !IsPickedUnit (context, unit))
        {
            connected.push_back (unit);
        }
    }

    if (connected.size() >= kPlayerCount)
    {
        order = connected;
    }
    else
    {
        for (const ControllerUnitKey & unit : context.logs.firstInput)
        {
            if (IsAttached (context.devices, unit) && !IsPickedUnit (context, unit))
            {
                order.push_back (unit);
            }
        }
    }

    for (player = 0; player < kPlayerCount; player++)
    {
        PlayerSlot  & slot = slots[player];

        if (context.entries[player].kind != PlayerEntryKind::Automatic || slot.holder.has_value())
        {
            continue;
        }

        for (const ControllerUnitKey & unit : order)
        {
            if (!IsCandidate (context, slots, unit))
            {
                continue;
            }

            slot.holder = unit;
            slot.state  = IsLogged (context.logs.firstInput, unit) ? PlayerSlotState::Playing : PlayerSlotState::Waiting;
            break;
        }
    }

    // A LONE CONTROLLER PLAYS AS PLAYER 1 AT ONCE, without waiting to be used,
    // so plugging one in is all a single player has to do. With two or more
    // and nothing used yet, which one the user will pick up cannot be known,
    // so neither plays until one is used.
    if (context.entries[0].kind != PlayerEntryKind::Automatic || slots[0].holder.has_value())
    {
        return;
    }

    for (const ControllerDeviceInfo & device : context.devices)
    {
        if (IsCandidate (context, slots, device.unit))
        {
            candidates++;
        }
    }

    if (candidates != 1)
    {
        return;
    }

    for (const ControllerDeviceInfo & device : context.devices)
    {
        if (IsCandidate (context, slots, device.unit))
        {
            slots[0].holder = device.unit;
            slots[0].state  = PlayerSlotState::Provisional;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTargets
//
//  A target the user set is kept; otherwise the holder's active profile says
//  joystick or paddle, and Player 2's comes to rest beside Player 1's. The
//  keys and the mouse play joystick 0. A second slot that repeats the first
//  one's controller or overlaps its paddles is refused rather than played.
//
////////////////////////////////////////////////////////////////////////////////

void PlayerSlotPolicy::SetTargets (const Context & context, PlayerSlots & slots)
{
    static const ControlMapping  kNoMapping;
    MultiplayerSetup             setup;
    size_t                       player = 0;



    for (player = 0; player < kPlayerCount; player++)
    {
        const PlayerEntry     & entry   = context.entries[player];
        const ControlMapping  * mapping = &kNoMapping;
        PlayerAxisTarget        other   = (player == 0) ? PlayerAxisTarget::Joystick0 : slots[0].target;

        if (slots[player].holder.has_value())
        {
            auto  found = context.mappings.find (ControllerTokens::UnitToToken (slots[player].holder.value()));

            if (found != context.mappings.end())
            {
                mapping = &found->second;
            }
        }

        if (IsHostInput (entry))
        {
            slots[player].target = PlayerAxisTarget::Joystick0;
        }
        else if (entry.target.has_value())
        {
            slots[player].target = entry.target.value();
        }
        else
        {
            slots[player].target = PlayerTargetRules::GetAutomaticTarget (player, *mapping, other);
        }
    }

    if (!slots[1].holder.has_value() || (!slots[0].holder.has_value() && !IsHostInput (context.entries[0])))
    {
        return;
    }

    // The keys and the mouse have no unit; an empty one stands in for them,
    // which can never equal a real controller's.
    setup.players[0].unit   = slots[0].holder.value_or (ControllerUnitKey());
    setup.players[0].target = slots[0].target;
    setup.players[1].unit   = slots[1].holder;
    setup.players[1].target = slots[1].target;

    setup = ControllerSelectionPolicy::Normalize (setup);

    if (!setup.players[1].unit.has_value())
    {
        slots[1].state = PlayerSlotState::Empty;
        slots[1].holder.reset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsRealInput
//
//  A controller counts as used on a button or D-pad press, a trigger past the
//  point where it counts as a button, or any axis outside its deadzone once
//  calibrated. The deadzone is what already defines rest for the controller,
//  so the same edge that moves a game's paddle is the one that claims a slot.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerSlotPolicy::IsRealInput (
    const ControllerSample       & sample,
    const ControllerCalibration  * calibration,
    float                          deadzone)
{
    ControllerSample  calibrated = (calibration != nullptr) ? calibration->Apply (sample) : sample;



    if (calibrated.buttons.any())
    {
        return true;
    }

    for (Byte hat : calibrated.hats)
    {
        if (hat != 0)
        {
            return true;
        }
    }

    for (float trigger : calibrated.triggers)
    {
        if (trigger > ButtonBinding::kTriggerThreshold)
        {
            return true;
        }
    }

    for (float axis : calibrated.axes)
    {
        if (std::fabs (axis) > deadzone)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsOnePlaying
//
//  Exactly one player drives, and the other slot is not held for a player who
//  left. That one drives everything a single controller always has. A held
//  slot keeps the rule off, so a player whose partner left keeps only their
//  own paddles and lines.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerSlotPolicy::IsOnePlaying (const PlayerSlots & slots, const PlayerEntries & entries)
{
    size_t  drivers = 0;
    bool    isHeld  = false;
    size_t  player  = 0;



    for (player = 0; player < kPlayerCount; player++)
    {
        if (IsDrivingSlot (slots[player]) || (player == 0 && IsHostInput (entries[player])))
        {
            drivers++;
        }

        isHeld = isHeld || slots[player].state == PlayerSlotState::Held;
    }

    return drivers == 1 && !isHeld;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDrivingSlot
//
//  A slot whose controller drives the game port: playing, or Automatic's lone
//  Player 1 before anyone has used it.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerSlotPolicy::IsDrivingSlot (const PlayerSlot & slot)
{
    return slot.holder.has_value()
           && (slot.state == PlayerSlotState::Playing || slot.state == PlayerSlotState::Provisional);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NeedsIdleWatch
//
//  Whether a player on Automatic is still waiting for a controller, which is
//  what makes a controller nobody holds worth watching for its first input.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerSlotPolicy::NeedsIdleWatch (const PlayerEntries & entries, const PlayerSlots & slots)
{
    size_t  player = 0;



    for (player = 0; player < kPlayerCount; player++)
    {
        PlayerSlotState  state = slots[player].state;

        if (entries[player].kind != PlayerEntryKind::Automatic)
        {
            continue;
        }

        if (state == PlayerSlotState::Empty || state == PlayerSlotState::Waiting || state == PlayerSlotState::Provisional)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DescribeAssignment
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PlayerSlotPolicy::DescribeAssignment (size_t player, const std::wstring & description)
{
    return L"Player " + std::to_wstring (player + 1) + L": " + description;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordHolders
//
//  A slot holds a controller while the controller is attached and the slot
//  is not only held for it: Automatic's lone Player 1 counts, and so does a
//  holder waiting for its first input. A holder that leaves is not recorded,
//  so the saved last holder outlives it. Only Automatic is announced: a pick
//  is what the user just chose, so it updates the last holder silently.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> PlayerSlotPolicy::RecordHolders (
    const PlayerSlots                        & slots,
    const std::vector<ControllerDeviceInfo>  & devices,
    PlayerLastHolders                        & lastHolders)
{
    std::vector<std::wstring>  notices;
    size_t                     player  = 0;



    for (player = 0; player < kPlayerCount; player++)
    {
        const PlayerSlot            & slot   = slots[player];
        const ControllerDeviceInfo  * device = nullptr;

        if (!slot.holder.has_value() || slot.state == PlayerSlotState::Held || slot.state == PlayerSlotState::Empty)
        {
            continue;
        }

        device = FindDevice (devices, slot.holder.value());

        if (device == nullptr || IsSameUnit (lastHolders[player], device->unit))
        {
            continue;
        }

        if (!slot.isPicked)
        {
            notices.push_back (DescribeAssignment (player, device->description));
        }

        lastHolders[player] = device->unit;
    }

    return notices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDriverRoute
//
//  A player whose target has none of its paddles on this machine is not read
//  at all, so neither its paddles nor its lines reach it.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<PlayerTargetRules::Route> PlayerSlotPolicy::GetDriverRoute (
    const PlayerSlots    & slots,
    const PlayerEntries  & entries,
    size_t                 player,
    size_t                 axisCount)
{
    PlayerTargetRules::Route  route;



    if (player >= kPlayerCount || !IsDrivingSlot (slots[player]))
    {
        return std::nullopt;
    }

    route = IsOnePlaying (slots, entries) ? PlayerTargetRules::GetSingleRoute (axisCount)
                                          : PlayerTargetRules::GetTargetRoute (slots[player].target, axisCount);

    if (PlayerTargetRules::CountPaddles (route) == 0)
    {
        return std::nullopt;
    }

    return route;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyPick
//
//  A controller can play for one player only, so picking the one the other
//  player picked sends the other player back to Automatic.
//
////////////////////////////////////////////////////////////////////////////////

PlayerEntries PlayerSlotPolicy::ApplyPick (PlayerEntries entries, size_t player, const PlayerEntry & entry)
{
    size_t  other = (player == 0) ? 1 : 0;



    if (player >= kPlayerCount)
    {
        return entries;
    }

    entries[player] = entry;

    if (entry.kind == PlayerEntryKind::Controller         &&
        entries[other].kind == PlayerEntryKind::Controller &&
        entries[other].unit == entry.unit)
    {
        entries[other].kind = PlayerEntryKind::Automatic;
        entries[other].unit.reset();
    }

    return NormalizeEntries (entries);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NormalizeEntries
//
//  The keys and the mouse are Player 1's, and Disabled is Player 2's; a pick
//  with no controller is no pick. Each of those reads as Automatic, and so
//  does Player 2 picking the controller Player 1 picked, since one controller
//  cannot play for both.
//
////////////////////////////////////////////////////////////////////////////////

PlayerEntries PlayerSlotPolicy::NormalizeEntries (PlayerEntries entries)
{
    size_t  player = 0;



    for (player = 0; player < kPlayerCount; player++)
    {
        PlayerEntry  & entry         = entries[player];
        bool           isWrongPlayer = (player != 0 && IsHostInput (entry)) || (player == 0 && entry.kind == PlayerEntryKind::Disabled);
        bool           isEmptyPick   = entry.kind == PlayerEntryKind::Controller && !entry.unit.has_value();

        if (isWrongPlayer || isEmptyPick)
        {
            entry.kind = PlayerEntryKind::Automatic;
        }

        if (entry.kind != PlayerEntryKind::Controller)
        {
            entry.unit.reset();
        }
    }

    if (entries[0].kind == PlayerEntryKind::Controller &&
        entries[1].kind == PlayerEntryKind::Controller &&
        entries[0].unit == entries[1].unit)
    {
        entries[1].kind = PlayerEntryKind::Automatic;
        entries[1].unit.reset();
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeSetupView
//
//  Each player's controller -- the pick, or what Automatic chose -- and the
//  target it plays. The two-player section shows while both players have a
//  controller, or while both are picks.
//
////////////////////////////////////////////////////////////////////////////////

MultiplayerSetup PlayerSlotPolicy::MakeSetupView (const PlayerEntries & entries, const PlayerSlots & slots)
{
    MultiplayerSetup  setup;
    size_t            player   = 0;
    bool              isPicked = true;



    for (player = 0; player < kPlayerCount; player++)
    {
        bool  isController = entries[player].kind == PlayerEntryKind::Controller;

        setup.players[player].unit   = isController ? entries[player].unit : slots[player].holder;
        setup.players[player].target = slots[player].target;
        isPicked                     = isPicked && isController;
    }

    setup.isEnabled = isPicked || (slots[0].holder.has_value() && slots[1].holder.has_value());

    return setup;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsAttached
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerSlotPolicy::IsAttached (const std::vector<ControllerDeviceInfo> & devices, const ControllerUnitKey & unit)
{
    return std::any_of (devices.begin(), devices.end(), [&unit] (const ControllerDeviceInfo & device) { return device.unit == unit; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsLogged
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerSlotPolicy::IsLogged (const std::vector<ControllerUnitKey> & log, const ControllerUnitKey & unit)
{
    return std::find (log.begin(), log.end(), unit) != log.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsHostInput
//
//  The keys or the mouse, which are in use from the moment they are picked.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerSlotPolicy::IsHostInput (const PlayerEntry & entry)
{
    return entry.kind == PlayerEntryKind::ArrowKeys || entry.kind == PlayerEntryKind::MousePaddle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPickedUnit
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerSlotPolicy::IsPickedUnit (const Context & context, const ControllerUnitKey & unit)
{
    size_t  player = 0;



    for (player = 0; player < kPlayerCount; player++)
    {
        if (context.picks[player] == unit || context.rawPicks[player] == unit)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CountsAsPlaying
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerSlotPolicy::CountsAsPlaying (const PlayerSlot & slot, const PlayerEntry & entry)
{
    return slot.state == PlayerSlotState::Playing || IsHostInput (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsCandidate
//
//  An attached controller no player picked and no slot holds.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerSlotPolicy::IsCandidate (const Context & context, const PlayerSlots & slots, const ControllerUnitKey & unit)
{
    bool  isHeld = std::any_of (slots.begin(), slots.end(), [&unit] (const PlayerSlot & slot) { return slot.holder == unit; });



    return IsAttached (context.devices, unit) && !IsPickedUnit (context, unit) && !isHeld;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsSameUnit
//
//  By unit token, the identity the last holder is saved under, so two
//  controllers of one product are told apart by their ordinal.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerSlotPolicy::IsSameUnit (const std::optional<ControllerUnitKey> & last, const ControllerUnitKey & unit)
{
    return last.has_value() && ControllerTokens::UnitToToken (last.value()) == ControllerTokens::UnitToToken (unit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindDevice
//
////////////////////////////////////////////////////////////////////////////////

const ControllerDeviceInfo * PlayerSlotPolicy::FindDevice (const std::vector<ControllerDeviceInfo> & devices, const ControllerUnitKey & unit)
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
