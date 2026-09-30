#include "Pch.h"

#include "Controllers/PlayerModeRules.h"

#include "Config/MachineInputPrefs.h"
#include "Controllers/ControllerTokens.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveMode
//
//  The mode a player plays in. Same as Player 1 is Player 1's mode, except
//  that with Player 1 in a jack it is the other jack, so the two never share
//  one; Player 1 has no one to follow and plays it as Joystick. A machine
//  without a Joyport plays either jack as Joystick, and the mode is kept for
//  the next machine that has one.
//
////////////////////////////////////////////////////////////////////////////////

PlayerMode PlayerModeRules::ResolveMode (const PlayerEntries & entries, size_t player, bool hasJoyport)
{
    PlayerMode  mode = PlayerMode::Joystick;



    if (player >= entries.size())
    {
        return mode;
    }

    mode = entries[player].mode;

    if (mode == PlayerMode::SameAsPlayer1)
    {
        mode = (player == kPlayerOne) ? PlayerMode::Joystick : GetOtherJackMode (ResolveMode (entries, kPlayerOne, hasJoyport));
    }

    if (!hasJoyport && IsJoyportMode (mode))
    {
        mode = PlayerMode::Joystick;
    }

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetOtherJackMode
//
//  The other jack for a jack, and any other mode as it is.
//
////////////////////////////////////////////////////////////////////////////////

PlayerMode PlayerModeRules::GetOtherJackMode (PlayerMode mode)
{
    if (mode == PlayerMode::JoyportLeft)
    {
        return PlayerMode::JoyportRight;
    }

    if (mode == PlayerMode::JoyportRight)
    {
        return PlayerMode::JoyportLeft;
    }

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsJoyportMode
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerModeRules::IsJoyportMode (PlayerMode mode)
{
    return mode == PlayerMode::JoyportLeft || mode == PlayerMode::JoyportRight;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPaddleMode
//
//  Paddle or Two paddles: the modes that play a Paddle profile, and the
//  ones the mouse plays in and the keys do not.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerModeRules::IsPaddleMode (PlayerMode mode)
{
    return mode == PlayerMode::Paddle || mode == PlayerMode::TwoPaddles;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetJack
//
////////////////////////////////////////////////////////////////////////////////

std::optional<size_t> PlayerModeRules::GetJack (PlayerMode mode)
{
    std::optional<size_t>  jack;



    if (mode == PlayerMode::JoyportLeft)
    {
        jack = JoyportJacks::kLeftJack;
    }
    else if (mode == PlayerMode::JoyportRight)
    {
        jack = JoyportJacks::kRightJack;
    }

    return jack;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPlaying
//
//  Every player but a Disabled Player 2, which stands in for nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerModeRules::IsPlaying (const PlayerEntries & entries, size_t player)
{
    return player < entries.size() && entries[player].kind != PlayerEntryKind::Disabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsOnJoyport
//
//  A playing player in one of the jacks of a machine that has a Joyport.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerModeRules::IsOnJoyport (const PlayerEntries & entries, size_t player, bool hasJoyport)
{
    return IsPlaying (entries, player) && IsJoyportMode (ResolveMode (entries, player, hasJoyport));
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsJoyportOn
//
//  The machine's Joyport is on while a player is in one of its jacks, and
//  off otherwise. There is no other setting for it.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerModeRules::IsJoyportOn (const PlayerEntries & entries, bool hasJoyport)
{
    return IsOnJoyport (entries, kPlayerOne, hasJoyport) || IsOnJoyport (entries, kPlayerTwo, hasJoyport);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AreButtonsCut
//
//  The Joyport owns all three button lines, so a player left on Joystick,
//  Paddle or Two paddles beside it keeps its paddles but not its buttons,
//  as on the hardware, where the switch that selects the Atari jacks cuts
//  the rear sockets' buttons off.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerModeRules::AreButtonsCut (const PlayerEntries & entries, size_t player, bool hasJoyport)
{
    return IsPlaying (entries, player) && IsJoyportOn (entries, hasJoyport) && !IsOnJoyport (entries, player, hasJoyport);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArePaddlesConnected
//
//  Whether the paddle inputs are read while the Joyport is on. The hardware
//  always reads them, from the rear sockets; with every playing player in a
//  jack nothing stands in for a paddle, so they read as no paddle connected,
//  which is what software that checks for one expects to find.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerModeRules::ArePaddlesConnected (const PlayerEntries & entries, bool hasJoyport)
{
    size_t  player = 0;



    for (player = 0; player < entries.size(); player++)
    {
        if (IsPlaying (entries, player) && !IsOnJoyport (entries, player, hasJoyport))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsModeTaken
//
//  A jack the other player holds. Player 2 on Same as Player 1 follows
//  Player 1 into the other jack rather than holding one, so it takes nothing
//  from Player 1; a Disabled Player 2 holds nothing either.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerModeRules::IsModeTaken (const PlayerEntries & entries, size_t player, PlayerMode mode, bool hasJoyport)
{
    size_t      other     = (player == kPlayerOne) ? kPlayerTwo : kPlayerOne;
    PlayerMode  otherMode = PlayerMode::Joystick;



    if (!IsJoyportMode (mode) || other >= entries.size() || !IsPlaying (entries, other))
    {
        return false;
    }

    otherMode = (entries[other].mode == PlayerMode::SameAsPlayer1) ? PlayerMode::SameAsPlayer1
                                                                    : ResolveMode (entries, other, hasJoyport);

    return otherMode == mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AreKeysOffered
//
//  The keys are a joystick: Player 1 in Joystick mode, or in a jack, where
//  they close its switches.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerModeRules::AreKeysOffered (const PlayerEntries & entries, bool hasJoyport)
{
    return !IsPaddleMode (ResolveMode (entries, kPlayerOne, hasJoyport));
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsMouseOffered
//
//  The mouse is a paddle, or two: Player 1 in Paddle or Two paddles mode
//  only.
//
////////////////////////////////////////////////////////////////////////////////

bool PlayerModeRules::IsMouseOffered (const PlayerEntries & entries, bool hasJoyport)
{
    return IsPaddleMode (ResolveMode (entries, kPlayerOne, hasJoyport));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetModeLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PlayerModeRules::GetModeLabel (PlayerMode mode)
{
    std::wstring  label;



    switch (mode)
    {
        case PlayerMode::Joystick:       label = L"Joystick";               break;
        case PlayerMode::JoyportLeft:    label = L"Joyport left (Atari)";   break;
        case PlayerMode::JoyportRight:   label = L"Joyport right (Atari)";  break;
        case PlayerMode::Paddle:         label = L"Paddle";                 break;
        case PlayerMode::TwoPaddles:     label = L"Two paddles";            break;
        case PlayerMode::SameAsPlayer1:  label = L"Automatic";              break;

        default:                                                            break;
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAutomaticModeLabel
//
//  Player 2's Automatic mode with what it resolves to: "Automatic
//  (joystick)", "Automatic (Joyport right)", "Automatic (paddle)",
//  "Automatic (two paddles)".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PlayerModeRules::GetAutomaticModeLabel (PlayerMode resolved)
{
    std::wstring  mode;



    switch (resolved)
    {
        case PlayerMode::Joystick:      mode = L"joystick";       break;
        case PlayerMode::JoyportLeft:   mode = L"Joyport left";   break;
        case PlayerMode::JoyportRight:  mode = L"Joyport right";  break;
        case PlayerMode::Paddle:        mode = L"paddle";         break;
        case PlayerMode::TwoPaddles:    mode = L"two paddles";    break;

        default:                                                  break;
    }

    return mode.empty() ? GetModeLabel (PlayerMode::SameAsPlayer1)
                        : GetModeLabel (PlayerMode::SameAsPlayer1) + L" (" + mode + L")";
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPlayerLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PlayerModeRules::GetPlayerLabel (size_t player)
{
    return std::format (L"Player {}", player + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildModeChoices
//
//  Joystick, the two jacks, Paddle and Two paddles, in that order, with
//  Automatic ahead of them for Player 2, showing the mode it resolves to.
//  Player 1 has no Automatic. The player's own mode is checked, Automatic included, and a
//  jack the other player holds is listed but cannot be chosen. A machine
//  without a Joyport lists no jacks, and a jack saved earlier is checked as
//  the Joystick it plays as there.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<InputModeRules::PlayerModeChoice> PlayerModeRules::BuildModeChoices (
    const PlayerEntries  & entries,
    size_t                 player,
    bool                   hasJoyport)
{
    std::vector<InputModeRules::PlayerModeChoice>  choices;
    std::vector<PlayerMode>                        modes;
    PlayerEntries                                  automatic = entries;
    PlayerMode                                     current   = PlayerMode::Joystick;



    if (player >= entries.size())
    {
        return choices;
    }

    current = entries[player].mode;

    if (current != PlayerMode::SameAsPlayer1 && !(hasJoyport && IsJoyportMode (current)))
    {
        current = ResolveMode (entries, player, hasJoyport);
    }

    if (player == kPlayerTwo)
    {
        modes.push_back (PlayerMode::SameAsPlayer1);
    }

    modes.push_back (PlayerMode::Joystick);

    if (hasJoyport)
    {
        modes.push_back (PlayerMode::JoyportLeft);
        modes.push_back (PlayerMode::JoyportRight);
    }

    modes.push_back (PlayerMode::Paddle);
    modes.push_back (PlayerMode::TwoPaddles);

    for (PlayerMode mode : modes)
    {
        InputModeRules::PlayerModeChoice  choice;

        choice.label     = GetModeLabel (mode);

        if (mode == PlayerMode::SameAsPlayer1)
        {
            automatic[player].mode = PlayerMode::SameAsPlayer1;
            choice.label           = GetAutomaticModeLabel (ResolveMode (automatic, player, hasJoyport));
        }

        choice.mode      = mode;
        choice.isChecked = mode == current;
        choice.isEnabled = !IsModeTaken (entries, player, mode, hasJoyport);

        choices.push_back (choice);
    }

    return choices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DescribeAssignment
//
//  "Player 1: <description>", with the jacks after the player for one in the
//  Joyport: "Player 1 (Joyport left and right): <description>" while it
//  drives both.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PlayerModeRules::DescribeAssignment (
    size_t                player,
    const std::wstring  & description,
    JackSet               jacks)
{
    std::wstring  label = GetPlayerLabel (player);



    if (jacks.all())
    {
        label += L" (Joyport left and right)";
    }
    else if (jacks.test (JoyportJacks::kLeftJack))
    {
        label += L" (Joyport left)";
    }
    else if (jacks.test (JoyportJacks::kRightJack))
    {
        label += L" (Joyport right)";
    }

    return label + L": " + description;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MigrateAdapter
//
//  The Joyport was once a global setting, and before that one saved with
//  each machine. It is read only while no player has a saved mode, since the
//  saved modes are what mark it read: a global value of Sirius Joyport puts
//  the players in its jacks, and a global value never set reads the launched
//  machine's own, which the //c cannot have. The global key is removed
//  whenever it is present; the machines' own values are left for older builds.
//
////////////////////////////////////////////////////////////////////////////////

JoyportMigration PlayerModeRules::MigrateAdapter (
    bool                 hasSavedModes,
    const std::string  & globalToken,
    const JsonValue    * launchedUiPrefs,
    bool                 launchedHasAnnunciators)
{
    JoyportMigration  migration;
    GamePortAdapter   adapter   = GamePortAdapter::None;



    migration.shouldRemoveKey = !globalToken.empty();

    if (hasSavedModes)
    {
        return migration;
    }

    adapter = globalToken.empty() ? MachineInputPrefs::ReadGamePortAdapter (launchedUiPrefs, launchedHasAnnunciators)
                                  : ControllerTokens::GamePortAdapterFromToken (globalToken);

    migration.isJoyport = adapter == GamePortAdapter::SiriusJoyport;

    return migration;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyMigration
//
//  What a Joyport turned on for everyone becomes: Player 1 in the left jack,
//  and Player 2 following into the right one.
//
////////////////////////////////////////////////////////////////////////////////

PlayerEntries PlayerModeRules::ApplyMigration (PlayerEntries entries)
{
    entries[kPlayerOne].mode = PlayerMode::JoyportLeft;
    entries[kPlayerTwo].mode = PlayerMode::SameAsPlayer1;

    return entries;
}
