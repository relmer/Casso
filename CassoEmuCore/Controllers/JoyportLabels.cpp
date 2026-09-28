#include "Pch.h"

#include "Controllers/JoyportLabels.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GetPlayerLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring JoyportLabels::GetPlayerLabel (size_t player, bool isJoyportInEffect)
{
    InputModeRules::PlayerLabels  labels = GetPickerLabels (isJoyportInEffect);



    return player < labels.players.size() ? labels.players[player] : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDisabledLabel
//
//  Player 2's Disabled entry. While the Joyport is on, a disabled Player 2
//  leaves Player 1 on both jacks, so the right jack is the same as the left.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring JoyportLabels::GetDisabledLabel (bool isJoyportInEffect)
{
    return GetPickerLabels (isJoyportInEffect).disabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAutomaticRowText
//
//  A player's picker row while they are on Automatic with no controller
//  playing. Joyport right then carries whatever drives the left jack, so it
//  says so.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring JoyportLabels::GetAutomaticRowText (size_t player, bool isJoyportInEffect)
{
    InputModeRules::PlayerLabels  labels = GetPickerLabels (isJoyportInEffect);
    std::wstring                  text   = kpszAutomatic;



    if (player >= labels.players.size())
    {
        return std::wstring();
    }

    if (!labels.idle[player].empty())
    {
        text = labels.idle[player];
    }

    return labels.players[player] + L": " + text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DescribeAssignment
//
//  The notice for a controller Automatic gave a player. While the Joyport is
//  on, a controller playing alone drives both jacks, and the notice says so.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring JoyportLabels::DescribeAssignment (
    size_t                player,
    const std::wstring  & description,
    bool                  isAlone,
    bool                  isJoyportInEffect)
{
    std::wstring  label = GetPlayerLabel (player, isJoyportInEffect);



    if (isJoyportInEffect && isAlone)
    {
        label = kpszBoth;
    }

    return label + L": " + description;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPickerLabels
//
//  The picker's own words with the Joyport off, and the jacks' with it on.
//  "Same as left" starts a submenu entry, and is lower case after the colon
//  of a player's row.
//
////////////////////////////////////////////////////////////////////////////////

InputModeRules::PlayerLabels JoyportLabels::GetPickerLabels (bool isJoyportInEffect)
{
    InputModeRules::PlayerLabels  labels;



    if (isJoyportInEffect)
    {
        labels.players       = { kpszLeft, kpszRight };
        labels.disabled      = kpszSameAsLeft;
        labels.disabledInRow = kpszRightIdle;
        labels.idle          = { std::wstring(), kpszRightIdle };
    }

    return labels;
}
