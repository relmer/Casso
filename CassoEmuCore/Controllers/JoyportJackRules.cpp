#include "Pch.h"

#include "Controllers/JoyportJackRules.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AssignJacks
//
//  One player driving alone is on both jacks, so a one-player game works
//  whichever jack it reads. Two driving split them, Player 1 left and
//  Player 2 right, whatever paddles their slots map to.
//
//  A HELD PLAYER KEEPS THEIR JACK, which reads open, and the one who stayed
//  does not take it: in a two-player game their stick would then move both
//  players. Player 2 set to Same as left plays nothing, so Player 1 drives
//  both jacks whatever Player 2's controller does.
//
////////////////////////////////////////////////////////////////////////////////

JoyportJackRules::JackSources JoyportJackRules::AssignJacks (const JoyportPlayers & players)
{
    JoyportPlayerState  first   = players.players[0];
    JoyportPlayerState  second  = players.isPlayer2Disabled ? JoyportPlayerState::Idle : players.players[1];
    JackSources         sources = { JoyportJackSource::None, JoyportJackSource::None };



    if (first == JoyportPlayerState::Driving && second == JoyportPlayerState::Idle)
    {
        sources = { JoyportJackSource::Player1, JoyportJackSource::Player1 };
    }
    else if (first == JoyportPlayerState::Idle && second == JoyportPlayerState::Driving)
    {
        sources = { JoyportJackSource::Player2, JoyportJackSource::Player2 };
    }
    else
    {
        sources[JoyportJacks::kLeftJack]  = first  == JoyportPlayerState::Driving ? JoyportJackSource::Player1 : JoyportJackSource::None;
        sources[JoyportJacks::kRightJack] = second == JoyportPlayerState::Driving ? JoyportJackSource::Player2 : JoyportJackSource::None;
    }

    return sources;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReducePlayerState
//
//  A controller in play drives, and so do the arrow keys, which are Player
//  1's alone. A slot kept for a player who left is held. Everything else is
//  idle: an empty slot, a holder that has not given input, Disabled, and the
//  mouse as paddle, which closes no switch.
//
////////////////////////////////////////////////////////////////////////////////

JoyportPlayerState JoyportJackRules::ReducePlayerState (size_t player, const PlayerSlot & slot, const PlayerEntry & entry)
{
    JoyportPlayerState  state = JoyportPlayerState::Idle;



    if (player == 0 && entry.kind == PlayerEntryKind::ArrowKeys)
    {
        state = JoyportPlayerState::Driving;
    }
    else if (PlayerSlotPolicy::IsDrivingSlot (slot))
    {
        state = JoyportPlayerState::Driving;
    }
    else if (slot.state == PlayerSlotState::Held)
    {
        state = JoyportPlayerState::Held;
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReducePlayers
//
////////////////////////////////////////////////////////////////////////////////

JoyportPlayers JoyportJackRules::ReducePlayers (const PlayerSlots & slots, const PlayerEntries & entries)
{
    JoyportPlayers  players;
    size_t          player  = 0;



    for (player = 0; player < PlayerSlotPolicy::kPlayerCount; player++)
    {
        players.players[player] = ReducePlayerState (player, slots[player], entries[player]);
    }

    players.isPlayer2Disabled = entries[1].kind == PlayerEntryKind::Disabled;

    return players;
}
