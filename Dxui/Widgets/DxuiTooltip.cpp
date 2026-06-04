#include "Pch.h"

#include "Widgets/DxuiTooltip.h"
#include "Win32/DxuiHostWindow.h"
#include "Win32/DxuiPopupHost.h"





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

        // Opt-in popup hosting: acquire a pooled WS_POPUP host with
        // pass-through input + OnPointerLeave dismiss.
        if (m_popupHost != nullptr && m_activePopup == nullptr)
        {
            HRESULT                    hrShow      = S_OK;
            DxuiPopupHost::ShowParams  showParams;

            m_activePopup = m_popupHost->AcquirePopup();
            if (m_activePopup != nullptr)
            {
                showParams.ownerHwnd        = m_popupHost->Hwnd();
                showParams.anchorRectScreen = m_anchor;
                showParams.placement        = DxuiPopupPlacement::Above;
                showParams.flipIfOffscreen  = true;
                showParams.dismiss          = DxuiPopupDismiss::OnPointerLeave;
                showParams.input            = DxuiPopupInput::PassThrough;
                showParams.shadow           = false;
                showParams.sizeDip.cx       = (int) (m_text.size() * 8 + 16);
                showParams.sizeDip.cy       = (int) m_fontDip + 8;
                showParams.content          = std::make_unique<DxuiPanel>();

                hrShow = m_activePopup->Show (std::move (showParams));
                if (FAILED (hrShow))
                {
                    m_popupHost->ReleasePopup (m_activePopup);
                    m_activePopup = nullptr;
                }
            }
        }
    }

    if (m_visible && m_hideAtMs != 0 && nowMs >= m_hideAtMs)
    {
        m_visible  = false;
        m_text.clear();
        m_hideAtMs = 0;

        if (m_activePopup != nullptr && m_popupHost != nullptr)
        {
            m_popupHost->ReleasePopup (m_activePopup);
            m_activePopup = nullptr;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTooltip::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
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
    float    textW     = 0.0f;
    float    textH     = 0.0f;
    float    width     = 0.0f;
    float    height    = 0.0f;
    float    boxLeft   = 0.0f;
    float    boxTop    = 0.0f;



    if (!m_visible || m_text.empty())
    {
        return;
    }

    hr = const_cast<IDxuiTextRenderer &> (text).MeasureString (m_text.c_str(),
                                                                fontPx, L"Segoe UI",
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

    painter.FillRect    (boxLeft, boxTop, width, height, s_kBgArgb);
    painter.OutlineRect (boxLeft, boxTop, width, height, borderPx, s_kBorderArgb);

    IGNORE_RETURN_VALUE (hr, text.DrawString (m_text.c_str(),
                                              boxLeft + padX,
                                              boxTop  + padY,
                                              width  - padX * 2.0f,
                                              height - padY * 2.0f,
                                              s_kTextArgb,
                                              fontPx,
                                              L"Segoe UI"));
}
