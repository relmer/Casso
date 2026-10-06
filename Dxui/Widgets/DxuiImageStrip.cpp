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
    else if (m_labelRoomDp > 0.0f)
    {
        PlaceStandingEndLabels();
        height = m_rc.bottom - m_rc.top;
    }

    thickness  = (std::max) (kMinThicknessPx, m_vertical ? width : height);
    m_idealPx  = GetCellLength (thickness, m_aspect, m_vertical);
    count      = GetCellCount (m_vertical ? height : width, m_idealPx);
    m_cellPx   = (count > 0) ? (m_vertical ? height : width) / count : 0;

    if (count != m_count)
    {
        SetHovered (-1);
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
//  A cell not drawn yet is the content background. The cell the source
//  marks gives up a band along the far edge, below a strip lying down and
//  right of one standing up, to a bar in the accent color, so the bar does
//  not depend on whether pictures draw over shapes or under them. The cell
//  under the pointer is framed in the theme's hover frame color instead, a
//  thin line rather than an accent one, so it cannot be mistaken for the
//  playhead line beside it, and its labels take the place of the line's
//  where the two would overlap. An end label under the pointer or pressed
//  has the toolbar's button chrome behind it.
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
    RECT                          cell      = {};
    RECT                          hoverCell = {};
    IDxuiImageStripSource::Image  image;
    HRESULT                       hr        = S_OK;
    int                           mark      = (m_source != nullptr) ? m_source->GetMarkedCell() : -1;
    LONG                          bar       = (LONG) std::lround (m_scaler.ToPxf (kMarkerDip));
    float                         line      = 0.0f;
    float                         hoverX    = 0.0f;
    bool                          isOn      = false;
    bool                          isBar     = false;
    bool                          isLabeled = false;
    std::wstring                  top;
    std::wstring                  bottom;
    std::wstring                  lead;
    std::wstring                  trail;
    std::wstring                  cellTop;
    std::wstring                  cellBottom;
    std::wstring                  dragTop;
    std::wstring                  dragBottom;



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
        isBar = i == mark;

        //  The bar takes its band out of the cell, so the picture cannot cover it.
        if (isBar && m_vertical)
        {
            cell.right -= bar;
            painter.FillRect ((float) cell.right, (float) cell.top, (float) bar, (float) (cell.bottom - cell.top), theme.Accent());
        }
        else if (isBar)
        {
            cell.bottom -= bar;
            painter.FillRect ((float) cell.left, (float) cell.bottom, (float) (cell.right - cell.left), (float) bar, theme.Accent());
        }

        if (image == nullptr || image->width <= 0 || image->height <= 0)
        {
            painter.FillRect ((float) cell.left, (float) cell.top, (float) (cell.right - cell.left), (float) (cell.bottom - cell.top), theme.ContentBackground());
            continue;
        }

        hr = text.DrawIconBitmap (image->bgraPremul.data(), image->width, image->height,
                                  (float) cell.left,
                                  (float) cell.top,
                                  (float) (cell.right - cell.left),
                                  (float) (cell.bottom - cell.top));
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    //  The hovered cell is framed, and its labels are the source's for the
    //  point under the pointer, centered on it.
    if (m_hovered >= 0 && m_hovered < m_count)
    {
        hoverCell = GetCellRect (m_rc, m_hovered, m_count, m_cellPx, m_vertical);
        hoverX    = (float) m_pointer.x;
        isLabeled = !m_vertical && m_source != nullptr && m_source->TryGetLabelsAt (m_hovered, GetOffsetAt (m_pointer.x, m_pointer.y), cellTop, cellBottom);

        PaintHoverFrame (text, theme, hoverCell);
    }

    PaintPartChrome (painter, theme, m_lead,  Part::Leading);
    PaintPartChrome (painter, theme, m_trail, Part::Trailing);

    PaintSideLabel (painter, text, theme, lead,  m_lead,  false);
    PaintSideLabel (painter, text, theme, trail, m_trail, true);

    //  A line being dragged stands under the pointer, labeled for the point
    //  there where the source has labels for it.
    if (isOn && m_isDragging)
    {
        line = m_dragOffset;

        if (m_source->TryGetLabelsAt (GetCellAtOffset (line), line, dragTop, dragBottom))
        {
            top.swap    (dragTop);
            bottom.swap (dragBottom);
        }
    }

    if (isOn)
    {
        if (isLabeled)
        {
            HideOverlap (text, top,    GetLineAlong (line), cellTop,    hoverX);
            HideOverlap (text, bottom, GetLineAlong (line), cellBottom, hoverX);
        }

        PaintPlayhead (painter, text, theme, line, top, bottom);
    }

    if (isLabeled)
    {
        PaintLabelPair (painter, text, theme, cellTop, cellBottom, hoverX);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetTooltipAt
//
//  The preview takes the place of a tip over the pictures; each end label
//  has the source's tip for it.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DxuiImageStrip::GetTooltipAt (
    int     x,
    int     y,
    RECT  & anchor) const
{
    Part  part = HitTestPart (x, y);



    if (m_source == nullptr || part == Part::None)
    {
        return nullptr;
    }

    m_tip  = (part == Part::Leading) ? m_source->GetLeadingTip() : m_source->GetTrailingTip();
    anchor = (part == Part::Leading) ? m_lead : m_trail;

    return m_tip.empty() ? nullptr : m_tip.c_str();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::HitTestPart
//
//  Which end label a point is over, if either.
//
////////////////////////////////////////////////////////////////////////////////

DxuiImageStrip::Part DxuiImageStrip::HitTestPart (
    int  x,
    int  y) const
{
    POINT  pt = { x, y };



    if (PtInRect (&m_lead, pt))
    {
        return Part::Leading;
    }

    return PtInRect (&m_trail, pt) ? Part::Trailing : Part::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::IsPartClickable
//
//  Whether an end label acts as a button now. The trailing one is a button
//  only while its source says so, as Live is only while replaying; the
//  leading one always is.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiImageStrip::IsPartClickable (Part part) const
{
    if (part == Part::None || m_source == nullptr)
    {
        return false;
    }

    return part == Part::Leading || m_source->IsTrailingLabelClickable();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::PaintPartChrome
//
//  An end label under the pointer, or pressed while the pointer is still
//  over it, takes the chrome the toolbar draws behind a button: a rounded
//  fill in the theme's hover or pressed color inside its border color. A
//  label that is not a button now takes none.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::PaintPartChrome (
    IDxuiPainter       & painter,
    const IDxuiTheme   & theme,
    const RECT         & rc,
    Part                 part)
{
    float     radius    = m_scaler.ToPxf (DxuiTheme::kCornerRadiusDip);
    float     left      = (float) rc.left;
    float     top       = (float) rc.top;
    float     width     = (float) (rc.right - rc.left);
    float     height    = (float) (rc.bottom - rc.top);
    bool      isHovered = m_hoverPart == part;
    bool      isPressed = isHovered && m_pressPart == part;
    uint32_t  fill      = isPressed ? theme.ButtonPressed() : theme.ButtonHover();



    if (!isHovered || !IsPartClickable (part) || width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    painter.FillRoundedRect    (left, top, width, height, radius, fill);
    painter.OutlineRoundedRect (left, top, width, height, radius, 1.0f, theme.ButtonBorder());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::OnClick
//
//  An end label acts only for a press and release both on it. A click on
//  the pictures goes to the source with the point under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiImageStrip::OnClick (
    int  x,
    int  y)
{
    int   index = HitTestCell (m_rc, m_count, m_cellPx, m_vertical, x, y);
    Part  part  = HitTestPart (x, y);
    Part  armed = m_armedPart;



    m_armedPart = Part::None;

    //  The release that ended a drag of the playhead line is not a click.
    if (m_isClickEaten)
    {
        m_isClickEaten = false;
        return true;
    }

    if (part != Part::None)
    {
        if (m_source != nullptr && part == armed && part == Part::Leading)
        {
            m_source->OnLeadingLabelClicked();
        }
        else if (m_source != nullptr && part == armed)
        {
            m_source->OnTrailingLabelClicked();
        }

        return true;
    }

    if (index < 0 || m_source == nullptr)
    {
        return false;
    }

    m_source->OnStripClicked (index, GetOffsetAt (x, y));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::OnMouseMove
//
//  A move onto another cell brings up its preview; a move within the cell
//  carries the preview along with the pointer.
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

    m_pointer   = POINT { x, y };
    m_hoverPart = HitTestPart (x, y);

    if (index != m_hovered)
    {
        SetHovered (index);
        Sync();
    }
    else
    {
        FollowPointer();
    }

    return index >= 0 || m_hoverPart != Part::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::SetHovered
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::SetHovered (int index)
{
    m_hovered = index;

    if (m_source != nullptr)
    {
        m_source->SetHoveredCell (index);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::OnLButtonDown
//
//  A press on a cell or an end label arms the entry for the click that
//  follows; the toolbar arms a labeled custom entry only through a part that
//  takes the press. A pressed end label shows pressed until the release. A
//  press on an end label that is not a button now is taken and goes no
//  further.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiImageStrip::OnLButtonDown (
    int  x,
    int  y)
{
    float  line   = 0.0f;
    Part   part   = HitTestPart (x, y);
    Part   button = IsPartClickable (part) ? part : Part::None;



    m_isClickEaten = false;
    m_pressPart    = button;
    m_armedPart    = button;

    if (part != Part::None)
    {
        m_hoverPart = part;
        return true;
    }

    //  A press within reach of the playhead line takes hold of it, and the
    //  preview gives way to the drag.
    if (IsOverPlayhead (x, y, line))
    {
        m_isDragging = true;
        m_dragOffset = line;
        SetHovered (-1);
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
    m_hoverPart = Part::None;
    m_pressPart = Part::None;

    //  A drag taken off the strip ends where it last was.
    if (m_isDragging)
    {
        m_isDragging = false;

        if (m_source != nullptr)
        {
            m_source->OnPlayheadDragged (m_dragOffset, true);
        }
    }

    SetHovered (-1);
    HidePreview();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::Sync
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

void DxuiImageStrip::Sync()
{
    IDxuiImageStripSource::Image  image;
    IDxuiImageStripSource::Image  thumb;
    bool                          isFull = false;
    bool                          isSame = false;



    if (m_hovered < 0 || m_source == nullptr)
    {
        HidePreview();
        return;
    }

    thumb  = m_source->GetCellImage (m_hovered);
    image  = m_source->GetPreviewImage (m_hovered);
    isFull = image != nullptr;
    isSame = m_preview != nullptr && thumb != nullptr && thumb == m_previewThumb;

    if (!isFull && !isSame && m_previewDip.cx > 0)
    {
        image = thumb;
    }

    //  Nothing of the cell's own to show: another cell's picture is no
    //  preview of this one.
    if (image == nullptr && !isSame)
    {
        HidePreview();
        return;
    }

    //  Nothing new to show: what is up stays, and keeps up with the pointer.
    if (image == nullptr || (m_preview != nullptr && image == m_previewImage))
    {
        m_previewThumb = thumb;
        FollowPointer();
        return;
    }

    m_previewImage = std::move (image);
    m_previewThumb = thumb;

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
    m_previewThumb = nullptr;

    if (popup != nullptr && m_popupHost != nullptr)
    {
        m_popupHost->ReleasePopup (popup);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::ShowPreview
//
//  Beside the strip: below one lying down, to the right of one standing up,
//  flipped when there is no room, centered on the pointer. The picture's
//  pixels are its size in DIPs, so the popup scales them for the window's
//  DPI. The popup lets the pointer through and never activates, so the strip
//  keeps the hover and the window keeps the focus.
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
//  The open popup redraws with its new picture where the pointer now is,
//  staying on screen throughout.
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
//  DxuiImageStrip::FollowPointer
//
//  The open popup moves to stay centered on the pointer, its picture as it
//  was, so it glides along the strip rather than jumping from cell to cell.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::FollowPointer()
{
    HRESULT  hr = S_OK;



    if (m_preview == nullptr)
    {
        return;
    }

    hr = m_preview->Reposition (GetPreviewAnchor());

    if (FAILED (hr))
    {
        HidePreview();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetPreviewAnchor
//
//  In screen pixels: as wide as the preview and centered on the pointer
//  along the strip, and across it the whole entry, labels included, so the
//  preview opens past the hovered cell's labels rather than over them.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiImageStrip::GetPreviewAnchor() const
{
    const DxuiDpiScaler  * scaler   = (m_popupHost != nullptr) ? &m_popupHost->GetScaler() : &m_scaler;
    int                    widthPx  = scaler->ToPx (m_previewDip.cx);
    int                    heightPx = scaler->ToPx (m_previewDip.cy);
    RECT                   anchor   = m_outer;
    POINT                  topLeft  = {};
    POINT                  botRight = {};
    HWND                   owner    = (m_popupHost != nullptr) ? m_popupHost->GetHwnd() : nullptr;



    if (m_vertical)
    {
        anchor.top    = m_pointer.y - heightPx / 2;
        anchor.bottom = anchor.top + heightPx;
    }
    else
    {
        anchor.left  = m_pointer.x - widthPx / 2;
        anchor.right = anchor.left + widthPx;
    }

    topLeft  = POINT { anchor.left,  anchor.top };
    botRight = POINT { anchor.right, anchor.bottom };

    // No window, as in tests: the client pixels stand in for the screen.
    if (owner != nullptr)
    {
        ClientToScreen (owner, &topLeft);
        ClientToScreen (owner, &botRight);
    }

    return RECT { topLeft.x, topLeft.y, botRight.x, botRight.y };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::PaintHoverFrame
//
//  A thin ring in the theme's hover frame color over the edge of the cell's
//  picture, inside a line of the theme's frame edge color where it has one.
//  Drawn as pictures are, since pictures draw over shapes.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::PaintHoverFrame (
    IDxuiTextRenderer & text,
    const IDxuiTheme  & theme,
    const RECT        & cell)
{
    float     frame = std::max (1.0f, (float) std::lround (m_scaler.ToPxf (kHoverFrameDip)));
    float     line  = std::max (1.0f, (float) std::lround (m_scaler.ToPxf (kHoverEdgeDip)));
    uint32_t  edge  = theme.PictureHoverFrameEdge();
    RECT      inner = cell;



    if (edge != 0)
    {
        PaintRing (text, cell, line, edge);
        InflateRect (&inner, -(int) line, -(int) line);
    }

    PaintRing (text, inner, frame, theme.PictureHoverFrame());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::PaintRing
//
//  Four sides `thick` pixels wide just inside rc, each one pixel stretched.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::PaintRing (
    IDxuiTextRenderer & text,
    const RECT        & rc,
    float               thick,
    uint32_t            argb)
{
    HRESULT  hr     = S_OK;
    float    left   = (float) rc.left;
    float    top    = (float) rc.top;
    float    width  = (float) (rc.right - rc.left);
    float    height = (float) (rc.bottom - rc.top);



    if (width < thick * 2.0f || height < thick * 2.0f)
    {
        return;
    }

    hr = text.DrawIconBitmap (&argb, 1, 1, left,                 top,                  width, thick);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawIconBitmap (&argb, 1, 1, left,                 top + height - thick, width, thick);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawIconBitmap (&argb, 1, 1, left,                 top + thick,          thick, height - thick * 2.0f);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawIconBitmap (&argb, 1, 1, left + width - thick, top + thick,          thick, height - thick * 2.0f);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetPreviewRectPx
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiImageStrip::GetPreviewRectPx() const
{
    return (m_preview != nullptr) ? m_preview->GetPlacedRectScreenPx() : RECT {};
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
//  Where an offset in cells lies along the strip, in pixels: the inverse of
//  GetOffsetAt.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiImageStrip::GetLineAlong (float offset) const
{
    float  start     = (float) (m_vertical ? m_rc.top : m_rc.left);
    float  lastStart = 0.0f;
    float  lastPixel = 0.0f;
    float  lastCell  = (float) (m_count - 1);



    GetLastCellSpan (lastStart, lastPixel);

    if (m_count <= 0 || offset <= lastCell || lastPixel <= lastStart)
    {
        return start + offset * (float) m_cellPx;
    }

    return start + lastStart + (offset - lastCell) * (lastPixel - lastStart);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetOffsetAt
//
//  The offset in cells of a point along the strip, every pixel along the
//  pictures its own: a cell's length of pixels to each cell, except that
//  the last cell, which takes whatever length is left over, runs from its
//  first pixel to the last pixel of the strip, so the first pixel is the
//  leading end, 0, and the last the trailing end, the cell count. A point
//  before or past the pictures is at the end it is past.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiImageStrip::GetOffsetAt (
    int  x,
    int  y) const
{
    float  start     = (float) (m_vertical ? m_rc.top : m_rc.left);
    float  along     = (float) (m_vertical ? y : x) - start;
    float  lastStart = 0.0f;
    float  lastPixel = 0.0f;



    if (m_cellPx <= 0 || m_count <= 0)
    {
        return 0.0f;
    }

    GetLastCellSpan (lastStart, lastPixel);

    along = std::clamp (along, 0.0f, std::max (0.0f, lastPixel));

    if (along < lastStart)
    {
        return along / (float) m_cellPx;
    }

    if (lastPixel <= lastStart)
    {
        return (float) m_count;
    }

    return (float) (m_count - 1) + (along - lastStart) / (lastPixel - lastStart);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetLastCellSpan
//
//  Along the pictures, from their leading edge: where the last cell starts,
//  and the last pixel.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::GetLastCellSpan (
    float  & outStart,
    float  & outLastPixel) const
{
    int  length = m_vertical ? m_rc.bottom - m_rc.top : m_rc.right - m_rc.left;



    outStart     = (float) ((std::max) (m_count - 1, 0) * m_cellPx);
    outLastPixel = (float) (length - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetCellAtOffset
//
//  The cell an offset along the strip lies in, the trailing end being in
//  the last.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiImageStrip::GetCellAtOffset (float offset) const
{
    return std::clamp ((int) offset, 0, (std::max) (m_count - 1, 0));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::IsOverPlayhead
//
//  Whether a point on the strip, not on an end label, is within the grip of
//  the source's playhead line, and where the line stands, in cells.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiImageStrip::IsOverPlayhead (
    int      x,
    int      y,
    float  & outLine) const
{
    POINT         pt   = { x, y };
    std::wstring  top;
    std::wstring  bottom;
    bool          isOn = false;



    outLine = 0.0f;

    if (m_source == nullptr || !PtInRect (&m_outer, pt) || HitTestPart (x, y) != Part::None)
    {
        return false;
    }

    isOn = m_source->TryGetPlayhead (outLine, top, bottom);

    return isOn && IsOnPlayhead (m_vertical ? y : x, GetLineAlong (outLine), m_scaler.ToPxf (kPlayheadGripDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::PaintPlayhead
//
//  A line in the accent color across the pictures and a little past them,
//  and lying down, its labels in the room above and below that, each kept
//  within the strip; standing up, its labels just above and below it.
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
    float               reach   = std::min (room, m_scaler.ToPxf (kLineReachDip));
    uint32_t            ink     = theme.Accent() | kOpaque;



    //  The line is drawn as a picture is, one pixel stretched, since pictures
    //  draw over shapes and the line must cross them.
    if (m_vertical)
    {
        hr = text.DrawIconBitmap (&ink, 1, 1, (float) m_outer.left, along - thick / 2.0f, (float) (m_outer.right - m_outer.left), thick);
        IGNORE_RETURN_VALUE (hr, S_OK);

        PaintStandingPlayheadLabels (painter, text, theme, along, thick, top, bottom);
        return;
    }

    hr = text.DrawIconBitmap (&ink, 1, 1, along - thick / 2.0f, (float) m_rc.top - reach, thick, (float) (m_rc.bottom - m_rc.top) + reach * 2.0f);
    IGNORE_RETURN_VALUE (hr, S_OK);

    PaintLabelPair (painter, text, theme, top, bottom, along);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::PaintLabelPair
//
//  Lying down, one label in the room above the pictures and one in the room
//  below, each clear of the playhead line's reach past them and centered on
//  centerX within the strip.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::PaintLabelPair (
    IDxuiPainter        & painter,
    IDxuiTextRenderer   & text,
    const IDxuiTheme    & theme,
    const std::wstring  & top,
    const std::wstring  & bottom,
    float                 centerX)
{
    float  room  = (float) (m_rc.top - m_outer.top);
    float  reach = std::min (room, m_scaler.ToPxf (kLineReachDip));



    if (m_vertical || room <= 0.0f)
    {
        return;
    }

    PaintLabel (painter, text, theme, top,    centerX, (float) m_outer.top,          room - reach);
    PaintLabel (painter, text, theme, bottom, centerX, (float) m_rc.bottom + reach, room - reach);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::HideOverlap
//
//  A label that would overlap another, each centered where it is drawn, is
//  cleared, so the other reads clearly.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::HideOverlap (
    IDxuiTextRenderer   & text,
    std::wstring        & label,
    float                 centerX,
    const std::wstring  & other,
    float                 otherX) const
{
    float  left       = 0.0f;
    float  width      = 0.0f;
    float  otherLeft  = 0.0f;
    float  otherWidth = 0.0f;



    if (label.empty() || other.empty())
    {
        return;
    }

    GetLabelSpan (text, label, centerX, left, width);
    GetLabelSpan (text, other, otherX,  otherLeft, otherWidth);

    if (left < otherLeft + otherWidth && otherLeft < left + width)
    {
        label.clear();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetLabelSpan
//
//  Where a label's plate lies along the strip: its text padded either side,
//  centered on centerX and kept within the strip.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::GetLabelSpan (
    IDxuiTextRenderer   & text,
    const std::wstring  & label,
    float                 centerX,
    float               & outLeft,
    float               & outWidth) const
{
    HRESULT  hr    = S_OK;
    float    textH = 0.0f;



    outWidth = 0.0f;

    hr = text.MeasureString (label.c_str(), m_scaler.ToPxf (kLabelFontDip), DxuiTheme::kBodyFace, outWidth, textH);
    IGNORE_RETURN_VALUE (hr, S_OK);

    outWidth += m_scaler.ToPxf (kLabelPadDip) * 2.0f;
    outLeft   = centerX - outWidth / 2.0f;
    outLeft   = std::clamp (outLeft, (float) m_outer.left, std::max ((float) m_outer.left, (float) m_outer.right - outWidth));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::PaintLabel
//
//  One line of text on a plate of the background, height tall from top,
//  centered on centerX and kept within the strip, so the pictures do not
//  show through.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::PaintLabel (
    IDxuiPainter        & painter,
    IDxuiTextRenderer   & text,
    const IDxuiTheme    & theme,
    const std::wstring  & label,
    float                 centerX,
    float                 top,
    float                 height)
{
    HRESULT  hr     = S_OK;
    float    size   = m_scaler.ToPxf (kLabelFontDip);
    float    width  = 0.0f;
    float    left   = 0.0f;



    if (label.empty())
    {
        return;
    }

    GetLabelSpan (text, label, centerX, left, width);

    painter.FillRect (left, top, width, height, theme.Background());

    hr = text.DrawString (label.c_str(), left, top, width, height, theme.Foreground(), size, DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::OnLButtonUp
//
//  Any release lets a pressed end label up. Letting go of the playhead line
//  ends its drag where it was let go, and the click the toolbar may send for
//  the same release is eaten.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::OnLButtonUp (
    int  x,
    int  y)
{
    m_pressPart = Part::None;

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
//  DxuiImageStrip::GetCursorAt
//
//  Over the playhead line's grip, and wherever the pointer is while the
//  line is dragged, the cursor that resizes along the strip: left and right
//  lying down, up and down standing up.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiImageStrip::GetCursorAt (
    int  x,
    int  y) const
{
    LPCWSTR  along = m_vertical ? IDC_SIZENS : IDC_SIZEWE;
    float    line  = 0.0f;



    if (m_isDragging)
    {
        return along;
    }

    return IsOverPlayhead (x, y, line) ? along : nullptr;
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
//  Standing up, a label's room is its height.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiImageStrip::HasOutgrownLabelRoom()
{
    int  lead  = 0;
    int  trail = 0;



    if (m_source == nullptr || m_labelRoomDp <= 0.0f)
    {
        return false;
    }

    if (m_vertical)
    {
        lead  = GetStandingLabelPx (m_source->GetLeadingLabel());
        trail = GetStandingLabelPx (m_source->GetTrailingLabel());

        return lead > m_lead.bottom - m_lead.top || trail > m_trail.bottom - m_trail.top;
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
    float     size     = m_scaler.ToPxf (kLabelFontDip);
    float     extra    = isTrailing ? dot + pad : 0.0f;
    float     textW    = 0.0f;
    float     shift    = 0.0f;
    bool      isAccent = isTrailing && m_source != nullptr && m_source->IsTrailingLabelAccented();
    uint32_t  ink      = isAccent ? theme.Accent() : theme.Foreground();



    if (label.empty() || width <= 0.0f)
    {
        return;
    }

    //  Standing up, the label and its dot are centered across the strip, at
    //  a size that fits its width.
    if (m_vertical)
    {
        size   = GetFittedFontPx (text, label, width - extra, textW);
        shift  = (std::max) (0.0f, (width - extra - textW) / 2.0f);
        left  += shift;
        width -= shift;
    }

    if (isTrailing)
    {
        if (isAccent)
        {
            painter.FillCircle (left + dot / 2.0f, (float) (rc.top + rc.bottom) / 2.0f, dot / 2.0f, ink);
        }

        left  += extra;
        width -= extra;
    }

    hr = text.DrawString (label.c_str(), left, (float) rc.top, width, (float) (rc.bottom - rc.top), ink,
                          size, DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetFittedFontPx
//
//  The label's font size, made smaller as far as it takes for the label to
//  fit `maxWidthPx`, and the label's width at that size.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiImageStrip::GetFittedFontPx (
    IDxuiTextRenderer   & text,
    const std::wstring  & label,
    float                 maxWidthPx,
    float               & outWidthPx) const
{
    HRESULT  hr     = S_OK;
    float    size   = m_scaler.ToPxf (kLabelFontDip);
    float    height = 0.0f;



    outWidthPx = 0.0f;

    hr = text.MeasureString (label.c_str(), size, DxuiTheme::kBodyFace, outWidthPx, height);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (outWidthPx > maxWidthPx && maxWidthPx > 0.0f)
    {
        size       *= maxWidthPx / outWidthPx;
        outWidthPx  = maxWidthPx;
    }

    return size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::GetStandingLabelPx
//
//  How tall a label above or below the pictures of a strip standing up is:
//  a line of text and a little room above and below it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiImageStrip::GetStandingLabelPx (const std::wstring & label) const
{
    HRESULT  hr     = S_OK;
    float    width  = 0.0f;
    float    height = 0.0f;



    if (label.empty() || m_text == nullptr)
    {
        return 0;
    }

    hr = m_text->MeasureString (label.c_str(), m_scaler.ToPxf (kLabelFontDip), DxuiTheme::kBodyFace, width, height);
    IGNORE_RETURN_VALUE (hr, S_OK);

    return (int) std::ceil (height + m_scaler.ToPxf (kLabelPadDip) * 2.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::PlaceStandingEndLabels
//
//  Standing up, where history begins goes above the first picture and the
//  trailing label below the last, each a line across the strip, while the
//  pictures keep at least half its length.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::PlaceStandingEndLabels()
{
    LONG  lead  = 0;
    LONG  trail = 0;



    if (m_source == nullptr)
    {
        return;
    }

    lead  = GetStandingLabelPx (m_source->GetLeadingLabel());
    trail = GetStandingLabelPx (m_source->GetTrailingLabel());

    if (lead + trail >= (m_rc.bottom - m_rc.top) / 2)
    {
        return;
    }

    m_lead       = RECT { m_rc.left, m_rc.top,            m_rc.right, m_rc.top + lead };
    m_trail      = RECT { m_rc.left, m_rc.bottom - trail, m_rc.right, m_rc.bottom     };
    m_rc.top    += lead;
    m_rc.bottom -= trail;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::PaintStandingPlayheadLabels
//
//  Standing up, the line's labels sit across the strip, the top one just
//  above the line and the bottom one just below it. Near an end, where one
//  would leave the strip, both go on the side with room.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::PaintStandingPlayheadLabels (
    IDxuiPainter        & painter,
    IDxuiTextRenderer   & text,
    const IDxuiTheme    & theme,
    float                 along,
    float                 thick,
    const std::wstring  & top,
    const std::wstring  & bottom)
{
    float  tall   = (float) (std::max) (GetStandingLabelPx (top), GetStandingLabelPx (bottom));
    float  above  = along - thick / 2.0f;
    float  below  = along + thick / 2.0f;
    float  first  = above - tall;
    float  second = below;



    if (tall <= 0.0f)
    {
        return;
    }

    if (first < (float) m_outer.top)
    {
        first  = below;
        second = below + tall;
    }
    else if (second + tall > (float) m_outer.bottom)
    {
        first  = above - tall * 2.0f;
        second = above - tall;
    }

    PaintStandingLabel (painter, text, theme, top,    first,  tall);
    PaintStandingLabel (painter, text, theme, bottom, second, tall);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiImageStrip::PaintStandingLabel
//
//  One line of text on a plate of the background, `height` tall from `top`,
//  centered across a strip standing up and no wider than it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiImageStrip::PaintStandingLabel (
    IDxuiPainter        & painter,
    IDxuiTextRenderer   & text,
    const IDxuiTheme    & theme,
    const std::wstring  & label,
    float                 top,
    float                 height)
{
    HRESULT  hr     = S_OK;
    float    pad    = m_scaler.ToPxf (kLabelPadDip);
    float    across = (float) (m_outer.right - m_outer.left);
    float    textW  = 0.0f;
    float    size   = 0.0f;
    float    width  = 0.0f;
    float    left   = 0.0f;



    if (label.empty())
    {
        return;
    }

    size  = GetFittedFontPx (text, label, across - pad * 2.0f, textW);
    width = textW + pad * 2.0f;
    left  = (float) m_outer.left + (std::max) (0.0f, (across - width) / 2.0f);

    painter.FillRect (left, top, width, height, theme.Background());

    hr = text.DrawString (label.c_str(), left, top, width, height, theme.Foreground(), size, DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





