#include "Pch.h"

#include "DxuiTabDragVisual.h"
#include "Core/DxuiDpiScaler.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiPopupHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabDragVisual::~DxuiTabDragVisual
//
////////////////////////////////////////////////////////////////////////////////

DxuiTabDragVisual::~DxuiTabDragVisual()
{
    Hide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabDragVisual::Show
//
//  The card is placed below an anchor of no height at the corner, then moved
//  there exactly, since placement clamps a popup to its monitor's work area
//  and the card has to go wherever the pointer goes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiTabDragVisual::Show (
    DxuiHwndSource  & popups,
    SIZE              sizePx,
    POINT             topLeftScreen,
    uint32_t          fillArgb,
    RenderFn          render)
{
    HRESULT                    hr       = S_OK;
    DxuiPopupHost::ShowParams  params;
    UINT                       dpi      = popups.GetScaler().GetDpi();
    RECT                       placed   = {};
    DxuiPopupHost            * acquired = nullptr;



    Hide();

    dpi      = (dpi == 0) ? (UINT) DxuiDpiScaler::kBaseDpi : dpi;
    m_popups = &popups;
    m_render = std::move (render);
    m_popup  = popups.AcquirePopup();

    //  The pool can be exhausted; the drag then has no card.
    CBR (m_popup != nullptr);

    params.ownerHwnd        = popups.GetHwnd();
    params.anchorRectScreen = RECT { topLeftScreen.x, topLeftScreen.y, topLeftScreen.x + sizePx.cx, topLeftScreen.y };
    params.placement        = DxuiPopupPlacement::Below;
    params.flipIfOffscreen  = false;
    params.dismiss          = DxuiPopupDismiss::Manual;
    params.input            = DxuiPopupInput::PassThrough;
    params.grabsCapture     = false;
    params.shadow           = true;
    params.sizeDip.cx       = (LONG) std::ceil ((float) sizePx.cx * (float) DxuiDpiScaler::kBaseDpi / (float) dpi);
    params.sizeDip.cy       = (LONG) std::ceil ((float) sizePx.cy * (float) DxuiDpiScaler::kBaseDpi / (float) dpi);
    params.backgroundArgb   = fillArgb | 0xFF000000u;
    params.renderContent    = [this] (IDxuiPainter & painter, IDxuiTextRenderer & text)
    {
        RECT  card = (m_popup != nullptr) ? m_popup->GetPlacedRectScreenPx() : RECT {};

        if (m_render)
        {
            m_render (painter, text, (float) (card.right - card.left), (float) (card.bottom - card.top));
        }
    };
    params.onClosed         = [this]() { m_popup = nullptr; };

    hr = m_popup->Show (std::move (params));
    CHR (hr);

    placed = m_popup->GetPlacedRectScreenPx();
    m_at   = POINT { placed.left, placed.top };

    MoveTo (topLeftScreen);

Error:
    if (FAILED (hr) && m_popup != nullptr)
    {
        acquired = m_popup;
        m_popup  = nullptr;
        popups.ReleasePopup (acquired);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabDragVisual::MoveTo
//
//  The window moves by as much as the card has to, whatever margin its
//  shadow adds around it, and stays above every other window.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabDragVisual::MoveTo (POINT topLeftScreen)
{
    HRESULT  hr     = S_OK;
    HWND     hwnd   = (m_popup != nullptr) ? m_popup->GetHwnd() : nullptr;
    RECT     window = {};
    BOOL     found  = FALSE;
    BOOL     moved  = FALSE;



    BAIL_OUT_IF (hwnd == nullptr, S_OK);

    found = GetWindowRect (hwnd, &window);
    CWRA (found);

    moved = SetWindowPos (hwnd, HWND_TOPMOST,
                          window.left + topLeftScreen.x - m_at.x, window.top + topLeftScreen.y - m_at.y, 0, 0,
                          SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    CWRA (moved);

    m_at = topLeftScreen;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabDragVisual::GetHwnd
//
////////////////////////////////////////////////////////////////////////////////

HWND DxuiTabDragVisual::GetHwnd() const
{
    return (m_popup != nullptr) ? m_popup->GetHwnd() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabDragVisual::Hide
//
//  The popup goes back to the pool no longer topmost, since a menu is the
//  next thing to borrow it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabDragVisual::Hide()
{
    DxuiPopupHost *  popup = m_popup;
    HWND             hwnd  = (popup != nullptr) ? popup->GetHwnd() : nullptr;
    BOOL             reset = FALSE;



    //  Null first, so the popup's onClosed cannot release it twice.
    m_popup = nullptr;

    if (hwnd != nullptr)
    {
        reset = SetWindowPos (hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        IGNORE_RETURN_VALUE (reset, TRUE);
    }

    if (popup != nullptr && m_popups != nullptr)
    {
        m_popups->ReleasePopup (popup);
    }
}
