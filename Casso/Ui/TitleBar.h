#pragma once

#include "Pch.h"

#include "TitleBarHitTest.h"






////////////////////////////////////////////////////////////////////////////////
//
//  TitleBar
//
//  P4 custom title bar. Has two halves:
//
//      TitleBarLayout (pure logic, no Win32 / no Rml)
//          Given a client width and a title-bar height, computes the
//          rects of the title bar, the three system buttons, and the
//          drag region. Exercised by TitleBarLayoutTests.
//
//      TitleBar (Rml-aware)
//          Owns a Rml::ElementDocument inlined from a small RML+RCSS
//          string literal pair. Show()/Hide() attach the doc to a
//          context. GetButtonRect() exposes the current per-button
//          rect (in client coordinates) so the WM_NCHITTEST helper
//          can hand the OS HTMINBUTTON / HTMAXBUTTON / HTCLOSE for
//          the right regions and Snap Layouts surfaces on hover.
//
//  P4 invariants:
//      * No theme system yet — markup is hard-coded.
//      * Click routing happens through WM_NCLBUTTONUP, not RML
//        listeners. The RML doc is decorative; the C++ side owns
//        button geometry.
//      * Layout uses GetSystemMetricsForDpi so it scales with the
//        active DPI.
//
////////////////////////////////////////////////////////////////////////////////

enum class SystemButton
{
    Minimize = 0,
    Maximize = 1,
    Close    = 2,
};


struct TitleBarLayoutInput
{
    int  clientWidth  = 0;
    int  titleHeight  = 0;
    int  buttonWidth  = 0;
};


struct TitleBarLayoutOutput
{
    // All rects are in client coordinates. Title-bar rect is the full
    // strip across the top; buttons stack right-to-left as
    // close|max|min.
    RECT  titleBar     = {};
    RECT  minButton    = {};
    RECT  maxButton    = {};
    RECT  closeButton  = {};

    // Drag region = title-bar rect minus the three button rects on
    // the right. Single rect; the chrome doesn't put anything on the
    // left edge yet.
    RECT  dragRegion   = {};
};


class TitleBarLayout
{
public:
    static TitleBarLayoutOutput Compute            (const TitleBarLayoutInput & in);
    static int                  DefaultTitleHeight (UINT dpi);
    static int                  DefaultButtonWidth (UINT dpi);
};





class TitleBar
{
public:
    TitleBar  ();
    ~TitleBar ();

    HRESULT Show           (Rml::Context * context);
    void    Hide           ();
    void    UpdateGeometry (int clientWidth, UINT dpi);

    int     GetTitleHeight    () const { return m_layout.titleBar.bottom - m_layout.titleBar.top; }
    RECT    GetTitleBarRect   () const { return m_layout.titleBar; }
    RECT    GetDragRegionRect () const { return m_layout.dragRegion; }
    RECT    GetButtonRect     (SystemButton which) const;

private:
    Rml::Context         * m_context  = nullptr;
    Rml::ElementDocument * m_doc      = nullptr;

    TitleBarLayoutOutput   m_layout   = {};
};
