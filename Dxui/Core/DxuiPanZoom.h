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
//    * vertical wheel                -> pan Y (content scroll)
//    * horizontal wheel              -> pan X (camera framing)
//    * Ctrl + wheel                  -> zoom (cursor-anchored)
//    * left-button drag              -> frame the zoomed view (panX + panYCam)
//    * Ctrl +/= , Ctrl - , Ctrl 0    -> zoom in / out / reset
//
//  Two vertical axes, because a zoomable 3D view has two distinct vertical
//  intents. panY is the CONTENT scroll (wheel / arrows -- review the document);
//  panYCam is CAMERA FRAMING (drag / cursor-anchored zoom -- move the eye over
//  the magnified scene to bring an off-center corner into view). panX doubles
//  as horizontal framing. Framing bounds grow with zoom and collapse to zero at
//  fit, so a drag does nothing until there is something off-screen to reach.
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
//  Overscroll (optional, panY only): once panY is pinned at a bound, further
//  vertical pan spills into a separate bounded offset (SetOverscrollYMax /
//  OverscrollY) instead of hitting a wall -- the host maps it to a whole-view
//  translation so the content nudges past its scroll limit up to a hard stop.
//  Follow mode springs it back to zero.
//
//  Cursor-anchored zoom: a wheel / pinch zoom keeps the content point under the
//  cursor fixed (the pan targets shift by the drag scale). Button / key zoom
//  stays centered. The host supplies the view center with SetViewCenter.
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

    // Camera vertical framing range (grows with zoom, zero at fit). Drag and
    // cursor-anchored zoom move panYCam within it to frame the magnified scene.
    void  SetPanYCamBounds (float lo, float hi);

    // Content units per screen pixel for drag, set from the owner's layout and
    // current zoom (a drag of N pixels moves the content by N * scale units).
    void  SetDragScale (float contentPerPixelX, float contentPerPixelY);

    // View center in the same pixel space as event positions, refreshed by the
    // owner from its content rect. Anchors a wheel / pinch zoom on the cursor.
    void  SetViewCenter (float cx, float cy) { m_viewCenterX = cx; m_viewCenterY = cy; }

    // Max panY overscroll (content units) past the bounds: once panY is pinned
    // at a bound, further pan spills here, up to +/- this. 0 = a hard stop.
    void  SetOverscrollYMax (float maxContent) { m_overscrollMax = (maxContent > 0.0f) ? maxContent : 0.0f; }

    // Current eased overscroll offset (0 within bounds); the host maps it to a
    // whole-view translation. Sign follows the pan direction that spilled it.
    float  OverscrollY () const { return (float) m_overscrollY.cur; }

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
    float  PanYCam () const { return (float) m_panYCam.cur; }   // camera vertical framing
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

    // Multiply the zoom target and clamp. When anchored, shift the pan targets
    // so the content under (anchorX, anchorY) stays put; otherwise zoom about
    // the view center.
    void  ApplyZoomFactor (double factor, bool anchored = false,
                           float anchorX = 0.0f, float anchorY = 0.0f);
    void  NudgePanX (double deltaContent);
    void  NudgePanY (double deltaContent, bool user);
    void  NudgePanYCam (double deltaContent);   // camera vertical framing (drag / zoom anchor)
    void  SpillPanY (double deltaContent);      // apply a delta, overflowing into overscroll
    void  ClampTargets ();
    bool  EaseToward (Eased & v, double dtSec, double tauSec);
    void  Changed ();

    Config  m_cfg;
    Eased   m_zoom;
    Eased   m_panX;
    Eased   m_panY;
    Eased   m_panYCam;       // camera vertical framing (independent of content scroll)
    Eased   m_overscrollY;   // bounded panY overflow -> host view translation

    double  m_panXlo = 0.0, m_panXhi = 0.0;
    double  m_panYlo = 0.0, m_panYhi = 0.0;
    double  m_panYCamLo = 0.0, m_panYCamHi = 0.0;
    float   m_overscrollMax = 0.0f;

    float   m_viewCenterX = 0.0f;
    float   m_viewCenterY = 0.0f;

    float   m_dragPerPxX = 1.0f;
    float   m_dragPerPxY = 1.0f;
    bool    m_dragging   = false;
    POINT   m_dragLast   = {};

    double  m_lastTickSec = -1.0;

    Fn      m_onChange;
    Fn      m_onUserPanY;
};
