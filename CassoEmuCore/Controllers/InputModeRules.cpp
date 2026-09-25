#include "Pch.h"

#include "Controllers/InputModeRules.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BuildPaddleSources
//
//  The picker's entries, in the order they are shown: REAL CONTROLLERS FIRST,
//  then the keys and the mouse. A physical stick plays these games better than
//  either, so it is what the list should offer first; the keys and the mouse
//  are always there to pick. It also puts the checked entry at the top
//  whenever a controller is driving, which is the common case once one is
//  plugged in.
//
//  ONLY ATTACHED CONTROLLERS GET A ROW. The selection follows the controllers
//  that are here (FR-008a), so a row for one that is gone would offer a pick
//  that drives nothing.
//
//  IN MULTIPLAYER ONLY THE MULTIPLAYER ROW IS CHECKED. One game port has one
//  thing driving it, and in that mode the answer is "two people", not either
//  of their controllers: checking a player's row as well asked the user why
//  two entries were checked and which of them won. Who holds which paddles is
//  the settings page's business, not this list's.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<InputModeRules::PaddleSource> InputModeRules::BuildPaddleSources (
    const State &                             state,
    const std::vector<ControllerDeviceInfo> & devices,
    const std::optional<ControllerUnitKey> &  selection,
    const MultiplayerSetup &                  multiplayer,
    size_t                                    axisCount)
{
    // isControllerMode: multiplayer is a controller mode by definition, since
    // picking the keys or the mouse turns it off, so there is no state where
    // it is on and they drive.
    std::vector<PaddleSource>  sources;
    PaddleSource               arrows;
    PaddleSource               paddle;
    PaddleSource               twoPlayer;
    bool                       isControllerMode = state.hasController || multiplayer.isEnabled;



    // The machine's axis count no longer decides any row's check: in
    // multiplayer the mode's row is the checked one whatever the players can
    // reach, and outside it the selection answers on its own. It stays a
    // parameter because what a player can play still belongs in this rule's
    // vocabulary, and the callers already have it.
    UNREFERENCED_PARAMETER (axisCount);

    for (const ControllerDeviceInfo & device : devices)
    {
        PaddleSource  entry;
        bool          isDriving = !multiplayer.isEnabled
                                  && selection.has_value()
                                  && selection.value() == device.unit;

        entry.label      = device.description;
        entry.shortLabel = Shorten (device.description);
        entry.formFactor = device.formFactor;
        entry.controller = device.unit;
        entry.isChecked  = isControllerMode && isDriving;

        sources.push_back (entry);
    }

    // The built-in entries say what they DO to hardware the user already has,
    // because neither "keys" nor "mouse" says on its own that it turns into a
    // joystick or a paddle. A controller needs no such sentence: its own
    // description is the whole answer.
    arrows.label       = L"Use keys as joystick";
    arrows.shortLabel  = L"Keys";
    arrows.isArrowKeys = true;
    arrows.isChecked   = state.arrowsJoystick && !isControllerMode;

    paddle.label         = L"Use mouse as paddle";
    paddle.shortLabel    = L"Mouse";
    paddle.isMousePaddle = true;
    paddle.isChecked     = state.mousePaddle && !isControllerMode;

    // Not a fourth source but a different answer to the question: the sources
    // above are one person playing, and this is two. It carries the ellipsis
    // because picking it opens the settings where the two players are set up.
    twoPlayer.label         = L"Multiplayer...";
    twoPlayer.shortLabel    = L"Multiplayer";
    twoPlayer.isMultiplayer = true;
    twoPlayer.isChecked     = multiplayer.isEnabled;

    sources.push_back (arrows);
    sources.push_back (paddle);
    sources.push_back (twoPlayer);

    return sources;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPaddleSourceLabel
//
//  One controller's name while two are driving would say the second drives
//  nothing, so several checked controllers read as a count instead.
//
//  THE MODE OUTRANKS THE COUNT. While two people are playing, the answer to
//  what drives the game port is the mode, not how many controllers it has
//  reached today: a player slot left empty, or filled with a controller this
//  machine has no paddles for, would otherwise drop the face back to one
//  controller's name while the machine is still in two-player mode.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InputModeRules::GetPaddleSourceLabel (const std::vector<PaddleSource> & sources)
{
    const PaddleSource  * first       = nullptr;
    size_t                controllers = 0;



    for (const PaddleSource & source : sources)
    {
        if (!source.isChecked)
        {
            continue;
        }

        if (source.isMultiplayer)
        {
            return source.shortLabel;
        }

        if (first == nullptr)
        {
            first = &source;
        }

        if (source.controller.has_value())
        {
            controllers++;
        }
    }

    if (controllers > 1)
    {
        return std::format (L"{} controllers", controllers);
    }

    // Nothing is driving the axes, which is a state worth showing rather than
    // leaving the picker blank.
    return first != nullptr ? first->shortLabel : std::wstring (L"Controller");
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetStandInBannerText
//
//  The line the persistent banner carries while the keys or the mouse drive
//  the game port, and nothing while a real controller does.
//
//  PERSISTENT RATHER THAN A FEW SECONDS, because it answers a question the
//  user has for as long as the mode lasts, not only at the moment they chose
//  it. Both modes bind host controls that carry no marking: nothing on
//  screen says X and Z became the buttons, or that Escape is the way out of
//  paddle mode. A notice that expires leaves a user who looked away with no
//  way to find out short of trying keys until one fires.
//
//  THE MODE, NOT THE POINTER CAPTURE. Capture is how paddle mode reads the
//  mouse, which is nothing the user asked about; Escape leaves paddle mode
//  whether or not the pointer is held at that moment, so the line is true
//  for as long as the mode is on.
//
//  A controller needs none of this. Its buttons are labeled on the device.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InputModeRules::GetStandInBannerText (const State & state)
{
    if (state.mousePaddle)
    {
        return L"Using the mouse for paddle input. Press Esc to exit paddle mode.";
    }

    if (state.arrowsJoystick)
    {
        return L"Using the arrow keys as a joystick. X and Z are the buttons.";
    }

    return std::wstring();
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
//  A selected controller owns the axes once it reads. Until it does -- the
//  moment after it is chosen, or after a read failed and before the rescan
//  moves the selection -- NOTHING DOES, and the axes rest at center.
//
//  THE ARROW KEYS NEVER TAKE THE AXES ON THEIR OWN. Arrows-to-joystick also
//  turns X and Z into the buttons, which takes them from the guest's
//  keyboard, so it is on only because the user turned it on (FR-008a).
//
////////////////////////////////////////////////////////////////////////////////

AxisOwner InputModeRules::GetAxisOwner (const State & state)
{
    if (state.hasController)
    {
        return state.isControllerAttached ? AxisOwner::Controller : AxisOwner::None;
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





////////////////////////////////////////////////////////////////////////////////
//
//  GetFireKeyButtons
//
//  The Alt keys are also the //e's Open Apple and Closed Apple. With the
//  Joyport attached those keys must change nothing the guest reads, so a held
//  left Alt cannot close the Joyport's fire switch; X and Z still fire.
//
////////////////////////////////////////////////////////////////////////////////

std::bitset<2> InputModeRules::GetFireKeyButtons (
    bool  xDown,
    bool  zDown,
    bool  leftAltDown,
    bool  rightAltDown,
    bool  isJoyportAttached)
{
    std::bitset<2>  buttons;
    bool            isAltUsed = !isJoyportAttached;



    buttons.set (0, xDown || (isAltUsed && leftAltDown));
    buttons.set (1, zDown || (isAltUsed && rightAltDown));

    return buttons;
}
