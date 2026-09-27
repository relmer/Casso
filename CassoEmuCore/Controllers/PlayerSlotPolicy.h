#pragma once

#include "Pch.h"

#include "Controllers/ControlMapping.h"
#include "Controllers/ControllerCalibration.h"
#include "Controllers/ControllerSelectionPolicy.h"
#include "Controllers/ControllerTypes.h"
#include "Controllers/PlayerTargetRules.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PlayerEntryKind
//
//  What the user chose for one player. ArrowKeys and MousePaddle are valid for
//  Player 1 only, and Disabled for Player 2 only.
//
////////////////////////////////////////////////////////////////////////////////

enum class PlayerEntryKind
{
    Automatic,
    Controller,
    ArrowKeys,
    MousePaddle,
    Disabled
};





////////////////////////////////////////////////////////////////////////////////
//
//  PlayerEntry
//
//  One player's choice. `unit` is set only for Controller: the picked
//  controller, attached or not. `target` is set only when the user chose what
//  the slot maps to; absent, the slot follows the controller's active profile.
//
////////////////////////////////////////////////////////////////////////////////

struct PlayerEntry
{
    PlayerEntryKind                   kind = PlayerEntryKind::Automatic;
    std::optional<ControllerUnitKey>  unit;
    std::optional<PlayerAxisTarget>   target;

    bool operator== (const PlayerEntry &) const = default;
};

using PlayerEntries = std::array<PlayerEntry, MultiplayerSetup::kPlayerCount>;





////////////////////////////////////////////////////////////////////////////////
//
//  PlayerSlotState
//
//  Empty       no controller: Automatic waiting for one, Disabled, or Player 1
//              on the keys or the mouse
//  Provisional Automatic's lone Player 1 before any controller has given
//              input: it drives the port, but is not in use
//  Waiting     a holder chosen by Automatic that has not given input, or a
//              picked controller that is not attached
//  Playing     picked and attached, or chosen by Automatic and used
//  Held        the holder left while the other player went on playing; kept
//              for it, and playing nothing
//
////////////////////////////////////////////////////////////////////////////////

enum class PlayerSlotState
{
    Empty,
    Provisional,
    Waiting,
    Playing,
    Held
};





////////////////////////////////////////////////////////////////////////////////
//
//  PlayerSlot
//
//  What one player is playing right now, as opposed to what they chose.
//  `holder` is the controller in the slot, including a held one that is gone.
//
////////////////////////////////////////////////////////////////////////////////

struct PlayerSlot
{
    PlayerSlotState                   state    = PlayerSlotState::Empty;
    std::optional<ControllerUnitKey>  holder;
    bool                              isPicked = false;
    PlayerAxisTarget                  target   = PlayerAxisTarget::Joystick0;

    bool operator== (const PlayerSlot &) const = default;
};

using PlayerSlots = std::array<PlayerSlot, MultiplayerSetup::kPlayerCount>;





////////////////////////////////////////////////////////////////////////////////
//
//  PlayerOrderLogs
//
//  The two orders Automatic fills the players in. `connected` lists each
//  distinct controller that arrived after the controllers present at startup;
//  `firstInput` lists each controller that gave its first real input while
//  Casso was active.
//
////////////////////////////////////////////////////////////////////////////////

struct PlayerOrderLogs
{
    std::vector<ControllerUnitKey>  connected;
    std::vector<ControllerUnitKey>  firstInput;

    bool operator== (const PlayerOrderLogs &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PlayerLastHolders
//
//  The controller that last held each player's slot, however it came to hold
//  it. Saved across launches only so that a notice is shown when Automatic
//  gives a slot a different controller; it never assigns one.
//
////////////////////////////////////////////////////////////////////////////////

using PlayerLastHolders = std::array<std::optional<ControllerUnitKey>, MultiplayerSetup::kPlayerCount>;





////////////////////////////////////////////////////////////////////////////////
//
//  PlayerSlotPolicy
//
//  Who plays for each player, from what the user chose, what is attached and
//  the order controllers arrived in and were first used. Pure: it reads no
//  device and no clock, so every sequence of arrivals, first inputs and
//  departures is a unit test.
//
////////////////////////////////////////////////////////////////////////////////

class PlayerSlotPolicy
{
public:

    static constexpr size_t  kPlayerCount = MultiplayerSetup::kPlayerCount;

    // Each attached controller's mapping for its active profile, by unit
    // token: what decides a slot's target when the user has not set it.
    using MappingsByUnit = std::map<std::string, ControlMapping>;

    static PlayerSlots  Evaluate           (const PlayerEntries                      & entries,
                                            const std::vector<ControllerDeviceInfo>  & devices,
                                            PlayerOrderLogs                          & logs,
                                            const PlayerSlots                        & previous,
                                            const MappingsByUnit                     & mappings);

    static bool         IsRealInput        (const ControllerSample       & sample,
                                            const ControllerCalibration  * calibration,
                                            float                          deadzone);

    static bool         IsOnePlaying       (const PlayerSlots & slots, const PlayerEntries & entries);
    static bool         IsDrivingSlot      (const PlayerSlot & slot);
    static bool         NeedsIdleWatch     (const PlayerEntries & entries, const PlayerSlots & slots);
    static std::wstring DescribeAssignment (size_t player, const std::wstring & description);

    // Each slot's attached holder becomes its last holder. Returns a notice
    // for each slot Automatic gave a controller other than its last holder.
    static std::vector<std::wstring>  RecordHolders (const PlayerSlots                        & slots,
                                                     const std::vector<ControllerDeviceInfo>  & devices,
                                                     PlayerLastHolders                        & lastHolders);

    // What a player's controller reaches on this machine, or nothing when
    // its slot plays no controller. One player playing alone reaches what a
    // single controller always has; two each reach their own target.
    static std::optional<PlayerTargetRules::Route>  GetDriverRoute (const PlayerSlots    & slots,
                                                                    const PlayerEntries  & entries,
                                                                    size_t                 player,
                                                                    size_t                 axisCount);

    // A pick, and picking a controller the other player holds returns the
    // other player to Automatic.
    static PlayerEntries  ApplyPick (PlayerEntries entries, size_t player, const PlayerEntry & entry);

    // Entries as they can be played: an entry its player cannot have, and a
    // second pick of the first player's controller, read as Automatic.
    static PlayerEntries  NormalizeEntries (PlayerEntries entries);

    // The Controllers page's view of the players as a two-slot setup, and a
    // setup edited there turned back into entries.
    static MultiplayerSetup  MakeSetupView  (const PlayerEntries & entries, const PlayerSlots & slots);
    static PlayerEntries     ApplySetupView (const PlayerEntries    & entries,
                                             const PlayerSlots      & slots,
                                             const MultiplayerSetup & setup);

private:

    struct Context;

    static bool  IsAttached         (const std::vector<ControllerDeviceInfo> & devices, const ControllerUnitKey & unit);
    static bool  IsLogged           (const std::vector<ControllerUnitKey> & log, const ControllerUnitKey & unit);
    static bool  IsHostInput        (const PlayerEntry & entry);
    static bool  IsPickedUnit       (const Context & context, const ControllerUnitKey & unit);
    static bool  CountsAsPlaying    (const PlayerSlot & slot, const PlayerEntry & entry);
    static void  ResolvePicks       (Context & context);
    static void  EvaluateBase      (const Context & context, size_t player, PlayerSlots & slots, std::array<bool, kPlayerCount> & isLeaving);
    static void  ResolveDepartures  (const Context & context, const std::array<bool, kPlayerCount> & isLeaving, PlayerSlots & slots);
    static void  GiveWayProvisional (const Context & context, PlayerSlots & slots);
    static bool  ShouldStartOver    (const PlayerSlots & previous, const PlayerSlots & slots);
    static bool  IsInUse            (const PlayerSlot & slot);
    static void  StartOver          (PlayerSlots & slots, PlayerOrderLogs & logs);
    static void  FillAutomatic      (const Context & context, PlayerSlots & slots);
    static void  SetTargets         (const Context & context, PlayerSlots & slots);
    static bool  IsCandidate        (const Context & context, const PlayerSlots & slots, const ControllerUnitKey & unit);
    static bool  IsSameUnit         (const std::optional<ControllerUnitKey> & last, const ControllerUnitKey & unit);

    static const ControllerDeviceInfo *  FindDevice (const std::vector<ControllerDeviceInfo> & devices, const ControllerUnitKey & unit);
};
