#include "Pch.h"

#include "Ui/Debugger/Panes/OperandTipController.h"

#include "Ui/Debugger/DisassemblyCells.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::OperandTipController
//
////////////////////////////////////////////////////////////////////////////////

OperandTipController::OperandTipController (IOperandTipHost & host, const ICodeViews & views) :
    m_host  (host),
    m_views (views)
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
//  pointer moves off the cell or does anything but move: a press, the wheel,
//  a button pressed and not released, a menu opening, or the PC's marker
//  being dragged. New text or
//  a new place takes down the popup that drew the old, so the new is drawn.
//
////////////////////////////////////////////////////////////////////////////////

void OperandTipController::Place (const DxuiMouseEvent & ev, bool isPressed, const DxuiDpiScaler & scaler, IDxuiTextRenderer & text,
                                  const OperandTipScreen & screen)
{
    OperandResultTip::Layout  layout;
    OperandTipCell            cell;
    bool                      isOver = false;



    isOver = ev.kind == DxuiMouseEventKind::Move && !isPressed && !m_host.IsTipBlocked() &&
             TryGet (ev.positionDip, scaler, text, screen, layout, cell);

    if (!isOver)
    {
        Hide();
        return;
    }

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
//  The next move of the pointer brings back a tip for the new cell. The
//  list fits its columns to new rows only as it paints, so they are fitted
//  here first, as that paint will fit them, and the tip is laid out again in
//  full, on the screen as it is now: a column ahead of the operand widened
//  by the new rows moves the cell as surely as new text does, and the PC
//  leaving the row changes its fill.
//
////////////////////////////////////////////////////////////////////////////////

void OperandTipController::Check (int view)
{
    OperandResultTip::Layout  layout;
    OperandTipCell            cell;
    bool                      isSame = false;



    if (!m_hasTip || view != m_cell.view || m_text == nullptr)
    {
        return;
    }

    if (m_popupHost != nullptr)
    {
        m_scaler = m_popupHost->GetScaler();
        m_screen = GetScreen (m_popupHost);
    }

    m_views.GetCodeList (view)->MeasureColumnsPx (*m_text);

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
//  The pointer anywhere but on the part of the cell the tip was put up for
//  takes the tip down, however it got there.
//
////////////////////////////////////////////////////////////////////////////////

void OperandTipController::CheckPointer (POINT clientPx)
{
    if (m_hasTip && !DxuiDockSite::Contains (m_cell.area, clientPx))
    {
        Hide();
    }
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

    isRead  = GetCursorPos (&pointer) != FALSE && ScreenToClient (hwnd, &pointer) != FALSE;
    isShown = IsWindowVisible (hwnd) != FALSE && IsIconic (hwnd) == FALSE;

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
    isSelected = IsTextSelectedUnderTip (m_views.GetCodeList (view), row, layout);
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
//  The tip over a row's operand and result when the pane cuts them off,
//  placed as the list draws the cell: where its text starts, the pane's
//  edges short of its scrollbar, its padding, face and size, and the theme's
//  colors under and around it, the row's own fill among them. The cell's
//  area is the part of it visible in the pane.
//
//  The tip is within the work area of the monitor where the cell's text
//  starts, joined by the next monitor's where the cell runs on into it, the
//  work area the popup host limits the tip's popup to: a tip over a cell
//  across two monitors covers it on both.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandTipController::TryMake (int view, int row, const DxuiDpiScaler & scaler, IDxuiTextRenderer & text, const OperandTipScreen & screen,
                                    OperandResultTip::Layout & out, OperandTipCell & outCell) const
{
    HRESULT                                   hr        = S_OK;
    const DxuiTheme                         * theme     = m_host.GetTipTheme();
    DxuiListView                            * list      = m_views.GetCodeList (view);
    const std::vector<DxuiListView::Cell>   * cells     = nullptr;
    RECT                                      bounds    = {};
    RECT                                      edge      = {};
    size_t                                    cellCount = 0;
    bool                                      isShown   = false;
    bool                                      hasRoom   = false;
    bool                                      isMade    = false;
    OperandResultTip::Placement               placement;



    isShown = theme != nullptr && list != nullptr && list->IsVisible() &&
              list->GetCellTextRectPx (row, DisassemblyCells::kOperandColumn, placement.textRect);
    CBR (isShown);

    cells     = &list->GetCellsOfRow (row);
    cellCount = cells->size();
    CBR (cellCount > DisassemblyCells::kOperandColumn);

    bounds = list->GetBounds();
    OffsetRect (&placement.textRect, bounds.left, bounds.top);

    placement.visibleLeft       = bounds.left;
    placement.visibleRight      = bounds.right - (list->IsScrollbarVisible() ? list->GetScrollbarWidthPx() : 0);
    placement.padLeftPx         = scaler.ToPx (list->GetCellPadLeftDip());
    placement.padRightPx        = scaler.ToPx (list->GetCellPadRightDip());
    placement.fontPx            = scaler.ToPxf (list->GetFontSizeDip());
    placement.face              = list->IsMonospace() ? DxuiTheme::kMonoFace : DxuiTheme::kBodyFace;
    placement.contentBackground = list->GetRowBackground (*theme);
    placement.rowFill           = GetRowFill (list, row);
    placement.ink               = theme->Foreground();
    placement.border            = theme->TooltipBorder();
    placement.dpi               = scaler.GetDpi();

    edge               = RECT { placement.textRect.left, placement.textRect.top, (std::min) (placement.textRect.right, (LONG) placement.visibleRight), placement.textRect.top };
    placement.workArea = DxuiPopupHost::GetEdgeWorkArea (edge, screen.workAreas);
    hasRoom            = placement.workArea.right > placement.workArea.left;
    CBR (hasRoom);

    isMade = OperandResultTip::TryMakeLayout ((*cells)[DisassemblyCells::kOperandColumn], placement, text, out);
    CBR (isMade);

    outCell.view = view;
    outCell.row  = row;
    outCell.area = RECT { (std::max) (placement.textRect.left - (LONG) placement.padLeftPx, bounds.left),
                          placement.textRect.top,
                          (std::min) (placement.textRect.right, (LONG) placement.visibleRight),
                          placement.textRect.bottom };

Error:
    return isMade;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::IsTextSelectedUnderTip
//
//  Text selected in a row's operand, or in that of a row the tip extends
//  over, below it or, by the outline's pixel, above it. The list fills a
//  selection the full height of its row.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandTipController::IsTextSelectedUnderTip (const DxuiListView * list, int row, const OperandResultTip::Layout & layout)
{
    int   height     = (std::max) (layout.lineHeight, 1);
    int   rowsAbove  = (layout.textOrigin.y - layout.rect.top + height - 1) / height;
    int   rowsBelow  = (layout.rect.bottom - layout.textOrigin.y + height - 1) / height;
    bool  isSelected = false;



    for (int each = row - rowsAbove; each < row + rowsBelow && !isSelected; each++)
    {
        isSelected = list->IsCellTextSelected (each, DisassemblyCells::kOperandColumn);
    }

    return isSelected;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::GetRowFill
//
//  What a disassembly list fills a row with under its cells, as it paints
//  them: the selection on the selected row while the list has the focus and
//  no text is selected, the hover on the row under the pointer, or its own
//  background.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t OperandTipController::GetRowFill (const DxuiListView * list, int row) const
{
    const DxuiTheme  & theme      = *m_host.GetTipTheme();
    bool               isSelected = list->IsListFocused() && !list->HasTextSelection() &&
                                    (list->IsMultiSelect() ? list->IsRowSelected (row) : row == list->GetSelectedRow());
    bool               isHovered  = row == list->GetHoveredRow();



    return isSelected ? theme.ContentSelection() : (isHovered ? theme.ContentHover() : list->GetRowBackground (theme));
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
