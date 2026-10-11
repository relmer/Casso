#pragma once

#include "Pch.h"

#include "Controllers/InputModeRules.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IInputHost
//
//  What the input component asks of the emulator that holds it: the chrome
//  that shows the input modes and the players, the window it captures the
//  pointer in, and the frame clock the keyboard repeats on. The emulator's
//  shell implements it; a test implements it with a notebook.
//
////////////////////////////////////////////////////////////////////////////////

class IInputHost
{
public:

    virtual ~IInputHost() = default;

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

    //  The real time since the last frame's key repeat, capped at capUs.
    //  CPU thread, once a frame.
    virtual uint32_t  TakeKeyRepeatElapsedUs  (uint32_t capUs)                    = 0;
};
