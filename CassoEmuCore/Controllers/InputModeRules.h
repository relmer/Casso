#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"
#include "Controllers/GamePortInputMixer.h"





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
        std::optional<ControllerUnitKey>  controller;   // absent for the two keyboard and mouse entries
        bool                              isArrowKeys   = false;
        bool                              isMousePaddle = false;
        bool                              isChecked     = false;
        bool                              isConnected   = true;

        bool operator== (const PaddleSource &) const = default;
    };

    // What the user has chosen, plus whether the chosen controller is there.
    struct State
    {
        bool  arrowsJoystick       = false;
        bool  mousePaddle          = false;
        bool  hasController        = false;
        bool  isControllerAttached = false;

        bool operator== (const State &) const = default;
    };

    static std::vector<PaddleSource>  BuildPaddleSources (
        const State &                             state,
        const std::vector<ControllerDeviceInfo> & devices,
        const std::optional<ControllerUnitKey> &  selection);

    static AxisOwner  GetAxisOwner            (const State & state);
    static State      AfterSelectingController (State state);
    static State      AfterSettingArrows       (State state, bool on);
    static State      AfterSettingMousePaddle  (State state, bool on);
};
