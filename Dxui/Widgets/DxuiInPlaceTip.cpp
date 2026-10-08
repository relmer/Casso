#include "Pch.h"

#include "DxuiInPlaceTip.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiPopupHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInPlaceTip::Show
//
//  Placed below a zero-height anchor at the rect's top, which puts the popup
//  exactly on the rect; never flipped, since moving it would defeat the
//  point of laying it over the row.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInPlaceTip::Show (const RECT & clientRect, uint32_t backgroundArgb, RenderFn render)
{
    DxuiPopupHost::ShowParams  params;
    POINT                      topLeft  = { clientRect.left,  clientRect.top    };
    POINT                      botRight = { clientRect.right, clientRect.bottom };
    HWND                       owner    = nullptr;
    UINT                       dpi      = 0;
    HRESULT                    hr       = S_OK;



    if (m_activePopup != nullptr && EqualRect (&m_rect, &clientRect))
    {
        return;
    }

    Hide();

    if (m_popupHost == nullptr || clientRect.right <= clientRect.left || clientRect.bottom <= clientRect.top)
    {
        return;
    }

    owner         = m_popupHost->GetHwnd();
    dpi           = m_popupHost->GetScaler().GetDpi();
    dpi           = (dpi == 0) ? (UINT) DxuiDpiScaler::kBaseDpi : dpi;
    m_activePopup = m_popupHost->AcquirePopup();

    if (m_activePopup == nullptr)
    {
        return;
    }

    ClientToScreen (owner, &topLeft);
    ClientToScreen (owner, &botRight);

    m_rect   = clientRect;
    m_render = std::move (render);

    params.ownerHwnd        = owner;
    params.anchorRectScreen = { topLeft.x, topLeft.y, botRight.x, topLeft.y };
    params.placement        = DxuiPopupPlacement::Below;
    params.flipIfOffscreen  = false;
    params.dismiss          = DxuiPopupDismiss::Manual;
    params.input            = DxuiPopupInput::PassThrough;
    params.shadow           = false;
    params.squareCorners    = true;
    params.grabsCapture     = false;
    params.sizeDip.cx       = (int) std::ceil ((float) (botRight.x - topLeft.x) * (float) DxuiDpiScaler::kBaseDpi / (float) dpi);
    params.sizeDip.cy       = (int) std::ceil ((float) (botRight.y - topLeft.y) * (float) DxuiDpiScaler::kBaseDpi / (float) dpi);
    params.backgroundArgb   = backgroundArgb | 0xFF000000u;
    params.renderContent    = [this] (IDxuiPainter & p, IDxuiTextRenderer & t) { if (m_render) { m_render (p, t); } };
    params.onClosed         = [this] () { m_activePopup = nullptr; };

    hr = m_activePopup->Show (std::move (params));

    if (FAILED (hr))
    {
        Hide();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInPlaceTip::Hide
//
////////////////////////////////////////////////////////////////////////////////

void DxuiInPlaceTip::Hide()
{
    DxuiPopupHost *  popup = m_activePopup;



    //  Cleared first, so the popup's onClosed, which lands here, does nothing.
    m_activePopup = nullptr;
    m_rect        = {};

    if (popup != nullptr && m_popupHost != nullptr)
    {
        m_popupHost->ReleasePopup (popup);
    }
}