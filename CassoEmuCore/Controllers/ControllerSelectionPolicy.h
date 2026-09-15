#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionChangeReason
//
//  Why the policy changed the selection, which decides what the user is told.
//
////////////////////////////////////////////////////////////////////////////////

enum class SelectionChangeReason
{
    None,
    AutomaticSelection,   // nothing was selected and a controller is attached
    Adoption,             // the selected unit returned under a different identity
    Replacement,          // the selected controller left and an attached one took over
    Cleared               // the selected controller left and nothing is attached
};





////////////////////////////////////////////////////////////////////////////////
//
//  PlayerAxisTarget
//
//  What one multiplayer slot maps to: a joystick, meaning two paddles wired to
//  one stick, or a single paddle. The four-axis game port offers two joysticks
//  or four paddles, and this is the one choice a player slot carries.
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
//  A machine is in one of two modes. In SINGLE-SOURCE mode one controller, or
//  the arrow keys, or the mouse drives the game port, which is what a machine
//  has always done. In MULTIPLAYER mode two player slots drive it and the
//  selection drives nothing.
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
//  BUTTONS FOLLOW THE PLAYER. Player 1's pb0 bindings drive PB0 and player 2's
//  drive PB1; a player's pb1 and pb2 bindings are kept in the profile and
//  ignored while multiplayer is on, and PB2 is unused. In single-source mode
//  the one controller drives PB0-PB2 as it always has.
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
//  Which controller drives the game port, given what is attached and what was
//  chosen before. Pure: it reads no device and writes nothing, so every rule
//  in it is reachable from the unit tests with a list of made-up devices.
//
//  THE SELECTION STAYS ON THE CONTROLLER IN USE until that controller goes or
//  the user picks another (FR-008a). Keeping an absent one chosen, with
//  another driving until it returned, left the user picking an entry the
//  picker already showed checked to keep the controller that had taken over.
//
////////////////////////////////////////////////////////////////////////////////

class ControllerSelectionPolicy
{
public:

    struct Decision
    {
        std::optional<ControllerUnitKey>  selection;
        SelectionChangeReason             reason                = SelectionChangeReason::None;
        bool                              hasChanged            = false;
        bool                              clearsOtherInputModes = false;
        std::wstring                      description;

        // The controller that left, when that is what moved the selection.
        std::wstring                      departedDescription;

        // Whether the user hears about it. A saved controller that is not
        // plugged in at launch was never driving anything, so dropping it is
        // not news.
        bool                              isAnnounced           = true;
    };

    // PDL0 and PDL1: what the one controller drives in single-source mode.
    static constexpr unsigned long  kSingleSourceAxisBits = 0x3;

    static Decision  Evaluate           (const std::optional<ControllerUnitKey> &   current,
                                         const std::vector<ControllerDeviceInfo> &  devices,
                                         bool                                       hasGamePort,
                                         const MultiplayerSetup &                   multiplayer = {});

    static bool      IsSelectedAttached (const std::optional<ControllerUnitKey> &   selection,
                                         const std::vector<ControllerDeviceInfo> &  devices);

    // The paddles a target maps to on a machine with axisCount axes. Paddles
    // past the count are left out, not removed from the slot, so a setup saved
    // on a //e plays again on returning to one (FR-035).
    static MultiplayerSetup::AxisSet  GetTargetAxes (PlayerAxisTarget  target,
                                                     size_t            axisCount);

    // What one player drives: nothing while multiplayer is off, while the slot
    // is empty, or while this machine has none of the slot's paddles.
    static MultiplayerSetup::AxisSet  GetAxesForPlayer (const MultiplayerSetup & setup,
                                                        size_t                   player,
                                                        size_t                   axisCount);

    // Which player holds this controller, or none. A controller in neither
    // slot plays nothing while multiplayer is on.
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

    static const ControllerDeviceInfo &  GetPreferredDevice (const std::vector<ControllerDeviceInfo> &  devices,
                                                             const MultiplayerSetup &                   multiplayer);

    static const ControllerDeviceInfo *  FindSoleSameModel  (const std::vector<ControllerDeviceInfo> &  devices,
                                                             const ControllerModelKey &                 model);
};
