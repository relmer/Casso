#pragma once

#include "Pch.h"

#include "Core/DxuiEvents.h"




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPanZoom
//
//  Reusable pan + zoom transform controller for a scrollable / zoomable view.
//  Pure logic -- no rendering, no window. The host view feeds it input events
//  (OnMouse / OnKey), refreshes its bounds and ticks it once per frame, then
//  reads the eased transform (Zoom / PanX / PanY) to render whatever it draws
//  (a 2D image, a 3D scene, a document...).
//
//  Interactions handled:
//    * vertical wheel                -> pan Y
//    * horizontal wheel              -> pan X
//    * Ctrl + wheel                  -> zoom
//    * left-button drag              -> pan (both axes)
//    * Ctrl +/= , Ctrl - , Ctrl 0    -> zoom in / out / reset
//
//  Windows Precision Touchpads deliver pinch-zoom as Ctrl+wheel and two-finger
//  pan as wheel / h-wheel, so those gestures ride the same paths for free --
//  no WM_POINTER / WM_GESTURE plumbing needed.
//
//  Fractional wheel deltas ACCUMULATE: a slow touchpad streams many sub-notch
//  events, and truncating each to whole content units would drop most of the
//  motion (the classic "scroll feels dead then lurches"). The remainder is
//  carried across events. Every value EASES toward its target so discrete
//  steps (a wheel notch, a key press, a follow-mode jump) read as continuous
//  motion; set easeTauSec = 0 to disable easing (snap straight to target).
//
//  Units: pan is in caller-defined CONTENT units (e.g. document rows, pixels).
//  The caller sets the drag scale (content units per screen pixel) and the
//  legal pan bounds; both typically change with zoom and as content grows.
//
//  Follow mode (e.g. a live document that scrolls itself): the owner drives
//  panY programmatically with SetPanYTarget each frame; a genuine USER pan
//  fires OnUserPanY so the owner can drop out of follow.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiPanZoom
{
public:
    struct Config
    {
        float   zoomMin    = 1.0f;
        float   zoomMax    = 4.0f;
        float   zoomStep   = 1.25f;    // multiplicative per wheel notch / key press
        float   wheelPanY  = 96.0f;    // content units per vertical wheel notch
        float   wheelPanX  = 96.0f;    // content units per horizontal wheel notch
        double  easeTauSec     = 0.06; // pan glide time constant (0 = snap, no ease)
        double  zoomEaseTauSec = 0.06; // zoom glide time constant (0 = instant zoom)
        // Direct manipulation (wheel / drag / PanByUser) tracks 1:1 with no
        // glide, so the content sticks to the fingers instead of trailing them;
        // only programmatic pan (SetPanYTarget follow mode) keeps easeTauSec's
        // glide. Leave false for a fully-eased view.
        bool    userPanInstant = false;
        bool    enableZoom = true;
        bool    enablePanX = true;
        bool    enablePanY = true;
        bool    enableDrag = true;     // left-drag pans
    };

    explicit DxuiPanZoom (const Config & cfg = Config ());

    // Input. Returns true when the event was consumed (the host should then
    // stop routing it and expect a redraw). Modifier flags on the event pick
    // zoom vs pan (Ctrl+wheel zooms).
    bool  OnMouse (const DxuiMouseEvent & ev);
    bool  OnKey   (const DxuiKeyEvent   & ev);

    // Ease current values toward their targets using the elapsed time since the
    // previous call. Returns true while still animating (host keeps redrawing).
    bool  Tick (double nowSec);

    // Legal pan range (inclusive), refreshed by the owner as zoom / content
    // change. panY is clamped into [lo, hi]; an empty/negative range pins it.
    void  SetPanYBounds (float lo, float hi);
    void  SetPanXBounds (float lo, float hi);

    // Content units per screen pixel for drag, set from the owner's layout and
    // current zoom (a drag of N pixels moves the content by N * scale units).
    void  SetDragScale (float contentPerPixelX, float contentPerPixelY);

    // Programmatic pan-Y (follow mode): moves the target WITHOUT counting as a
    // user pan, so it never trips OnUserPanY.
    void   SetPanYTarget (float y);
    float  PanYTarget () const { return (float) m_panY.target; }

    // Programmatic USER pan (host-routed keys like arrows / page keys): moves
    // the pan target and DOES count as a user pan (fires OnUserPanY), so a
    // follow-mode owner drops out of follow just as a wheel or drag would.
    void   PanByUser (float deltaContentX, float deltaContentY);

    // Teleport panY (both current and target) with no glide -- e.g. when the
    // content is torn off and replaced, so the view does not slide across it.
    void   SnapPanY (float y);

    void   ZoomIn    ();   // one step in  (multiply target by zoomStep)
    void   ZoomOut   ();   // one step out (divide  target by zoomStep)
    void   ResetZoom ();   // zoom target -> zoomMin

    // Eased current transform -- what the host renders this frame.
    float  Zoom () const { return (float) m_zoom.cur; }
    float  PanX () const { return (float) m_panX.cur; }
    float  PanY () const { return (float) m_panY.cur; }
    float  ZoomTarget () const { return (float) m_zoom.target; }

    bool   Zoomed () const { return m_zoom.target > (double) m_cfg.zoomMin + 1e-3; }

    using Fn = std::function<void ()>;
    void  SetOnChange   (Fn fn) { m_onChange   = std::move (fn); }
    void  SetOnUserPanY (Fn fn) { m_onUserPanY = std::move (fn); }

    const Config &  Cfg () const { return m_cfg; }

private:
    struct Eased
    {
        double  cur    = 0.0;
        double  target = 0.0;
    };

    void  ApplyZoomFactor (double factor);   // multiply zoom target, clamp
    void  NudgePanX (double deltaContent);
    void  NudgePanY (double deltaContent, bool user);
    void  ClampTargets ();
    bool  EaseToward (Eased & v, double dtSec, double tauSec);
    void  Changed ();

    Config  m_cfg;
    Eased   m_zoom;
    Eased   m_panX;
    Eased   m_panY;

    double  m_panXlo = 0.0, m_panXhi = 0.0;
    double  m_panYlo = 0.0, m_panYhi = 0.0;

    float   m_dragPerPxX = 1.0f;
    float   m_dragPerPxY = 1.0f;
    bool    m_dragging   = false;
    POINT   m_dragLast   = {};

    double  m_lastTickSec = -1.0;

    Fn      m_onChange;
    Fn      m_onUserPanY;
};
