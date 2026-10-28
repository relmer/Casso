#pragma once

#include "Pch.h"

class EmulatorShell;
class ShellChrome;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellStandInBar
//
//  The pointer-capture bar: "Press Esc to release the mouse and exit paddle
//  mode", in its band under the command strip while the pointer is
//  captured, over an opaque panel, and the bookkeeping around its
//  dock: a band left standing after the capture ended, and the one
//  WM_CANCELMODE the dock's resize sends back.
//
////////////////////////////////////////////////////////////////////////////////

class ShellStandInBar
{
public:
    ShellStandInBar (EmulatorShell & shell, ShellChrome & chrome) : m_shell (shell), m_chrome (chrome) {}

    // Driven from the per-frame UI upkeep.
    void  SyncStandInBanner ();

    // How tall the capture bar's band is right now: zero unless the pointer is
    // held, and zero in fullscreen, where there are no bands at all and the
    // bar rides under the toolbar reveal instead.
    int   GetCaptureBandThicknessPx (int clientWidthPx) const;

    // How tall the bar is at a given width, for the band that reserves the
    // room and the paint that fills it. One answer, so the two cannot
    // disagree about whether the band is big enough.
    int   GetStandInBarHeightPx (float widthPx) const;

    // True once, then cleared, when a capture band was left docked after the
    // grab ended.
    bool  TakeStandInBandStale ();

    // True, once, for a cancel within s_kCaptureReflowEchoMs of the capture
    // bar's dock.
    bool  TakeCaptureReflowEcho (int64_t nowMs);

private:
    friend class ShellChrome;

    EmulatorShell  & m_shell;
    ShellChrome    & m_chrome;

    // "Press Esc to release the mouse and exit paddle mode", on screen for as
    // long as the capture holds. The joystick button carries the same words, but
    // it is chrome: fullscreen hides it, and a captured pointer with the
    // cursor gone and no way out shown is how a user ends up killing the
    // process. A message bar rather than a caption over the picture: it says
    // something and asks nothing, which is what an info banner is, and the
    // chrome under the command strip is the one place it covers nothing.
    DxuiInfoBanner             m_standInBar;

    // An opaque panel behind it, the way the drive bar has one. The banner's
    // own fill is a tint meant to sit on chrome, and the bar does not: it
    // hangs over the picture, and over the desk scene the monitor read
    // straight through the words.
    DxuiSurface                m_standInBarSurface;

    // Set when a capture band was found standing with no capture behind it,
    // and cleared by the re-dock at the top of the next frame. A flag rather
    // than the re-dock itself, because the sync that spots it runs inside the
    // frame the re-dock would repaint.
    bool                       m_standInBandStale = false;

    // When the capture's own band was last docked, on the monotonic clock.
    // The resize that follows bounces WM_CANCELMODE back at whoever holds the
    // pointer, and that one cancel is ours to ignore -- see OnCancelMode.
    // Zeroed once it has been used, so exactly one is ever swallowed.
    int64_t                    m_captureReflowMs = 0;
};
