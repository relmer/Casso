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
    const std::optional<ControllerUnitKey> &  selection,
    const std::optional<ControllerUnitKey> &  standIn,
    const std::wstring &                      selectionDescription)
{
    std::vector<PaddleSource>  sources;
    PaddleSource               arrows;
    PaddleSource               paddle;
    bool                       isSelectionAttached = false;



    for (const ControllerDeviceInfo & device : devices)
    {
        PaddleSource  entry;

        entry.label      = device.description;
        entry.deviceName = device.description;
        entry.shortLabel = Shorten (device.description);
        entry.formFactor = device.formFactor;
        entry.controller = device.unit;
        entry.isChosen   = selection.has_value() && selection.value() == device.unit;
        entry.isStandIn  = standIn.has_value() && standIn.value() == device.unit;

        // The CHECK MARKS WHAT IS DRIVING, not what was chosen. While a
        // stand-in has the axes it is the one driving, so the check is on it
        // and the chosen controller's own row says it is not connected.
        entry.isChecked  = entry.isChosen || entry.isStandIn;

        if (entry.isChosen)
        {
            isSelectionAttached = true;
        }

        sources.push_back (entry);
    }

    if (selection.has_value() && !isSelectionAttached)
    {
        PaddleSource  missing;

        // The saved description if the controller has been seen this session,
        // and otherwise the generic word: a selection restored at launch is
        // only a token until the controller turns up.
        missing.deviceName  = selectionDescription.empty() ? std::wstring (L"Controller") : selectionDescription;
        missing.label       = missing.deviceName + L" (not connected)";
        missing.shortLabel  = L"Not connected";
        missing.controller  = selection;
        missing.isChosen    = true;
        missing.isChecked   = !standIn.has_value();
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
//  BuildPaddleTip
//
//  What the picker shows on hover: the control's own purpose, or, when the
//  chosen controller is not connected, that fact and what is driving the
//  paddles instead. The picker's face shows only the source that is driving,
//  so neither of those two can appear there (FR-008a, FR-013).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InputModeRules::BuildPaddleTip (const std::vector<PaddleSource> & sources)
{
    const PaddleSource *  chosen  = nullptr;
    const PaddleSource *  standIn = nullptr;



    for (const PaddleSource & source : sources)
    {
        if (source.isChosen && !source.isConnected)
        {
            chosen = &source;
        }

        if (source.isStandIn)
        {
            standIn = &source;
        }
    }

    if (chosen == nullptr)
    {
        return L"Joystick and paddle source";
    }

    if (standIn != nullptr)
    {
        return chosen->deviceName + L" is not connected. " + standIn->deviceName + L" is driving the paddles.";
    }

    return chosen->deviceName + L" is not connected.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DescribeSource
//
//  One line for the notice band when the user picks a source, stating what
//  now drives the paddles and the buttons. The arrow keys need it most:
//  choosing them binds controls that carry no marking, so a user who picks
//  "Use keys as joystick" has no way to learn that X and Z became the buttons
//  except by pressing every key.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InputModeRules::DescribeSource (const PaddleSource & source)
{
    if (source.isArrowKeys)
    {
        return L"The arrow keys drive the joystick. X and Z are the buttons.";
    }

    //  Mouse paddle mode says nothing here. Capturing the pointer raises a
    //  banner that stays up for as long as the capture holds, and it already
    //  states both what the mouse is doing and how to stop.
    if (source.isMousePaddle)
    {
        return std::wstring();
    }

    if (source.deviceName.empty())
    {
        return std::wstring();
    }

    return source.deviceName + L" drives the paddles.";
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
//  A chosen controller owns the axes while it is attached, and so does the
//  controller standing in for it while it is not: which controller the
//  service reads is its own question, and either way a controller is what
//  drives the axes (FR-008a).
//
//  With the chosen controller gone and nothing to stand in, THE ARROW KEYS
//  TAKE THE AXES WHETHER OR NOT THE USER HAS THEM ON. Choosing a controller
//  is what turned them off, so leaving them off here would answer a
//  disconnect by taking the game away entirely; a selection is kept across a
//  disconnect, so the controller takes the axes back the moment it returns.
//
////////////////////////////////////////////////////////////////////////////////

AxisOwner InputModeRules::GetAxisOwner (const State & state)
{
    if (state.hasController && (state.isControllerAttached || state.hasStandIn))
    {
        return AxisOwner::Controller;
    }

    if (state.hasController)
    {
        return AxisOwner::ArrowKeys;
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
