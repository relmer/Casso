#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"
#include "Controllers/GamePortInputMixer.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InputModeRules
//
//  Who owns the paddle axes, and what turning one input source on does to the
//  others. Three sources drive PDL0/PDL1 -- the arrow keys, the mouse in
//  paddle mode, and a controller -- and only one of them can, so choosing any
//  of them gives up the other two.
//
//  This is pure so the exclusivity can be asserted without a machine. It was
//  spread across `SetArrowsJoystick`, `SetPointerMapping` and
//  `SyncGamePortAxisOwner` before, where each path enforced its own half and
//  nothing could state the whole rule.
//
////////////////////////////////////////////////////////////////////////////////

class InputModeRules
{
public:

    // One entry in the paddle-source picker: the arrow keys, the mouse as a
    // paddle, or one attached controller (FR-008).
    //
    // The //c IOU mouse is deliberately absent. It drives a slot card rather
    // than the game port, so it is not an answer to this question and keeps
    // its own control.
    struct PaddleSource
    {
        std::wstring                      label;

        // What the command bar shows while this one is driving. Short because
        // the picker wears it on its face, and a label that grows moves every
        // button to its right.
        std::wstring                      shortLabel;

        // What the picker draws while this one is driving. Follows the
        // DEVICE, not the API that reads it (FR-008b).
        ControllerFormFactor              formFactor    = ControllerFormFactor::Gamepad;

        std::optional<ControllerUnitKey>  controller;   // absent for the two keyboard and mouse entries
        bool                              isArrowKeys   = false;
        bool                              isMousePaddle = false;
        bool                              isChecked     = false;

        bool operator== (const PaddleSource &) const = default;
    };

    // What the user has chosen, plus whether the selected controller reads.
    struct State
    {
        bool  arrowsJoystick       = false;
        bool  mousePaddle          = false;
        bool  hasController        = false;
        bool  isControllerAttached = false;

        bool operator== (const State &) const = default;
    };

    // Longest a source's short label may run on the command bar before it is
    // cut. Fits the longest built-in entry and a typical device description.
    static constexpr size_t  kShortLabelLimit = 18;

    static std::wstring  Shorten (const std::wstring & text);

    // What the picker is built from: the attached controllers, then the keys
    // and the mouse.
    static std::vector<PaddleSource>  BuildPaddleSources (
        const State &                             state,
        const std::vector<ControllerDeviceInfo> & devices,
        const std::optional<ControllerUnitKey> &  selection);

    // The line the persistent banner carries while the keys or the mouse
    // drive the game port, and empty while a controller drives or nothing
    // does. Follows the MODE; how the mouse is read while paddle
    // mode is on is not a question this answers.
    static std::wstring  GetStandInBannerText (const State & state);

    static AxisOwner  GetAxisOwner            (const State & state);
    static State      AfterSelectingController (State state);
    static State      AfterSettingArrows       (State state, bool on);
    static State      AfterSettingMousePaddle  (State state, bool on);
};
