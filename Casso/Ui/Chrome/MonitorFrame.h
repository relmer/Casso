#pragma once

#include "Pch.h"

#include "CassoTheme.h"
#include "Core/IDxuiControl.h"




//
//  MonitorFrame is Casso-specific (skeuomorphic period CRT monitor). Like
//  DriveWidget it is an IDxuiControl painted procedurally through IDxuiPainter,
//  and static_casts the theme to CassoTheme for palette. Unlike the drives it
//  does not have an intrinsic size: it is laid out to the emulator's center
//  rect and frames the display -- Layout computes an inset screen recess
//  (ScreenRect) the viewport composites into, and Paint draws the housing in
//  the ring AROUND that recess (never over it, so the composited emulator
//  frame reads through the hole).
//
//  Phase 1 models the Apple Monitor //c (snow-white/platinum, 9" mono). The
//  bezel bows slightly (convex) to wrap the CRT tube. Machine- and
//  colour-mode-specific housings come later; today it draws the //c shell
//  whenever the skeuomorphic theme is active.
//
class MonitorFrame : public IDxuiControl
{
public:
    MonitorFrame ();

    void   Layout (const RECT          & boundsDip,
                   const DxuiDpiScaler & scaler) override;

    void   Paint  (IDxuiPainter        & painter,
                   IDxuiTextRenderer   & text,
                   const IDxuiTheme    & theme) override;

    // The inset CRT screen recess (host client px) the emulator viewport
    // should fill so the display sits inside the monitor glass. Empty when
    // the frame is hidden.
    RECT   ScreenRect () const { return m_screenRect; }

    // Inverse of Layout, for the initial/reset window size: the center
    // (viewport-area) size in px whose Layout yields a screen recess of exactly
    // screenWpx x screenHpx -- so the default window can host the emulator image
    // at 100% zoom inside the housing, chrome + framing sized around it.
    static SIZE  CenterSizeForScreenPx (int screenWpx, int screenHpx);

    // Collapse the frame: no housing, and ScreenRect() falls back to the
    // full center rect so a non-skeuo theme shows the bare display.
    void   Hide ()
    {
        m_hidden      = true;
        m_screenRect  = m_centerRect;
        m_housingRect = m_centerRect;
        m_standRect   = {};
    }

private:
    void   PaintStand (IDxuiPainter & painter);

    RECT   m_centerRect  = {};  // full available area (== Layout bounds); desk backdrop fills it
    RECT   m_housingRect = {};  // the monitor's outer platinum shell (bounded, ~4:3-ish)
    RECT   m_screenRect  = {};  // inset CRT recess inside the housing (viewport composites here)
    RECT   m_standRect   = {};  // tilt/swivel foot below the housing (neck + base drawn within)
    UINT   m_dpi         = 96;
    bool   m_hidden      = false;
};
