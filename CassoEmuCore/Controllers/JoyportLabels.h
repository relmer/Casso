#pragma once

#include "Pch.h"

#include "Controllers/InputModeRules.h"





////////////////////////////////////////////////////////////////////////////////
//
//  JoyportLabels
//
//  The words the picker and the assignment notices use for the players.
//  While the Joyport is in effect the players are the jacks they drive,
//  Joyport left and Joyport right; otherwise they are Player 1 and Player 2.
//
////////////////////////////////////////////////////////////////////////////////

class JoyportLabels
{
public:

    static std::wstring                  GetPlayerLabel      (size_t player, bool isJoyportInEffect);
    static std::wstring                  GetDisabledLabel    (bool isJoyportInEffect);
    static std::wstring                  GetAutomaticRowText (size_t player, bool isJoyportInEffect);
    static std::wstring                  DescribeAssignment  (size_t                player,
                                                              const std::wstring  & description,
                                                              bool                  isAlone,
                                                              bool                  isJoyportInEffect);
    static InputModeRules::PlayerLabels  GetPickerLabels     (bool isJoyportInEffect);

private:

    static constexpr const wchar_t *  kpszLeft       = L"Joyport left";
    static constexpr const wchar_t *  kpszRight      = L"Joyport right";
    static constexpr const wchar_t *  kpszBoth       = L"Joyport left and right";
    static constexpr const wchar_t *  kpszSameAsLeft = L"Same as left";
    static constexpr const wchar_t *  kpszRightIdle  = L"same as left";
    static constexpr const wchar_t *  kpszAutomatic  = L"Automatic";
};
