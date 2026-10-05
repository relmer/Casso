#include "Pch.h"

#include "DxuiImageStrip.h"
#include "Theme/DxuiTheme.h"
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
    int   width     = rc.right - rc.left;
    int   height    = rc.bottom - rc.top;
    int   thickness = 0;
    int   count     = 0;
    LONG  room      = 0;
    LONG  lead      = 0;
    LONG  trail     = 0;



    UNREFERENCED_PARAMETER (labeled);

    m_scaler   = scaler;
    m_outer    = rc;
    m_rc       = rc;
    m_vertical = height > width;
    m_lead     = RECT {};
    m_trail    = RECT {};

    //  Lying down, the pictures keep out of the label room above and below,
    //  and the end labels sit beside them, each centered on the pictures.
    if (!m_vertical && m_labelRoomDp > 0.0f)
    {
        room         = (LONG) std::lround (scaler.ToPxf (m_labelRoomDp));
        room         = std::min (room, (LONG) ((height - kMinThicknessPx) / 2));
        m_rc.top    += room;
        m_rc.bottom -= room;
        height       = m_rc.bottom - m_rc.top;

        if (m_source != nullptr)
        {
            lead  = GetSideLabelPx (m_source->GetLeadingLabel(),  false);
            trail = GetSideLabelPx (m_source->GetTrailingLabel(), true);
        }

        //  The pictures keep at least half the strip.
        if (lead + trail < width / 2)
        {
            m_lead       = RECT { m_rc.left, m_rc.top, m_rc.left + lead, m_rc.bottom };
            m_trail      = RECT { m_rc.right - trail, m_rc.top, m_rc.right, m_rc.bottom };
            m_rc.left   += lead;
            m_rc.right  -= trail;
            width        = m_rc.right - m_rc.left;
        }
    }

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
    float                         line  = 0.0f;
    bool                          isOn  = false;
    std::wstring                  top;
    std::wstring                  bottom;
    std::wstring                  lead;
    std::wstring                  trail;



    UNREFERENCED_PARAMETER (hovered);
    UNREFERENCED_PARAMETER (pressed);
    UNREFERENCED_PARAMETER (labeled);

    //  The playhead line takes the place of the marked cell's bar; while it
    //  is dragged it follows the pointer rather than the source.
    isOn  = m_source != nullptr && m_source->TryGetPlayhead (line, top, bottom);
    mark  = isOn ? -1 : mark;
    lead  = (m_source != nullptr && m_lead.right  > m_lead.left)  ? m_source->GetLeadingLabel()  : std::wstring();
    trail = (m_source != nullptr && m_trail.right > m_trail.left) ? m_source->GetTrailingLabel() : std::wstring();

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

    PaintSideLabel (painter, text, theme, lead,  m_lead,  false);
    PaintSideLabel (painter, text, theme, trail, m_trail, true);

    if (isOn)
    {
        PaintPlayhead (painter, text, theme, m_isDragging ? m_dragOffset : line, top, bottom);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetTooltipAt
//
//  The preview takes the place of a tip over the pictures; the trailing
//  label has the source's tip.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DxuiImageStrip::GetTooltipAt (
    int     x,
    int     y,
    RECT  & anchor) const
{
    POINT  pt = { x, y };



    if (m_source == nullptr || !PtInRect (&m_trail, pt))
    {
        return nullptr;
    }

    m_tip  = m_source->GetTrailingTip();
    anchor = m_trail;

    return m_tip.empty() ? nullptr : m_tip.c_str();
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
    int    index = HitTestCell (m_rc, m_count, m_cellPx, m_vertical, x, y);
    POINT  pt    = { x, y };



    //  The release that ended a drag of the playhead line is not a click.
    if (m_isClickEaten)
    {
        m_isClickEaten = false;
        return true;
    }

    if (m_source != nullptr && PtInRect (&m_trail, pt))
    {
        m_source->OnTrailingLabelClicked();
        return true;
    }

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



    if (m_isDragging)
    {
        m_dragOffset = GetOffsetAt (x, y);

        if (m_source != nullptr)
        {
            m_source->OnPlayheadDragged (m_dragOffset, false);
        }

        return true;
    }

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
    float         line   = 0.0f;
    std::wstring  top;
    std::wstring  bottom;
    bool          isOn   = m_source != nullptr && m_source->TryGetPlayhead (line, top, bottom);
    POINT         pt     = { x, y };



    m_isClickEaten = false;

    if (PtInRect (&m_trail, pt))
    {
        return true;
    }

    //  A press within reach of the playhead line takes hold of it, and the
    //  preview gives way to the drag.
    if (isOn && IsOnPlayhead (m_vertical ? y : x, GetLineAlong (line), m_scaler.ToPxf (kPlayheadGripDip)))
    {
        m_isDragging = true;
        m_dragOffset = line;
        m_hovered    = -1;
        HidePreview();
        return true;
    }

    return HitTestCell (m_rc, m_count, m_cellPx, m_vertical, x, y) >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::OnMouseLeave
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::OnMouseLeave()
{
    //  A drag taken off the strip ends where it last was.
    if (m_isDragging)
    {
        m_isDragging = false;

        if (m_source != nullptr)
        {
            m_source->OnPlayheadDragged (m_dragOffset, true);
        }
    }

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





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::IsOnPlayhead
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiImageStrip::IsOnPlayhead (
    int    along,
    float  lineAlong,
    float  gripPx)
{
    return std::fabs ((float) along - lineAlong) <= gripPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetLineAlong
//
//  Where an offset in cells lies along the strip, in pixels.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiImageStrip::GetLineAlong (float offset) const
{
    float  start = (float) (m_vertical ? m_rc.top : m_rc.left);



    return start + offset * (float) m_cellPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetOffsetAt
//
//  The offset in cells of a point along the strip, within the strip.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiImageStrip::GetOffsetAt (
    int  x,
    int  y) const
{
    float  start = (float) (m_vertical ? m_rc.top : m_rc.left);
    float  along = (float) (m_vertical ? y : x);



    if (m_cellPx <= 0)
    {
        return 0.0f;
    }

    return std::clamp ((along - start) / (float) m_cellPx, 0.0f, (float) m_count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::PaintPlayhead
//
//  A line in the accent color across the pictures, and lying down, its
//  labels in the room above and below them, each kept within the strip.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::PaintPlayhead (
    IDxuiPainter        & painter,
    IDxuiTextRenderer   & text,
    const IDxuiTheme    & theme,
    float                 offset,
    const std::wstring  & top,
    const std::wstring  & bottom)
{
    constexpr uint32_t  kOpaque = 0xFF000000u;
    HRESULT             hr      = S_OK;
    float               thick   = std::max (1.0f, m_scaler.ToPxf (kPlayheadDip));
    float               along   = GetLineAlong (offset);
    float               room    = (float) (m_rc.top - m_outer.top);
    uint32_t            ink     = theme.Accent() | kOpaque;



    //  The line is drawn as a picture is, one pixel stretched, since pictures
    //  draw over shapes and the line must cross them.
    if (m_vertical)
    {
        hr = text.DrawIconBitmap (&ink, 1, 1, (float) m_outer.left, along - thick / 2.0f, (float) (m_outer.right - m_outer.left), thick);
        IGNORE_RETURN_VALUE (hr, S_OK);
        return;
    }

    hr = text.DrawIconBitmap (&ink, 1, 1, along - thick / 2.0f, (float) m_rc.top, thick, (float) (m_rc.bottom - m_rc.top));
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (room <= 0.0f)
    {
        return;
    }

    PaintLabel (painter, text, theme, top,    along, (float) m_outer.top);
    PaintLabel (painter, text, theme, bottom, along, (float) m_rc.bottom);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::PaintLabel
//
//  One line of text in the label room starting at top, centered on centerX,
//  on a plate of the background so the line and the pictures do not show
//  through.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::PaintLabel (
    IDxuiPainter        & painter,
    IDxuiTextRenderer   & text,
    const IDxuiTheme    & theme,
    const std::wstring  & label,
    float                 centerX,
    float                 top)
{
    HRESULT  hr     = S_OK;
    float    size   = m_scaler.ToPxf (kLabelFontDip);
    float    pad    = m_scaler.ToPxf (kLabelPadDip);
    float    room   = (float) (m_rc.top - m_outer.top);
    float    width  = 0.0f;
    float    height = 0.0f;
    float    left   = 0.0f;



    if (label.empty())
    {
        return;
    }

    hr = text.MeasureString (label.c_str(), size, DxuiTheme::kBodyFace, width, height);
    IGNORE_RETURN_VALUE (hr, S_OK);

    width += pad * 2.0f;
    left   = centerX - width / 2.0f;
    left   = std::clamp (left, (float) m_outer.left, std::max ((float) m_outer.left, (float) m_outer.right - width));

    painter.FillRect (left, top, width, room, theme.Background());

    hr = text.DrawString (label.c_str(), left, top, width, room, theme.Foreground(), size, DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::OnLButtonUp
//
//  Letting go of the playhead line ends its drag where it was let go, and
//  the click the toolbar may send for the same release is eaten.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::OnLButtonUp (
    int  x,
    int  y)
{
    if (!m_isDragging)
    {
        return;
    }

    m_isDragging   = false;
    m_isClickEaten = true;
    m_dragOffset   = GetOffsetAt (x, y);

    if (m_source != nullptr)
    {
        m_source->OnPlayheadDragged (m_dragOffset, true);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetSideLabelPx
//
//  The room a label beside the pictures takes, padded either side; the
//  trailing label keeps room for its dot whether or not it shows, so the
//  strip does not shift when it comes and goes.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiImageStrip::GetSideLabelPx (
    const std::wstring  & label,
    bool                  isTrailing) const
{
    HRESULT  hr     = S_OK;
    float    width  = 0.0f;
    float    height = 0.0f;
    float    pad    = m_scaler.ToPxf (kSidePadDip);



    if (label.empty() || m_text == nullptr)
    {
        return 0;
    }

    hr = m_text->MeasureString (label.c_str(), m_scaler.ToPxf (kLabelFontDip), DxuiTheme::kBodyFace, width, height);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (isTrailing)
    {
        width += m_scaler.ToPxf (kLiveDotDip) + pad;
    }

    return (int) std::ceil (width + pad * 2.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::HasOutgrownLabelRoom
//
//  Only growth counts: a label that shrinks, as a time of day does from one
//  second to the next, keeps its room rather than moving the pictures.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiImageStrip::HasOutgrownLabelRoom()
{
    int  lead  = 0;
    int  trail = 0;



    if (m_source == nullptr || m_vertical || m_labelRoomDp <= 0.0f)
    {
        return false;
    }

    lead  = GetSideLabelPx (m_source->GetLeadingLabel(),  false);
    trail = GetSideLabelPx (m_source->GetTrailingLabel(), true);

    return lead > m_lead.right - m_lead.left || trail > m_trail.right - m_trail.left;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::PaintSideLabel
//
//  A plain line of text in its room beside the pictures, centered on them
//  top to bottom. The trailing label, while the source accents it, is in the
//  accent color behind a dot of it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::PaintSideLabel (
    IDxuiPainter        & painter,
    IDxuiTextRenderer   & text,
    const IDxuiTheme    & theme,
    const std::wstring  & label,
    const RECT          & rc,
    bool                  isTrailing)
{
    HRESULT   hr       = S_OK;
    float     pad      = m_scaler.ToPxf (kSidePadDip);
    float     dot      = m_scaler.ToPxf (kLiveDotDip);
    float     left     = (float) rc.left + pad;
    float     width    = (float) (rc.right - rc.left) - pad * 2.0f;
    bool      isAccent = isTrailing && m_source != nullptr && m_source->IsTrailingLabelAccented();
    uint32_t  ink      = isAccent ? theme.Accent() : theme.Foreground();



    if (label.empty() || width <= 0.0f)
    {
        return;
    }

    if (isTrailing)
    {
        if (isAccent)
        {
            painter.FillCircle (left + dot / 2.0f, (float) (rc.top + rc.bottom) / 2.0f, dot / 2.0f, ink);
        }

        left  += dot + pad;
        width -= dot + pad;
    }

    hr = text.DrawString (label.c_str(), left, (float) rc.top, width, (float) (rc.bottom - rc.top), ink,
                          m_scaler.ToPxf (kLabelFontDip), DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





