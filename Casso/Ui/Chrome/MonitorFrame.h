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
//  Today it models the Apple Monitor //c (snow-white/platinum, 9" mono). The
//  bezel bows slightly (convex) to wrap the CRT tube. Machine- and
//  color-mode-specific housings come later; the //c shell draws whenever the
//  skeuomorphic theme is active.
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

    // Draw size relative to the native 100%-zoom size (1.0 == recess equals
    // the native framebuffer); the desk-scene peripherals scale by this so the
    // whole scene zooms together. 1.0 while hidden.
    float  SceneScale () const { return m_hidden ? 1.0f : m_sceneScale; }

    // Inverse of Layout: the center size whose Layout yields exactly this
    // screen recess, for sizing the default/reset window to 100% zoom.
    static SIZE  CenterSizeForScreenPx (int screenWpx, int screenHpx);

    // Collapse the frame: no housing, and ScreenRect() falls back to the
    // full center rect so a non-skeuo theme shows the bare display.
    void   Hide ()
    {
        m_hidden      = true;
        m_screenRect  = m_centerRect;
        m_housingRect = m_centerRect;
    }

private:
    static uint32_t  LerpArgb       (uint32_t a, uint32_t b, float t);
    static float     EdgeInset      (float y, float top, float bottom,
                                     float radius, float barrel);
    static void      FillSpan       (IDxuiPainter & painter,
                                     float xLeft, float xRight, float y, uint32_t argb);
    static void      ShearFillQuad  (IDxuiPainter & painter,
                                     float xLeft, float yTop, float w, float h,
                                     float tan, float refBottom, uint32_t argb);
    static void      PaintPowerLamp (IDxuiPainter & painter,
                                     float x, float y, float bboxW, float h);

    RECT   m_centerRect  = {};  // full available area (== Layout bounds); desk backdrop fills it
    RECT   m_housingRect = {};  // the monitor's outer platinum shell (bounded, ~4:3-ish)
    RECT   m_screenRect  = {};  // inset CRT recess inside the housing (viewport composites here)
    UINT   m_dpi         = 96;
    float  m_sceneScale  = 1.0f;
    bool   m_hidden      = false;
};
