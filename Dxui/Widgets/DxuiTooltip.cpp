#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTooltip.h"
#include "Window/DxuiHostWindow.h"
#include "Window/DxuiPopupHost.h"




static constexpr float     s_kPadXDip         = 8.0f;
static constexpr float     s_kPadYDip         = 4.0f;
static constexpr float     s_kBorderDip       = 1.0f;
static constexpr const wchar_t * s_kFontFamily    = DxuiTheme::kBodyFace;

//
//  Fallback glyph metrics used to size the balloon when precise text
//  measurement is unavailable (e.g. test mode, where the popup has no
//  DWrite factory). Deliberately a little generous so text never clips.
//
static constexpr float     s_kEstCharWidthEm  = 0.62f;
static constexpr float     s_kEstLineHeightEm = 1.4f;




////////////////////////////////////////////////////////////////////////////////
//
//  RequestShow
//
//  Queues the tooltip for display after the open dwell timeout. If
//  the tooltip is already up over a different anchor, swap text +
//  anchor instantly.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::RequestShow (const RECT & anchor, const std::wstring & text, int64_t nowMs)
{
    if (m_visible)
    {
        bool  changed = (text != m_text) ||
                        anchor.left   != m_anchor.left  ||
                        anchor.top    != m_anchor.top   ||
                        anchor.right  != m_anchor.right ||
                        anchor.bottom != m_anchor.bottom;

        m_anchor   = anchor;
        m_text     = text;
        m_hideAtMs = 0;

        // Already-up popup pointing at a different control: re-show it at
        // the new anchor/text. Skip churn when nothing moved (consumers
        // re-issue RequestShow on every mouse-move over the same control).
        if (changed && m_popupHost != nullptr)
        {
            ReleaseActivePopup();
            ShowPopup();
        }
        return;
    }

    m_pendingAnchor = anchor;
    m_pendingText   = text;
    m_pending       = true;
    m_showAtMs      = nowMs + (int64_t) m_dwellOpenMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RequestHide
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::RequestHide (int64_t nowMs)
{
    if (m_pending)
    {
        m_pending = false;
    }

    if (m_visible)
    {
        m_hideAtMs = nowMs + (int64_t) m_dwellCloseMs;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::Tick (int64_t nowMs)
{
    if (m_pending && nowMs >= m_showAtMs)
    {
        m_anchor   = m_pendingAnchor;
        m_text     = m_pendingText;
        m_visible  = true;
        m_pending  = false;
        m_hideAtMs = 0;

        ShowPopup();
    }

    if (m_visible && m_hideAtMs != 0 && nowMs >= m_hideAtMs)
    {
        m_visible  = false;
        m_text.clear();
        m_hideAtMs = 0;

        ReleaseActivePopup();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HideImmediate
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::HideImmediate ()
{
    m_pending  = false;
    m_visible  = false;
    m_hideAtMs = 0;
    m_text.clear();

    ReleaseActivePopup();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowPopup
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::ShowPopup ()
{
    DxuiPopupHost::ShowParams  showParams;
    POINT                      topLeft  = {};
    POINT                      botRight = {};
    HWND                       owner    = nullptr;
    HRESULT                    hr       = S_OK;
    float                      textWDip = 0.0f;
    float                      textHDip = 0.0f;
    float                      boxWDip  = 0.0f;
    float                      boxHDip  = 0.0f;



    if (m_popupHost == nullptr || m_activePopup != nullptr || m_text.empty())
    {
        return;
    }

    owner         = m_popupHost->Hwnd();
    m_activePopup = m_popupHost->AcquirePopup();
    if (m_activePopup == nullptr)
    {
        return;
    }

    // Size the balloon to its text. MeasureText runs on the pooled
    // popup's text renderer before Show builds the swap chain; if it is
    // unavailable (test mode) fall back to a glyph-count estimate.
    hr = m_activePopup->MeasureText (m_text.c_str(), m_fontDip, s_kFontFamily, textWDip, textHDip);
    if (FAILED (hr) || textWDip <= 0.0f)
    {
        textWDip = (float) m_text.size() * m_fontDip * s_kEstCharWidthEm;
    }
    if (textHDip <= 0.0f)
    {
        textHDip = m_fontDip * s_kEstLineHeightEm;
    }

    boxWDip = std::ceil (textWDip) + s_kPadXDip * 2.0f;
    boxHDip = std::ceil (textHDip) + s_kPadYDip * 2.0f;

    // Anchor arrives in client pixels; the popup wants screen pixels.
    topLeft.x  = m_anchor.left;
    topLeft.y  = m_anchor.top;
    botRight.x = m_anchor.right;
    botRight.y = m_anchor.bottom;
    ClientToScreen (owner, &topLeft);
    ClientToScreen (owner, &botRight);

    showParams.ownerHwnd        = owner;
    showParams.anchorRectScreen = { topLeft.x, topLeft.y, botRight.x, botRight.y };
    showParams.placement        = DxuiPopupPlacement::Below;
    showParams.flipIfOffscreen  = true;
    showParams.dismiss          = DxuiPopupDismiss::Manual;
    showParams.input            = DxuiPopupInput::PassThrough;
    showParams.shadow           = false;
    showParams.sizeDip.cx       = (int) boxWDip;
    showParams.sizeDip.cy       = (int) boxHDip;
    showParams.backgroundArgb   = m_bgArgb;
    showParams.renderContent    = [this] (IDxuiPainter & p, IDxuiTextRenderer & t) { RenderPopup (p, t); };
    showParams.onClosed         = [this] () { m_activePopup = nullptr; };

    hr = m_activePopup->Show (std::move (showParams));
    if (FAILED (hr))
    {
        m_popupHost->ReleasePopup (m_activePopup);
        m_activePopup = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseActivePopup
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::ReleaseActivePopup ()
{
    DxuiPopupHost *  popup = m_activePopup;


    // Null the pointer first so the popup's onClosed callback (which
    // routes back here) is a no-op and cannot double-release.
    m_activePopup = nullptr;

    if (popup != nullptr && m_popupHost != nullptr)
    {
        m_popupHost->ReleasePopup (popup);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    constexpr float     s_kAnchorGapDip = 4.0f;

    HRESULT  hr        = S_OK;
    float    fontPx    = m_scaler.Pxf (m_fontDip);
    float    padX      = m_scaler.Pxf (s_kPadXDip);
    float    padY      = m_scaler.Pxf (s_kPadYDip);
    float    borderPx  = m_scaler.Pxf (s_kBorderDip);
    float    anchorGap = m_scaler.Pxf (s_kAnchorGapDip);
    float    textW     = 0.0f;
    float    textH     = 0.0f;
    float    width     = 0.0f;
    float    height    = 0.0f;
    float    boxLeft   = 0.0f;
    float    boxTop    = 0.0f;



    if (!m_visible || m_text.empty() || m_activePopup != nullptr)
    {
        return;
    }

    hr = const_cast<IDxuiTextRenderer &> (text).MeasureString (m_text.c_str(),
                                                                fontPx, s_kFontFamily,
                                                                textW, textH);
    IGNORE_RETURN_VALUE (hr, S_OK);

    width   = std::ceil (textW)  + padX * 2.0f;
    height  = std::ceil (textH)  + padY * 2.0f;
    boxLeft = (float) m_anchor.left;
    boxTop  = (float) m_anchor.bottom + anchorGap;

    if (m_viewportWPx > 0)
    {
        float  edgePad = m_scaler.Pxf (s_kAnchorGapDip);

        if (boxLeft + width > (float) m_viewportWPx - edgePad)
        {
            boxLeft = (float) m_viewportWPx - edgePad - width;
        }
        if (boxLeft < edgePad)
        {
            boxLeft = edgePad;
        }
    }

    if (m_viewportHPx > 0)
    {
        float  flippedTop = (float) m_anchor.top - anchorGap - height;

        if (boxTop + height > (float) m_viewportHPx && flippedTop >= 0.0f)
        {
            boxTop = flippedTop;
        }
    }

    painter.FillRect    (boxLeft, boxTop, width, height, m_bgArgb);
    painter.OutlineRect (boxLeft, boxTop, width, height, borderPx, m_borderArgb);

    IGNORE_RETURN_VALUE (hr, text.DrawString (m_text.c_str(),
                                              boxLeft + padX,
                                              boxTop  + padY,
                                              width  - padX * 2.0f,
                                              height - padY * 2.0f,
                                              m_textArgb,
                                              fontPx,
                                              s_kFontFamily));
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltip::Layout  (IDxuiControl override)
//
//  The popup geometry is driven by RequestShow / Paint anchor-based
//  placement; the override just records bounds and DPI for the panel.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.Dpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltip::Paint  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    SetTheme (theme);
    static_cast<const DxuiTooltip *> (this)->Paint (painter, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderPopup
//
//  Popup-host render hook. The host has already cleared the back buffer
//  to s_kBgArgb, so this only draws the border (painter / D3D, under the
//  text) and the text (D2D, composited on top). Coordinates are popup-
//  local pixels with the origin at the balloon's top-left.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::RenderPopup (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    HRESULT  hr       = S_OK;
    RECT     placed   = {};
    float    width    = 0.0f;
    float    height   = 0.0f;
    float    padX     = m_scaler.Pxf (s_kPadXDip);
    float    padY     = m_scaler.Pxf (s_kPadYDip);
    float    borderPx = m_scaler.Pxf (s_kBorderDip);
    float    fontPx   = m_scaler.Pxf (m_fontDip);



    if (m_activePopup == nullptr)
    {
        return;
    }

    placed = m_activePopup->PlacedRectScreenPx();
    width  = (float) (placed.right  - placed.left);
    height = (float) (placed.bottom - placed.top);

    painter.OutlineRect (0.0f, 0.0f, width, height, borderPx, m_borderArgb);

    IGNORE_RETURN_VALUE (hr, text.DrawString (m_text.c_str(),
                                              padX,
                                              padY,
                                              width  - padX * 2.0f,
                                              height - padY * 2.0f,
                                              m_textArgb,
                                              fontPx,
                                              s_kFontFamily));
}
