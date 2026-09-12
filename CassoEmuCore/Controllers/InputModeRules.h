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

        // The controller's own description, with nothing appended. `label`
        // carries the same thing plus "(not connected)" on a row for a
        // controller that is gone, and a sentence about that controller wants
        // the bare name. Empty on the keys and mouse rows.
        std::wstring                      deviceName;

        std::optional<ControllerUnitKey>  controller;   // absent for the two keyboard and mouse entries
        bool                              isArrowKeys   = false;
        bool                              isMousePaddle = false;
        bool                              isChecked     = false;
        bool                              isConnected   = true;

        // The controller the user chose, and the one driving in its place
        // while it is away (FR-008a). Both false on an ordinary row; the two
        // are never true together, since a controller that is there needs no
        // stand-in.
        bool                              isChosen      = false;
        bool                              isStandIn     = false;

        bool operator== (const PaddleSource &) const = default;
    };

    // What the user has chosen, plus whether the chosen controller is there.
    struct State
    {
        bool  arrowsJoystick       = false;
        bool  mousePaddle          = false;
        bool  hasController        = false;
        bool  isControllerAttached = false;

        // Another controller is driving in the chosen one's place (FR-008a).
        bool  hasStandIn           = false;

        bool operator== (const State &) const = default;
    };

    // Longest a source's short label may run on the command bar before it is
    // cut. Fits the longest built-in entry and a typical device description.
    static constexpr size_t  kShortLabelLimit = 18;

    static std::wstring  Shorten (const std::wstring & text);

    // What the picker is built from. `standIn` is the controller driving in
    // the chosen one's place, and `selectionDescription` names the chosen one
    // even while it is gone, so its row can say which controller it is.
    static std::vector<PaddleSource>  BuildPaddleSources (
        const State &                             state,
        const std::vector<ControllerDeviceInfo> & devices,
        const std::optional<ControllerUnitKey> &  selection,
        const std::optional<ControllerUnitKey> &  standIn              = std::nullopt,
        const std::wstring &                      selectionDescription = std::wstring());

    // The picker's tooltip. The face of the control wears the source that is
    // DRIVING, which is the stand-in's own name while one is standing in, so
    // the tooltip is where the chosen controller and its absence are said
    // (FR-008a, FR-013).
    static std::wstring  BuildPaddleTip (const std::vector<PaddleSource> & sources);

    // The line the persistent banner carries while the keys or the mouse
    // stand in for a controller, and empty while a controller drives or
    // nothing does. Follows the MODE; how the mouse is read while paddle
    // mode is on is not a question this answers.
    static std::wstring  GetStandInBannerText (const State & state);

    static AxisOwner  GetAxisOwner            (const State & state);
    static State      AfterSelectingController (State state);
    static State      AfterSettingArrows       (State state, bool on);
    static State      AfterSettingMousePaddle  (State state, bool on);
};
