#include "Pch.h"

#include "DxuiCaptionBar.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"



namespace
{
    constexpr int      s_kBaseDpi          = 96;
    constexpr int      s_kButtonWidthDip   = 46;
    constexpr float    s_kTitleFontDip     = 14.0f;
    constexpr float    s_kTitlePadDip      = 14.0f;
    constexpr float    s_kIconPadFraction  = 0.18f;
    constexpr wchar_t  s_kTitleFamily[]    = L"Segoe UI";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCaptionBar
//
//  Constructor — defers entirely to DxuiPanel; host-owned rendering is
//  opt-in via ConfigureButtons.
//
////////////////////////////////////////////////////////////////////////////////

DxuiCaptionBar::DxuiCaptionBar()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigureButtons
//
//  Builds the owned min / max / close DxuiSystemButton children for the
//  requested layout and switches the bar into self-rendering mode
//  (gradient + icon + title). Idempotent enough for construction-time
//  use; not intended to be toggled at runtime.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCaptionBar::ConfigureButtons (Buttons buttons)
{
    DXUI_ASSERT_UI_THREAD();

    m_buttons       = buttons;
    m_renderCaption = true;

    if (buttons == Buttons::MinMaxClose)
    {
        m_minBtn = std::make_unique<DxuiSystemButton> (DxuiSystemButtonKind::Min);
        m_maxBtn = std::make_unique<DxuiSystemButton> (DxuiSystemButtonKind::Max);
        Adopt (*m_minBtn);
        Adopt (*m_maxBtn);
    }

    if (buttons == Buttons::MinMaxClose || buttons == Buttons::CloseOnly)
    {
        m_closeBtn = std::make_unique<DxuiSystemButton> (DxuiSystemButtonKind::Close);
        Adopt (*m_closeBtn);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSystemHwnd
//
//  Hands the owning window's HWND to every system button so each can
//  dispatch its standard Win32 system command on click.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCaptionBar::SetSystemHwnd (HWND hwnd)
{
    DXUI_ASSERT_UI_THREAD();

    if (m_minBtn)   { m_minBtn->SetHwnd (hwnd);   }
    if (m_maxBtn)   { m_maxBtn->SetHwnd (hwnd);   }
    if (m_closeBtn) { m_closeBtn->SetHwnd (hwnd); }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTitle
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCaptionBar::SetTitle (const std::wstring & title)
{
    DXUI_ASSERT_UI_THREAD();

    m_title = title;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetAppIcon
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCaptionBar::SetAppIcon (std::vector<uint32_t> bgraPremul, int widthPx, int heightPx)
{
    DXUI_ASSERT_UI_THREAD();

    m_iconPixels = std::move (bgraPremul);
    m_iconW      = widthPx;
    m_iconH      = heightPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMaximized
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCaptionBar::SetMaximized (bool maximized)
{
    DXUI_ASSERT_UI_THREAD();

    m_maximized = maximized;
    if (m_maxBtn)
    {
        m_maxBtn->SetMaximized (maximized);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreferredHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiCaptionBar::PreferredHeightPx (const DxuiDpiScaler & scaler) const
{
    return scaler.Px (kCaptionHeightDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreferredHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int DxuiCaptionBar::PreferredHeightDip () const
{
    return kCaptionHeightDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeightPxForDpi
//
////////////////////////////////////////////////////////////////////////////////

int DxuiCaptionBar::HeightPxForDpi (UINT dpi)
{
    return MulDiv (kCaptionHeightDip, (int) dpi, s_kBaseDpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  Captures the scaler and right-aligns the system buttons in DIP
//  space (each button is one fixed-width column at full caption
//  height). DxuiPanel::Layout only recurses into child panels, so the
//  non-panel buttons must be laid out explicitly here.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCaptionBar::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int   right  = 0;
    int   top    = 0;
    int   bottom = 0;
    RECT  rc     = {};



    DXUI_ASSERT_UI_THREAD();

    DxuiPanel::Layout (boundsDip, scaler);
    m_scaler = scaler;

    right  = boundsDip.right;
    top    = boundsDip.top;
    bottom = boundsDip.bottom;

    if (m_closeBtn)
    {
        rc = { right - s_kButtonWidthDip, top, right, bottom };
        m_closeBtn->Layout (rc, scaler);
        right -= s_kButtonWidthDip;
    }
    if (m_maxBtn)
    {
        rc = { right - s_kButtonWidthDip, top, right, bottom };
        m_maxBtn->Layout (rc, scaler);
        right -= s_kButtonWidthDip;
    }
    if (m_minBtn)
    {
        rc = { right - s_kButtonWidthDip, top, right, bottom };
        m_minBtn->Layout (rc, scaler);
        right -= s_kButtonWidthDip;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  Host-owned mode: gradient fill + app icon + title text, then the
//  base panel walk paints the system-button children on top. Plain-
//  container mode (m_renderCaption == false) paints only the children
//  so existing DxuiDialog usage is unchanged.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiCaptionBar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT     b            = {};
    float    xPx          = 0.0f;
    float    yPx          = 0.0f;
    float    wPx          = 0.0f;
    float    hPx          = 0.0f;
    float    iconPadPx    = 0.0f;
    float    iconSizePx   = 0.0f;
    float    textLeftPx   = 0.0f;
    float    textOffsetPx = 0.0f;
    float    titleWidthPx = 0.0f;
    float    fontPx       = 0.0f;
    float    buttonStripPx = 0.0f;
    int      buttonCount   = 0;



    DXUI_ASSERT_UI_THREAD();

    if (!m_renderCaption)
    {
        DxuiPanel::Paint (painter, text, theme);
        return;
    }

    b   = Bounds();
    xPx = (float) m_scaler.Px (b.left);
    yPx = (float) m_scaler.Px (b.top);
    wPx = (float) m_scaler.Px (b.right - b.left);
    hPx = (float) m_scaler.Px (b.bottom - b.top);

    painter.FillGradientRect (xPx, yPx, wPx, hPx, theme.TitleBarTop(), theme.TitleBarBottom());

    iconPadPx    = hPx * s_kIconPadFraction;
    iconSizePx   = hPx - iconPadPx * 2.0f;
    textOffsetPx = m_scaler.Pxf (s_kTitlePadDip);

    if (iconSizePx > 0.0f && !m_iconPixels.empty() && m_iconW > 0 && m_iconH > 0)
    {
        HRESULT  hrIcon = text.DrawIconBitmap (m_iconPixels.data(),
                                               m_iconW, m_iconH,
                                               xPx + textOffsetPx,
                                               yPx + iconPadPx,
                                               iconSizePx, iconSizePx);
        IGNORE_RETURN_VALUE (hrIcon, S_OK);
        textOffsetPx = m_scaler.Pxf (s_kTitlePadDip) + iconSizePx + iconPadPx;
    }

    buttonCount   = (m_buttons == Buttons::MinMaxClose) ? 3 : (m_buttons == Buttons::CloseOnly ? 1 : 0);
    buttonStripPx = (float) buttonCount * m_scaler.Pxf ((float) s_kButtonWidthDip);

    textLeftPx   = xPx + textOffsetPx;
    titleWidthPx = wPx - textOffsetPx - buttonStripPx - m_scaler.Pxf (s_kTitlePadDip);
    if (titleWidthPx < 0.0f)
    {
        titleWidthPx = 0.0f;
    }

    fontPx = m_scaler.Pxf (s_kTitleFontDip);

    {
        HRESULT  hrText = text.DrawString (m_title.c_str(),
                                           textLeftPx,
                                           yPx,
                                           titleWidthPx,
                                           hPx,
                                           theme.CaptionForeground(),
                                           fontPx,
                                           s_kTitleFamily,
                                           DxuiTextHAlign::Left,
                                           DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hrText, S_OK);
    }

    DxuiPanel::Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClassifyHit
//
//  Front-to-back walk of children: the topmost child whose bounds
//  contain the point gets to classify. Blank caption area (no child
//  consumed it) falls through to DxuiHitTestKind::Caption.
//
////////////////////////////////////////////////////////////////////////////////

DxuiHitTestKind DxuiCaptionBar::ClassifyHit (POINT clientDip) const
{
    IDxuiControl *   child = nullptr;
    DxuiHitTestKind  kind  = DxuiHitTestKind::Caption;
    RECT             rc    = {};
    size_t           n     = 0;
    size_t           i     = 0;



    n = ChildCount();

    // Reverse order so visually-topmost children win.
    for (i = n; i > 0; --i)
    {
        child = Child (i - 1);
        if (child == nullptr || !child->Visible())
        {
            continue;
        }

        rc = child->Bounds();
        if (clientDip.x < rc.left || clientDip.x >= rc.right ||
            clientDip.y < rc.top  || clientDip.y >= rc.bottom)
        {
            continue;
        }

        kind = child->ClassifyHit (clientDip);
        if (kind != DxuiHitTestKind::None)
        {
            return kind;
        }
    }

    return DxuiHitTestKind::Caption;
}
