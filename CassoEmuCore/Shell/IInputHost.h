#pragma once

#include "Pch.h"

#include "Controllers/ControllerTypes.h"
#include "Controllers/InputModeRules.h"

class ClipboardManager;
class IFileSystem;
class JsonValue;
class UserConfigStore;
struct GlobalUserPrefs;





////////////////////////////////////////////////////////////////////////////////
//
//  IInputHost
//
//  What the input component asks of the emulator that holds it: the window
//  it captures the pointer in and the picture the pointer maps onto, the
//  chrome that shows the input modes, the players and the notices, the
//  frame clock the keyboard repeats on, the clipboard, and the preferences.
//  The emulator's shell implements it; a test implements it with a notebook.
//
////////////////////////////////////////////////////////////////////////////////

class IInputHost
{
public:

    virtual ~IInputHost() = default;

    //  The emulator's window, which the pointer is captured in and the
    //  controller and game port threads post to.
    virtual HWND      GetMainWindow           () const                            = 0;

    //  Where the host pointer falls on the picture, as a 16-bit fraction of
    //  each axis packed x in the high half: through the curved glass when the
    //  CRT is shown, else across the viewport. Empty off the picture.
    virtual std::optional<uint32_t>  MapPointerToPicture (int xPx, int yPx)       = 0;

    //  The chrome's tooltips, hidden before the pointer is captured: the move
    //  that would dismiss one never comes once the capture has the pointer.
    virtual void      HideTooltips            ()                                  = 0;

    //  The players' picker, onto the Machine menu and the command bar's
    //  drop-down, with the bar laid out again for the picker's new width.
    virtual void      SetPlayerPicker         (InputModeRules::Picker picker)     = 0;

    //  The capture's notice takes a band of the window: laid out before the
    //  pointer is taken, so the resize cannot cancel the grab, and laid out
    //  again once it is let go. Windowed only; fullscreen keeps its layout.
    virtual void      ClaimCaptureBand        ()                                  = 0;
    virtual void      ReleaseCaptureBand      ()                                  = 0;

    //  Whether the fullscreen drive strip has the guest mouse released while
    //  it is summoned.
    virtual bool      IsGuestMouseSuppressed  () const                            = 0;

    //  A notice over the picture, from the UI thread or from any thread.
    virtual void      ShowNotice              (const std::wstring & text)         = 0;
    virtual void      PostNotice              (const std::wstring & text)         = 0;

    //  The character Windows makes of a key the guest took as a special key
    //  is not typed a second time.
    virtual void      SwallowNextChar         ()                                  = 0;

    //  The real time since the last frame's key repeat, capped at capUs.
    //  CPU thread, once a frame.
    virtual uint32_t  TakeKeyRepeatElapsedUs  (uint32_t capUs)                    = 0;

    //  The clipboard a paste reads, or null before it exists.
    virtual ClipboardManager *  GetClipboard  ()                                  = 0;

    //  Settings opened on the Controllers page with the New Profile dialog up
    //  for a controller.
    virtual void      StartNewControllerProfile (ControllerUnitKey unit)          = 0;

    //  The preferences, saved at once; and the machine's own, where each
    //  machine keeps its input modes and players.
    virtual GlobalUserPrefs   & GetPrefs           ()                             = 0;
    virtual void                SavePrefs          ()                             = 0;
    virtual UserConfigStore   * GetConfigStore     ()                             = 0;
    virtual IFileSystem       & GetFileSystem      ()                             = 0;
    virtual void                LoadMachineUiPrefs (JsonValue & outDoc, const JsonValue * & outUiPrefs) = 0;
};
