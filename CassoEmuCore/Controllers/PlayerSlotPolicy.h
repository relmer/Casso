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
//  PlayerMode
//
//  What a player stands in for on the game port: a joystick, two paddles
//  wired to one stick; an Atari stick in one of the Joyport's two jacks; a
//  single paddle; or two paddles, each a knob of its own. The two players'
//  modes alone decide which paddles, button lines and jacks each drives, as
//  the hardware wires them, and the Joyport is on while either player is in
//  one of its jacks. SameAsPlayer1 is Player 2's alone: Player 1's mode, or
//  with Player 1 in a jack, the other jack.
//
////////////////////////////////////////////////////////////////////////////////

enum class PlayerMode
{
    Joystick,
    JoyportLeft,
    JoyportRight,
    Paddle,
    SameAsPlayer1,
    TwoPaddles
};





////////////////////////////////////////////////////////////////////////////////
//
//  PlayerEntry
//
//  One player's choice. `unit` is set only for Controller: the picked
//  controller, attached or not. `mode` is the player's own, kept whatever
//  the entry: the keys play in Joystick mode or a Joyport jack, and the
//  mouse in Paddle or Two paddles mode.
//
////////////////////////////////////////////////////////////////////////////////

struct PlayerEntry
{
    PlayerEntryKind                   kind = PlayerEntryKind::Automatic;
    std::optional<ControllerUnitKey>  unit;
    PlayerMode                        mode = PlayerMode::Joystick;

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
//  Waiting     a picked controller that is not attached
//  Playing     picked and attached, or chosen by Automatic: by its first
//              input, or by connecting while Casso runs
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
//  `target` is what the two players' modes give this one.
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

    // `hasJoyport` is whether the running machine has a Joyport, which
    // decides whether a player's jack is played as one.
    static PlayerSlots  Evaluate           (const PlayerEntries                      & entries,
                                            const std::vector<ControllerDeviceInfo>  & devices,
                                            PlayerOrderLogs                          & logs,
                                            const PlayerSlots                        & previous,
                                            bool                                       hasJoyport = false);

    static bool         IsRealInput        (const ControllerSample       & sample,
                                            const ControllerCalibration  * calibration,
                                            float                          deadzone,
                                            const ControllerSample       * rest = nullptr);

    static bool         IsOnePlaying       (const PlayerSlots & slots, const PlayerEntries & entries);
    static bool         IsDrivingSlot      (const PlayerSlot & slot);
    static bool         NeedsIdleWatch     (const PlayerEntries & entries, const PlayerSlots & slots);

    // Each slot's attached holder becomes its last holder. Returns a notice
    // for each slot Automatic gave a controller other than its last holder,
    // giving the jacks of a player in the Joyport.
    static std::vector<std::wstring>  RecordHolders (const PlayerSlots                        & slots,
                                                     const std::vector<ControllerDeviceInfo>  & devices,
                                                     PlayerLastHolders                        & lastHolders,
                                                     const PlayerEntries                      & entries    = {},
                                                     bool                                       hasJoyport = false);

    // What a player's controller reaches on this machine, or nothing when
    // its slot plays no controller. One player playing alone reaches what a
    // single controller of its mode always has; two each reach their own
    // target. A player beside the Joyport reaches no button line.
    static std::optional<PlayerTargetRules::Route>  GetDriverRoute (const PlayerSlots    & slots,
                                                                    const PlayerEntries  & entries,
                                                                    size_t                 player,
                                                                    size_t                 axisCount,
                                                                    bool                   hasJoyport = false);

    // The same whether or not the player drives, which is what the settings
    // show for a player waiting for a controller, buttons included.
    static PlayerTargetRules::Route                 GetPlayerRoute (const PlayerSlots    & slots,
                                                                    const PlayerEntries  & entries,
                                                                    size_t                 player,
                                                                    size_t                 axisCount,
                                                                    bool                   hasJoyport = false);

    // What the two players' modes give each one, as the slots take it.
    static std::array<PlayerAxisTarget, kPlayerCount>  GetModeTargets (const PlayerEntries & entries, bool hasJoyport);

    // A pick, and picking a controller the other player holds returns the
    // other player to Automatic. The player keeps its mode, except that the
    // keys leave a paddle mode for Joystick and the mouse takes Paddle mode
    // unless it is in Two paddles mode.
    static PlayerEntries  ApplyPick (PlayerEntries entries, size_t player, const PlayerEntry & entry);

    // A player's mode. Player 1's keys in a paddle mode and mouse in any
    // other mode are entries that mode cannot have, and return to Automatic.
    static PlayerEntries  ApplyMode (PlayerEntries entries, size_t player, PlayerMode mode);

    // Entries as they can be played: an entry its player cannot have, and a
    // second pick of the first player's controller, read as Automatic; the
    // keys play in Joystick mode or a jack and the mouse in a paddle mode;
    // Same as Player 1 is Player 2's alone, and a jack both players claim is
    // left to Player 1, with Player 2 following into the other.
    static PlayerEntries  NormalizeEntries (PlayerEntries entries);

    // The players before anything is chosen: both on Automatic, Player 1 in
    // Joystick mode and Player 2 on Same as Player 1.
    static PlayerEntries  MakeDefaultEntries ();

    // The players as they play, as a two-slot setup: each one's controller
    // and the target it plays.
    static MultiplayerSetup  MakeSetupView  (const PlayerEntries & entries, const PlayerSlots & slots);

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
