#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PlayerAxisTarget
//
//  What one player slot maps to: a joystick, meaning two paddles wired to one
//  stick, or a single paddle. The four-axis game port offers two joysticks or
//  four paddles, and this is the one choice a player slot carries.
//
////////////////////////////////////////////////////////////////////////////////

enum class PlayerAxisTarget
{
    Joystick0,   // PDL0 and PDL1
    Joystick1,   // PDL2 and PDL3
    Paddle0,
    Paddle1,
    Paddle2,
    Paddle3
};





////////////////////////////////////////////////////////////////////////////////
//
//  MultiplayerSlot
//
//  One player: the controller they hold and what it maps to. A slot with no
//  controller is empty and plays nothing.
//
////////////////////////////////////////////////////////////////////////////////

struct MultiplayerSlot
{
    std::optional<ControllerUnitKey>  unit;
    PlayerAxisTarget                  target = PlayerAxisTarget::Joystick0;

    bool operator== (const MultiplayerSlot &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MultiplayerSetup
//
//  The two player slots as the Controllers page edits them and a machine's
//  saved two-player block holds them: each player's controller and what it
//  maps to. `isEnabled` is whether the two-player section is in use.
//
//  EXACTLY TWO SLOTS. The Apple II game port reads two buttons a game can tell
//  apart, so a third player has no button line to be given.
//
//  THE TWO SLOTS MAY NOT OVERLAP. One controller cannot be both players, and
//  two players cannot claim one paddle, so a configuration that says otherwise
//  is normalized rather than played (ControllerSelectionPolicy::Normalize).
//
//  THE ASSIGNMENT REMAPS, THE PROFILE BINDS. A player's controller plays its
//  own profile: its PDL0-PDL3 targets land on the paddles the slot maps to, in
//  ascending order. A slot mapped to a single paddle plays only the
//  controller's pdl0 bindings. So two players share one Default or Paddles
//  profile without a per-player copy.
//
////////////////////////////////////////////////////////////////////////////////

struct MultiplayerSetup
{
    using AxisSet = std::bitset<GamePortContribution::kAxisCount>;

    static constexpr size_t  kPlayerCount = 2;

    bool                                       isEnabled = false;
    std::array<MultiplayerSlot, kPlayerCount>  players;

    bool operator== (const MultiplayerSetup &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControllerSelectionPolicy
//
//  The rules the two player slots share whoever fills them: which paddles a
//  target maps to, what the settings may offer a slot, how a repeated or
//  overlapping pair is refused, and how a picked controller that came back
//  under another identity is recognized. Pure: it reads no device and writes
//  nothing, so every rule in it is reachable from the unit tests with a list
//  of made-up devices.
//
////////////////////////////////////////////////////////////////////////////////

class ControllerSelectionPolicy
{
public:

    // The attached controller a pick is played as: the pick itself when it is
    // attached, or else the one attached unit of its model. A unit that moved
    // comes back under a different identity -- a DirectInput device on
    // another port, an Xbox-class controller in another XInput slot -- so the
    // one unit of its model is taken to be it; with two of that model, which
    // one moved is a coin flip, and neither is.
    static std::optional<ControllerUnitKey>  FindAdoptedUnit (const ControllerUnitKey &                  pick,
                                                              const std::vector<ControllerDeviceInfo> &  devices);

    // Moves each player slot still naming an Xbox-class controller by XInput
    // slot, as preferences written before units were keyed by product do,
    // onto the unit that slot holds now. Returns whether any slot moved, so
    // the caller can save the result and the move happens once.
    static bool      AdoptSlotKeyedPlayers (MultiplayerSetup &                         setup,
                                            const std::vector<ControllerDeviceInfo> &  devices);

    // The paddles a target maps to on a machine with axisCount axes. Paddles
    // past the count are left out, not removed from the slot, so a setup saved
    // on a //e plays again on returning to one.
    static MultiplayerSetup::AxisSet  GetTargetAxes (PlayerAxisTarget  target,
                                                     size_t            axisCount);

    // What one player drives in a setup: nothing while it is not enabled,
    // while the slot is empty, or while this machine has none of the slot's
    // paddles.
    static MultiplayerSetup::AxisSet  GetAxesForPlayer (const MultiplayerSetup & setup,
                                                        size_t                   player,
                                                        size_t                   axisCount);

    // Which player holds this controller in a setup, or none.
    static std::optional<size_t>  FindPlayer (const MultiplayerSetup &   setup,
                                              const ControllerUnitKey &  unit);

    // The setup as it will be played: a second slot that repeats the first
    // slot's controller, or claims a paddle it already holds, is emptied
    // rather than trusted. Every setter and the prefs reader run through this.
    static MultiplayerSetup  Normalize (MultiplayerSetup setup);

    // What a slot may map to on this machine, less whatever the other slot
    // holds. This is the list the settings page offers.
    static std::vector<PlayerAxisTarget>  GetTargetChoices (const MultiplayerSetup & setup,
                                                            size_t                   player,
                                                            size_t                   axisCount);

private:

    static const ControllerDeviceInfo *  FindUnit           (const std::vector<ControllerDeviceInfo> &  devices,
                                                             const ControllerUnitKey &                  unit);

    static const ControllerDeviceInfo *  FindSoleSameModel  (const std::vector<ControllerDeviceInfo> &  devices,
                                                             const ControllerModelKey &                 model);
};
