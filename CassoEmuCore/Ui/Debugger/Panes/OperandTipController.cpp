#include "Pch.h"

#include "Ui/Debugger/Panes/OperandTipController.h"

#include "Ui/Debugger/DisassemblyCells.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::OperandTipController
//
////////////////////////////////////////////////////////////////////////////////

OperandTipController::OperandTipController (IOperandTipHost & host) :
    m_host (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::Update
//
//  The screen is read only for a move over a disassembly row's operand, the
//  one place a tip can appear. A window with no HWND, as a test builds it,
//  measures with the renderer and screen the test sets.
//
////////////////////////////////////////////////////////////////////////////////

void OperandTipController::Update (const DxuiMouseEvent & ev, bool isPressed)
{
    DxuiHwndSource     * host   = m_host.GetTipPopupHost();
    IDxuiTextRenderer  * text   = (host != nullptr) ? m_host.GetTipText() : m_testText;
    DxuiDpiScaler        scaler = (host != nullptr) ? host->GetScaler() : m_host.GetTipScaler();
    OperandTipScreen     screen = (host != nullptr) ? OperandTipScreen() : m_testScreen;
    bool                 isMove = ev.kind == DxuiMouseEventKind::Move && !isPressed;
    int                  view   = -1;
    int                  row    = -1;



    if (host != m_popupHost)
    {
        Hide();
    }

    m_popupHost = host;

    if (host != nullptr && isMove && m_host.TryFindTipRow (ev.positionDip, view, row))
    {
        screen = GetScreen (host);
    }

    if (text != nullptr)
    {
        Place (ev, isPressed, scaler, *text, screen);
    }
    else
    {
        Hide();
    }

    Show();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::Place
//
//  The tip is up as soon as the pointer is over the cell, and goes when the
//  pointer moves off the cell or does anything but move or turn the wheel: a
//  press, a button pressed and not released, a menu opening, or the PC's
//  marker being dragged. The pointer does not move with the wheel, and the
//  view scrolls only after this event, so PlaceAtPointer lays the tip out
//  once the view has scrolled.
//
////////////////////////////////////////////////////////////////////////////////

void OperandTipController::Place (const DxuiMouseEvent & ev, bool isPressed, const DxuiDpiScaler & scaler, IDxuiTextRenderer & text,
                                  const OperandTipScreen & screen)
{
    OperandResultTip::Layout  layout;
    OperandTipCell            cell;
    bool                      isOver = false;



    if (ev.kind == DxuiMouseEventKind::Wheel)
    {
        m_pointer   = ev.positionDip;
        m_isResting = true;
        return;
    }

    m_pointer   = ev.positionDip;
    m_isResting = ev.kind == DxuiMouseEventKind::Move && !isPressed;

    isOver = m_isResting && !m_host.IsTipBlocked() && TryGet (ev.positionDip, scaler, text, screen, layout, cell);

    //  A tip that wraps below its row stays up while the pointer moves from
    //  the row into it.
    if (!isOver && ev.kind == DxuiMouseEventKind::Move && !isPressed && IsOverTipArea (ev.positionDip))
    {
        return;
    }

    if (!isOver)
    {
        Hide();
        return;
    }

    SetTip (std::move (layout), cell, text, scaler, screen);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::PlaceAtPointer
//
//  The tip for the cell under a pointer that has not moved, after the wheel
//  scrolled or zoomed the view under it or new rows or a new layout moved
//  its cells: up for a cut-off cell, and down where none is under the
//  pointer. A tip that wraps below its row stays up for a pointer in it as
//  long as the tip's cell is as it was. A tip laid out for another cell takes
//  the color tip below the old one down with it, since nothing moved to put
//  the color tip up again.
//
////////////////////////////////////////////////////////////////////////////////

void OperandTipController::PlaceAtPointer()
{
    HRESULT                     hr     = S_OK;
    DxuiHwndSource            * host   = m_popupHost;
    IDxuiTextRenderer         * text   = (host != nullptr) ? m_host.GetTipText() : m_testText;
    DxuiDpiScaler               scaler = (host != nullptr) ? host->GetScaler() : m_host.GetTipScaler();
    OperandTipScreen            screen = (host != nullptr) ? OperandTipScreen() : m_testScreen;
    OperandResultTip::Layout    layout;
    OperandTipCell              cell;
    int                         view   = -1;
    int                         row    = -1;
    bool                        isOver = false;
    bool                        isSame = false;



    isOver = m_isResting && text != nullptr && !m_host.IsTipBlocked() && m_host.TryFindTipRow (m_pointer, view, row);
    CBRF (isOver, Hide());

    if (host != nullptr)
    {
        screen = GetScreen (host);
    }

    FitColumns (view, *text);

    if (m_hasTip && view != m_cell.view)
    {
        FitColumns (m_cell.view, *text);
    }

    isOver = TryGet (m_pointer, scaler, *text, screen, layout, cell);
    isSame = !isOver && m_hasTip && IsOverTipArea (m_pointer) && TryMake (m_cell.view, m_cell.row, scaler, *text, screen, layout, cell) &&
             OperandResultTip::IsSame (layout, m_layout);
    CBRF (isOver || isSame, Hide());

    if (m_hasTip && m_colorTip != nullptr && !OperandResultTip::IsSame (layout, m_layout))
    {
        m_colorTip->HideImmediate();
        m_colorTip = nullptr;
    }

    SetTip (std::move (layout), cell, *text, scaler, screen);

Error:
    Show();
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::SetTip
//
//  New text or a new place takes down the popup that drew the old, so the
//  new is drawn.
//
////////////////////////////////////////////////////////////////////////////////

void OperandTipController::SetTip (
    OperandResultTip::Layout  && layout,
    const OperandTipCell       & cell,
    IDxuiTextRenderer          & text,
    const DxuiDpiScaler        & scaler,
    const OperandTipScreen     & screen)
{
    if (m_hasTip && !OperandResultTip::IsSame (layout, m_layout))
    {
        m_tip.Hide();
    }

    m_hasTip = true;
    m_layout = std::move (layout);
    m_cell   = cell;
    m_text   = &text;
    m_scaler = scaler;
    m_screen = screen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::FitColumns
//
//  The list fits its columns to new rows only as it paints, so they are
//  fitted here first, as that paint will fit them: a column ahead of the
//  operand widened by the new rows moves the cell as surely as new text
//  does.
//
////////////////////////////////////////////////////////////////////////////////

void OperandTipController::FitColumns (int view, IDxuiTextRenderer & text) const
{
    DxuiListView  * list = (view >= 0) ? m_host.GetTipList (view) : nullptr;



    if (list != nullptr)
    {
        list->MeasureColumnsPx (text);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::Show
//
//  The popup over the tip, in the window it was laid out for; a popup
//  already up over the same rect is not redrawn.
//
////////////////////////////////////////////////////////////////////////////////

void OperandTipController::Show()
{
    OperandResultTip::Layout  layout = m_layout;



    if (!m_hasTip)
    {
        m_tip.Hide();
        return;
    }

    m_tip.SetPopupHost (m_popupHost);
    m_tip.Show (layout.rect, layout.fill, [layout] (IDxuiPainter & painter, IDxuiTextRenderer & tipText)
    {
        OperandResultTip::Paint (layout, painter, tipText);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::Hide
//
//  A color tip put up below the tip goes with it. Left up, or coming up
//  once its dwell ends, it would hang below a tip no longer there, over rows
//  its text is not about; the next move brings back the tip for the place
//  under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void OperandTipController::Hide()
{
    if (m_hasTip && m_colorTip != nullptr)
    {
        m_colorTip->HideImmediate();
    }

    m_tip.Hide();

    m_hasTip   = false;
    m_layout   = OperandResultTip::Layout();
    m_cell     = OperandTipCell();
    m_text     = nullptr;
    m_colorTip = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::Check
//
//  With the pointer resting over the view or the tip's cell, in the window
//  whose events are routed now, the tip stays up after a step or new rows,
//  laid out again for the cell under the pointer. Outside that window the pointer
//  cannot be tested against its panes, so a tip whose cell the change moves
//  goes, and the next move of the pointer brings back a tip for the new
//  cell. The tip is laid out again in full, on the screen as it is now: the
//  PC leaving the row changes its fill.
//
////////////////////////////////////////////////////////////////////////////////

void OperandTipController::Check (int view)
{
    OperandResultTip::Layout  layout;
    OperandTipCell            cell;
    int                       pointerView = -1;
    int                       pointerRow  = -1;
    bool                      isAtPointer = m_isResting && m_popupHost == m_host.GetTipPopupHost();
    bool                      isOver      = isAtPointer && m_host.TryFindTipRow (m_pointer, pointerView, pointerRow) && pointerView == view;
    bool                      isSame      = false;



    if (isOver || (isAtPointer && m_hasTip && view == m_cell.view))
    {
        PlaceAtPointer();
        return;
    }

    if (!m_hasTip || view != m_cell.view || m_text == nullptr)
    {
        return;
    }

    if (m_popupHost != nullptr)
    {
        m_scaler = m_popupHost->GetScaler();
        m_screen = GetScreen (m_popupHost);
    }

    FitColumns (view, *m_text);

    isSame = TryMake (view, m_cell.row, m_scaler, *m_text, m_screen, layout, cell) && OperandResultTip::IsSame (layout, m_layout);

    if (!isSame)
    {
        Hide();
        return;
    }

    m_cell = cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::CheckPointer
//
//  The pointer anywhere but on the part of the cell the tip was put up for,
//  or on the tip itself, takes the tip down, however it got there.
//
////////////////////////////////////////////////////////////////////////////////

void OperandTipController::CheckPointer (POINT clientPx)
{
    if (m_hasTip && !IsOverTipArea (clientPx))
    {
        Hide();
        m_isResting = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::IsOverTipArea
//
//  The cell the tip was put up for and the tip's rect, joined into one
//  area, so the pointer can move from the row down into a tip that wraps
//  below it without crossing a gap.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandTipController::IsOverTipArea (POINT clientPx) const
{
    RECT  area = m_cell.area;



    if (!m_hasTip)
    {
        return false;
    }

    UnionRect (&area, &m_cell.area, &m_layout.rect);
    return DxuiDockSite::Contains (area, clientPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::FollowPointer
//
//  The window receives mouse events only while the pointer moves over its
//  client area, so the tip would otherwise stay up for a pointer that moves
//  straight from the cell onto the frame or the caption, into another window
//  or onto another monitor, or that is no longer over the window after it
//  moved or was minimized. Where the pointer cannot be read, the tip goes too.
//
////////////////////////////////////////////////////////////////////////////////

void OperandTipController::FollowPointer()
{
    HRESULT  hr      = S_OK;
    HWND     hwnd    = (m_popupHost != nullptr) ? m_popupHost->GetHwnd() : nullptr;
    POINT    pointer = {};
    bool     isUp    = m_hasTip && hwnd != nullptr;
    bool     isRead  = false;
    bool     isShown = false;



    BAIL_OUT_IF (!isUp, S_OK);

    isRead  = DxuiInputState::GetCursorPos (&pointer) != FALSE && ScreenToClient (hwnd, &pointer) != FALSE;
    isShown = IsWindowVisible (hwnd) != FALSE && IsIconic (hwnd) == FALSE;

    m_isResting = m_isResting && isRead && isShown;

    CBRF (isRead,  Hide());
    CBRF (isShown, Hide());

    CheckPointer (pointer);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::OnPopupHostClosed
//
////////////////////////////////////////////////////////////////////////////////

void OperandTipController::OnPopupHostClosed (const DxuiHwndSource * host)
{
    if (m_popupHost != nullptr && m_popupHost == host)
    {
        Hide();
        m_popupHost = nullptr;
        m_isResting = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::ForgetColorTip
//
////////////////////////////////////////////////////////////////////////////////

void OperandTipController::ForgetColorTip (const DxuiTooltip * tip)
{
    if (m_colorTip == tip)
    {
        m_colorTip = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::TryGet
//
//  Over the operand and result of a row in any disassembly view, the tip
//  for them when the pane cuts them off, unless something lying over the
//  pane overlaps the tip or is under the pointer, or text selected in the
//  list lies under the tip. The tip would hide the selection, and with its
//  text wrapped otherwise than the cell's, could not draw it where it was
//  made;
//  so while there is one, the list's own drawing of the cell, selection and
//  all, remains in view.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandTipController::TryGet (POINT clientPx, const DxuiDpiScaler & scaler, IDxuiTextRenderer & text, const OperandTipScreen & screen,
                                   OperandResultTip::Layout & out, OperandTipCell & outCell) const
{
    HRESULT                     hr         = S_OK;
    OperandResultTip::Layout    layout;
    OperandTipCell              cell;
    int                         view       = -1;
    int                         row        = -1;
    bool                        isOver     = false;
    bool                        isMade     = false;
    bool                        isCovered  = false;
    bool                        isSelected = false;
    bool                        hasTip     = false;



    isOver = m_host.TryFindTipRow (clientPx, view, row);
    CBR (isOver);

    isMade = TryMake (view, row, scaler, text, screen, layout, cell);
    CBR (isMade);

    isOver     = DxuiDockSite::Contains (cell.area, clientPx);
    isCovered  = m_host.IsTipCovered (view, clientPx, layout.rect, screen);
    isSelected = (m_host.GetTipList (view) != nullptr) ? IsTextSelectedUnderTip (m_host.GetTipList (view), row, m_host.GetTipColumn (view, row), layout, cell)
                                                       : m_host.GetTipTextView (view)->HasSelection();
    CBR (isOver && !isCovered && !isSelected);

    out     = std::move (layout);
    outCell = cell;
    hasTip  = true;

Error:
    return hasTip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::TryMake
//
//  Over a list's row, or a text view's for a source document.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandTipController::TryMake (int view, int row, const DxuiDpiScaler & scaler, IDxuiTextRenderer & text, const OperandTipScreen & screen,
                                    OperandResultTip::Layout & out, OperandTipCell & outCell) const
{
    return (m_host.GetTipTextView (view) != nullptr) ? TryMakeOnText (view, row, scaler, text, screen, out, outCell)
                                                     : TryMakeOnList (view, row, scaler, text, screen, out, outCell);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::TryMakeOnList
//
//  The tip over a row's operand and result when the pane cuts them off,
//  placed as the list draws the cell: where its text starts and where it is
//  clipped, the pane's edges short of its scrollbar, its padding, face and
//  size, and the theme's colors under and around it, the row's fill among
//  them. The cell's area is the part of it visible in the pane. A cell that
//  is a whole source or data line has the line's start, in the column the
//  line starts in, ahead of it, and its area starts there.
//
//  The tip is within the work area of the monitor where the cell's text
//  starts, joined by the next monitor's where the cell runs on into it, the
//  work area the popup host limits the tip's popup to: a tip over a cell
//  across two monitors covers it on both. A cell that runs on to the row's
//  end is cut off only at the pane's edge.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandTipController::TryMakeOnList (int view, int row, const DxuiDpiScaler & scaler, IDxuiTextRenderer & text, const OperandTipScreen & screen,
                                          OperandResultTip::Layout & out, OperandTipCell & outCell) const
{
    HRESULT                                   hr        = S_OK;
    const DxuiTheme                         * theme     = m_host.GetTipTheme();
    DxuiListView                            * list      = m_host.GetTipList (view);
    size_t                                    column    = m_host.GetTipColumn (view, row);
    size_t                                    start     = column;
    const std::vector<DxuiListView::Cell>   * cells     = nullptr;
    RECT                                      bounds    = {};
    RECT                                      edge      = {};
    RECT                                      lead      = {};
    size_t                                    cellCount = 0;
    bool                                      isShown   = false;
    bool                                      isHidden  = false;
    bool                                      hasRoom   = false;
    bool                                      isMade    = false;
    OperandResultTip::Placement               placement;



    isShown = theme != nullptr && list != nullptr && list->IsVisible() &&
              list->GetCellTextRectPx (row, column, placement.textRect) &&
              list->TryGetCellTextClipPx (row, column, placement.clipLeftPx, placement.clipRightPx);
    CBR (isShown);

    cells     = &list->GetCellsOfRow (row);
    cellCount = cells->size();
    CBR (cellCount > column);

    bounds = list->GetBounds();

    placement.isWholeLine = m_host.TryGetTipLineColumn (view, row, start);

    if (placement.isWholeLine && start < column && list->GetCellTextRectPx (row, start, lead))
    {
        placement.lead     = (*cells)[start];
        placement.leadLeft = lead.left + bounds.left;
    }
    else
    {
        placement.leadLeft = placement.textRect.left + bounds.left;
    }

    if ((*cells)[column].spansRow)
    {
        placement.textRect.right = (std::max) (placement.textRect.right, bounds.right - bounds.left);
    }

    OffsetRect (&placement.textRect, bounds.left, bounds.top);

    placement.clipLeftPx       += (float) bounds.left;
    placement.clipRightPx      += (float) bounds.left;
    placement.visibleLeft       = bounds.left;
    placement.visibleRight      = bounds.right - (list->IsScrollbarVisible() ? list->GetScrollbarWidthPx() : 0);
    placement.padLeftPx         = scaler.ToPx (list->GetCellPadLeftDip());
    placement.padRightPx        = scaler.ToPx (list->GetCellPadRightDip());
    placement.fontPx            = scaler.ToPxf (list->GetFontSizeDip());
    placement.face              = list->IsMonospace() ? DxuiTheme::kMonoFace : DxuiTheme::kBodyFace;
    placement.contentBackground = list->GetRowBackground (*theme);
    placement.rowFill           = GetRowFill (list, row);
    placement.ink               = theme->Foreground();
    placement.dpi               = scaler.GetDpi();

    isHidden           = OperandResultTip::IsCellHidden ((*cells)[column], placement, text);
    edge               = GetTipEdge (placement, isHidden);
    placement.workArea = DxuiPopupHost::GetEdgeWorkArea (edge, screen.workAreas);
    hasRoom            = placement.workArea.right > placement.workArea.left;
    CBR (hasRoom);

    isMade = OperandResultTip::TryMakeLayout ((*cells)[column], placement, text, out);
    CBR (isMade);

    outCell.view = view;
    outCell.row  = row;
    outCell.area = GetTipArea (placement, isHidden);

Error:
    return isMade;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::TryMakeOnText
//
//  The tip over a source line's operand and result when the view cuts them
//  off at its right edge, placed as the view draws them: where the cell's
//  text starts and where it is clipped, in the view's face at its zoom, in
//  the colors of the cell's runs, on the line's fill.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandTipController::TryMakeOnText (int view, int row, const DxuiDpiScaler & scaler, IDxuiTextRenderer & text, const OperandTipScreen & screen,
                                          OperandResultTip::Layout & out, OperandTipCell & outCell) const
{
    HRESULT                       hr       = S_OK;
    const DxuiTheme             * theme    = m_host.GetTipTheme();
    DxuiTextView                * textView = m_host.GetTipTextView (view);
    size_t                        column   = m_host.GetTipColumn (view, row);
    const DxuiTextView::Row     * shown    = nullptr;
    DxuiListView::Cell            cell;
    RECT                          edge     = {};
    bool                          isShown  = false;
    bool                          isHidden = false;
    bool                          hasRoom  = false;
    bool                          isMade   = false;
    OperandResultTip::Placement   placement;



    isShown = theme != nullptr && textView != nullptr && textView->IsVisible() && row >= 0 && row < (int) textView->GetRows().size() &&
              textView->GetRows()[(size_t) row].cells.size() > column && textView->GetCellRectPx (row, (int) column, placement.textRect) &&
              textView->TryGetCellTextClipPx (row, (int) column, placement.clipLeftPx, placement.clipRightPx);
    CBR (isShown);

    shown     = &textView->GetRows()[(size_t) row];
    cell.text = shown->cells[column];

    for (const DxuiTextView::Span & span : shown->spans)
    {
        if (span.cell == (int) column && span.argb != 0)
        {
            cell.colorRanges.emplace_back (span.start, span.start + span.length, span.argb);
        }
    }

    placement.visibleLeft       = textView->GetTextAreaLeftPx();
    placement.visibleRight      = textView->GetTextAreaRightPx();
    placement.padLeftPx         = scaler.ToPx (kTextPadDip);
    placement.padRightPx        = scaler.ToPx (kTextPadDip);
    placement.fontPx            = scaler.ToPxf (theme->MonospaceFont().sizeDip * textView->GetZoom());
    placement.face              = theme->MonospaceFont().face;
    placement.contentBackground = theme->TextViewBackground();
    placement.rowFill           = shown->background;
    placement.ink               = theme->Foreground();
    placement.dpi               = scaler.GetDpi();

    isHidden           = OperandResultTip::IsCellHidden (cell, placement, text);
    edge               = GetTipEdge (placement, isHidden);
    placement.workArea = DxuiPopupHost::GetEdgeWorkArea (edge, screen.workAreas);
    hasRoom            = placement.workArea.right > placement.workArea.left;
    CBR (hasRoom);

    isMade = OperandResultTip::TryMakeLayout (cell, placement, text, out);
    CBR (isMade);

    outCell.view = view;
    outCell.row  = row;
    outCell.area = GetTipArea (placement, isHidden);

Error:
    return isMade;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::GetTipEdge
//
//  The top edge of the part of the cell in view. The tip lies within the
//  work areas of the monitors this edge crosses. For a cell wholly outside
//  the pane the edge spans the pane, since the tip lies below the row.
//
////////////////////////////////////////////////////////////////////////////////

RECT OperandTipController::GetTipEdge (const OperandResultTip::Placement & placement, bool isHidden)
{
    LONG  top = placement.textRect.top;



    if (isHidden)
    {
        return RECT { (LONG) placement.visibleLeft, top, (LONG) placement.visibleRight, top };
    }

    return RECT { placement.textRect.left, top, (std::min) (placement.textRect.right, (LONG) placement.visibleRight), top };
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::GetTipArea
//
//  Where the pointer has to be for the tip to come up: the part of the cell
//  in view from the line's start, or the whole row in view for a cell
//  wholly outside the pane.
//
////////////////////////////////////////////////////////////////////////////////

RECT OperandTipController::GetTipArea (const OperandResultTip::Placement & placement, bool isHidden)
{
    LONG  left = placement.isWholeLine ?(std::min) (placement.textRect.left, (LONG) placement.leadLeft) : placement.textRect.left;
    RECT  area = { (std::max) (left - (LONG) placement.padLeftPx, (LONG) placement.visibleLeft),
                   placement.textRect.top,
                   (std::min) (placement.textRect.right, (LONG) placement.visibleRight),
                   placement.textRect.bottom };



    if (isHidden)
    {
        area.left  = placement.visibleLeft;
        area.right = placement.visibleRight;
    }

    return area;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::IsTextSelectedUnderTip
//
//  Text selected in a row's operand, or in that of a row the tip extends
//  over, below it or, by the outline's pixel, above it. The list fills a
//  selection the full height of its row.
//
//  Below its row, or on its row from a line's start ahead of the cell, every
//  column is checked, whichever ones the tip covers.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandTipController::IsTextSelectedUnderTip (const DxuiListView * list, int row, size_t column, const OperandResultTip::Layout & layout,
                                                   const OperandTipCell & cell)
{
    int     height     = (std::max) (layout.lineHeight, 1);
    int     rowsAbove  = (cell.area.top - layout.rect.top + height - 1) / height;
    int     rowsBelow  = (layout.rect.bottom - cell.area.top + height - 1) / height;
    bool    hasLead    = layout.leadLeft < layout.textOrigin.x;
    size_t  columns    = list->GetColumnCount();
    bool    isSelected = false;



    for (int each = row - rowsAbove; each < row + rowsBelow && !isSelected; each++)
    {
        isSelected = list->IsCellTextSelected (each, column);

        for (size_t other = 0; (each > row || hasLead) && other < columns && !isSelected; other++)
        {
            isSelected = list->IsCellTextSelected (each, other);
        }
    }

    return isSelected;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::GetRowFill
//
//  What a disassembly list fills a row with under its cells, as it paints
//  them: the selection on the selected row while the list has the focus and
//  no text is selected, or else the hover. A tip's row is always the row
//  under the pointer, and the window passes a move to the tip ahead of the
//  list, so the list may not have marked the row as hovered yet; a fill
//  read from the list's hover then changed under a tip already up, and took
//  it down.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t OperandTipController::GetRowFill (const DxuiListView * list, int row) const
{
    const DxuiTheme  & theme      = *m_host.GetTipTheme();
    bool               isSelected = list->IsListFocused() && !list->HasTextSelection() &&
                                    (list->IsMultiSelect() ? list->IsRowSelected (row) : row == list->GetSelectedRow());



    return isSelected ? theme.ContentSelection() : theme.ContentHover();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::GetScreen
//
//  The screen around a window, in its client pixels: every monitor's work
//  area, and every window the debugger window owns that is visible, other
//  than the one the tip is in and the popups. A floating pane, command bar
//  or timeline is such a window, and lies over this one; a popup, such as a
//  tooltip, a menu, a color key or the tip itself, is not. Nothing where the
//  window has no HWND.
//
////////////////////////////////////////////////////////////////////////////////

OperandTipScreen OperandTipController::GetScreen (const DxuiHwndSource * host) const
{
    HRESULT           hr       = S_OK;
    OperandTipScreen  screen;
    WindowSearch      search;
    HWND              hwnd     = host->GetHwnd();
    POINT             origin   = {};
    bool              isPlaced = false;
    bool              isListed = false;



    isPlaced = hwnd != nullptr && ClientToScreen (hwnd, &origin) != FALSE;
    CBR (isPlaced);

    search.owner = m_host.GetTipOwner();
    search.tipIn = hwnd;

    isListed = EnumThreadWindows (GetWindowThreadProcessId (hwnd, nullptr), CollectWindowOver, (LPARAM) &search) != FALSE;
    IGNORE_RETURN_VALUE (isListed, false);

    screen.workAreas   = DxuiPopupHost::GetMonitorWorkAreas();
    screen.overWindows = std::move (search.rects);

    for (std::vector<RECT> * rects : { &screen.workAreas, &screen.overWindows })
    {
        for (RECT & each : *rects)
        {
            OffsetRect (&each, -origin.x, -origin.y);
        }
    }

Error:
    return screen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::CollectWindowOver
//
//  EnumThreadWindows' callback for GetScreen: a window the search's owner
//  owns, visible and not minimized, and not a popup. A popup has
//  WS_EX_NOACTIVATE while it is up; a tool window has it only while it is
//  being created.
//
////////////////////////////////////////////////////////////////////////////////

BOOL CALLBACK OperandTipController::CollectWindowOver (HWND hwnd, LPARAM param)
{
    HRESULT          hr      = S_OK;
    WindowSearch   & search  = *(WindowSearch *) param;
    LONG_PTR         exStyle = GetWindowLongPtrW (hwnd, GWL_EXSTYLE);
    RECT             rect    = {};
    bool             isOver  = false;
    bool             isRead  = false;



    isOver = hwnd != search.tipIn && GetWindow (hwnd, GW_OWNER) == search.owner && IsWindowVisible (hwnd) != FALSE &&
             IsIconic (hwnd) == FALSE && (exStyle & WS_EX_NOACTIVATE) == 0;
    CBR (isOver);

    isRead = GetWindowRect (hwnd, &rect) != FALSE;
    CBR (isRead);

    search.rects.push_back (rect);

Error:
    return TRUE;
}
