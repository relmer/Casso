#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetLevels
//
//  One level per address for each kind; empty vectors are a cold map.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetLevels (const std::vector<Byte> & execute, const std::vector<Byte> & read, const std::vector<Byte> & write)
{
    m_cells.SetLevels (execute, read, write);
    m_grid.MarkFrameStale();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetPalette
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetPalette (const Palette & palette)
{
    if (m_cells.SetPalette (palette))
    {
        m_grid.MarkFrameStale();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetMode
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetMode (Mode mode)
{
    m_cells.SetMode (mode);
    m_grid.MarkFrameStale();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetOptions
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetOptions (const HeatMapOptions & options)
{
    m_cells.SetOptions (options);
    m_grid.MarkFrameStale();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetSymbols
//
//  The same copy again changes nothing; another is marked and drawn.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetSymbols (std::shared_ptr<const HeatMapSymbols> symbols)
{
    if (m_cells.SetSymbols (std::move (symbols)))
    {
        m_grid.MarkFrameStale();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::Layout
//
//  The map is laid out again at the pane's new bounds.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    m_boundsDip = boundsPx;
    m_grid.Layout (boundsPx, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetRegions
//
//  Regions arrive or go away as the current bank changes, which cuts the
//  map into sections or joins them, so it is laid out again.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetRegions (const std::vector<uint16_t> & regions)
{
    bool  wasEmpty = m_cells.GetRegions().empty();



    m_cells.SetRegions (regions);

    if (wasEmpty != regions.empty())
    {
        m_grid.PlaceAgain();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetMachineId
//
//  The sections of the system ROM's parts start where the machine's ROM
//  changes part, so a new machine cuts the map again.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetMachineId (const std::string & machineId)
{
    if (machineId == m_cells.GetMachineId())
    {
        return;
    }

    m_cells.SetMachineId (machineId);
    m_grid.OnMachineChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::IsPressed
//
//  A press on the map, or the scrollbar's thumb or the zoom being dragged.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::IsPressed() const
{
    return m_press.has_value() || m_grid.GetScrollbar().IsDragging() || m_grid.GetZoomWidget().IsDragging();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::TryGetTipAt
//
//  The tip is anchored on the cell the pick framed, so it sits clear of the
//  frame and follows the pick from cell to cell.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::TryGetTipAt (POINT point, RECT & anchor, std::wstring & text, std::vector<uint32_t> & swatches) const
{
    std::vector<uint32_t>  gradient;
    float                  mark = -1.0f;



    return TryGetTipAt (point, anchor, text, swatches, gradient, mark);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::TryGetTipAt
//
//  With the cell's scale and its level on it, as a code view's heat tip
//  draws them.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::TryGetTipAt (POINT point, RECT & anchor, std::wstring & text, std::vector<uint32_t> & swatches,
                               std::vector<uint32_t> & gradient, float & mark) const
{
    std::optional<Place>  pick;
    TipLead               lead;
    std::wstring          contents;
    std::wstring          rest;



    if (!m_visible || IsPressed() || m_grid.GetZoomWidget().HitTest (point) != HeatMapZoomWidget::Hit::None)
    {
        return false;
    }

    pick = m_grid.GetPickPlaceAt (point);

    if (!pick.has_value())
    {
        return false;
    }

    anchor   = m_grid.GetPlaceRect (*pick);
    InflateRect (&anchor, kStreetPx, kStreetPx);
    lead     = m_cells.GetTipLead (m_grid.GetAddressOf (*pick));
    text     = m_cells.GetTipText (m_grid.GetAddressOf (*pick));
    contents = m_cells.GetContentsLine (m_grid.GetAddressOf (*pick));
    rest     = contents.empty() ? text : text.substr (contents.size() + 1);

    //  The cell's contents come first, with no swatch. An untouched cell's
    //  next line already ends in "untouched", so the swatch goes on that line
    //  in place of a lead line.
    if (m_cells.GetCellKind (m_grid.GetAddressOf (*pick)) != CellKind::Untouched)
    {
        rest = lead.meaning + L"\n" + rest;
    }

    text = contents.empty() ? rest : contents + L"\n" + rest;
    swatches.assign (contents.empty() ? 1 : 2, 0);
    swatches.back() = lead.argb | 0xFF000000u;

    gradient.clear();
    mark = -1.0f;

    if (!m_cells.TryGetCellScale (m_grid.GetAddressOf (*pick), gradient, mark))
    {
        gradient.clear();
        mark = -1.0f;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::IsTipComplete
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::IsTipComplete (POINT point) const
{
    std::optional<Place>  pick    = m_grid.GetPickPlaceAt (point);
    Word                  address = 0;



    if (!pick.has_value() || !m_onHoverChanged)
    {
        return true;
    }

    address = m_grid.GetAddressOf (*pick);

    return m_cells.HasHoverAccess (address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    const std::vector<uint32_t>  * frame  = nullptr;
    RECT                           map    = {};
    long                           width  = 0;
    long                           height = 0;
    HRESULT                        hr     = S_OK;



    if (!m_visible || m_boundsDip.right <= m_boundsDip.left)
    {
        return;
    }

    m_grid.MeasureGutter (text, theme);

    frame  = &m_grid.GetPixels (m_visible);
    map    = m_grid.GetMapRect();
    width  = map.right  - map.left;
    height = map.bottom - map.top;

    m_painter.PaintBar       (painter, text, theme);
    m_painter.PaintRowLabels (text, theme);

    if (width > 0 && height > 0 && frame->size() == (size_t) (width * height))
    {
        hr = text.DrawFramebuffer (frame->data(), (int) width, (int) height, (float) map.left, (float) map.top, (float) width, (float) height);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    m_painter.PaintOverlays (text, theme);

    if (m_grid.IsSplit())
    {
        m_painter.PaintSections (text, theme);
    }

    m_painter.PaintHeaders (text, theme);
    m_painter.PaintHover   (text, theme);

    m_grid.PlaceZoomWidget();
    m_grid.GetZoomWidget().Paint (text, theme, m_grid.GetCellPx(), m_grid.GetStartCellPx(), kMaxCellPx);

    if (m_grid.HasVerticalScroll())
    {
        m_grid.GetScrollbar().Paint (painter, theme.ForegroundMuted());
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnKey
//
//  The arrow, page, Home and End keys move the frame to another cell, as the
//  mouse does, and the map is scrolled so the cell is in view. Enter opens
//  the cell's address in a memory window, as a click does. With no cell
//  framed, the first key frames the first cell in view.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnKey (const DxuiKeyEvent & ev)
{
    std::optional<Place>  target;



    if (ev.kind != DxuiKeyEventKind::Down || m_grid.GetPlaced().empty())
    {
        return false;
    }

    if (ev.vk == VK_RETURN)
    {
        if (m_grid.GetHoverPlace().has_value() && m_onPickAddress)
        {
            m_onPickAddress (m_grid.GetAddressOf (*m_grid.GetHoverPlace()));
        }

        return m_grid.GetHoverPlace().has_value();
    }

    target = GetKeyTarget (ev);

    if (!target.has_value())
    {
        return false;
    }

    m_grid.ScrollToPlace (*target);
    SetHover (target);
    m_grid.MarkFrameStale();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetKeyTarget
//
//  The cell a key moves the frame to, within the frame's range: the arrows
//  a cell or a row, an instruction drawn as one cell at a time; Page Up and
//  Page Down a map's height of rows; Home and End the row's ends, or with
//  Ctrl the range's. None for any other key.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<HeatMapView::Place> HeatMapView::GetKeyTarget (const DxuiKeyEvent & ev) const
{
    std::optional<Place>   from    = m_grid.GetHoverPlace().has_value() ? m_grid.GetHoverPlace() : m_grid.GetPlaceAt (POINT { m_grid.GetMapRect().left + 1, m_grid.GetMapRect().top + 1 });
    std::optional<Merged>  merged;
    long                   columns = 0;
    long                   count   = 0;
    long                   page    = 0;
    long                   offset  = 0;



    if (!from.has_value() || from->band >= m_grid.GetPlaced().size())
    {
        return std::nullopt;
    }

    if (!m_grid.GetHoverPlace().has_value())
    {
        return from;
    }

    merged  = m_grid.GetMergedAt (*from);
    columns = m_grid.GetPlaced()[from->band].columns;
    count   = m_grid.GetBands()[from->band].count;
    page    = std::max (1L, (m_grid.GetMapRect().bottom - m_grid.GetMapRect().top) / std::max (1, m_grid.GetPitch())) * columns;
    offset  = from->offset;

    switch (ev.vk)
    {
    case VK_LEFT:  offset -= 1;                                                                    break;
    case VK_RIGHT: offset  = merged.has_value() ? merged->first.offset + merged->length : offset + 1; break;
    case VK_UP:    offset -= columns;                                                              break;
    case VK_DOWN:  offset += columns;                                                              break;
    case VK_PRIOR: offset -= page;                                                                 break;
    case VK_NEXT:  offset += page;                                                                 break;
    case VK_HOME:  offset  = ev.ctrl ? 0         : offset - offset % columns;                      break;
    case VK_END:   offset  = ev.ctrl ? count - 1 : offset - offset % columns + columns - 1;        break;
    default:       return std::nullopt;
    }

    offset = std::clamp (offset, 0L, count - 1);
    merged = m_grid.GetMergedAt (Place { from->band, offset });

    return merged.has_value() ? merged->first : Place { from->band, offset };
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnPress
//
//  A press on the scrollbar moves it, its thumb by a drag; one on the map
//  starts what is a click or a drag, which the release and the moves decide.
//  The view is chosen on the window's strip in the row of views.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnPress (const DxuiMouseEvent & ev)
{
    POINT  at = ev.positionDip;



    if (m_grid.HasVerticalScroll() && m_grid.GetScrollbar().HitTest (at.x, at.y))
    {
        m_grid.GetScrollbar().OnMouseDown (at.x, at.y);
        m_grid.ScrollBy (m_grid.GetScrollbar().GetScrollPos() - (int) m_grid.GetScrollTop());
        return true;
    }

    if (!m_grid.GetAddressAt (at).has_value())
    {
        return false;
    }

    m_press      = at;
    m_pressTop   = m_grid.GetScrollTop();
    m_isDragging = false;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnRelease
//
//  A press that never became a drag is a click on the cell it picked, for
//  what the keys held ask (GetPickAction).
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnRelease (const DxuiMouseEvent & ev)
{
    std::optional<Word>  picked;
    PickAction           action = PickAction::ShowMemory;



    if (m_grid.GetScrollbar().IsDragging())
    {
        m_grid.GetScrollbar().OnMouseUp();
        return true;
    }

    if (!m_press.has_value())
    {
        return false;
    }

    if (!m_isDragging)
    {
        picked = m_grid.GetPickAt (*m_press);
    }

    m_press.reset();
    m_isDragging = false;

    SetHover (m_grid.GetPickPlaceAt (ev.positionDip));

    if (!picked.has_value())
    {
        return true;
    }

    action = GetPickAction (ev.ctrl, ev.shift, ev.alt);

    if (action == PickAction::ShowMemory && m_onPickAddress)
    {
        m_onPickAddress (*picked);
    }
    else if (action != PickAction::ShowMemory && m_onPickAccess)
    {
        m_onPickAccess (*picked, action);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnDragOrHover
//
//  The scrollbar's thumb follows the mouse while it is dragged. With the
//  button down on the map, once the mouse has gone kDragDip the map follows
//  it up and down; otherwise the pick follows the mouse, and the scrollbar
//  under it widens.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnDragOrHover (const DxuiMouseEvent & ev)
{
    POINT  at = ev.positionDip;
    long   dx = 0;
    long   dy = 0;



    if (m_grid.GetScrollbar().IsDragging())
    {
        m_grid.GetScrollbar().OnMouseMove (at.x, at.y);
        m_grid.ScrollBy (m_grid.GetScrollbar().GetScrollPos() - (int) m_grid.GetScrollTop());
        return true;
    }

    if (!m_press.has_value())
    {
        (void) m_grid.GetScrollbar().SetHover (m_grid.HasVerticalScroll() && m_grid.GetScrollbar().HitTest (at.x, at.y), at);

        SetHover (m_grid.GetPickPlaceAt (at));
        return false;
    }

    dx = at.x - m_press->x;
    dy = at.y - m_press->y;

    if (!m_isDragging && std::max (std::abs (dx), std::abs (dy)) >= m_grid.GetScaler().ToPx (kDragDip))
    {
        m_isDragging = true;
        SetHover (std::nullopt);
    }

    if (!m_isDragging)
    {
        return true;
    }

    m_grid.DragTo (m_pressTop - dy, m_pressTop);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnWheel
//
//  Over the map or its scrollbar: Ctrl with the wheel zooms about the mouse;
//  the wheel alone scrolls up and down, a few rows a notch. The rows always
//  fit across, so Shift with the wheel, or a sideways wheel, is left for the
//  window.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnWheel (const DxuiMouseEvent & ev)
{
    long  pitch = m_grid.GetPitch();
    long  rows  = std::max (1L, (long) std::lround ((double) m_grid.GetScaler().ToPx (kWheelStepDip) / (double) pitch));
    int   step  = (int) std::lround (ev.wheelDelta * (float) (rows * pitch));



    //  Over the zoom's button or its panel, the wheel zooms about the map's
    //  middle, as the slider does.
    if (m_grid.GetZoomWidget().HitTest (ev.positionDip) != HeatMapZoomWidget::Hit::None && ev.wheelDelta != 0.0f && !ev.wheelHorizontal)
    {
        m_grid.ZoomAt ({ (m_grid.GetMapRect().left + m_grid.GetMapRect().right) / 2, (m_grid.GetMapRect().top + m_grid.GetMapRect().bottom) / 2 }, ev.wheelDelta);
        return true;
    }

    if (!m_grid.IsOverMap (ev.positionDip) || ev.wheelDelta == 0.0f || ev.wheelHorizontal)
    {
        return false;
    }

    if (ev.ctrl)
    {
        m_grid.ZoomAt (ev.positionDip, ev.wheelDelta);
    }
    else if (ev.shift)
    {
        return false;
    }
    else
    {
        m_grid.ScrollBy (-step);
    }

    SetHover (m_grid.GetPickPlaceAt (ev.positionDip));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnZoomWidget
//
//  A press on the zoom's button opens its panel or closes it; one on the
//  track zooms to the cell size there and drags the zoom along it until the
//  button comes up; one on Reset goes back to the starting zoom. A press
//  anywhere else closes the panel and goes on to the map. The mouse over the
//  widget frames no cell.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnZoomWidget (const DxuiMouseEvent & ev)
{
    using Hit = HeatMapZoomWidget::Hit;

    POINT  center = { (m_grid.GetMapRect().left + m_grid.GetMapRect().right) / 2, (m_grid.GetMapRect().top + m_grid.GetMapRect().bottom) / 2 };
    Hit    hit    = Hit::None;



    m_grid.PlaceZoomWidget();
    hit = m_grid.GetZoomWidget().HitTest (ev.positionDip);

    if (m_grid.GetZoomWidget().IsDragging())
    {
        if (ev.kind == DxuiMouseEventKind::Move)
        {
            m_grid.ApplyCellPx (m_grid.GetZoomWidget().GetCellPxAt (ev.positionDip, kMaxCellPx), center);
        }
        else if (ev.kind == DxuiMouseEventKind::Up)
        {
            m_grid.GetZoomWidget().SetDragging (false);
        }

        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Up)
    {
        return hit != Hit::None;
    }

    if (ev.kind != DxuiMouseEventKind::Down || ev.button != DxuiMouseButton::Left)
    {
        return false;
    }

    switch (hit)
    {
    case Hit::Button:
        m_grid.GetZoomWidget().Toggle();
        return true;

    case Hit::Track:
        m_grid.GetZoomWidget().SetDragging (true);
        m_grid.ApplyCellPx (m_grid.GetZoomWidget().GetCellPxAt (ev.positionDip, kMaxCellPx), center);
        return true;

    case Hit::Reset:
        m_grid.ResetZoom();
        return true;

    case Hit::Panel:
        return true;

    default:
        m_grid.GetZoomWidget().SetOpen (false);
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnFocusChanged
//
//  The zoom slider closes when the focus moves to another pane, as a menu
//  does, since a click there isn't routed to the map.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::OnFocusChanged (bool focused)
{
    if (!focused)
    {
        m_grid.GetZoomWidget().SetOpen (false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapView::OnMouse (const DxuiMouseEvent & ev)
{
    //  The zoom slider opens with the pointer over its button, and closes
    //  once the pointer moves away from it or off the map, except while its
    //  thumb is being dragged.
    if (ev.kind == DxuiMouseEventKind::Move && !m_grid.GetZoomWidget().IsDragging())
    {
        m_grid.PlaceZoomWidget();
        m_grid.GetZoomWidget().OnHover (ev.positionDip);
    }
    else if (ev.kind == DxuiMouseEventKind::Leave && m_grid.GetZoomWidget().IsOpen() && !m_grid.GetZoomWidget().IsDragging())
    {
        m_grid.GetZoomWidget().SetOpen (false);
    }

    //  The pointer over the zoom frames no cell, and the move is left for the
    //  window, which shows the zoom's tip.
    if (ev.kind == DxuiMouseEventKind::Move && !IsPressed() && m_grid.GetZoomWidget().HitTest (ev.positionDip) != HeatMapZoomWidget::Hit::None)
    {
        SetHover (std::nullopt);
        return false;
    }

    if (OnZoomWidget (ev))
    {
        return true;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Leave:
        if (!IsPressed())
        {
            SetHover (std::nullopt);
            (void) m_grid.GetScrollbar().SetHover (false);
        }

        return false;

    case DxuiMouseEventKind::Move:
        return OnDragOrHover (ev);

    case DxuiMouseEventKind::Down:
        return ev.button == DxuiMouseButton::Left && OnPress (ev);

    case DxuiMouseEventKind::Up:
        return ev.button == DxuiMouseButton::Left && OnRelease (ev);

    case DxuiMouseEventKind::Wheel:
        return OnWheel (ev);

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::GetPickAction
//
//  A plain click shows the address in memory. Ctrl shows the last writer in
//  the disassembly, and Shift with it the last reader; Alt with Ctrl goes
//  back through history to that write or read instead.
//
////////////////////////////////////////////////////////////////////////////////

HeatMapView::PickAction HeatMapView::GetPickAction (bool ctrl, bool shift, bool alt)
{
    if (!ctrl)
    {
        return PickAction::ShowMemory;
    }

    if (alt)
    {
        return shift ? PickAction::RewindToRead : PickAction::RewindToWrite;
    }

    return shift ? PickAction::ShowReader : PickAction::ShowWriter;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapView::SetHover
//
//  The cell framed, and whoever follows it told when it changes.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapView::SetHover (const std::optional<Place> & hover)
{
    std::optional<Word>  before = m_grid.GetHover();



    m_grid.SetHoverPlace (hover);

    if (m_grid.GetHover() != before && m_onHoverChanged)
    {
        m_onHoverChanged (m_grid.GetHover());
    }
}





