#pragma once

////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTitleBarHitTest
//
//  Pure-logic Win32 WM_NCHITTEST helper for a custom-chromed window.
//  The caller passes the client rect, the mouse point (in client
//  coordinates), the rect of the RML title-bar element, and the
//  per-button rects (min/max/close) and gets back an HT* constant.
//
//  Hit-test priority (highest first):
//      1. Resize edges — 8-pixel inset on each side of the client rect,
//         producing the eight HT{LEFT,RIGHT,TOP,BOTTOM,...} codes when
//         the mouse is over them.
//      2. System buttons — min/max/close. If the mouse is inside a
//         button rect the rect wins, even when the button rect overlaps
//         the caption rect (which it always does — buttons live ON the
//         title bar).
//      3. Caption — anywhere else in the title-bar rect maps to
//         HTCAPTION (gives Win32 free drag/double-click-to-maximize).
//      4. Client — everything else.
//
//  All input coordinates are in client space (top-left == 0,0).
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiTitleBarHitTestInput
{
    // Client-area dimensions (i.e. GetClientRect output).
    int  clientWidth   = 0;
    int  clientHeight  = 0;

    // Mouse position in client coordinates.
    int  mouseX        = 0;
    int  mouseY        = 0;

    // Title-bar rect (in client coords). If empty (height == 0), only
    // the resize edges + client rule apply.
    int  titleLeft     = 0;
    int  titleTop      = 0;
    int  titleRight    = 0;
    int  titleBottom   = 0;

    // System-button rects (zero-sized means absent).
    int  minLeft       = 0;
    int  minTop        = 0;
    int  minRight      = 0;
    int  minBottom     = 0;
    int  maxLeft       = 0;
    int  maxTop        = 0;
    int  maxRight      = 0;
    int  maxBottom     = 0;
    int  closeLeft     = 0;
    int  closeTop      = 0;
    int  closeRight    = 0;
    int  closeBottom   = 0;

    // Pixel inset for the eight resize edges.
    int  resizeBorderPx = 8;
};


class DxuiTitleBarHitTest
{
public:
    static LRESULT Test (const DxuiTitleBarHitTestInput & in);
};
