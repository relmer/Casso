#include "Pch.h"

#include "DxuiImageStrip.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiPopupHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::~DxuiImageStrip
//
////////////////////////////////////////////////////////////////////////////////

DxuiImageStrip::~DxuiImageStrip()
{
    HidePreview();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::SetPopupHost
//
//  A preview open in the old host's window goes with it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::SetPopupHost (DxuiHwndSource * host)
{
    if (host != m_popupHost)
    {
        HidePreview();
    }

    m_popupHost = host;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetCellLength
//
//  Across the strip a cell is as thick as the strip; along it, the picture's
//  width for its height lying down, or its height for its width standing up.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiImageStrip::GetCellLength (
    int    thicknessPx,
    float  aspect,
    bool   vertical)
{
    float  length = 0.0f;



    if (thicknessPx <= 0 || aspect <= 0.0f)
    {
        return 0;
    }

    length = vertical ? (float) thicknessPx / aspect : (float) thicknessPx * aspect;

    return (std::max) (1, (int) std::lround (length));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetCellCount
//
////////////////////////////////////////////////////////////////////////////////

int DxuiImageStrip::GetCellCount (
    int  lengthPx,
    int  cellPx)
{
    if (lengthPx <= 0 || cellPx <= 0)
    {
        return 0;
    }

    return (std::max) (1, (lengthPx + cellPx / 2) / cellPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetCellRect
//
//  Each cell starts where the one before it ends; the last takes whatever
//  length is left over, so the cells fill the strip.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiImageStrip::GetCellRect (
    const RECT  & rc,
    int           index,
    int           count,
    int           cellPx,
    bool          vertical)
{
    int  start = index * cellPx;
    int  end   = start + cellPx;



    if (index == count - 1)
    {
        end = vertical ? rc.bottom - rc.top : rc.right - rc.left;
    }

    if (vertical)
    {
        return RECT { rc.left, rc.top + start, rc.right, rc.top + end };
    }

    return RECT { rc.left + start, rc.top, rc.left + end, rc.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::HitTestCell
//
////////////////////////////////////////////////////////////////////////////////

int DxuiImageStrip::HitTestCell (
    const RECT  & rc,
    int           count,
    int           cellPx,
    bool          vertical,
    int           x,
    int           y)
{
    int  along = vertical ? y - rc.top : x - rc.left;
    int  index = 0;



    if (x < rc.left || x >= rc.right || y < rc.top || y >= rc.bottom || cellPx <= 0 || along < 0)
    {
        return -1;
    }

    index = (std::min) (along / cellPx, count - 1);

    return (index >= 0) ? index : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetWidthPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiImageStrip::GetWidthPx (
    bool                  labeled,
    const DxuiDpiScaler & scaler,
    IDxuiTextRenderer   * text) const
{
    UNREFERENCED_PARAMETER (labeled);
    UNREFERENCED_PARAMETER (text);

    return (m_preferredPx > 0) ? m_preferredPx : scaler.ToPx (kDefaultLengthDp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetMinWidthPx
//
//  One cell, once the strip knows how long a cell is; before that, it keeps
//  its full length.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiImageStrip::GetMinWidthPx (const DxuiDpiScaler & scaler) const
{
    UNREFERENCED_PARAMETER (scaler);

    return (m_idealPx > 0) ? m_idealPx : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::Layout
//
//  The strip stands on end when its rect is taller than it is wide. The
//  source hears the cell count and size every time, so it can plan the
//  pictures for them.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::Layout (
    const RECT          & rc,
    bool                  labeled,
    const DxuiDpiScaler & scaler)
{
    int  width     = rc.right - rc.left;
    int  height    = rc.bottom - rc.top;
    int  thickness = 0;
    int  count     = 0;



    UNREFERENCED_PARAMETER (labeled);

    m_scaler   = scaler;
    m_rc       = rc;
    m_vertical = height > width;
    thickness  = (std::max) (kMinThicknessPx, m_vertical ? width : height);
    m_idealPx  = GetCellLength (thickness, m_aspect, m_vertical);
    count      = GetCellCount (m_vertical ? height : width, m_idealPx);
    m_cellPx   = (count > 0) ? (m_vertical ? height : width) / count : 0;

    if (count != m_count)
    {
        m_hovered = -1;
        HidePreview();
    }

    m_count = count;

    if (m_source != nullptr)
    {
        m_source->SetCellLayout (m_count, m_vertical ? SIZE { thickness, m_cellPx } : SIZE { m_cellPx, thickness });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::Paint
//
//  A cell not drawn yet is the content background. The cell under the
//  pointer is framed by drawing its picture inset over the accent color, so
//  the frame does not depend on whether pictures draw over shapes or under
//  them. The cell the source marks gives up a band along its far edge, below
//  a strip lying down and right of one standing up, to a bar in the accent
//  color, for the same reason.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::Paint (
    IDxuiPainter      & painter,
    IDxuiTextRenderer & text,
    const IDxuiTheme  & theme,
    bool                hovered,
    bool                pressed,
    bool                labeled)
{
    float                         edge  = m_scaler.ToPxf (kHoverEdgeDip);
    float                         inset = 0.0f;
    RECT                          cell  = {};
    IDxuiImageStripSource::Image  image;
    HRESULT                       hr    = S_OK;
    int                           mark  = (m_source != nullptr) ? m_source->GetMarkedCell() : -1;
    LONG                          bar   = (LONG) std::lround (m_scaler.ToPxf (kMarkerDip));



    UNREFERENCED_PARAMETER (hovered);
    UNREFERENCED_PARAMETER (pressed);
    UNREFERENCED_PARAMETER (labeled);

    for (int i = 0; i < m_count; i++)
    {
        cell  = GetCellRect (m_rc, i, m_count, m_cellPx, m_vertical);
        image = (m_source != nullptr) ? m_source->GetCellImage (i) : nullptr;
        inset = (i == m_hovered) ? edge : 0.0f;

        //  The bar takes its band out of the cell, so the picture cannot cover it.
        if (i == mark && m_vertical)
        {
            cell.right -= bar;
            painter.FillRect ((float) cell.right, (float) cell.top, (float) bar, (float) (cell.bottom - cell.top), theme.Accent());
        }
        else if (i == mark)
        {
            cell.bottom -= bar;
            painter.FillRect ((float) cell.left, (float) cell.bottom, (float) (cell.right - cell.left), (float) bar, theme.Accent());
        }

        if (i == m_hovered)
        {
            painter.FillRect ((float) cell.left, (float) cell.top, (float) (cell.right - cell.left), (float) (cell.bottom - cell.top), theme.Accent());
        }

        if (image == nullptr || image->width <= 0 || image->height <= 0)
        {
            painter.FillRect ((float) cell.left + inset,
                              (float) cell.top + inset,
                              (float) (cell.right - cell.left) - inset * 2.0f,
                              (float) (cell.bottom - cell.top) - inset * 2.0f,
                              theme.ContentBackground());
            continue;
        }

        hr = text.DrawIconBitmap (image->bgraPremul.data(), image->width, image->height,
                                  (float) cell.left + inset,
                                  (float) cell.top + inset,
                                  (float) (cell.right - cell.left) - inset * 2.0f,
                                  (float) (cell.bottom - cell.top) - inset * 2.0f);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetTooltipAt
//
//  The preview takes the place of a tip.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DxuiImageStrip::GetTooltipAt (
    int     x,
    int     y,
    RECT  & anchor) const
{
    UNREFERENCED_PARAMETER (x);
    UNREFERENCED_PARAMETER (y);
    UNREFERENCED_PARAMETER (anchor);

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::OnClick
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiImageStrip::OnClick (
    int  x,
    int  y)
{
    int  index = HitTestCell (m_rc, m_count, m_cellPx, m_vertical, x, y);



    if (index < 0 || m_source == nullptr)
    {
        return false;
    }

    m_source->OnCellClicked (index);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiImageStrip::OnMouseMove (
    int  x,
    int  y)
{
    int  index = HitTestCell (m_rc, m_count, m_cellPx, m_vertical, x, y);



    if (index != m_hovered)
    {
        m_hovered = index;
        Sync();
    }

    return index >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::OnLButtonDown
//
//  A press on a cell arms the entry for the click that follows; the toolbar
//  arms a labeled custom entry only through a part that takes the press.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiImageStrip::OnLButtonDown (
    int  x,
    int  y)
{
    return HitTestCell (m_rc, m_count, m_cellPx, m_vertical, x, y) >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::OnMouseLeave
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::OnMouseLeave()
{
    m_hovered = -1;
    HidePreview();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::Sync
//
//  The preview follows the cell under the pointer in one popup, opened once
//  and moved from cell to cell, never closed and opened again on the way: a
//  window hidden and shown for each cell crossed flickers. A cell whose
//  full-size picture is not drawn yet shows its thumbnail scaled up until it
//  is, and a picture already up stays until its replacement is ready.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::Sync()
{
    IDxuiImageStripSource::Image  image;
    bool                          isFull = true;



    if (m_hovered < 0 || m_source == nullptr)
    {
        HidePreview();
        return;
    }

    image = m_source->GetPreviewImage (m_hovered);

    if (image == nullptr)
    {
        // What is up for this cell stays until its full-size picture arrives.
        if (m_preview != nullptr && m_previewCell == m_hovered)
        {
            return;
        }

        image  = m_source->GetCellImage (m_hovered);
        isFull = false;
    }

    // No picture at all, or no size to scale a thumbnail to yet: keep what is up.
    if (image == nullptr || (!isFull && m_previewDip.cx <= 0))
    {
        return;
    }

    if (m_preview != nullptr && image == m_previewImage && m_previewCell == m_hovered)
    {
        return;
    }

    m_previewImage = std::move (image);
    m_previewCell  = m_hovered;

    if (isFull)
    {
        m_previewDip = SIZE { m_previewImage->width, m_previewImage->height };
    }

    if (m_preview != nullptr)
    {
        MovePreview();
    }
    else
    {
        ShowPreview();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::HidePreview
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::HidePreview()
{
    DxuiPopupHost  * popup = m_preview;



    // Cleared first, so the popup's close callback finds nothing to clear.
    m_preview      = nullptr;
    m_previewImage = nullptr;
    m_previewCell  = -1;

    if (popup != nullptr && m_popupHost != nullptr)
    {
        m_popupHost->ReleasePopup (popup);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::ShowPreview
//
//  Beside the cell: below a strip lying down, to the right of one standing
//  up, flipped when there is no room. The picture's pixels are its size in
//  DIPs, so the popup scales them for the window's DPI. The popup lets the
//  pointer through and never activates, so the strip keeps the hover and the
//  window keeps the focus.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::ShowPreview()
{
    DxuiPopupHost::ShowParams  params;
    HRESULT                    hr = S_OK;



    if (m_popupHost == nullptr || m_previewImage == nullptr)
    {
        return;
    }

    m_preview = m_popupHost->AcquirePopup();

    if (m_preview == nullptr)
    {
        return;
    }

    params.ownerHwnd        = m_popupHost->GetHwnd();
    params.anchorRectScreen = GetPreviewAnchor();
    params.placement        = m_vertical ? DxuiPopupPlacement::Right : DxuiPopupPlacement::Below;
    params.flipIfOffscreen  = true;
    params.dismiss          = DxuiPopupDismiss::Manual;
    params.input            = DxuiPopupInput::PassThrough;
    params.shadow           = true;
    params.grabsCapture     = false;
    params.sizeDip          = m_previewDip;
    params.backgroundArgb   = DxuiPopupHost::kDefaultMenuBackgroundArgb;
    params.renderContent    = [this] (IDxuiPainter & painter, IDxuiTextRenderer & text) { RenderPreview (painter, text); };
    params.onClosed         = [this] () { m_preview = nullptr; };

    hr = m_preview->Show (std::move (params));

    if (FAILED (hr))
    {
        HidePreview();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::MovePreview
//
//  The open popup moves beside the cell now under the pointer and redraws
//  with its picture, staying on screen throughout.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::MovePreview()
{
    HRESULT  hr = S_OK;



    hr = m_preview->MoveTo (GetPreviewAnchor(), m_previewDip);

    if (FAILED (hr))
    {
        HidePreview();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetPreviewAnchor
//
//  The previewed cell, in screen pixels.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiImageStrip::GetPreviewAnchor() const
{
    RECT   cell     = GetCellRect (m_rc, m_previewCell, m_count, m_cellPx, m_vertical);
    POINT  topLeft  = { cell.left, cell.top };
    POINT  botRight = { cell.right, cell.bottom };
    HWND   owner    = (m_popupHost != nullptr) ? m_popupHost->GetHwnd() : nullptr;



    // No window, as in tests: the cell's client pixels stand in for the screen.
    if (owner != nullptr)
    {
        ClientToScreen (owner, &topLeft);
        ClientToScreen (owner, &botRight);
    }

    return RECT { topLeft.x, topLeft.y, botRight.x, botRight.y };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::RenderPreview
//
//  Popup-local pixels: the picture fills the popup.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::RenderPreview (
    IDxuiPainter      & painter,
    IDxuiTextRenderer & text)
{
    const DxuiDpiScaler  * scaler = nullptr;
    HRESULT                hr     = S_OK;



    UNREFERENCED_PARAMETER (painter);

    if (m_previewImage == nullptr || m_popupHost == nullptr)
    {
        return;
    }

    scaler = &m_popupHost->GetScaler();

    hr = text.DrawFramebuffer (m_previewImage->bgraPremul.data(), m_previewImage->width, m_previewImage->height,
                               0.0f, 0.0f,
                               scaler->ToPxf ((float) m_previewDip.cx),
                               scaler->ToPxf ((float) m_previewDip.cy));
    IGNORE_RETURN_VALUE (hr, S_OK);
}
