#include "Pch.h"

#include "Ui/Debugger/OperandResultTip.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/Panes/SourcePane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::TryMakeLayout
//
//  Only where the list cuts the text off: at the pane's edge, or at the
//  column's edge where the column has been made narrower than its text. The
//  tip extends past the pane to the end of its text, wraps only at the
//  screen's right edge, and covers the whole of the cell in view, so none of
//  the cut text is visible beside it; the cell's padding is either side of
//  the text, and its outline a pixel past the row above and below, as
//  Explorer's is.
//
//  The tip stays inside the work area it is given without moving off the
//  cell: a popup kept on the screen by being moved would no longer line its
//  text up with the cell's.
//
//  A cell wholly outside the pane has no text in view to lay the first line
//  over, so every line, the operand's and then the result's, lies below the
//  row, from the pane's left edge.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandResultTip::TryMakeLayout (
    const DxuiListView::Cell  & cell,
    const Placement           & placement,
    IDxuiTextRenderer         & text,
    Layout                    & out)
{
    HRESULT  hr         = S_OK;
    Layout   layout;
    bool     isMade     = false;
    bool     hasText    = !cell.text.empty();
    bool     isUsable   = false;
    bool     isHidden   = false;
    bool     isOnScreen = false;
    bool     doesFit    = false;
    int      textLeft   = (int) placement.textRect.left;
    int      rowTop     = (int) placement.textRect.top;
    int      rowHeight  = (int) (placement.textRect.bottom - placement.textRect.top);
    int      cellRight  = (std::min) ((int) placement.textRect.right, placement.visibleRight);
    float    fullPx     = 0.0f;
    float    widestPx   = 0.0f;



    isUsable = hasText && placement.fontPx > 0.0f && placement.face != nullptr && rowHeight > 0;
    CBR (isUsable);

    isHidden = IsCellHidden (cell, placement, text);

    if (isHidden)
    {
        textLeft  = (std::max) (placement.visibleLeft, (int) placement.workArea.left) + placement.padLeftPx;
        rowTop   += rowHeight;
        cellRight = textLeft;
    }

    //  A start scrolled out of sight, or off the screen, leaves nothing to
    //  lay the tip over.
    isOnScreen = (isHidden || (textLeft >= placement.visibleLeft && placement.clipLeftPx < placement.clipRightPx)) &&
                 textLeft >= placement.workArea.left && textLeft + placement.padRightPx < placement.workArea.right &&
                 rowTop >= placement.workArea.top;
    CBR (isOnScreen);

    layout.text       = cell.text;
    layout.fontPx     = placement.fontPx;
    layout.face       = placement.face;
    layout.lineHeight = rowHeight;
    layout.border     = placement.border;
    layout.textOrigin = POINT { textLeft, rowTop };
    layout.leadLeft   = textLeft;
    layout.fill       = MakeFill   (cell, placement.rowFill, placement.contentBackground);
    layout.colors     = MakeColors (cell, placement.ink, layout.fill);

    //  Text that cannot be measured cannot be said to be cut off. Measured
    //  against the box the view clips its text to, the tip is up exactly
    //  when the view's drawing of the text is clipped.
    fullPx = MeasurePx (text, cell.text, placement.fontPx, placement.face);
    CBR (fullPx > 0.0f && (isHidden || fullPx > placement.clipRightPx - placement.clipLeftPx));

    if (placement.isWholeLine && !isHidden)
    {
        SetLead (placement, text, layout);
    }

    doesFit = TryWrapOnScreen (text, placement, layout, widestPx);
    CBR (doesFit);

    layout.rect = PlaceRect (placement, layout, cellRight, widestPx);

    out    = std::move (layout);
    isMade = true;

Error:
    return isMade;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::IsCellHidden
//
//  Its text starts where the view clips it, or past the pane's right edge;
//  or the drawn part of its text ends left of the pane's left edge. A cell
//  with no text, or text that cannot be measured, is not hidden.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandResultTip::IsCellHidden (
    const DxuiListView::Cell  & cell,
    const Placement           & placement,
    IDxuiTextRenderer         & text)
{
    bool   isUsable  = !cell.text.empty() && placement.fontPx > 0.0f && placement.face != nullptr;
    float  textPx    = isUsable ? MeasurePx (text, cell.text, placement.fontPx, placement.face) : 0.0f;
    float  shownEnd  = (std::min) (placement.clipLeftPx + textPx, placement.clipRightPx);
    float  rightEdge = (std::min) (placement.clipRightPx, (float) placement.visibleRight);



    return textPx > 0.0f && (placement.clipLeftPx >= rightEdge || shownEnd <= (float) placement.visibleLeft);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::IsSame
//
////////////////////////////////////////////////////////////////////////////////

bool OperandResultTip::IsSame (
    const Layout  & a,
    const Layout  & b)
{
    return a.rect.left    == b.rect.left    &&
           a.leadLeft     == b.leadLeft     &&
           a.leadLength   == b.leadLength   &&
           a.rect.top     == b.rect.top     &&
           a.rect.right   == b.rect.right   &&
           a.rect.bottom  == b.rect.bottom  &&
           a.textOrigin.x == b.textOrigin.x &&
           a.textOrigin.y == b.textOrigin.y &&
           a.lineHeight   == b.lineHeight   &&
           a.fontPx       == b.fontPx       &&
           a.face         == b.face         &&
           a.fill         == b.fill         &&
           a.border       == b.border       &&
           a.text         == b.text         &&
           a.colors       == b.colors       &&
           a.lines        == b.lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::Paint
//
//  The tip's popup is cleared to the layout's fill before this runs, so only
//  the outline and the text are drawn: the lead from its left on the first
//  line, then each line from its indent.
//
////////////////////////////////////////////////////////////////////////////////

void OperandResultTip::Paint (
    const Layout       & layout,
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text)
{
    float  width  = (float) (layout.rect.right  - layout.rect.left);
    float  height = (float) (layout.rect.bottom - layout.rect.top);
    float  x      = (float) (layout.textOrigin.x - layout.rect.left);
    float  y      = (float) (layout.textOrigin.y - layout.rect.top);
    float  lead   = (float) (layout.leadLeft     - layout.rect.left);
    float  edge   = (float) kBorderPx;



    painter.OutlineRect (0.0f, 0.0f, width, height, edge, layout.border);

    if (layout.leadLength > 0)
    {
        PaintLine (layout, Line { 0, layout.leadLength }, lead, y, x - lead, text);
    }

    for (const Line & line : layout.lines)
    {
        PaintLine (layout, line, x + line.indentPx, y, width - x - line.indentPx, text);
        y += (float) layout.lineHeight;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::GetPopupPx
//
//  The in-place tip gives its popup a size in DIPs, rounded up, and the popup
//  turns that back into pixels, rounded to the nearest: at 125%, 150% and
//  175% that can come back a pixel larger than it went in.
//
////////////////////////////////////////////////////////////////////////////////

int OperandResultTip::GetPopupPx (
    int   px,
    UINT  dpi)
{
    UINT  scale = (dpi == 0) ? DxuiDpiScaler::kBaseDpi : dpi;
    int   dip   = (int) std::ceil ((float) px * (float) DxuiDpiScaler::kBaseDpi / (float) scale);



    return MulDiv (dip, (int) scale, (int) DxuiDpiScaler::kBaseDpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::MeasurePx
//
//  Zero where the renderer cannot measure.
//
////////////////////////////////////////////////////////////////////////////////

float OperandResultTip::MeasurePx (
    IDxuiTextRenderer   & text,
    const std::wstring  & chars,
    float                 fontPx,
    const wchar_t       * face)
{
    HRESULT  hr     = S_OK;
    float    width  = 0.0f;
    float    height = 0.0f;



    hr = text.MeasureString (chars.c_str(), fontPx, face, width, height);
    IGNORE_RETURN_VALUE (hr, S_OK);

    return width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::MeasureWidestPx
//
//  From the text origin to the end of the line that ends furthest right.
//
////////////////////////////////////////////////////////////////////////////////

float OperandResultTip::MeasureWidestPx (
    IDxuiTextRenderer  & text,
    const Layout       & layout)
{
    float  widestPx = 0.0f;
    float  linePx   = 0.0f;



    for (const Line & line : layout.lines)
    {
        linePx   = MeasurePx (text, layout.text.substr ((size_t) line.start, (size_t) line.length), layout.fontPx, layout.face);
        widestPx = (std::max) (widestPx, line.indentPx + linePx);
    }

    return widestPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::GetBottomPx
//
//  A row for each line, and the outline a pixel past the last.
//
////////////////////////////////////////////////////////////////////////////////

int OperandResultTip::GetBottomPx (const Layout & layout)
{
    return (int) layout.textOrigin.y + (int) layout.lines.size() * layout.lineHeight + kBorderPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::TryWrapOnScreen
//
//  The text extends to the right to its end, and wraps only where it is
//  wider than the room on the screen right of it. False where it is
//  then too tall for the screen below the row.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandResultTip::TryWrapOnScreen (
    IDxuiTextRenderer  & text,
    const Placement    & placement,
    Layout             & layout,
    float              & widestPx)
{
    float  roomPx   = (float) (placement.workArea.right - placement.padRightPx - layout.textOrigin.x);
    bool   hasLines = false;



    if (placement.isWholeLine)
    {
        WrapLine (text, layout, roomPx);
    }
    else
    {
        WrapCell (text, layout, roomPx);
    }

    hasLines = !layout.lines.empty();
    widestPx = MeasureWidestPx (text, layout);

    return hasLines && widestPx <= roomPx && GetBottomPx (layout) <= placement.workArea.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::PlaceRect
//
//  From the line's left, the lead's or the cell's, to the end of its widest
//  line, or to the edge of the cell in view where that is further, so the
//  tip covers the whole of what the list draws. The padding and the outline
//  are dropped at the pane's edge and the screen's.
//
//  Its width and height are ones its popup comes up at exactly, larger where
//  the screen has room and smaller where it does not, so no strip of the
//  popup's fill shows past the outline.
//
////////////////////////////////////////////////////////////////////////////////

RECT OperandResultTip::PlaceRect (
    const Placement  & placement,
    const Layout     & layout,
    int                cellRight,
    float              widestPx)
{
    int   textLeft  = (int) layout.textOrigin.x;
    int   textRight = textLeft + (int) std::ceil (widestPx) + placement.padRightPx;
    RECT  rect      = {};



    rect.left   = (std::max) ({ layout.leadLeft - placement.padLeftPx, placement.visibleLeft, (int) placement.workArea.left });
    rect.top    = (std::max) ((int) layout.textOrigin.y - kBorderPx, (int) placement.workArea.top);
    rect.right  = (std::min) ((std::max) (cellRight, textRight), (int) placement.workArea.right);
    rect.bottom = GetBottomPx (layout);

    rect.right  = rect.left + FitPopupPx (rect.right  - rect.left, placement.workArea.right  - rect.left, placement.dpi);
    rect.bottom = rect.top  + FitPopupPx (rect.bottom - rect.top,  placement.workArea.bottom - rect.top,  placement.dpi);

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::FitPopupPx
//
//  The nearest size from `px` up to `limitPx` that comes back from the
//  popup as it went in, or failing that the nearest below `px`.
//
////////////////////////////////////////////////////////////////////////////////

int OperandResultTip::FitPopupPx (
    int   px,
    int   limitPx,
    UINT  dpi)
{
    int   fit     = px;
    bool  isFound = false;



    for (int each = px; each <= limitPx && !isFound; each++)
    {
        isFound = GetPopupPx (each, dpi) == each;
        fit     = isFound ? each : fit;
    }

    for (int each = px - 1; each > 0 && !isFound; each--)
    {
        isFound = GetPopupPx (each, dpi) == each;
        fit     = isFound ? each : fit;
    }

    return fit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::MakeFill
//
//  The cell's own fill over the row's, over the list's, as the list paints
//  them: opaque, as each layer laid over the one under it leaves it.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t OperandResultTip::MakeFill (
    const DxuiListView::Cell  & cell,
    uint32_t                    rowFill,
    uint32_t                    contentBackground)
{
    return DxuiColor::Composite (cell.background, DxuiColor::Composite (rowFill, contentBackground));
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::MakeColors
//
//  Each character's color: the cell's own, or `ink` where it gives none, and
//  each range's over it, every one moved until it reads on `fill`, as the
//  list moves them.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint32_t> OperandResultTip::MakeColors (
    const DxuiListView::Cell  & cell,
    uint32_t                    ink,
    uint32_t                    fill)
{
    std::vector<uint32_t>  colors (cell.text.size(), (cell.argb != 0) ? cell.argb : ink);



    for (const auto & [first, end, argb] : cell.colorRanges)
    {
        for (int k = (std::max) (0, first); k < end && k < (int) colors.size(); k++)
        {
            colors[(size_t) k] = argb;
        }
    }

    for (uint32_t & each : colors)
    {
        each = DxuiColor::ComputeInkForContrast (each, fill, DebuggerTextColors::s_kMinTextContrast);
    }

    return colors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::SetLead
//
//  The tip starts at the line's start where that is left of the cell's text.
//  The part of the line drawn there goes ahead of the cell's text, in its
//  colors on the tip's fill, where it ends short of the cell's text.
//
////////////////////////////////////////////////////////////////////////////////

void OperandResultTip::SetLead (
    const Placement    & placement,
    IDxuiTextRenderer  & text,
    Layout             & layout)
{
    const std::wstring     & lead    = placement.lead.text;
    std::vector<uint32_t>    colors  = MakeColors (placement.lead, placement.ink, layout.fill);
    float                    leadPx  = lead.empty() ? 0.0f : MeasurePx (text, lead, placement.fontPx, placement.face);
    bool                     isAhead = placement.leadLeft < layout.textOrigin.x;
    bool                     doesFit = (float) placement.leadLeft + leadPx <= (float) layout.textOrigin.x;



    if (!isAhead)
    {
        return;
    }

    layout.leadLeft = placement.leadLeft;

    if (lead.empty() || !doesFit)
    {
        return;
    }

    layout.text.insert   (0, lead);
    layout.colors.insert (layout.colors.begin(), colors.begin(), colors.end());
    layout.leadLength = (int) lead.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::WrapCell
//
//  The operand, then the result from the start of a line of its own. The
//  result is where the cell's text gives its prefix; the spaces that set it
//  apart from the operand on one line go with the break.
//
////////////////////////////////////////////////////////////////////////////////

void OperandResultTip::WrapCell (
    IDxuiTextRenderer  & text,
    Layout             & layout,
    float                maxWidthPx)
{
    size_t  resultAt   = layout.text.find (SourcePane::kpszResultPrefix, (size_t) layout.leadLength);
    int     size       = (int) layout.text.size();
    int     operandEnd = (resultAt == std::wstring::npos) ? size : (int) resultAt;



    while (operandEnd > layout.leadLength && layout.text[(size_t) (operandEnd - 1)] == L' ')
    {
        operandEnd--;
    }

    WrapRun (text, layout, layout.leadLength, operandEnd, maxWidthPx, Indent());

    if (resultAt != std::wstring::npos)
    {
        WrapRun (text, layout, (int) resultAt, size, maxWidthPx, Indent());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::WrapLine
//
//  The whole line on one line where it fits the room, and else broken at its
//  spaces. A line that starts in the comment starts under the comment's
//  first character, after the ; and the spaces past it; a line that starts
//  before the comment starts four characters right of the code's start. An
//  indent with less than the least room after it is dropped.
//
////////////////////////////////////////////////////////////////////////////////

void OperandResultTip::WrapLine (
    IDxuiTextRenderer  & text,
    Layout             & layout,
    float                maxWidthPx)
{
    int     first     = layout.leadLength;
    int     size      = (int) layout.text.size();
    int     codeAt    = first;
    float   minRoomPx = MeasurePx (text, std::wstring (kMinIndentRoomChars, L'0'), layout.fontPx, layout.face);
    float   stepPx    = MeasurePx (text, std::wstring (kCodeIndentChars,    L'0'), layout.fontPx, layout.face);
    Indent  indent;



    while (codeAt < size && iswspace (layout.text[(size_t) codeAt]))
    {
        codeAt++;
    }

    indent.commentAt = FindCommentText (layout.text, first);
    indent.codePx    = MeasurePx (text, layout.text.substr ((size_t) first, (size_t) (codeAt - first)), layout.fontPx, layout.face) + stepPx;
    indent.commentPx = (indent.commentAt < size)
                           ? MeasurePx (text, layout.text.substr ((size_t) first, (size_t) (indent.commentAt - first)), layout.fontPx, layout.face)
                           : 0.0f;

    indent.codePx    = (maxWidthPx - indent.codePx    < minRoomPx) ? 0.0f : indent.codePx;
    indent.commentPx = (maxWidthPx - indent.commentPx < minRoomPx) ? 0.0f : indent.commentPx;

    WrapRun (text, layout, first, size, maxWidthPx, indent);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::FindCommentText
//
//  Where the text of a line's comment starts: past the first ; outside a
//  string and the spaces after it. A quote with no closing quote after it,
//  as an immediate character's can be, doesn't start a string. The line's end
//  where it has no comment text.
//
////////////////////////////////////////////////////////////////////////////////

int OperandResultTip::FindCommentText (
    const std::wstring  & chars,
    int                   first)
{
    int     size  = (int) chars.size();
    int     at    = size;
    size_t  close = 0;



    for (int k = first; k < size && at == size; k++)
    {
        if (chars[(size_t) k] == L'"' || chars[(size_t) k] == L'\'')
        {
            close = chars.find (chars[(size_t) k], (size_t) k + 1);
            k     = (close != std::wstring::npos) ? (int) close : k;
            continue;
        }

        if (chars[(size_t) k] == L';')
        {
            at = k;
        }
    }

    while (at < size && (chars[(size_t) at] == L';' || iswspace (chars[(size_t) at])))
    {
        at++;
    }

    return at;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::WrapRun
//
//  The characters from `first` to `end` on as many lines as they need, each
//  broken at its last space where it has one, and mid-word only where a word
//  is wider than a line. The spaces at a break go with it. The lines after
//  the first start at the indent for where they start, with that much less
//  room.
//
////////////////////////////////////////////////////////////////////////////////

void OperandResultTip::WrapRun (
    IDxuiTextRenderer  & text,
    Layout             & layout,
    int                  first,
    int                  end,
    float                maxWidthPx,
    const Indent       & indent)
{
    int    start    = first;
    int    stop     = 0;
    int    next     = 0;
    bool   isShort  = false;
    float  indentPx = 0.0f;



    while (start < end)
    {
        indentPx = (start == first) ? 0.0f : (start >= indent.commentAt) ? indent.commentPx : indent.codePx;
        stop     = FindFitEnd (text, layout, start, end, maxWidthPx - indentPx);
        isShort  = stop < end;

        for (int k = stop; isShort && k > start; k--)
        {
            if (layout.text[(size_t) k] == L' ')
            {
                stop = k;
                break;
            }
        }

        next = stop;

        while (stop > start && layout.text[(size_t) (stop - 1)] == L' ')
        {
            stop--;
        }

        if (stop > start)
        {
            layout.lines.push_back ({ start, stop - start, indentPx });
        }

        start = next;

        while (start < end && layout.text[(size_t) start] == L' ')
        {
            start++;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::FindFitEnd
//
//  The furthest end, from `first` up to `end`, that keeps the run within
//  `maxWidthPx`: never less than one character, so a line always moves on.
//
////////////////////////////////////////////////////////////////////////////////

int OperandResultTip::FindFitEnd (
    IDxuiTextRenderer  & text,
    const Layout       & layout,
    int                  first,
    int                  end,
    float                maxWidthPx)
{
    int    low   = first + 1;
    int    high  = end;
    int    mid   = 0;
    float  width = 0.0f;



    while (low < high)
    {
        mid   = low + (high - low + 1) / 2;
        width = MeasurePx (text, layout.text.substr ((size_t) first, (size_t) (mid - first)), layout.fontPx, layout.face);

        if (width <= maxWidthPx)
        {
            low = mid;
        }
        else
        {
            high = mid - 1;
        }
    }

    return low;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::PaintLine
//
//  A run at a time in each color, each placed by measuring the line before
//  it, as the list draws a cell of several colors.
//
////////////////////////////////////////////////////////////////////////////////

void OperandResultTip::PaintLine (
    const Layout       & layout,
    const Line         & line,
    float                x,
    float                y,
    float                widthPx,
    IDxuiTextRenderer  & text)
{
    HRESULT  hr     = S_OK;
    int      pos    = line.start;
    int      end    = line.start + line.length;
    int      runEnd = 0;
    float    offset = 0.0f;



    while (pos < end)
    {
        runEnd = pos + 1;

        while (runEnd < end && layout.colors[(size_t) runEnd] == layout.colors[(size_t) pos])
        {
            runEnd++;
        }

        offset = (pos > line.start)
                     ? MeasurePx (text, layout.text.substr ((size_t) line.start, (size_t) (pos - line.start)), layout.fontPx, layout.face)
                     : 0.0f;

        hr = text.DrawString (layout.text.substr ((size_t) pos, (size_t) (runEnd - pos)).c_str(),
                              x + offset,
                              y,
                              widthPx - offset,
                              (float) layout.lineHeight,
                              layout.colors[(size_t) pos],
                              layout.fontPx,
                              layout.face,
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::CenterOnCapHeight,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        pos = runEnd;
    }
}
