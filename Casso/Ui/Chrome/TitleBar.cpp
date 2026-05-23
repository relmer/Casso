#include "Pch.h"

#include "TitleBar.h"





namespace
{
    constexpr int       s_kBaseTitleHeightPx = 32;
    constexpr int       s_kBaseButtonWidthPx = 46;
    constexpr int       s_kBaseDpi           = 96;
    constexpr float     s_kTitleFontDip      = 14.0f;
    constexpr float     s_kTitlePadDip       = 14.0f;
    constexpr wchar_t   s_kMdl2Family[]      = L"Segoe MDL2 Assets";
    constexpr wchar_t   s_kTitle[]           = L"Casso";
    constexpr wchar_t   s_kMinGlyph[]        = L"\xE921";
    constexpr wchar_t   s_kMaxGlyph[]        = L"\xE922";
    constexpr wchar_t   s_kCloseGlyph[]      = L"\xE8BB";


    bool RectContains (const RECT & r, int x, int y)
    {
        return (x >= r.left) && (x < r.right) && (y >= r.top) && (y < r.bottom);
    }


    void PaintButton (DxUiPainter             & painter,
                      DwriteTextRenderer      & text,
                      const RECT              & rect,
                      const wchar_t           * glyph,
                      uint32_t                  fillArgb,
                      uint32_t                  textArgb)
    {
        HRESULT  hr = S_OK;



        painter.FillRect ((float) rect.left,
                          (float) rect.top,
                          (float) (rect.right - rect.left),
                          (float) (rect.bottom - rect.top),
                          fillArgb);
        IGNORE_RETURN_VALUE (hr, text.DrawString (glyph,
                                                  (float) rect.left,
                                                  (float) rect.top,
                                                  (float) (rect.right - rect.left),
                                                  (float) (rect.bottom - rect.top),
                                                  textArgb,
                                                  11.0f,
                                                  s_kMdl2Family,
                                                  DwriteTextRenderer::HAlign::Center,
                                                  DwriteTextRenderer::VAlign::Center));
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  TitleBarLayout::Compute
//
////////////////////////////////////////////////////////////////////////////////

TitleBarLayoutOutput TitleBarLayout::Compute (const TitleBarLayoutInput & in)
{
    TitleBarLayoutOutput  out          = {};
    int                   buttonStripW = in.buttonWidth * 3;
    int                   buttonsLeft  = in.clientWidth - buttonStripW;
    bool                  collapse     = (buttonsLeft < 0) || (in.titleHeight <= 0) || (in.buttonWidth <= 0);



    out.titleBar.left    = 0;
    out.titleBar.top     = 0;
    out.titleBar.right   = in.clientWidth;
    out.titleBar.bottom  = in.titleHeight;

    if (collapse)
    {
        out.dragRegion.left   = 0;
        out.dragRegion.top    = 0;
        out.dragRegion.right  = in.clientWidth;
        out.dragRegion.bottom = in.titleHeight;
        return out;
    }

    out.minButton.left   = buttonsLeft;
    out.minButton.top    = 0;
    out.minButton.right  = buttonsLeft + in.buttonWidth;
    out.minButton.bottom = in.titleHeight;
    out.maxButton.left   = out.minButton.right;
    out.maxButton.top    = 0;
    out.maxButton.right  = out.maxButton.left + in.buttonWidth;
    out.maxButton.bottom = in.titleHeight;
    out.closeButton.left   = out.maxButton.right;
    out.closeButton.top    = 0;
    out.closeButton.right  = out.closeButton.left + in.buttonWidth;
    out.closeButton.bottom = in.titleHeight;
    out.dragRegion.left   = 0;
    out.dragRegion.top    = 0;
    out.dragRegion.right  = buttonsLeft;
    out.dragRegion.bottom = in.titleHeight;

    return out;
}




////////////////////////////////////////////////////////////////////////////////
//
//  TitleBarLayout::DefaultTitleHeight
//
////////////////////////////////////////////////////////////////////////////////

int TitleBarLayout::DefaultTitleHeight (UINT dpi)
{
    UINT  effectiveDpi = (dpi == 0) ? s_kBaseDpi : dpi;



    return MulDiv (s_kBaseTitleHeightPx, static_cast<int> (effectiveDpi), s_kBaseDpi);
}




////////////////////////////////////////////////////////////////////////////////
//
//  TitleBarLayout::DefaultButtonWidth
//
////////////////////////////////////////////////////////////////////////////////

int TitleBarLayout::DefaultButtonWidth (UINT dpi)
{
    UINT  effectiveDpi = (dpi == 0) ? s_kBaseDpi : dpi;



    return MulDiv (s_kBaseButtonWidthPx, static_cast<int> (effectiveDpi), s_kBaseDpi);
}




////////////////////////////////////////////////////////////////////////////////
//
//  TitleBarLayout::WindowsUiFontFamily
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * TitleBarLayout::WindowsUiFontFamily ()
{
    return L"Segoe UI";
}




////////////////////////////////////////////////////////////////////////////////
//
//  TitleBarLayout::WindowsUiFontWeight
//
////////////////////////////////////////////////////////////////////////////////

int TitleBarLayout::WindowsUiFontWeight ()
{
    return FW_NORMAL;
}




////////////////////////////////////////////////////////////////////////////////
//
//  TitleBar
//
////////////////////////////////////////////////////////////////////////////////

TitleBar::TitleBar ()
{
}




////////////////////////////////////////////////////////////////////////////////
//
//  ~TitleBar
//
////////////////////////////////////////////////////////////////////////////////

TitleBar::~TitleBar ()
{
}




////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
////////////////////////////////////////////////////////////////////////////////

void TitleBar::Show ()
{
}




////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void TitleBar::Hide ()
{
}




////////////////////////////////////////////////////////////////////////////////
//
//  UpdateGeometry
//
////////////////////////////////////////////////////////////////////////////////

void TitleBar::UpdateGeometry (int clientWidth, UINT dpi)
{
    TitleBarLayoutInput  in = {};



    in.clientWidth = clientWidth;
    in.titleHeight = TitleBarLayout::DefaultTitleHeight (dpi);
    in.buttonWidth = TitleBarLayout::DefaultButtonWidth (dpi);
    m_layout = TitleBarLayout::Compute (in);
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetMousePosition
//
////////////////////////////////////////////////////////////////////////////////

void TitleBar::SetMousePosition (int x, int y, bool leftDown)
{
    m_leftDown     = leftDown;
    m_hasHotButton = true;



    if (RectContains (m_layout.minButton, x, y))
    {
        m_hotButton = SystemButton::Minimize;
    }
    else if (RectContains (m_layout.maxButton, x, y))
    {
        m_hotButton = SystemButton::Maximize;
    }
    else if (RectContains (m_layout.closeButton, x, y))
    {
        m_hotButton = SystemButton::Close;
    }
    else
    {
        m_hasHotButton = false;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  ButtonVisual
//
////////////////////////////////////////////////////////////////////////////////

ChromeButtonVisual TitleBar::ButtonVisual (SystemButton which) const
{
    if (!m_hasHotButton || m_hotButton != which)
    {
        return ChromeButtonVisual::Idle;
    }

    if (m_leftDown)
    {
        return ChromeButtonVisual::Pressed;
    }

    return ChromeButtonVisual::Hover;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void TitleBar::Paint (
    DxUiPainter             & painter,
    DwriteTextRenderer      & text,
    const ChromeVisualState & /*visual*/,
    const ChromeTheme       & theme)
{
    HRESULT  hr = S_OK;
    uint32_t minColor = (ButtonVisual (SystemButton::Minimize) == ChromeButtonVisual::Pressed) ? theme.sysButtonPressedArgb :
                        (ButtonVisual (SystemButton::Minimize) == ChromeButtonVisual::Hover)   ? theme.sysButtonHoverArgb   : theme.sysButtonIdleArgb;
    uint32_t maxColor = (ButtonVisual (SystemButton::Maximize) == ChromeButtonVisual::Pressed) ? theme.sysButtonPressedArgb :
                        (ButtonVisual (SystemButton::Maximize) == ChromeButtonVisual::Hover)   ? theme.sysButtonHoverArgb   : theme.sysButtonIdleArgb;
    uint32_t closeColor = (ButtonVisual (SystemButton::Close) == ChromeButtonVisual::Pressed) ? theme.sysButtonPressedArgb :
                          (ButtonVisual (SystemButton::Close) == ChromeButtonVisual::Hover)   ? theme.sysButtonCloseHoverArgb : theme.sysButtonIdleArgb;



    painter.FillGradientRect ((float) m_layout.titleBar.left,
                              (float) m_layout.titleBar.top,
                              (float) (m_layout.titleBar.right - m_layout.titleBar.left),
                              (float) (m_layout.titleBar.bottom - m_layout.titleBar.top),
                              theme.titleBarTopArgb,
                              theme.titleBarBottomArgb);
    IGNORE_RETURN_VALUE (hr, text.DrawString (s_kTitle,
                                              (float) m_layout.titleBar.left + s_kTitlePadDip,
                                              (float) m_layout.titleBar.top,
                                              160.0f,
                                              (float) (m_layout.titleBar.bottom - m_layout.titleBar.top),
                                              theme.titleTextArgb,
                                              s_kTitleFontDip,
                                              TitleBarLayout::WindowsUiFontFamily(),
                                              DwriteTextRenderer::HAlign::Left,
                                              DwriteTextRenderer::VAlign::Center));
    PaintButton (painter, text, m_layout.minButton,   s_kMinGlyph,   minColor,   theme.titleTextArgb);
    PaintButton (painter, text, m_layout.maxButton,   s_kMaxGlyph,   maxColor,   theme.titleTextArgb);
    PaintButton (painter, text, m_layout.closeButton, s_kCloseGlyph, closeColor, theme.titleTextArgb);
}




////////////////////////////////////////////////////////////////////////////////
//
//  GetButtonRect
//
////////////////////////////////////////////////////////////////////////////////

RECT TitleBar::GetButtonRect (SystemButton which) const
{
    switch (which)
    {
    case SystemButton::Minimize: return m_layout.minButton;
    case SystemButton::Maximize: return m_layout.maxButton;
    case SystemButton::Close:    return m_layout.closeButton;
    }

    return RECT { 0, 0, 0, 0 };
}
