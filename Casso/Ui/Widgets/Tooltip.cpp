#include "Pch.h"

#include "Tooltip.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RequestShow
//
//  Queues the tooltip for display after the open dwell timeout. If
//  the tooltip is already up over a different anchor, swap text +
//  anchor instantly.
//
////////////////////////////////////////////////////////////////////////////////

void Tooltip::RequestShow (const RECT & anchor, const std::wstring & text, int64_t nowMs)
{
    if (m_visible)
    {
        m_anchor = anchor;
        m_text   = text;
        m_hideAtMs = 0;
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

void Tooltip::RequestHide (int64_t nowMs)
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
//  DismissAfter
//
//  Schedules a self-dismiss `delayMs` from now and cancels any pending
//  (not-yet-shown) request. Used when the pointer is about to stop
//  generating events over this tooltip -- e.g. paddle-mode mouse capture --
//  so a visible balloon still fades on its own instead of sticking forever.
//
////////////////////////////////////////////////////////////////////////////////

void Tooltip::DismissAfter (int64_t nowMs, int delayMs)
{
    m_pending = false;

    if (m_visible)
    {
        m_hideAtMs = nowMs + (int64_t) delayMs;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
////////////////////////////////////////////////////////////////////////////////

void Tooltip::Tick (int64_t nowMs)
{
    if (m_pending && nowMs >= m_showAtMs)
    {
        m_anchor   = m_pendingAnchor;
        m_text     = m_pendingText;
        m_visible  = true;
        m_pending  = false;
        m_hideAtMs = 0;
    }

    if (m_visible && m_hideAtMs != 0 && nowMs >= m_hideAtMs)
    {
        m_visible  = false;
        m_text.clear();
        m_hideAtMs = 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void Tooltip::Paint (DxUiPainter & painter, DwriteTextRenderer & text) const
{
    constexpr uint32_t  s_kBgArgb     = 0xFF2D2D2D;
    constexpr uint32_t  s_kBorderArgb = 0xFF606060;
    constexpr uint32_t  s_kTextArgb   = 0xFFE8EEF4;
    constexpr float     s_kPadXDip    = 8.0f;
    constexpr float     s_kPadYDip    = 4.0f;
    constexpr float     s_kBorderDip  = 1.0f;
    constexpr float     s_kAnchorGapDip = 4.0f;

    HRESULT  hr        = S_OK;
    float    fontPx    = m_scaler.Pxf (m_fontDip);
    float    padX      = m_scaler.Pxf (s_kPadXDip);
    float    padY      = m_scaler.Pxf (s_kPadYDip);
    float    borderPx  = m_scaler.Pxf (s_kBorderDip);
    float    anchorGap = m_scaler.Pxf (s_kAnchorGapDip);
    float    maxContentPx = FLT_MAX;
    float    textW     = 0.0f;
    float    textH     = 0.0f;
    float    width     = 0.0f;
    float    height    = 0.0f;
    float    boxLeft   = 0.0f;
    float    boxTop    = 0.0f;
    bool     wrap      = false;



    UNREFERENCED_PARAMETER (painter);

    if (!m_visible || m_text.empty())
    {
        return;
    }

    // When a max width is set, constrain the content box so the text wraps
    // to multiple lines instead of running off the window edge. The cap is
    // further limited to the viewport so a wide tooltip near a screen edge
    // still fits.
    if (m_maxWidthDip > 0.0f)
    {
        float  maxBoxPx = m_scaler.Pxf (m_maxWidthDip);

        if (m_viewportWPx > 0)
        {
            float  viewportCapPx = (float) m_viewportWPx - anchorGap * 2.0f;

            if (maxBoxPx > viewportCapPx)
            {
                maxBoxPx = viewportCapPx;
            }
        }

        maxContentPx = maxBoxPx - padX * 2.0f;

        if (maxContentPx < 1.0f)
        {
            maxContentPx = 1.0f;
        }

        wrap = true;
    }

    hr = const_cast<DwriteTextRenderer &> (text).MeasureString (m_text.c_str(),
                                                                fontPx, L"Segoe UI",
                                                                textW, textH,
                                                                maxContentPx);
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

    // Draw the background + border through the text (D2D) layer, not the
    // geometry painter: the painter composites in a separate pass UNDER all
    // text, so a painter-drawn balloon would let underlying labels (e.g. a
    // drive widget's "DRIVE 2") bleed through. Drawing here, after the
    // chrome's text and before our own, occludes them. The border is an
    // outer fill with the background inset by one border width.
    text.FillRect (boxLeft, boxTop, width, height, s_kBorderArgb);
    text.FillRect (boxLeft + borderPx,
                   boxTop  + borderPx,
                   width  - borderPx * 2.0f,
                   height - borderPx * 2.0f,
                   s_kBgArgb);

    IGNORE_RETURN_VALUE (hr, text.DrawString (m_text.c_str(),
                                              boxLeft + padX,
                                              boxTop  + padY,
                                              width  - padX * 2.0f,
                                              height - padY * 2.0f,
                                              s_kTextArgb,
                                              fontPx,
                                              L"Segoe UI",
                                              DwriteTextRenderer::HAlign::Left,
                                              DwriteTextRenderer::VAlign::Top,
                                              DWRITE_FONT_WEIGHT_NORMAL,
                                              wrap));
}
