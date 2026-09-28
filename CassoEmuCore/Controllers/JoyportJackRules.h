#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"
#include "Controllers/PlayerSlotPolicy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  JoyportPlayerState
//
//  One player as the Joyport sees them: driving (a controller in play, or the
//  arrow keys), held (their controller left while the other played on), or
//  idle.
//
////////////////////////////////////////////////////////////////////////////////

enum class JoyportPlayerState
{
    Idle,
    Driving,
    Held
};





////////////////////////////////////////////////////////////////////////////////
//
//  JoyportPlayers
//
//  Both players, and whether Player 2 is Disabled, which reads Same as left
//  while the Joyport is on.
//
////////////////////////////////////////////////////////////////////////////////

struct JoyportPlayers
{
    std::array<JoyportPlayerState, PlayerSlotPolicy::kPlayerCount>  players           = {};
    bool                                                            isPlayer2Disabled = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  JoyportJackSource
//
//  Whose switches a jack carries. None reads every switch open.
//
////////////////////////////////////////////////////////////////////////////////

enum class JoyportJackSource
{
    None,
    Player1,
    Player2
};





////////////////////////////////////////////////////////////////////////////////
//
//  JoyportJackRules
//
//  Which player drives which Joyport jack. Pure, so every combination of the
//  players' states is a unit test.
//
////////////////////////////////////////////////////////////////////////////////

class JoyportJackRules
{
public:

    using JackSources = std::array<JoyportJackSource, JoyportJacks::kJackCount>;

    static JackSources         AssignJacks       (const JoyportPlayers & players);
    static JoyportPlayerState  ReducePlayerState (size_t player, const PlayerSlot & slot, const PlayerEntry & entry);
    static JoyportPlayers      ReducePlayers     (const PlayerSlots & slots, const PlayerEntries & entries);
};
