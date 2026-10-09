#include "Pch.h"

#include "Ui/Debugger/OperandResultTip.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/Panes/SourcePane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::TryMakeLayout
//
//  Only where the list cuts the text off: at the pane's edge, or at the
//  column's own where the column has been made narrower than its text. The
//  tip wraps in the room the text has there, so it grows down rather than
//  out, and covers the whole of the cell the pane shows, so none of the cut
//  text shows beside it; the cell's padding is either side of the text, and
//  its outline a pixel past the row above and below, as Explorer's is.
//
//  The tip stays inside the work area it is given without moving off the
//  cell: a popup kept on the screen by being moved would no longer line its
//  text up with the cell's.
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
    bool     isOnScreen = false;
    bool     doesFit    = false;
    int      textLeft   = (int) placement.textRect.left;
    int      rowTop     = (int) placement.textRect.top;
    int      rowHeight  = (int) (placement.textRect.bottom - placement.textRect.top);
    int      cellRight  = (std::min) ((int) placement.textRect.right, placement.visibleRight);
    int      shownRight = (std::min) ((int) placement.textRect.right - placement.padRightPx, placement.visibleRight);
    float    fullPx     = 0.0f;
    float    widestPx   = 0.0f;



    isUsable = hasText && placement.fontPx > 0.0f && placement.face != nullptr && rowHeight > 0;
    CBR (isUsable);

    //  A start scrolled out of sight, or off the screen, leaves nothing to
    //  lay the tip over.
    isOnScreen = textLeft >= placement.visibleLeft && textLeft < shownRight &&
                 textLeft >= placement.workArea.left && textLeft + placement.padRightPx < placement.workArea.right &&
                 rowTop >= placement.workArea.top;
    CBR (isOnScreen);

    layout.text       = cell.text;
    layout.fontPx     = placement.fontPx;
    layout.face       = placement.face;
    layout.lineHeight = rowHeight;
    layout.border     = placement.border;
    layout.textOrigin = POINT { textLeft, rowTop };

    //  Text that cannot be measured cannot be said to be cut off.
    fullPx = MeasurePx (text, cell.text, placement.fontPx, placement.face);
    CBR (fullPx > 0.0f && fullPx > (float) (shownRight - textLeft));

    doesFit = TryWrapOnScreen (text, placement, (float) (cellRight - placement.padRightPx - textLeft), layout, widestPx);
    CBR (doesFit);

    layout.fill   = MakeFill   (cell, placement.rowFill, placement.contentBackground);
    layout.colors = MakeColors (cell, placement.ink, layout.fill);
    layout.rect   = PlaceRect  (placement, layout, cellRight, widestPx);

    out    = std::move (layout);
    isMade = true;

Error:
    return isMade;
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
//  The popup is cleared to the layout's fill before this runs, so only the
//  outline and the text are drawn.
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



    painter.OutlineRect (0.0f, 0.0f, width, height, (float) kBorderPx, layout.border);

    for (const Line & line : layout.lines)
    {
        PaintLine (layout, line, x, y, width - x, text);
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
////////////////////////////////////////////////////////////////////////////////

float OperandResultTip::MeasureWidestPx (
    IDxuiTextRenderer  & text,
    const Layout       & layout)
{
    float  widestPx = 0.0f;



    for (const Line & line : layout.lines)
    {
        widestPx = (std::max) (widestPx, MeasurePx (text, layout.text.substr ((size_t) line.start, (size_t) line.length), layout.fontPx, layout.face));
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
//  The text wraps in the room the pane gives it, never narrower than the
//  tip's least width and never wider than the screen has room for right of
//  the text; at the screen's right edge it grows down instead. Where that is
//  too tall for the screen below the row, it wraps again across all the room
//  the screen has. False where it still does not fit.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandResultTip::TryWrapOnScreen (
    IDxuiTextRenderer  & text,
    const Placement    & placement,
    float                paneRoomPx,
    Layout             & layout,
    float              & widestPx)
{
    float  roomPx   = (float) (placement.workArea.right - placement.padRightPx - layout.textOrigin.x);
    float  minPx    = MeasurePx (text, std::wstring (kMinWrapChars, L'0'), placement.fontPx, placement.face);
    float  wrapPx   = (std::min) ((std::max) (paneRoomPx, minPx), roomPx);
    bool   hasLines = false;



    WrapCell (text, layout, wrapPx);

    if (GetBottomPx (layout) > placement.workArea.bottom && wrapPx < roomPx)
    {
        layout.lines.clear();
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
//  From the cell's left to the end of its widest line, or to the edge of the
//  cell the pane shows where that is further, so the tip covers the whole of
//  what the list draws. The cell's padding and the outline give way at the
//  pane's edge and the screen's.
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



    rect.left   = (std::max) ({ textLeft - placement.padLeftPx, placement.visibleLeft, (int) placement.workArea.left });
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
    size_t  resultAt   = layout.text.find (SourcePane::kpszResultPrefix);
    int     size       = (int) layout.text.size();
    int     operandEnd = (resultAt == std::wstring::npos) ? size : (int) resultAt;



    while (operandEnd > 0 && layout.text[(size_t) (operandEnd - 1)] == L' ')
    {
        operandEnd--;
    }

    WrapRun (text, layout, 0, operandEnd, maxWidthPx);

    if (resultAt != std::wstring::npos)
    {
        WrapRun (text, layout, (int) resultAt, size, maxWidthPx);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::WrapRun
//
//  The characters from `first` to `end` on as many lines as they need, each
//  broken at its last space where it has one, and mid-word only where a word
//  is wider than a line. The spaces at a break go with it.
//
////////////////////////////////////////////////////////////////////////////////

void OperandResultTip::WrapRun (
    IDxuiTextRenderer  & text,
    Layout             & layout,
    int                  first,
    int                  end,
    float                maxWidthPx)
{
    int   start   = first;
    int   stop    = 0;
    int   next    = 0;
    bool  isShort = false;



    while (start < end)
    {
        stop    = FindFitEnd (text, layout, start, end, maxWidthPx);
        isShort = stop < end;

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
            layout.lines.push_back ({ start, stop - start });
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
