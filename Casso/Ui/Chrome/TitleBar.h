#pragma once

#include "Pch.h"

#include "ChromeTheme.h"
#include "../TitleBarHitTest.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"





enum class SystemButton
{
    Minimize = 0,
    Maximize = 1,
    Close    = 2,
};


enum class ChromeButtonVisual
{
    Idle,
    Hover,
    Pressed,
};


struct TitleBarLayoutInput
{
    int  clientWidth  = 0;
    int  titleHeight  = 0;
    int  buttonWidth  = 0;
};


struct TitleBarLayoutOutput
{
    RECT  titleBar     = {};
    RECT  minButton    = {};
    RECT  maxButton    = {};
    RECT  closeButton  = {};
    RECT  dragRegion   = {};
};


class TitleBarLayout
{
public:
    static TitleBarLayoutOutput Compute              (const TitleBarLayoutInput & in);
    static int                  DefaultTitleHeight   (UINT dpi);
    static int                  DefaultButtonWidth   (UINT dpi);
    static const wchar_t      * WindowsUiFontFamily  ();
    static int                  WindowsUiFontWeight  ();
};


class TitleBar
{
public:
    TitleBar  ();
    ~TitleBar ();

    void                Show              ();
    void                Hide              ();
    void                UpdateGeometry    (int clientWidth, UINT dpi);
    void                SetMousePosition  (int x, int y, bool leftDown);
    void                Paint             (DxUiPainter             & painter,
                                            DwriteTextRenderer      & text,
                                            const ChromeVisualState & visual,
                                            const ChromeTheme       & theme);

    int                 GetTitleHeight    () const { return m_layout.titleBar.bottom - m_layout.titleBar.top; }
    RECT                GetTitleBarRect   () const { return m_layout.titleBar; }
    RECT                GetDragRegionRect () const { return m_layout.dragRegion; }
    RECT                GetButtonRect     (SystemButton which) const;
    SystemButton        HotButton         () const { return m_hotButton; }
    ChromeButtonVisual  ButtonVisual      (SystemButton which) const;

private:
    TitleBarLayoutOutput   m_layout       = {};
    SystemButton           m_hotButton    = SystemButton::Minimize;
    bool                   m_hasHotButton = false;
    bool                   m_leftDown     = false;
};
