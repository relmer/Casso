#include "Pch.h"

#include "Ui/Debugger/Panes/OperandTipController.h"

#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/Panes/SourcePane.h"





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
//  The screen is read only for a move over a code row, the one place a tip
//  can appear. A window with no HWND, as a test builds it, measures with the
//  renderer and screen the test sets.
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
//  The tip is up as soon as the pointer is over the row, and goes when the
//  pointer moves off the row or does anything but move or turn the wheel: a
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

    SetHighlight (m_host.GetTipTextView (cell.view), cell.row);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::SetHighlight
//
//  A text view doesn't draw a hover, so the row a tip is up for is
//  highlighted as a list highlights the row under the pointer, from the
//  view's left edge up to the tip's fill.
//
////////////////////////////////////////////////////////////////////////////////

void OperandTipController::SetHighlight (DxuiTextView * view, int row)
{
    if (m_lit != nullptr && m_lit != view)
    {
        m_lit->SetHighlightRow (-1);
    }

    m_lit = view;

    if (m_lit != nullptr)
    {
        m_lit->SetHighlightRow (row);
    }
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
    m_tip.ShowShaped (layout.rect, [layout] (IDxuiPainter & painter, IDxuiTextRenderer & tipText)
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
    SetHighlight (nullptr, -1);

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
//  The pointer anywhere but on the part of the row in view the tip was put
//  up for, or on the tip itself, takes the tip down, however it got there.
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
//  The row the tip was put up for and the tip's rect, joined into one
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
//  OperandTipController::IsSymbolShownAt
//
////////////////////////////////////////////////////////////////////////////////

bool OperandTipController::IsSymbolShownAt (POINT clientPx) const
{
    return m_hasTip && m_layout.hasSymbol && DxuiDockSite::Contains (m_cell.area, clientPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::TryGet
//
//  Over a row of any code view, the tip for the text the view cuts off,
//  unless something lying over the pane overlaps the tip or is under the
//  pointer, or text selected in the view lies under the tip. The tip would
//  hide the selection, and with its text laid out otherwise than the row's,
//  could not draw it where it was made; so while there is one, the view's
//  drawing of the row, selection and all, remains in view.
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
    isSelected = (m_host.GetTipList (view) != nullptr) ? IsTextSelectedUnderTip (m_host.GetTipList (view), row, layout)
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
////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::TryMakeOnList
//
//  The tip over a list's row where the list cuts its text off, placed as the
//  list draws each cell: where its text starts and where it is clipped, the
//  pane's edges short of its scrollbar, its padding, face and size, and the
//  theme's colors under and around it, the row's fill among them. The row
//  starts at the list's left edge, as the list's highlight does, and its
//  area is the row in view.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandTipController::TryMakeOnList (int view, int row, const DxuiDpiScaler & scaler, IDxuiTextRenderer & text, const OperandTipScreen & screen,
                                          OperandResultTip::Layout & out, OperandTipCell & outCell) const
{
    HRESULT                                   hr         = S_OK;
    const DxuiTheme                         * theme      = m_host.GetTipTheme();
    DxuiListView                            * list       = m_host.GetTipList (view);
    OperandTipParts                           parts      = m_host.GetTipParts (view, row);
    const std::vector<DxuiListView::Cell>   * cells      = nullptr;
    RECT                                      bounds     = {};
    RECT                                      cell       = {};
    uint32_t                                  background = 0;
    bool                                      isShown    = false;
    bool                                      isPlaced   = false;
    bool                                      isAdded    = false;
    bool                                      hasRoom    = false;
    bool                                      isMade     = false;
    float                                     clipLeft   = 0.0f;
    float                                     clipRight  = 0.0f;
    OperandResultTip::Placement               placement;



    isShown = theme != nullptr && list != nullptr && list->IsVisible() && row >= 0 && row < list->GetRowCount();
    CBR (isShown);

    cells  = &list->GetCellsOfRow (row);
    bounds = list->GetBounds();

    for (size_t c = 0; c < cells->size() && c < list->GetColumnCount(); c++)
    {
        if (!list->GetCellTextRectPx (row, c, cell))
        {
            continue;
        }

        if (!isPlaced)
        {
            placement.row = RECT { bounds.left, bounds.top + cell.top, bounds.right - (list->IsScrollbarVisible() ? list->GetScrollbarWidthPx() : 0),
                                   bounds.top + cell.bottom };
            isPlaced      = true;
        }

        if (c == parts.codeColumn && list->TryGetCellTextClipPx (row, c, clipLeft, clipRight))
        {
            placement.codeLeftPx = clipLeft + (float) bounds.left;
        }

        isAdded    = TryAddListSegment (*list, row, c, parts, theme->Foreground(), placement);
        background = (isAdded && background == 0) ? (*cells)[c].background : background;

        //  The list draws nothing after a cell that runs on to the row's end.
        if ((*cells)[c].spansRow)
        {
            break;
        }
    }

    CBR (isPlaced);

    placement.codeLeftPx        = (placement.codeLeftPx > 0.0f || placement.segments.empty()) ? placement.codeLeftPx : placement.segments.front().leftPx;
    placement.visibleLeft       = bounds.left;
    placement.symbol            = parts.symbol;
    placement.padLeftPx         = scaler.ToPx (list->GetCellPadLeftDip());
    placement.padRightPx        = scaler.ToPx (list->GetCellPadRightDip());
    placement.fontPx            = scaler.ToPxf (list->GetFontSizeDip());
    placement.face              = list->IsMonospace() ? DxuiTheme::kMonoFace : DxuiTheme::kBodyFace;
    placement.fill              = DxuiColor::Composite (background, DxuiColor::Composite (GetRowFill (list, row), list->GetRowBackground (*theme)));
    placement.dpi               = scaler.GetDpi();
    placement.workArea          = GetRowWorkArea (placement.row, screen.workAreas);

    hasRoom = placement.workArea.right > placement.workArea.left;
    CBR (hasRoom);

    isMade = OperandResultTip::TryMakeLayout (placement, text, out);
    CBR (isMade);

    outCell.view = view;
    outCell.row  = row;
    outCell.area = placement.row;

Error:
    return isMade;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::TryAddListSegment
//
//  A cell with text, as the list draws it, in its colors and its italics.
//  The operand and result cell has where its result starts, and on a source
//  line's row the comment after it, two spaces past them; any other cell
//  that runs on to the row's end is a source or data line. False for a cell
//  with no text or none drawn.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandTipController::TryAddListSegment (
    const DxuiListView              & list,
    int                               row,
    size_t                            column,
    const OperandTipParts           & parts,
    uint32_t                          ink,
    OperandResultTip::Placement     & placement)
{
    constexpr const wchar_t      * kpszGap   = L"  ";
    const DxuiListView::Cell     & cell      = list.GetCellsOfRow (row)[column];
    RECT                           bounds    = list.GetBounds();
    bool                           isOperand = column == parts.operandColumn;
    size_t                         resultAt  = cell.text.find (SourcePane::kpszResultPrefix);
    size_t                         commentAt = cell.text.find (std::wstring (kpszGap) + L";");
    OperandResultTip::Segment      segment;



    if (cell.text.empty() || !list.TryGetCellTextClipPx (row, column, segment.leftPx, segment.clipRightPx))
    {
        return false;
    }

    segment.text         = cell.text;
    segment.colors       = GetCellColors (cell, ink);
    segment.leftPx      += (float) bounds.left;
    segment.clipRightPx += (float) bounds.left;
    segment.isItalic     = cell.weight == DxuiFontWeight::Italic;
    segment.isWholeLine  = cell.spansRow && !isOperand;

    if (isOperand)
    {
        segment.resultAt      = (resultAt  != std::wstring::npos) ? (int) resultAt : -1;
        segment.commentAt     = (cell.spansRow && commentAt != std::wstring::npos) ? (int) (commentAt + wcslen (kpszGap)) : -1;
        placement.operand     = (int) placement.segments.size();
        placement.symbolInk   = (cell.argb != 0) ? cell.argb : ink;
    }

    placement.segments.push_back (std::move (segment));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::GetCellColors
//
//  Each character's color: the cell's color, or `ink` where the cell's is
//  zero, and each range's color over it.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint32_t> OperandTipController::GetCellColors (
    const DxuiListView::Cell  & cell,
    uint32_t                    ink)
{
    std::vector<uint32_t>  colors (cell.text.size(), (cell.argb != 0) ? cell.argb : ink);



    for (const auto & [first, end, argb] : cell.colorRanges)
    {
        for (int k = (std::max) (0, first); k < end && k < (int) colors.size(); k++)
        {
            colors[(size_t) k] = argb;
        }
    }

    return colors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::TryMakeOnText
//
//  The tip over a source line where the view cuts its text off, placed as
//  the view draws each cell: where its text starts and where it is clipped,
//  in the view's face at its zoom, in the colors of the cell's runs, on the
//  line's fill under the hover. The row starts at the view's left edge.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandTipController::TryMakeOnText (int view, int row, const DxuiDpiScaler & scaler, IDxuiTextRenderer & text, const OperandTipScreen & screen,
                                          OperandResultTip::Layout & out, OperandTipCell & outCell) const
{
    HRESULT                       hr       = S_OK;
    const DxuiTheme             * theme    = m_host.GetTipTheme();
    DxuiTextView                * textView = m_host.GetTipTextView (view);
    OperandTipParts               parts    = m_host.GetTipParts (view, row);
    RECT                          cell     = {};
    bool                          isShown  = false;
    bool                          hasRoom  = false;
    bool                          isMade   = false;
    OperandResultTip::Placement   placement;



    isShown = theme != nullptr && textView != nullptr && textView->IsVisible() && row >= 0 && row < (int) textView->GetRows().size() &&
              textView->GetCellRectPx (row, 0, cell);
    CBR (isShown);

    placement.row               = RECT { textView->GetBounds().left, cell.top, textView->GetTextAreaRightPx(), cell.bottom };
    placement.visibleLeft       = textView->GetTextAreaLeftPx();
    placement.symbol            = parts.symbol;
    placement.padLeftPx         = scaler.ToPx (kTextPadDip);
    placement.padRightPx        = scaler.ToPx (kTextPadDip);
    placement.fontPx            = scaler.ToPxf (theme->MonospaceFont().sizeDip * textView->GetZoom());
    placement.face              = theme->MonospaceFont().face;
    placement.fill              = DxuiColor::Composite (textView->GetRows()[(size_t) row].background,
                                                        DxuiColor::Composite (theme->ContentHover(), theme->TextViewBackground()));
    placement.dpi               = scaler.GetDpi();
    placement.workArea          = GetRowWorkArea (placement.row, screen.workAreas);

    AddTextSegments (*textView, row, parts, theme->Foreground(), text, placement);

    hasRoom = placement.workArea.right > placement.workArea.left;
    CBR (hasRoom);

    isMade = OperandResultTip::TryMakeLayout (placement, text, out);
    CBR (isMade);

    outCell.view = view;
    outCell.row  = row;
    outCell.area = placement.row;

Error:
    return isMade;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::AddTextSegments
//
//  Each cell of a source line with text, from where the view starts it to
//  where it clips it, in the colors of its runs. The source text is a whole
//  line, which wraps by its comment, and the row's code starts the parts'
//  characters into it.
//
////////////////////////////////////////////////////////////////////////////////

void OperandTipController::AddTextSegments (
    const DxuiTextView              & view,
    int                               row,
    const OperandTipParts           & parts,
    uint32_t                          ink,
    IDxuiTextRenderer               & text,
    OperandResultTip::Placement     & placement)
{
    const DxuiTextView::Row  & textRow = view.GetRows()[(size_t) row];
    HRESULT                    hr      = S_OK;
    float                      height  = 0.0f;
    float                      codePx  = 0.0f;



    for (size_t c = 0; c < textRow.cells.size(); c++)
    {
        OperandResultTip::Segment  segment;

        if (textRow.cells[c].empty() || !view.TryGetCellTextClipPx (row, (int) c, segment.leftPx, segment.clipRightPx))
        {
            continue;
        }

        segment.text        = textRow.cells[c];
        segment.colors.assign (segment.text.size(), ink);
        segment.isWholeLine = c == parts.codeColumn;

        for (const DxuiTextView::Span & span : textRow.spans)
        {
            for (int k = (std::max) (0, span.start); span.cell == (int) c && span.argb != 0 && k < span.start + span.length && k < (int) segment.colors.size(); k++)
            {
                segment.colors[(size_t) k] = span.argb;
            }
        }

        if (c == parts.codeColumn)
        {
            hr = text.MeasureString (segment.text.substr (0, (size_t) parts.codeChar).c_str(), placement.fontPx, placement.face, codePx, height);
            IGNORE_RETURN_VALUE (hr, S_OK);

            placement.codeLeftPx = segment.leftPx + codePx;
        }

        if (c == parts.operandColumn)
        {
            segment.resultAt    = (int) segment.text.find (SourcePane::kpszResultPrefix);
            placement.operand   = (int) placement.segments.size();
            placement.symbolInk = segment.colors.front();
        }

        placement.segments.push_back (std::move (segment));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::GetRowWorkArea
//
//  The work area of the monitor the row starts on, joined by the monitor's
//  directly to its right, if any; none where the row's start is on no
//  monitor.
//
////////////////////////////////////////////////////////////////////////////////

RECT OperandTipController::GetRowWorkArea (
    const RECT               & row,
    const std::vector<RECT>  & workAreas)
{
    RECT  start = {};



    for (const RECT & area : workAreas)
    {
        if (row.left >= area.left && row.left < area.right && row.top >= area.top && row.top < area.bottom)
        {
            start = area;
        }
    }

    if (start.right <= start.left)
    {
        return start;
    }

    return DxuiPopupHost::GetEdgeWorkArea (RECT { row.left, row.top, start.right + 1, row.top }, workAreas);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandTipController::IsTextSelectedUnderTip
//
//  Text selected in any column of the row or of a row the tip extends over.
//  The list fills a selection the full height of its row.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandTipController::IsTextSelectedUnderTip (const DxuiListView * list, int row, const OperandResultTip::Layout & layout)
{
    int     height     = (std::max) (layout.lineHeight, 1);
    int     first      = row + (layout.rect.top - layout.rowTop) / height;
    int     end        = row + (std::max) (layout.lineCount, 1);
    size_t  columns    = list->GetColumnCount();
    bool    isSelected = false;



    for (int each = first; each < end && !isSelected; each++)
    {
        for (size_t column = 0; column < columns && !isSelected; column++)
        {
            isSelected = list->IsCellTextSelected (each, column);
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
