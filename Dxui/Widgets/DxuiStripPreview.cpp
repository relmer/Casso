#include "Pch.h"

#include "DxuiStripPreview.h"
#include "Core/DxuiIconImage.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiPopupHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripPreview::~DxuiStripPreview
//
////////////////////////////////////////////////////////////////////////////////

DxuiStripPreview::~DxuiStripPreview()
{
    Hide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripPreview::SetPopupHost
//
//  A preview open in the old host's window goes with it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripPreview::SetPopupHost (DxuiHwndSource * host)
{
    if (host != m_popupHost)
    {
        Hide();
    }

    m_popupHost = host;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripPreview::Choose
//
//  The preview follows the pointer in one popup, opened once and moved along
//  as the pointer crosses cells and redrawn with each new picture; a popup
//  shown for each cell crossed flickers. The preview always shows the
//  snapshot the hovered cell shows, whichever cell the pointer came from:
//  its full-size picture, or until that is drawn, the cell's thumbnail
//  scaled up. A sharp picture already up stays only while the cell shows the
//  same picture it was chosen for, as neighbors showing one snapshot do; a
//  cell with no picture at all has no preview.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripPreview::Choose (
    Image  full,
    Image  thumb)
{
    Image  image  = std::move (full);
    bool   isFull = image != nullptr;
    bool   isSame = m_popup != nullptr && thumb != nullptr && thumb == m_thumb;



    if (!isFull && !isSame && m_sizeDip.cx > 0)
    {
        image = thumb;
    }

    //  Nothing of the cell's own to show: another cell's picture is no
    //  preview of this one.
    if (image == nullptr && !isSame)
    {
        Hide();
        return;
    }

    //  Nothing new to show: what is up stays, and keeps up with the pointer.
    if (image == nullptr || (m_popup != nullptr && image == m_image))
    {
        m_thumb = thumb;
        Follow();
        return;
    }

    m_image = std::move (image);
    m_thumb = thumb;

    if (isFull)
    {
        m_sizeDip = SIZE { m_image->width, m_image->height };
    }

    if (m_popup != nullptr)
    {
        Move();
    }
    else
    {
        Open();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripPreview::Hide
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripPreview::Hide()
{
    DxuiPopupHost  * popup = m_popup;



    // Cleared first, so the popup's close callback finds nothing to clear.
    m_popup = nullptr;
    m_image = nullptr;
    m_thumb = nullptr;

    if (popup != nullptr && m_popupHost != nullptr)
    {
        m_popupHost->ReleasePopup (popup);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripPreview::Open
//
//  The picture's pixels are its size in DIPs, so the popup scales them for
//  the window's DPI.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripPreview::Open()
{
    DxuiPopupHost::ShowParams  params;
    HRESULT                    hr = S_OK;



    if (m_popupHost == nullptr || m_image == nullptr || !m_anchor)
    {
        return;
    }

    m_popup = m_popupHost->AcquirePopup();

    if (m_popup == nullptr)
    {
        return;
    }

    params.ownerHwnd        = m_popupHost->GetHwnd();
    params.anchorRectScreen = m_anchor (m_sizeDip);
    params.placement        = m_isVertical ? DxuiPopupPlacement::Right : DxuiPopupPlacement::Below;
    params.flipIfOffscreen  = true;
    params.dismiss          = DxuiPopupDismiss::Manual;
    params.input            = DxuiPopupInput::PassThrough;
    params.shadow           = true;
    params.grabsCapture     = false;
    params.sizeDip          = m_sizeDip;
    params.backgroundArgb   = DxuiPopupHost::kDefaultMenuBackgroundArgb;
    params.renderContent    = [this] (IDxuiPainter & painter, IDxuiTextRenderer & text) { Render (painter, text); };
    params.onClosed         = [this] () { m_popup = nullptr; };

    hr = m_popup->Show (std::move (params));

    if (FAILED (hr))
    {
        Hide();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripPreview::Move
//
//  The open popup redraws with its new picture where the pointer now is,
//  staying on screen throughout.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripPreview::Move()
{
    HRESULT  hr = S_OK;



    hr = m_popup->MoveTo (m_anchor (m_sizeDip), m_sizeDip);

    if (FAILED (hr))
    {
        Hide();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripPreview::Follow
//
//  The popup stays centered on the pointer, so it glides along the strip
//  rather than jumping from cell to cell.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripPreview::Follow()
{
    HRESULT  hr = S_OK;



    if (m_popup == nullptr || !m_anchor)
    {
        return;
    }

    hr = m_popup->Reposition (m_anchor (m_sizeDip));

    if (FAILED (hr))
    {
        Hide();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripPreview::GetRectPx
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiStripPreview::GetRectPx() const
{
    return (m_popup != nullptr) ? m_popup->GetPlacedRectScreenPx() : RECT {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStripPreview::Render
//
//  Popup-local pixels: the picture fills the popup.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiStripPreview::Render (
    IDxuiPainter      & painter,
    IDxuiTextRenderer & text)
{
    const DxuiDpiScaler  * scaler = nullptr;
    HRESULT                hr     = S_OK;



    UNREFERENCED_PARAMETER (painter);

    if (m_image == nullptr || m_popupHost == nullptr)
    {
        return;
    }

    scaler = &m_popupHost->GetScaler();

    hr = text.DrawFramebuffer (m_image->bgraPremul.data(), m_image->width, m_image->height,
                               0.0f, 0.0f,
                               scaler->ToPxf ((float) m_sizeDip.cx),
                               scaler->ToPxf ((float) m_sizeDip.cy));
    IGNORE_RETURN_VALUE (hr, S_OK);
}
