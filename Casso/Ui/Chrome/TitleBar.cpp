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
    constexpr wchar_t   s_kMinGlyph[]        = L"\xE921";
    constexpr wchar_t   s_kMaxGlyph[]        = L"\xE922";
    constexpr wchar_t   s_kCloseGlyph[]      = L"\xE8BB";


    bool RectContains (const RECT & r, int x, int y)
    {
        return (x >= r.left) && (x < r.right) && (y >= r.top) && (y < r.bottom);
    }


    void PaintButton (IDxuiPainter            & painter,
                      IDxuiTextRenderer       & text,
                      const RECT              & rect,
                      const wchar_t           * glyph,
                      uint32_t                  fillArgb,
                      uint32_t                  textArgb,
                      UINT                      dpi)
    {
        HRESULT  hr             = S_OK;
        float    glyphFontDip   = 11.0f * (float) dpi / (float) s_kBaseDpi;



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
                                                  glyphFontDip,
                                                  s_kMdl2Family,
                                                  DxuiTextRenderer::HAlign::Center,
                                                  DxuiTextRenderer::VAlign::Center));
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
    m_dpi    = (dpi == 0) ? (UINT) s_kBaseDpi : dpi;
    SetBounds (m_layout.titleBar);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  IDxuiControl override — delegates to UpdateGeometry using the
//  bounds rect's width and the scaler's DPI. The title bar always
//  anchors to the host's top-left client corner so boundsDip.left /
//  top are ignored; SetBounds writes the computed title-bar rect.
//
////////////////////////////////////////////////////////////////////////////////

void TitleBar::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    UpdateGeometry (boundsDip.right - boundsDip.left, scaler.Dpi());
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
//  ClearHover
//
//  Drop the hot-button state so caption buttons paint as Idle. Called
//  when the cursor leaves the window (WM_NCMOUSELEAVE) so a hover
//  visual doesn't latch on.
//
////////////////////////////////////////////////////////////////////////////////

void TitleBar::ClearHover ()
{
    m_hasHotButton = false;
    m_leftDown     = false;
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
    IDxuiPainter      & painter,
    IDxuiTextRenderer & text,
    const IDxuiTheme  & theme)
{
    HRESULT                    hr            = S_OK;
    ChromeButtonVisual         minVis        = ButtonVisual (SystemButton::Minimize);
    ChromeButtonVisual         maxVis        = ButtonVisual (SystemButton::Maximize);
    ChromeButtonVisual         closeVis      = ButtonVisual (SystemButton::Close);
    uint32_t                   minColor      = 0;
    uint32_t                   maxColor      = 0;
    uint32_t                   closeColor    = 0;
    uint32_t                   glyphArgb     = theme.CaptionForeground();
    uint32_t                   closeGlyph    = glyphArgb;
    UINT                       dpi           = (m_dpi == 0) ? s_kBaseDpi : m_dpi;
    float                      titleFontDip  = s_kTitleFontDip * (float) dpi / (float) s_kBaseDpi;



    if      (minVis == ChromeButtonVisual::Pressed) { minColor = theme.SystemButtonPressed(); }
    else if (minVis == ChromeButtonVisual::Hover)   { minColor = theme.SystemButtonHover();   }

    if      (maxVis == ChromeButtonVisual::Pressed) { maxColor = theme.SystemButtonPressed(); }
    else if (maxVis == ChromeButtonVisual::Hover)   { maxColor = theme.SystemButtonHover();   }

    if (closeVis == ChromeButtonVisual::Pressed)
    {
        closeColor = theme.SystemClosePressed();
        closeGlyph = 0xFFFFFFFF;
    }
    else if (closeVis == ChromeButtonVisual::Hover)
    {
        closeColor = theme.SystemCloseHover();
        closeGlyph = 0xFFFFFFFF;
    }

    painter.FillGradientRect ((float) m_layout.titleBar.left,
                              (float) m_layout.titleBar.top,
                              (float) (m_layout.titleBar.right - m_layout.titleBar.left),
                              (float) (m_layout.titleBar.bottom - m_layout.titleBar.top),
                              theme.TitleBarTop(),
                              theme.TitleBarBottom());

    // App icon (drawn left of the title text). Title-bar height drives
    // the icon size with a small padding inset so it sits visually
    // inside the bar rather than touching the edges.
    float  titleH         = (float) (m_layout.titleBar.bottom - m_layout.titleBar.top);
    float  iconPadDip     = titleH * 0.18f;
    float  iconSizeDip    = titleH - iconPadDip * 2.0f;
    float  iconLeftDip    = (float) m_layout.titleBar.left + s_kTitlePadDip;
    float  textOffsetDip  = s_kTitlePadDip;

    if (iconSizeDip > 0.0f && !m_appIconPixels.empty() && m_appIconW > 0 && m_appIconH > 0)
    {
        HRESULT  hrIcon = text.DrawIconBitmap (m_appIconPixels.data(),
                                               m_appIconW, m_appIconH,
                                               iconLeftDip,
                                               (float) m_layout.titleBar.top + iconPadDip,
                                               iconSizeDip, iconSizeDip);
        IGNORE_RETURN_VALUE (hrIcon, S_OK);
        textOffsetDip = s_kTitlePadDip + iconSizeDip + iconPadDip;
    }

    float  titleTextLeft   = (float) m_layout.titleBar.left + textOffsetDip;
    float  titleTextWidth  = (float) m_layout.dragRegion.right - titleTextLeft - s_kTitlePadDip;

    if (titleTextWidth < 0.0f)
    {
        titleTextWidth = 0.0f;
    }

    IGNORE_RETURN_VALUE (hr, text.DrawString (m_title.c_str(),
                                              titleTextLeft,
                                              (float) m_layout.titleBar.top,
                                              titleTextWidth,
                                              (float) (m_layout.titleBar.bottom - m_layout.titleBar.top),
                                              glyphArgb,
                                              titleFontDip,
                                              TitleBarLayout::WindowsUiFontFamily(),
                                              DxuiTextRenderer::HAlign::Left,
                                              DxuiTextRenderer::VAlign::Center));
    PaintButton (painter, text, m_layout.minButton,   s_kMinGlyph,   minColor,   glyphArgb,  dpi);
    PaintButton (painter, text, m_layout.maxButton,   s_kMaxGlyph,   maxColor,   glyphArgb,  dpi);
    PaintButton (painter, text, m_layout.closeButton, s_kCloseGlyph, closeColor, closeGlyph, dpi);
}




////////////////////////////////////////////////////////////////////////////////
//
//  ClassifyHit
//
//  Map a point inside the title bar to the matching system-button hit
//  kind, or fall through to Caption for the drag strip / blank area.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHitTestKind TitleBar::ClassifyHit (POINT clientDip) const
{
    if (RectContains (m_layout.minButton, clientDip.x, clientDip.y))
    {
        return DxuiHitTestKind::MinButton;
    }

    if (RectContains (m_layout.maxButton, clientDip.x, clientDip.y))
    {
        return DxuiHitTestKind::MaxButton;
    }

    if (RectContains (m_layout.closeButton, clientDip.x, clientDip.y))
    {
        return DxuiHitTestKind::CloseButton;
    }

    return DxuiHitTestKind::Caption;
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





////////////////////////////////////////////////////////////////////////////////
//
//  SetAppIcon
//
////////////////////////////////////////////////////////////////////////////////

void TitleBar::SetAppIcon (std::vector<uint32_t> bgraPremulPixels,
                           int                    widthPx,
                           int                    heightPx)
{
    m_appIconPixels = std::move (bgraPremulPixels);
    m_appIconW      = widthPx;
    m_appIconH      = heightPx;
}
