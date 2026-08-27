#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTooltip.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiPopupHost.h"




static constexpr float     s_kPadXDip         = 8.0f;
static constexpr float     s_kPadYDip         = 4.0f;
static constexpr float     s_kBorderDip       = 1.0f;
static constexpr const wchar_t * s_kFontFamily    = DxuiTheme::kBodyFace;

//
//  Text wider than this wraps onto additional lines instead of growing the
//  balloon past the window edge.
//
static constexpr float     s_kMaxTextWidthDip = 340.0f;

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
//  ShowTimed
//
//  Shows the tooltip immediately and schedules an auto-hide durationMs
//  later, for notices that no pointer-leave will dismiss.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::ShowTimed (const RECT & anchor, const std::wstring & text, int64_t nowMs, int durationMs)
{
    bool  changed = !m_visible || text != m_text;



    m_anchor   = anchor;
    m_text     = text;
    m_pending  = false;
    m_visible  = true;
    m_hideAtMs = nowMs + (int64_t) durationMs;

    if (m_popupHost != nullptr && (changed || m_activePopup == nullptr))
    {
        ReleaseActivePopup();
        ShowPopup();
    }
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

        // A TOOLTIP HAS A LIFETIME. A hover tip used to set no hide time at
        // all, so it stayed up for as long as the pointer rested -- and a
        // pointer that has been captured, or simply parked, rests forever.
        // The OS dismisses its own after a few seconds for the same reason:
        // the tip has been read by then, and what is left is an obstruction
        // sitting over the thing it was explaining.
        m_hideAtMs = nowMs + kMaxVisibleMs;

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

void DxuiTooltip::HideImmediate()
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
//  Raises the tooltip balloon in a pooled popup window, sized to its text.
//
//  Three conditions mean "nothing to do" and none is an error: no host, no
//  text, or a balloon ALREADY UP. That last one is why the test cannot be
//  re-derived from m_activePopup further down -- by then this function may
//  have just acquired one, and it would look like the already-up case.
//
//  The DPI is taken from the host at show time, which folds what used to be an
//  explicit SetDpi push from every consumer into this one path.
//
//  Text is measured on the pooled popup's own text renderer BEFORE Show builds
//  the swap chain, so the balloon is created at the right size rather than
//  resized after appearing. When that renderer is unavailable -- test mode has
//  no device -- it falls back to a glyph-count estimate, so placement logic
//  stays testable without a GPU.
//
//  An exhausted pool simply shows nothing. A tooltip is an enhancement, and
//  failing to show one must never disturb what the user is doing.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::ShowPopup()
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
    bool                       shows    = false;



    // No host, nothing to say, or a balloon ALREADY UP: all three mean there
    // is nothing to do, and none of them is an error. The already-up case is
    // why this cannot be re-derived from m_activePopup further down -- by
    // then this function may have just acquired one.
    shows = (m_popupHost != nullptr && m_activePopup == nullptr && !m_text.empty());

    if (shows)
    {
        // The tooltip's DPI follows its host window, folding what used to be
        // an explicit SetDpi push from the consumer into the show path.
        m_scaler.SetDpi (m_popupHost->Scaler().Dpi());

        owner         = m_popupHost->Hwnd();
        m_activePopup = m_popupHost->AcquirePopup();

        // The pool can be exhausted, leaving no balloon to fill in.
        shows = (m_activePopup != nullptr);
    }

    if (shows)
    {
        // Size the balloon to its text, wrapping long messages instead of
        // growing past the window edge. MeasureTextWrapped runs on the
        // pooled popup's text renderer before Show builds the swap chain;
        // if it is unavailable (test mode) fall back to a glyph-count
        // estimate wrapped the same way.
        hr = m_activePopup->MeasureTextWrapped (m_text.c_str(), m_fontDip, s_kFontFamily,
                                                s_kMaxTextWidthDip, textWDip, textHDip);
        if (FAILED (hr) || textWDip <= 0.0f)
        {
            float  estWDip  = (float) m_text.size() * m_fontDip * s_kEstCharWidthEm;
            float  estLines = std::ceil (estWDip / s_kMaxTextWidthDip);

            textWDip = std::min (estWDip, s_kMaxTextWidthDip);
            textHDip = std::max (estLines, 1.0f) * m_fontDip * s_kEstLineHeightEm;
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseActivePopup
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::ReleaseActivePopup()
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
//  Draws the IN-WINDOW tooltip: the fallback used when no popup host is
//  available.
//
//  It exits immediately when a popup IS active, because that balloon renders
//  itself in its own window. Painting both would double-draw the tooltip, once
//  clipped to the client area and once not.
//
//  The box is placed below the anchor and then CLAMPED to the viewport on both
//  axes, so a tooltip near a window edge stays fully visible instead of being
//  clipped away -- which is the whole limitation of the in-window path, and
//  why the popup-hosted version exists.
//
//  Text is measured at paint time rather than cached, since the string changes
//  with whatever is hovered and the measurement is one call for a short label.
//
//  Dimensions are ceiled before padding is added, so a fractional text width
//  cannot round down and clip the final glyph.
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

    hr = const_cast<IDxuiTextRenderer &> (text).MeasureStringWrapped (
             m_text.c_str(), fontPx, s_kFontFamily,
             m_scaler.Pxf (s_kMaxTextWidthDip), textW, textH);
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

    hr = text.DrawString (m_text.c_str(),
                          boxLeft + padX,
                          boxTop  + padY,
                          width  - padX * 2.0f,
                          height - padY * 2.0f,
                          m_textArgb,
                          fontPx,
                          s_kFontFamily);
    IGNORE_RETURN_VALUE (hr, S_OK);
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

    hr = text.DrawString (m_text.c_str(),
                          padX,
                          padY,
                          width  - padX * 2.0f,
                          height - padY * 2.0f,
                          m_textArgb,
                          fontPx,
                          s_kFontFamily);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
