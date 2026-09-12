#include "Pch.h"

#include "Controllers/InputModeRules.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BuildPaddleSources
//
//  The picker's entries, in the order they are shown: REAL CONTROLLERS FIRST,
//  then the keys and the mouse. A physical stick plays these games better than
//  either stand-in, so it is what the list should offer first; the keys and
//  the mouse are what a user falls back to, and they are always there to fall
//  back to. It also puts the checked entry at the top whenever a controller is
//  driving, which is the common case once one is plugged in.
//
//  A CHOSEN CONTROLLER THAT IS NOT ATTACHED STILL GETS A ROW, marked as not
//  connected, and stays with the controllers rather than sinking below the
//  fallbacks. Dropping it would leave the picker showing the keys checked
//  while the user's actual choice is a controller whose battery died, and
//  picking the controller again would look like the only way back.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<InputModeRules::PaddleSource> InputModeRules::BuildPaddleSources (
    const State &                             state,
    const std::vector<ControllerDeviceInfo> & devices,
    const std::optional<ControllerUnitKey> &  selection)
{
    std::vector<PaddleSource>  sources;
    PaddleSource               arrows;
    PaddleSource               paddle;
    bool                       isSelectionAttached = false;



    // The two built-in entries say what they DO to hardware the user already
    // has, because neither "keys" nor "mouse" says on its own that it turns
    // into a joystick or a paddle. A controller needs no such sentence: its
    // own description is the whole answer.
    for (const ControllerDeviceInfo & device : devices)
    {
        PaddleSource  entry;

        entry.label      = device.description;
        entry.shortLabel = Shorten (device.description);
        entry.controller = device.unit;
        entry.isChecked  = selection.has_value() && selection.value() == device.unit;

        if (entry.isChecked)
        {
            isSelectionAttached = true;
        }

        sources.push_back (entry);
    }

    if (selection.has_value() && !isSelectionAttached)
    {
        PaddleSource  missing;

        missing.label       = L"(not connected)";
        missing.shortLabel  = L"Not connected";
        missing.controller  = selection;
        missing.isChecked   = true;
        missing.isConnected = false;

        sources.push_back (missing);
    }

    // The built-in entries say what they DO to hardware the user already has,
    // because neither "keys" nor "mouse" says on its own that it turns into a
    // joystick or a paddle. A controller needs no such sentence: its own
    // description is the whole answer.
    arrows.label       = L"Use keys as joystick";
    arrows.shortLabel  = L"Keys";
    arrows.isArrowKeys = true;
    arrows.isChecked   = state.arrowsJoystick && !state.hasController;

    paddle.label         = L"Use mouse as paddle";
    paddle.shortLabel    = L"Mouse";
    paddle.isMousePaddle = true;
    paddle.isChecked     = state.mousePaddle && !state.hasController;

    sources.push_back (arrows);
    sources.push_back (paddle);

    return sources;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shorten
//
//  Cuts a device description down to what the command bar can wear. A single
//  ellipsis, not three dots, matching the drive labels.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InputModeRules::Shorten (const std::wstring & text)
{
    std::wstring  shortened = text;
    size_t        paren     = std::wstring::npos;



    // A trailing parenthetical is the vendor and product that tell two units
    // of a model apart. That is worth having where a user is choosing between
    // them; on the strip it is noise, and cutting into it mid-word reads as a
    // truncation bug rather than as a name.
    if (!shortened.empty() && shortened.back() == L')')
    {
        paren = shortened.rfind (L" (");

        if (paren != std::wstring::npos && paren > 0)
        {
            shortened.erase (paren);
        }
    }

    if (shortened.size() <= kShortLabelLimit)
    {
        return shortened;
    }

    return shortened.substr (0, kShortLabelLimit - 1) + s_kchEllipsis;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetAxisOwner
//
//  A chosen controller owns the axes while it is attached. While it is not,
//  the arrow keys stand in if the user also has them on, and otherwise the
//  axes rest at center -- a selection is kept across a disconnect, so the
//  controller takes them back the moment it returns (FR-008a).
//
//  The stand-in by ANOTHER CONTROLLER (FR-008a) is not decided here: it
//  changes which controller is read, not which kind of source owns the axes,
//  so the service picks the stand-in and this still answers Controller.
//
////////////////////////////////////////////////////////////////////////////////

AxisOwner InputModeRules::GetAxisOwner (const State & state)
{
    if (state.hasController && state.isControllerAttached)
    {
        return AxisOwner::Controller;
    }

    if (state.mousePaddle)
    {
        return AxisOwner::MousePaddle;
    }

    if (state.arrowsJoystick)
    {
        return AxisOwner::ArrowKeys;
    }

    return AxisOwner::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AfterSelectingController
//
//  Choosing a controller gives the axes to it, so the arrow keys and the
//  paddle stop driving them (FR-008).
//
////////////////////////////////////////////////////////////////////////////////

InputModeRules::State InputModeRules::AfterSelectingController (State state)
{
    state.hasController  = true;
    state.arrowsJoystick = false;
    state.mousePaddle    = false;

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AfterSettingArrows
//
//  Turning the arrow keys on takes the axes from the paddle and from the
//  controller, which clears the selection: the user asked for the keys.
//
////////////////////////////////////////////////////////////////////////////////

InputModeRules::State InputModeRules::AfterSettingArrows (State state, bool on)
{
    state.arrowsJoystick = on;

    if (on)
    {
        state.mousePaddle   = false;
        state.hasController = false;
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AfterSettingMousePaddle
//
////////////////////////////////////////////////////////////////////////////////

InputModeRules::State InputModeRules::AfterSettingMousePaddle (State state, bool on)
{
    state.mousePaddle = on;

    if (on)
    {
        state.arrowsJoystick = false;
        state.hasController  = false;
    }

    return state;
}
