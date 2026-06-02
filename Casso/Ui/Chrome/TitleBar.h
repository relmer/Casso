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
    void                SetTitle          (const std::wstring & title) { m_title = title; }
    void                UpdateGeometry    (int clientWidth, UINT dpi);
    void                SetMousePosition  (int x, int y, bool leftDown);
    void                ClearHover        ();
    void                SetAppIcon        (std::vector<uint32_t> bgraPremulPixels,
                                            int                    widthPx,
                                            int                    heightPx);
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

    std::vector<uint32_t>  m_appIconPixels;     // premultiplied BGRA8
    int                    m_appIconW    = 0;
    int                    m_appIconH    = 0;
    std::wstring           m_title       = L"Casso";
};
