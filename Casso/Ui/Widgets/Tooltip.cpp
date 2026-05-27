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
    constexpr uint32_t  s_kBgArgb     = 0xF02D2D2D;
    constexpr uint32_t  s_kBorderArgb = 0xFF606060;
    constexpr uint32_t  s_kTextArgb   = 0xFFE8EEF4;
    constexpr float     s_kPadX       = 8.0f;
    constexpr float     s_kPadY       = 4.0f;
    constexpr float     s_kBorderPx   = 1.0f;
    constexpr float     s_kCharWidth  = 7.0f;
    constexpr float     s_kAnchorGap  = 4.0f;

    HRESULT  hr      = S_OK;
    float    width   = 0.0f;
    float    height  = 0.0f;
    float    boxLeft = 0.0f;
    float    boxTop  = 0.0f;



    if (!m_visible || m_text.empty())
    {
        return;
    }

    width  = (float) m_text.size() * s_kCharWidth + s_kPadX * 2.0f;
    height = m_fontDip + s_kPadY * 2.0f;
    boxLeft = (float) m_anchor.left;
    boxTop  = (float) m_anchor.bottom + s_kAnchorGap;

    painter.FillRect    (boxLeft, boxTop, width, height, s_kBgArgb);
    painter.OutlineRect (boxLeft, boxTop, width, height, s_kBorderPx, s_kBorderArgb);

    IGNORE_RETURN_VALUE (hr, text.DrawString (m_text.c_str(),
                                              boxLeft + s_kPadX,
                                              boxTop  + s_kPadY,
                                              width  - s_kPadX * 2.0f,
                                              height - s_kPadY * 2.0f,
                                              s_kTextArgb,
                                              m_fontDip,
                                              L"Segoe UI"));
}
