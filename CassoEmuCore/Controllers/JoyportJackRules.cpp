#include "Pch.h"

#include "Controllers/JoyportJackRules.h"

#include "Controllers/PlayerModeRules.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AssignJacks
//
//  Each player drives the jack its mode puts it in. A jack is free while no
//  player is in it or its player is idle, and a player driving in the other
//  jack then drives it too, so a one-player game works whichever jack it
//  reads.
//
//  A HELD PLAYER KEEPS THEIR JACK, which reads open, and the one who stayed
//  does not take it: in a two-player game their stick would then move both
//  players.
//
////////////////////////////////////////////////////////////////////////////////

JoyportJackRules::JackSources JoyportJackRules::AssignJacks (const JoyportPlayers & players)
{
    JackSources                                                  sources = { JoyportJackSource::None, JoyportJackSource::None };
    std::array<std::optional<size_t>, JoyportJacks::kJackCount>  owners;
    size_t                                                       player  = 0;
    size_t                                                       jack    = 0;



    for (player = 0; player < PlayerSlotPolicy::kPlayerCount; player++)
    {
        if (players.jacks[player].has_value() && players.jacks[player].value() < owners.size())
        {
            owners[players.jacks[player].value()] = player;
        }
    }

    for (jack = 0; jack < JoyportJacks::kJackCount; jack++)
    {
        std::optional<size_t>  owner = owners[jack];
        std::optional<size_t>  other = owners[JoyportJacks::kJackCount - 1 - jack];

        if (owner.has_value() && players.players[owner.value()] == JoyportPlayerState::Driving)
        {
            sources[jack] = GetSource (owner.value());
        }
        else if (owner.has_value() && players.players[owner.value()] == JoyportPlayerState::Held)
        {
            continue;
        }
        else if (other.has_value() && players.players[other.value()] == JoyportPlayerState::Driving)
        {
            sources[jack] = GetSource (other.value());
        }
    }

    return sources;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSource
//
////////////////////////////////////////////////////////////////////////////////

JoyportJackSource JoyportJackRules::GetSource (size_t player)
{
    return (player == 0) ? JoyportJackSource::Player1 : JoyportJackSource::Player2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPlayerJacks
//
////////////////////////////////////////////////////////////////////////////////

std::bitset<JoyportJacks::kJackCount> JoyportJackRules::GetPlayerJacks (const JackSources & sources, size_t player)
{
    std::bitset<JoyportJacks::kJackCount>  jacks;
    size_t                                 jack  = 0;



    for (jack = 0; jack < JoyportJacks::kJackCount; jack++)
    {
        jacks.set (jack, sources[jack] == GetSource (player));
    }

    return jacks;
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

JoyportPlayers JoyportJackRules::ReducePlayers (const PlayerSlots & slots, const PlayerEntries & entries, bool hasJoyport)
{
    JoyportPlayers  players;
    size_t          player  = 0;



    for (player = 0; player < PlayerSlotPolicy::kPlayerCount; player++)
    {
        players.players[player] = ReducePlayerState (player, slots[player], entries[player]);

        if (PlayerModeRules::IsOnJoyport (entries, player, hasJoyport))
        {
            players.jacks[player] = PlayerModeRules::GetJack (PlayerModeRules::ResolveMode (entries, player, hasJoyport));
        }
    }

    return players;
}
