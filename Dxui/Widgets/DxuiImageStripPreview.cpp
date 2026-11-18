#include "Pch.h"

#include "DxuiImageStripPreview.h"
#include "IDxuiImageStripPreviewHost.h"
#include "Core/DxuiDpiScaler.h"
#include "Core/DxuiIconImage.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/DxuiTheme.h"
#include "Theme/IDxuiTheme.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiPopupHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStripPreview::~DxuiImageStripPreview
//
////////////////////////////////////////////////////////////////////////////////

DxuiImageStripPreview::~DxuiImageStripPreview()
{
    Hide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStripPreview::SetPopupHost
//
//  A preview open in the old host's window goes with it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStripPreview::SetPopupHost (DxuiHwndSource * popupHost)
{
    if (popupHost != m_popupHost)
    {
        Hide();
    }

    m_popupHost = popupHost;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStripPreview::SetThemeColors
//
//  The popup's card has the tooltip background, outlined in the toolbar
//  border color, both taken from the strip's last paint.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStripPreview::SetThemeColors (const IDxuiTheme & theme)
{
    m_borderArgb = theme.ButtonBorder();
    m_backArgb   = theme.TooltipBackground();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStripPreview::Sync
//
//  The preview follows the pointer in one popup, opened once and moved along
//  the strip, never closed and opened again on the way: a window hidden and
//  shown for each cell crossed flickers. The preview always shows the
//  snapshot the hovered cell shows, whichever cell the pointer came from:
//  its full-size picture, or until that is drawn, the cell's own thumbnail
//  scaled up. A sharp picture already up stays only while the cell shows the
//  same picture it was chosen for, as neighbors showing one snapshot do; a
//  cell with no picture at all has no preview.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStripPreview::Sync (
    IDxuiImageStripSource  * source,
    int                      hovered)
{
    IDxuiImageStripSource::Image  image;
    IDxuiImageStripSource::Image  thumb;
    bool                          isFull = false;
    bool                          isSame = false;



    if (hovered < 0 || source == nullptr)
    {
        Hide();
        return;
    }

    thumb  = source->GetCellImage (hovered);
    image  = source->GetPreviewImage (hovered);
    isFull = image != nullptr;
    isSame = m_popup != nullptr && thumb != nullptr && thumb == m_thumb;

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
        FollowPointer();
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
        Show();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStripPreview::Hide
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStripPreview::Hide()
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
//  DxuiImageStripPreview::Show
//
//  Beside the strip: below one lying down, to the right of one standing up,
//  flipped when there is no room, centered on the pointer. The picture's
//  pixels are its size in DIPs, so the popup scales them for the window's
//  DPI. The popup lets the pointer through and never activates, so the strip
//  keeps the hover and the window keeps the focus.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStripPreview::Show()
{
    DxuiPopupHost::ShowParams  params;
    HRESULT                    hr = S_OK;



    if (m_popupHost == nullptr || m_image == nullptr)
    {
        return;
    }

    m_popup = m_popupHost->AcquirePopup();

    if (m_popup == nullptr)
    {
        return;
    }

    params.ownerHwnd        = m_popupHost->GetHwnd();
    params.anchorRectScreen = m_host.GetPreviewAnchorFor (GetSizeDip());
    params.placement        = m_host.GetPreviewPlacement();
    params.flipIfOffscreen  = true;
    params.dismiss          = DxuiPopupDismiss::Manual;
    params.input            = DxuiPopupInput::PassThrough;
    params.shadow           = true;
    params.grabsCapture     = false;
    params.sizeDip          = GetSizeDip();
    params.backgroundArgb   = (m_backArgb != 0) ? m_backArgb : DxuiPopupHost::kDefaultMenuBackgroundArgb;
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
//  DxuiImageStripPreview::Move
//
//  The open popup redraws with its new picture where the pointer now is,
//  staying on screen throughout.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStripPreview::Move()
{
    HRESULT  hr = S_OK;



    hr = m_popup->MoveTo (m_host.GetPreviewAnchorFor (GetSizeDip()), GetSizeDip());

    if (FAILED (hr))
    {
        Hide();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStripPreview::FollowPointer
//
//  The open popup moves to stay centered on the pointer, its picture as it
//  was, so it glides along the strip rather than jumping from cell to cell.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStripPreview::FollowPointer()
{
    HRESULT  hr = S_OK;



    if (m_popup == nullptr)
    {
        return;
    }

    hr = m_popup->Reposition (m_host.GetPreviewAnchorFor (GetSizeDip()));

    if (FAILED (hr))
    {
        Hide();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStripPreview::GetSizeDip
//
//  The popup's size: the picture and the frame's padding on each side.
//
////////////////////////////////////////////////////////////////////////////////

SIZE DxuiImageStripPreview::GetSizeDip() const
{
    return SIZE { m_sizeDip.cx + 2 * kPadDip, m_sizeDip.cy + 2 * kPadDip };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStripPreview::GetRectPx
//
//  Where the popup sits on screen, empty with none up.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiImageStripPreview::GetRectPx() const
{
    return (m_popup != nullptr) ? m_popup->GetPlacedRectScreenPx() : RECT {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStripPreview::Render
//
//  Popup-local pixels: a frame as a tooltip's, the picture inset by kPadDip
//  on the popup's card, which has the tooltip background and a floating
//  pane's rounded corners. The card is outlined one pixel wide in the
//  toolbar border color, along its curve. The padding is wider than the
//  space the curve cuts from a corner, so the outline never crosses the
//  picture.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStripPreview::Render (
    IDxuiPainter      & painter,
    IDxuiTextRenderer & text)
{
    const DxuiDpiScaler  * scaler = nullptr;
    HRESULT                hr     = S_OK;
    RECT                   placed = {};
    float                  width  = 0.0f;
    float                  height = 0.0f;
    float                  wide   = 0.0f;
    float                  tall   = 0.0f;
    float                  line   = 0.0f;
    float                  pad    = 0.0f;



    if (m_image == nullptr || m_popupHost == nullptr)
    {
        return;
    }

    scaler = &m_popupHost->GetScaler();
    width  = scaler->ToPxf ((float) m_sizeDip.cx);
    height = scaler->ToPxf ((float) m_sizeDip.cy);
    pad    = std::round (scaler->ToPxf ((float) kPadDip));

    hr = text.DrawFramebuffer (m_image->bgraPremul.data(), m_image->width, m_image->height, pad, pad, width, height);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (m_borderArgb == 0 || m_popup == nullptr)
    {
        return;
    }

    placed = m_popup->GetPlacedRectScreenPx();
    wide   = (float) (placed.right  - placed.left);
    tall   = (float) (placed.bottom - placed.top);
    line   = 1.0f;

    painter.OutlineRoundedRect (0.0f, 0.0f, wide, tall, scaler->ToPxf (DxuiTheme::kOverlayCornerRadiusDip), line, m_borderArgb | 0xFF000000u);
}
