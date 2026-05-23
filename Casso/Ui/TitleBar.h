#pragma once

#include "Pch.h"

#include "TitleBarHitTest.h"






////////////////////////////////////////////////////////////////////////////////
//
//  TitleBar
//
//  Custom title-bar surface. Two halves:
//
//      TitleBarLayout (pure logic, no Win32 / no painter)
//          Given a client width and a title-bar height, computes the
//          rects of the title bar, the three system buttons, and the
//          drag region. Exercised by TitleBarLayoutTests.
//
//      TitleBar (chrome owner)
//          Owns the per-button rect cache the WM_NCHITTEST helper
//          queries so the OS hands us HTMINBUTTON / HTMAXBUTTON /
//          HTCLOSE for the right regions. The native painter pass
//          (introduced in a later phase) takes over actual drawing;
//          for now Show/Hide are no-ops while UpdateGeometry keeps
//          the cached rects fresh.
//
//  Layout uses GetSystemMetricsForDpi so it scales with the active
//  per-window DPI.
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
    static const wchar_t      * WindowsUiFontFamily ();
    static int                  WindowsUiFontWeight ();
};





class TitleBar
{
public:
    TitleBar  ();
    ~TitleBar ();

    void    Show           ();
    void    Hide           ();
    void    UpdateGeometry (int clientWidth, UINT dpi);

    int     GetTitleHeight    () const { return m_layout.titleBar.bottom - m_layout.titleBar.top; }
    RECT    GetTitleBarRect   () const { return m_layout.titleBar; }
    RECT    GetDragRegionRect () const { return m_layout.dragRegion; }
    RECT    GetButtonRect     (SystemButton which) const;

private:
    TitleBarLayoutOutput   m_layout   = {};
};
