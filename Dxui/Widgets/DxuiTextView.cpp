#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTextView.h"
#include "Theme/DxuiColor.h"
#include "Core/DxuiClipboard.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Core/DxuiPaneMetrics.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetRows
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetRows (std::vector<Row> rows)
{
    m_rows     = std::move (rows);
    m_anchor   = Position();
    m_caret    = Position();
    m_topLine  = m_followEnd ? m_topLine : 0;
    m_dragging = false;

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetCellSize
//
//  A size set here is kept across DPI changes, as a test's is.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetCellSize (int widthPx, int heightPx)
{
    m_cellWidthPx  = (std::max) (widthPx,  0);
    m_cellHeightPx = (std::max) (heightPx, 0);
    m_cellAdvance  = (float) m_cellWidthPx;
    m_cellPinned   = true;

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLineCap
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetLineCap() const
{
    int  height = (int) (m_boundsDip.bottom - m_boundsDip.top) - m_scaler.ToPx (s_kPadDip) * 2;
    int  used   = 0;
    int  count  = 0;
    int  line   = m_topLine;



    if (m_cellHeightPx <= 0)
    {
        return 1;
    }

    //  Past the end, a line is taken as a full one, so the count does not
    //  depend on how much text there is.
    for (;;)
    {
        int  lineHeight = (line < (int) m_lines.size()) ? GetLineHeightPx (line) : m_cellHeightPx;

        if (used + lineHeight > height)
        {
            break;
        }

        used += lineHeight;
        count++;
        line++;
    }

    return (std::max) (1, count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLineHeightPx
//
//  A full line, or the fraction of one a short row asks for, never less
//  than a pixel.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetLineHeightPx (int lineIndex) const
{
    float  fraction = m_rows[(size_t) m_lines[(size_t) lineIndex].row].height;



    if (fraction >= 1.0f)
    {
        return m_cellHeightPx;
    }

    return (std::max) (1, (int) ((float) m_cellHeightPx * fraction + 0.5f));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLineOffsetPx
//
//  How far below the first line shown a line's top is; negative for a line
//  above it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetLineOffsetPx (int lineIndex) const
{
    int  offset = 0;



    for (int line = m_topLine; line < lineIndex; line++)
    {
        offset += (line < (int) m_lines.size()) ? GetLineHeightPx (line) : m_cellHeightPx;
    }

    for (int line = lineIndex; line < m_topLine; line++)
    {
        offset -= GetLineHeightPx (line);
    }

    return offset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTopForLast
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTopForLast (int lineIndex) const
{
    int  height = (int) (m_boundsDip.bottom - m_boundsDip.top) - m_scaler.ToPx (s_kPadDip) * 2;
    int  top    = lineIndex;
    int  used   = 0;



    if (lineIndex < 0 || lineIndex >= (int) m_lines.size() || m_cellHeightPx <= 0)
    {
        return 0;
    }

    used = GetLineHeightPx (lineIndex);

    while (top > 0 && used + GetLineHeightPx (top - 1) <= height)
    {
        top--;
        used += GetLineHeightPx (top);
    }

    return top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetFirstLineOfRow
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetFirstLineOfRow (int row) const
{
    for (size_t i = 0; i < m_lines.size(); i++)
    {
        if (m_lines[i].row == row)
        {
            return (int) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetCellAnchorPx
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::GetCellAnchorPx (int row, int cell, float & outX, float & outY) const
{
    int  line = GetFirstLineOfRow (row);



    if (line < m_topLine || line >= m_topLine + GetLineCap() || m_cellHeightPx <= 0)
    {
        return false;
    }

    outX = (float) GetTextLeft() + (float) GetCellStart (m_rows[(size_t) row], cell) * m_cellAdvance;
    outY = (float) m_boundsDip.top + (float) m_scaler.ToPx (s_kPadDip) + (float) GetLineOffsetPx (line) + (float) GetLineHeightPx (line) * 0.5f;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLinesSpanPx
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::GetLinesSpanPx (float & outTop, float & outBottom) const
{
    int  shown = (std::max) (0, (std::min) (GetLineCap(), (int) m_lines.size() - m_topLine));



    outTop    = (float) m_boundsDip.top + (float) m_scaler.ToPx (s_kPadDip);
    outBottom = outTop + (float) GetLineOffsetPx (m_topLine + shown);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetTopLine
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetTopLine (int line)
{
    int  maxTop = GetTopForLast ((int) m_lines.size() - 1);



    m_topLine = (std::max) (0, (std::min) (line, maxTop));
    m_atEnd   = m_topLine >= maxTop;

    SyncScrollbar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTextColumns
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTextColumns (bool scrollbar) const
{
    int  width = (int) (m_boundsDip.right - m_boundsDip.left) - GetPadLeftPx() - m_scaler.ToPx (s_kPadDip) - GetGutterPx();



    width -= scrollbar ? m_scaler.ToPx (s_kScrollbarWidthDip) : 0;

    return (m_cellWidthPx > 0) ? (std::max) (1, (int) ((float) width / m_cellAdvance)) : 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetCellStart
//
//  The character column a cell begins in: past the warning mark, and past
//  every earlier column and the gap after it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetCellStart (const Row & row, int cell) const
{
    int  column = row.warning ? kWarningCells : 0;



    for (int c = 0; c < cell && c < (int) m_columnCells.size(); c++)
    {
        column += m_columnCells[(size_t) c] + kColumnGapCells;
    }

    return column;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetCellBase
//
//  Where a cell's first character is in the row's text.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetCellBase (const Row & row, int cell) const
{
    int  base = 0;



    for (int c = 0; c < cell && c < (int) row.cells.size(); c++)
    {
        base += (int) row.cells[(size_t) c].size() + 1;
    }

    return base;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetRowLength
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetRowLength (const Row & row) const
{
    return row.cells.empty() ? 0 : GetCellBase (row, (int) row.cells.size() - 1) + (int) row.cells.back().size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetRowText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTextView::GetRowText (const Row & row) const
{
    std::wstring  text;



    for (size_t c = 0; c < row.cells.size(); c++)
    {
        text += (c > 0 ? L"\t" : L"") + row.cells[c];
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Rebuild
//
//  Column widths from every row, then the drawn lines, laid out again
//  narrower if they turn out to need the scrollbar.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Rebuild()
{
    m_lines.clear();
    m_columnCells.clear();

    if (m_cellWidthPx <= 0 || m_cellHeightPx <= 0)
    {
        return;
    }

    for (const Row & row : m_rows)
    {
        for (size_t c = 0; c + 1 < row.cells.size(); c++)
        {
            if (m_columnCells.size() <= c)
            {
                m_columnCells.resize (c + 1, 0);
            }

            m_columnCells[c] = (std::max) (m_columnCells[c], (int) row.cells[c].size());
        }
    }

    BuildLines (GetTextColumns (false));

    if (IsScrollbarVisible())
    {
        BuildLines (GetTextColumns (true));
    }

    SetTopLine ((m_followEnd && m_atEnd) ? INT_MAX : m_topLine);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::BuildLines
//
//  Wraps each row's last cell at the last space that fits, or mid-word when
//  none does.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::BuildLines (int columns)
{
    m_lines.clear();

    for (int r = 0; r < (int) m_rows.size(); r++)
    {
        const Row &     row    = m_rows[(size_t) r];
        std::wstring    last   = row.cells.empty() ? std::wstring() : row.cells.back();
        int             start  = GetCellStart (row, (int) row.cells.size() - 1);
        int             avail  = (std::max) (1, columns - start);
        int             length = (int) last.size();
        int             pos    = 0;
        bool            first  = true;

        do
        {
            int     take  = (std::min) (avail, length - pos);
            size_t  space = std::wstring::npos;

            if (pos + take < length)
            {
                space = last.rfind (L' ', (size_t) (pos + take - 1));
                take  = (space != std::wstring::npos && (int) space >= pos) ? (int) space + 1 - pos : take;
            }

            m_lines.push_back (Line { r, pos, take, first });

            pos  += take;
            first = false;
        }
        while (pos < length);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SyncScrollbar
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SyncScrollbar()
{
    int         barW = m_scaler.ToPx (s_kScrollbarWidthDip);
    SCROLLINFO  info = { sizeof (info) };



    m_vertScroll.Configure (DxuiScrollbar::Orientation::Vertical, barW, barW, 1);
    m_vertScroll.SetTrack (RECT { m_boundsDip.right - barW, m_boundsDip.top, m_boundsDip.right, m_boundsDip.bottom });

    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;
    info.nMax  = (std::max) (0, (int) m_lines.size() - 1);
    info.nPage = (UINT) GetLineCap();
    info.nPos  = m_topLine;
    m_vertScroll.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::HitTest
//
//  The character nearest a point, rounding to the closer side of a cell. A
//  point in the gap after a column lands at the end of that column's cell.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextView::Position DxuiTextView::HitTest (POINT point) const
{
    Position  pos;
    int       pad       = m_scaler.ToPx (s_kPadDip);
    int       y         = point.y - (int) m_boundsDip.top - pad;
    int       x         = point.x - GetTextLeft();
    int       lineIndex = 0;
    int       column    = 0;
    int       last      = 0;



    if (m_lines.empty() || m_cellWidthPx <= 0 || m_cellHeightPx <= 0)
    {
        return pos;
    }

    lineIndex = (y < 0) ? m_topLine - 1 : m_topLine;

    while (y >= 0 && lineIndex + 1 < (int) m_lines.size() && GetLineOffsetPx (lineIndex + 1) <= y)
    {
        lineIndex++;
    }

    lineIndex = (std::max) (0, (std::min) (lineIndex, (int) m_lines.size() - 1));
    column    = (x < 0) ? 0 : (int) (((float) x + m_cellAdvance / 2.0f) / m_cellAdvance);

    const Line &  line = m_lines[(size_t) lineIndex];
    const Row &   row  = m_rows[(size_t) line.row];

    pos.row = line.row;
    last    = (int) row.cells.size() - 1;

    if (last < 0)
    {
        return pos;
    }

    for (int c = 0; line.first && c < last; c++)
    {
        int  start  = GetCellStart (row, c);
        int  length = (int) row.cells[(size_t) c].size();

        if (column < GetCellStart (row, c + 1))
        {
            pos.offset = GetCellBase (row, c) + (std::max) (0, (std::min) (column - start, length));
            return pos;
        }
    }

    pos.offset = GetCellBase (row, last) + line.start + (std::max) (0, (std::min) (column - GetCellStart (row, last), line.length));

    return pos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Select
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Select (Position anchor, Position caret)
{
    m_anchor = anchor;
    m_caret  = caret;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SelectAll
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SelectAll()
{
    m_anchor = Position();
    m_caret  = m_rows.empty() ? Position() : Position { (int) m_rows.size() - 1, GetRowLength (m_rows.back()) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::ClearSelection
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::ClearSelection()
{
    m_anchor = m_caret;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SelectWordAt
//
//  The run of characters around a position that holds no space or tab.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SelectWordAt (Position pos)
{
    GetWordBounds (pos, m_anchor, m_caret);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetWordBounds
//
//  The start and end of the run of characters around a position, bounded by
//  spaces and the tabs between cells.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::GetWordBounds (Position pos, Position & outFirst, Position & outLast) const
{
    std::wstring  text  = (pos.row < (int) m_rows.size()) ? GetRowText (m_rows[(size_t) pos.row]) : std::wstring();
    int           size  = (int) text.size();
    int           first = (std::min) (pos.offset, size);
    int           last  = first;
    int           kind  = 0;



    //  The run is of whatever kind is under the position, or just before it at
    //  the end of the row, so a run of spaces is taken as one too.
    if (first < size)
    {
        kind = GetCharClass (text[(size_t) first]);
    }
    else if (first > 0)
    {
        kind = GetCharClass (text[(size_t) first - 1]);
    }

    while (first > 0 && GetCharClass (text[(size_t) first - 1]) == kind)
    {
        first--;
    }

    while (last < size && GetCharClass (text[(size_t) last]) == kind)
    {
        last++;
    }

    outFirst = Position { pos.row, first };
    outLast  = Position { pos.row, last };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetSelectionText
//
//  The selected characters, a tab between cells and a line break between rows.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTextView::GetSelectionText() const
{
    Position      from = (std::min) (m_anchor, m_caret);
    Position      to   = (std::max) (m_anchor, m_caret);
    std::wstring  out;



    for (int r = from.row; HasSelection() && r <= to.row && r < (int) m_rows.size(); r++)
    {
        std::wstring  text  = GetRowText (m_rows[(size_t) r]);
        int           begin = (r == from.row) ? (std::min) (from.offset, (int) text.size()) : 0;
        int           end   = (r == to.row)   ? (std::min) (to.offset,   (int) text.size()) : (int) text.size();

        out += text.substr ((size_t) begin, (size_t) (std::max) (0, end - begin));

        if (r < to.row)
        {
            out += L"\r\n";
        }
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::CopySelection
//
//  With nothing selected the clipboard is left as it was.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::CopySelection() const
{
    if (HasSelection())
    {
        DxuiClipboard::SetText (m_hwnd, GetSelectionText());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SelectMatch
//
//  A forward search starts at the end of the selection and a backward one at
//  its start, so repeating either steps from match to match. With nothing
//  selected both start at the caret, where the last click left it.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextView::FindResult DxuiTextView::SelectMatch (const std::wstring & needle, bool matchCase, bool wholeWord, bool forward)
{
    std::vector<std::wstring>  texts;
    Position                   from   = forward ? (std::max) (m_anchor, m_caret) : (std::min) (m_anchor, m_caret);
    Position                   start;
    FindResult                 result = FindResult::NotFound;



    texts.reserve (m_rows.size());

    for (const Row & row : m_rows)
    {
        texts.push_back (GetRowText (row));
    }

    result = FindInRows (texts, needle, matchCase, wholeWord, forward, from, start);

    if (result != FindResult::NotFound)
    {
        m_anchor = start;
        m_caret  = Position { start.row, start.offset + (int) needle.size() };

        ScrollToPosition (start);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SelectMatch  (with a regular expression option)
//
//  From the selection as the plain search starts: forward, the first match
//  starting at or after its end; backward, the last starting before its
//  start; each going round when there is none.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextView::FindResult DxuiTextView::SelectMatch (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, bool forward, int & outIndex, int & outCount)
{
    std::vector<FindMatch>     matches;
    Position                   from   = forward ? (std::max) (m_anchor, m_caret) : (std::min) (m_anchor, m_caret);
    FindResult                 result = FindResult::Wrapped;
    size_t                     pick   = 0;



    outIndex = 0;
    outCount = 0;

    KeepUserSelection();

    (void) GetScopedMatches (needle, matchCase, wholeWord, isRegex, matches);
    m_highlights = matches;

    if (matches.empty())
    {
        return FindResult::NotFound;
    }

    if (forward)
    {
        auto  it = std::ranges::find_if (matches, [from] (const FindMatch & match) { return !(match.start < from); });

        pick   = (it == matches.end()) ? 0 : (size_t) (it - matches.begin());
        result = (it == matches.end()) ? FindResult::Wrapped : FindResult::Found;
    }
    else
    {
        pick = matches.size() - 1;

        for (size_t i = matches.size(); i > 0; i--)
        {
            if (matches[i - 1].start < from)
            {
                pick   = i - 1;
                result = FindResult::Found;
                break;
            }
        }
    }

    m_anchor = matches[pick].start;
    m_caret  = Position { matches[pick].start.row, matches[pick].start.offset + matches[pick].length };
    ScrollToPosition (m_anchor);

    outIndex = (int) pick + 1;
    outCount = (int) matches.size();
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SelectMatchInView
//
//  A match is on screen when the line it starts on is one the view shows.
//  Without one there, the first starting below the top of the view is taken,
//  going round to the first in the text when there is none below.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextView::FindResult DxuiTextView::SelectMatchInView (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, int & outIndex, int & outCount)
{
    std::vector<FindMatch>  matches;
    int                     cap    = GetLineCap();
    size_t                  pick   = 0;
    FindResult              result = FindResult::Found;



    outIndex = 0;
    outCount = 0;

    KeepUserSelection();

    (void) GetScopedMatches (needle, matchCase, wholeWord, isRegex, matches);
    m_highlights = matches;

    if (matches.empty())
    {
        return FindResult::NotFound;
    }

    auto  onScreen = std::ranges::find_if (matches, [this, cap] (const FindMatch & match)
    {
        int  line = GetLineOfPosition (match.start);

        return line >= m_topLine && line < m_topLine + cap;
    });

    auto  below = std::ranges::find_if (matches, [this] (const FindMatch & match) { return GetLineOfPosition (match.start) >= m_topLine; });

    if (onScreen != matches.end())
    {
        pick = (size_t) (onScreen - matches.begin());
    }
    else if (below != matches.end())
    {
        pick = (size_t) (below - matches.begin());
    }
    else
    {
        result = FindResult::Wrapped;
    }

    m_anchor = matches[pick].start;
    m_caret  = Position { matches[pick].start.row, matches[pick].start.offset + matches[pick].length };
    ScrollToPosition (m_anchor);

    outIndex = (int) pick + 1;
    outCount = (int) matches.size();
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetScopedMatches
//
//  Every match in the text, less those outside the scope while find in
//  selection is on. False when the needle is not a valid expression.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::GetScopedMatches (const std::wstring & needle, bool matchCase, bool wholeWord, bool isRegex, std::vector<FindMatch> & outMatches) const
{
    std::vector<std::wstring>  texts;
    bool                       valid = false;



    texts.reserve (m_rows.size());

    for (const Row & row : m_rows)
    {
        texts.push_back (GetRowText (row));
    }

    valid = FindAllInRows (texts, needle, matchCase, wholeWord, isRegex, outMatches);

    if (valid && m_scoped)
    {
        std::erase_if (outMatches, [this] (const FindMatch & match)
        {
            Position  end { match.start.row, match.start.offset + match.length };

            return match.start < m_scopeStart || m_scopeEnd < end;
        });
    }

    return valid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetFindHighlightFill
//
//  3:1 is what WCAG asks of a mark that is not text. A dark background
//  takes the accent lighter, a light one darker.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTextView::GetFindHighlightFill (const IDxuiTheme & theme)
{
    constexpr float  kMinRatio = 3.0f;
    constexpr int    kMaxSteps = 40;



    uint32_t  background = theme.ContentBackground() | 0xFF000000u;
    uint32_t  fill       = theme.WarningAccent()     | 0xFF000000u;
    bool      darkBack   = DxuiColor::ComputeRelativeLuminance (background) < 0.18f;



    for (int step = 0; step < kMaxSteps && DxuiColor::ComputeContrastRatio (fill, background) < kMinRatio; step++)
    {
        fill = darkBack ? DxuiColor::Lighten (fill, 0.1f) : DxuiColor::Darken (fill, 0.9f);
    }

    return fill;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetFindHighlightText
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTextView::GetFindHighlightText (const IDxuiTheme & theme)
{
    uint32_t  fill  = GetFindHighlightFill (theme);
    uint32_t  fore  = theme.Foreground()        | 0xFF000000u;
    uint32_t  back  = theme.ContentBackground() | 0xFF000000u;



    return (DxuiColor::ComputeContrastRatio (fore, fill) >= DxuiColor::ComputeContrastRatio (back, fill)) ? fore : back;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::IsCurrentMatch
//
//  The match the search is at is the one the selection covers exactly.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::IsCurrentMatch (const FindMatch & match) const
{
    Position  from = (std::min) (m_anchor, m_caret);
    Position  to   = (std::max) (m_anchor, m_caret);



    return from == match.start && to == Position { match.start.row, match.start.offset + match.length };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetFindFillOf
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiTextView::GetFindFillOf (const FindMatch & match, const IDxuiTheme & theme) const
{
    if (m_findCurrentArgb != 0 && IsCurrentMatch (match))
    {
        return m_findCurrentArgb;
    }

    return (m_findMatchArgb != 0) ? m_findMatchArgb : GetFindHighlightFill (theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetFindScopeToSelection
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetFindScopeToSelection()
{
    bool  byFind = std::ranges::any_of (m_highlights, [this] (const FindMatch & match) { return IsCurrentMatch (match); });



    //  A search selects its match, so the selection to keep to is the one
    //  there before the search moved it.
    m_scoped     = byFind ? !(m_userAnchor == m_userCaret) : HasSelection();
    m_scopeStart = byFind ? (std::min) (m_userAnchor, m_userCaret) : (std::min) (m_anchor, m_caret);
    m_scopeEnd   = byFind ? (std::max) (m_userAnchor, m_userCaret) : (std::max) (m_anchor, m_caret);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::KeepUserSelection
//
//  Called before a search moves the selection: a selection the search did
//  not make is kept, for find in selection to take.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::KeepUserSelection()
{
    bool  byFind = std::ranges::any_of (m_highlights, [this] (const FindMatch & match) { return IsCurrentMatch (match); });



    if (!byFind)
    {
        m_userAnchor = m_anchor;
        m_userCaret  = m_caret;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::FindAllInRows
//
//  A plain needle is found as a substring, lowered on both sides without
//  matching case. A regular expression uses ECMAScript syntax; a match of
//  no characters is passed over, since it selects nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::FindAllInRows (
    const std::vector<std::wstring> & rows,
    const std::wstring              & needle,
    bool                              matchCase,
    bool                              wholeWord,
    bool                              isRegex,
    std::vector<FindMatch>          & outMatches)
{
    std::wregex  pattern;
    std::wstring key = matchCase ? needle : GetLowered (needle);



    outMatches.clear();

    if (needle.empty())
    {
        return true;
    }

    if (isRegex)
    {
        try
        {
            pattern = std::wregex (needle, matchCase ? std::regex_constants::ECMAScript : (std::regex_constants::ECMAScript | std::regex_constants::icase));
        }
        catch (const std::regex_error &)
        {
            return false;
        }
    }

    for (size_t r = 0; r < rows.size(); r++)
    {
        const std::wstring &  text = rows[r];

        if (isRegex)
        {
            for (std::wsregex_iterator it (text.begin(), text.end(), pattern), end; it != end; ++it)
            {
                size_t  at     = (size_t) it->position (0);
                size_t  length = (size_t) it->length (0);

                if (length > 0 && (!wholeWord || IsWholeWordAt (text, at, length)))
                {
                    outMatches.push_back (FindMatch { Position { (int) r, (int) at }, (int) length });
                }
            }

            continue;
        }

        std::wstring  hay = matchCase ? text : GetLowered (text);

        for (size_t at = hay.find (key); at != std::wstring::npos; at = hay.find (key, at + 1))
        {
            if (!wholeWord || IsWholeWordAt (text, at, key.size()))
            {
                outMatches.push_back (FindMatch { Position { (int) r, (int) at }, (int) key.size() });
            }
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::FindInRows
//
//  Without a match past the starting point the search goes round: forward
//  from the top, backward from the bottom. Without matching case, both sides
//  are lowered first, which keeps every offset where it was, and lowering
//  leaves a word character a word character.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextView::FindResult DxuiTextView::FindInRows (
    const std::vector<std::wstring> & rows,
    const std::wstring              & needle,
    bool                              matchCase,
    bool                              wholeWord,
    bool                              forward,
    Position                          from,
    Position                        & outStart)
{
    std::vector<std::wstring>          folded;
    const std::vector<std::wstring>  * haystack = &rows;
    std::wstring                       key      = needle;
    Position                           around   = forward ? Position() : Position { (int) rows.size(), 0 };



    if (needle.empty())
    {
        return FindResult::NotFound;
    }

    if (!matchCase)
    {
        for (const std::wstring & text : rows)
        {
            folded.push_back (GetLowered (text));
        }

        key      = GetLowered (needle);
        haystack = &folded;
    }

    if (TryFindOnce (*haystack, key, wholeWord, forward, from, outStart))
    {
        return FindResult::Found;
    }

    if (TryFindOnce (*haystack, key, wholeWord, forward, around, outStart))
    {
        return FindResult::Wrapped;
    }

    return FindResult::NotFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLowered
//
//  One character for one, so an offset in the result is the same offset in
//  the text it came from.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiTextView::GetLowered (const std::wstring & text)
{
    std::wstring  lowered = text;



    for (wchar_t & ch : lowered)
    {
        ch = (wchar_t) towlower (ch);
    }

    return lowered;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::TryFindOnce
//
//  One pass, with no going round: forward, the first match starting at or
//  after `from`; backward, the last match starting before it. A match that is
//  part of a longer word is passed over when only whole words count.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::TryFindOnce (
    const std::vector<std::wstring> & rows,
    const std::wstring              & needle,
    bool                              wholeWord,
    bool                              forward,
    Position                          from,
    Position                        & outStart)
{
    int     count = (int) rows.size();
    size_t  at    = std::wstring::npos;



    if (forward)
    {
        for (int r = (std::max) (from.row, 0); r < count; r++)
        {
            const std::wstring &  text  = rows[(size_t) r];
            size_t                start = (r == from.row) ? (size_t) (std::max) (from.offset, 0) : 0;

            at = (start <= text.size()) ? text.find (needle, start) : std::wstring::npos;

            while (wholeWord && at != std::wstring::npos && !IsWholeWordAt (text, at, needle.size()))
            {
                at = text.find (needle, at + 1);
            }

            if (at != std::wstring::npos)
            {
                outStart = Position { r, (int) at };
                return true;
            }
        }

        return false;
    }

    for (int r = (std::min) (from.row, count - 1); r >= 0; r--)
    {
        const std::wstring &  text = rows[(size_t) r];

        if (r == from.row && from.offset <= 0)
        {
            continue;
        }

        at = text.rfind (needle, (r == from.row) ? (size_t) (from.offset - 1) : std::wstring::npos);

        while (wholeWord && at != std::wstring::npos && !IsWholeWordAt (text, at, needle.size()))
        {
            at = (at == 0) ? std::wstring::npos : text.rfind (needle, at - 1);
        }

        if (at != std::wstring::npos)
        {
            outStart = Position { r, (int) at };
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::IsWholeWordAt
//
//  Whether the run of text at `at` has a line end or a character that is not
//  part of a word on each side of it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::IsWholeWordAt (const std::wstring & text, size_t at, size_t length)
{
    size_t  end = at + length;



    if (at > 0 && IsWordChar (text[at - 1]))
    {
        return false;
    }

    return end >= text.size() || !IsWordChar (text[end]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetLineOfPosition
//
//  The drawn line a position falls on: its row's first line, or a later one
//  when the position is in the part of the last cell that wrapped. -1 when
//  the row is not laid out.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetLineOfPosition (Position pos) const
{
    int  line = GetFirstLineOfRow (pos.row);
    int  base = 0;



    if (line < 0)
    {
        return line;
    }

    base = GetCellBase (m_rows[(size_t) pos.row], (int) m_rows[(size_t) pos.row].cells.size() - 1);

    while (line + 1 < (int) m_lines.size() && m_lines[(size_t) line + 1].row == pos.row &&
           base + m_lines[(size_t) line + 1].start <= pos.offset)
    {
        line++;
    }

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::ScrollToPosition
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::ScrollToPosition (Position pos)
{
    int  line = GetLineOfPosition (pos);
    int  cap  = GetLineCap();



    if (line < 0)
    {
        return;
    }

    if (line < m_topLine)
    {
        SetTopLine (line);
    }
    else if (line >= m_topLine + cap)
    {
        SetTopLine (GetTopForLast (line));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetZoom
//
//  A new size is measured again on the next paint, unless the host set the
//  cells itself.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetZoom (float zoom)
{
    if (zoom == m_zoom || zoom <= 0.0f)
    {
        return;
    }

    m_zoom = zoom;

    if (!m_cellPinned)
    {
        m_cellWidthPx  = 0;
        m_cellHeightPx = 0;
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::SetLineSpacing
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::SetLineSpacing (float spacing)
{
    if (spacing == m_lineSpacing || spacing < 1.0f)
    {
        return;
    }

    m_lineSpacing = spacing;

    if (!m_cellPinned)
    {
        m_cellWidthPx  = 0;
        m_cellHeightPx = 0;
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());

    if (!m_cellPinned && m_measuredDpi != scaler.GetDpi())
    {
        m_cellWidthPx  = 0;
        m_cellHeightPx = 0;
    }

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::EnsureCellSize
//
//  Measured at the size the face is drawn at, so cells and glyphs agree.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::EnsureCellSize (IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DxuiFontHandle  font   = theme.MonospaceFont();
    float           width  = 0.0f;
    float           height = 0.0f;
    HRESULT         hr     = S_OK;



    if (m_cellWidthPx > 0)
    {
        return;
    }

    hr = text.MeasureString (L"00000000", m_scaler.ToPxf (font.sizeDip * m_zoom), font.face, width, height);

    if (FAILED (hr) || width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    m_cellWidthPx  = (std::max) (1, (int) (width / 8.0f + 0.5f));
    m_cellAdvance  = (std::max) (1.0f, width / 8.0f);
    m_cellHeightPx = (std::max) (1, (int) (height * m_lineSpacing + 0.5f));
    m_textTopPx    = (std::max) (0, (m_cellHeightPx - (int) (height + 0.5f)) / 2);
    m_measuredDpi  = m_scaler.GetDpi();

    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT         hr   = S_OK;
    DxuiFontHandle  font = theme.MonospaceFont();
    int             last = 0;



    font.sizeDip *= m_zoom;

    EnsureCellSize (text, theme);

    painter.FillRect ((float) m_boundsDip.left, (float) m_boundsDip.top,
                      (float) (m_boundsDip.right - m_boundsDip.left), (float) (m_boundsDip.bottom - m_boundsDip.top),
                      theme.ContentBackground());

    if (m_cellWidthPx <= 0 || m_cellHeightPx <= 0)
    {
        return;
    }

    font.sizeDip = m_scaler.ToPxf (font.sizeDip);
    last         = (std::min) ((int) m_lines.size(), m_topLine + GetLineCap() + 1);

    hr = text.PushClipRect ((float) m_boundsDip.left, (float) m_boundsDip.top,
                            (float) (m_boundsDip.right - m_boundsDip.left), (float) (m_boundsDip.bottom - m_boundsDip.top));
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (int lineIndex = m_topLine; lineIndex < last; lineIndex++)
    {
        PaintLine (painter, text, theme, lineIndex, font);
    }

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (IsScrollbarVisible())
    {
        m_vertScroll.Paint (painter, theme.ForegroundMuted());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::PaintLine
//
//  The selection fill under a line's characters, then the characters. On a
//  row's first line every cell is drawn; on the lines after, only the run of
//  the last cell that wraps there.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::PaintLine (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, int lineIndex, const DxuiFontHandle & font)
{
    HRESULT                  hr       = S_OK;
    const Line             & line     = m_lines[(size_t) lineIndex];
    const Row              & row      = m_rows[(size_t) line.row];
    int                      left     = GetTextLeft();
    int                      y        = (int) m_boundsDip.top + m_scaler.ToPx (s_kPadDip) + GetLineOffsetPx (lineIndex);
    int                      last     = (int) row.cells.size() - 1;
    Position                 from     = (std::min) (m_anchor, m_caret);
    Position                 to       = (std::max) (m_anchor, m_caret);
    bool                     selected = HasSelection() && from.row <= line.row && line.row <= to.row;
    int                      selFrom  = (line.row == from.row) ? from.offset : 0;
    int                      selTo    = (line.row == to.row)   ? to.offset   : INT_MAX;
    uint32_t                 selArgb  = theme.SelectionBackground();
    std::vector<uint32_t>    colors   = GetRowColors (row);
    int                      gutter   = GetGutterPx();
    uint32_t                 litText  = GetFindHighlightText (theme);
    uint32_t                 litEdge  = (litText == (theme.Foreground() | 0xFF000000u)) ? theme.ContentBackground() : theme.Foreground();
    uint32_t                 fore     = theme.Foreground()        | 0xFF000000u;
    uint32_t                 back     = theme.ContentBackground() | 0xFF000000u;
    std::vector<bool>        lit;



    //  A row's fill runs across the view on each of its lines, gutter and all.
    if (row.background != 0)
    {
        painter.FillRect ((float) m_boundsDip.left, (float) y, (float) (m_boundsDip.right - m_boundsDip.left), (float) GetLineHeightPx (lineIndex),
                          row.background);
    }

    if (line.first && row.icon != nullptr && !row.icon->bgraPremul.empty() && gutter > 0)
    {
        float  iconPx = m_scaler.ToPxf ((float) m_gutterIconDip);

        hr = text.DrawIconBitmap (row.icon->bgraPremul.data(), row.icon->width, row.icon->height,
                                  (float) (left - gutter) + ((float) gutter - iconPx) * 0.5f,
                                  (float) y + ((float) m_cellHeightPx - iconPx) * 0.5f,
                                  iconPx, iconPx);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (line.first && row.warning)
    {
        hr = text.DrawString (s_kpszMdl2WarningSolid,
                              (float) left, (float) y, (float) (kWarningCells * m_cellAdvance), (float) m_cellHeightPx,
                              theme.WarningAccent(),
                              (float) m_cellHeightPx * 0.8f,
                              m_iconFace,
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    //  The current match's own fill is opaque, so its characters take the
    //  color with more contrast on it; the other matches' is a tint, which
    //  leaves their text its own colors.
    if (m_findCurrentArgb != 0)
    {
        litText = (DxuiColor::ComputeContrastRatio (fore, m_findCurrentArgb | 0xFF000000u) >= DxuiColor::ComputeContrastRatio (back, m_findCurrentArgb | 0xFF000000u)) ? fore : back;
    }

    //  A highlighted match's characters are drawn in the color that reads on
    //  its fill.
    for (const FindMatch & match : m_highlights)
    {
        bool  ownTint = (m_findCurrentArgb != 0 && !IsCurrentMatch (match)) || (m_findCurrentArgb == 0 && m_findMatchArgb != 0);

        if (match.start.row == line.row && !ownTint)
        {
            lit.resize ((size_t) GetRowLength (row) + 1, false);

            for (int i = match.start.offset; i < match.start.offset + match.length && i < (int) lit.size(); i++)
            {
                lit[(size_t) i] = true;
            }
        }
    }

    for (int c = 0; line.first && c < last; c++)
    {
        const std::wstring &  cell   = row.cells[(size_t) c];
        int                   column = GetCellStart (row, c);

        if (selected)
        {
            FillSelectedRange (painter, y, column, GetCellBase (row, c), (int) cell.size(),
                               GetCellStart (row, c + 1) - column - (int) cell.size(), selFrom, selTo, selArgb);
        }

        FillHighlights (painter, theme, y, column, GetCellBase (row, c), (int) cell.size(), line.row, litEdge);
        DrawRun (text, theme, font, y, column, GetCellBase (row, c), cell, selected, selFrom, selTo, colors, lit, litText);
    }

    if (last >= 0)
    {
        std::wstring  run    = row.cells[(size_t) last].substr ((size_t) line.start, (size_t) line.length);
        int           column = GetCellStart (row, last);

        if (selected)
        {
            FillSelectedRange (painter, y, column, GetCellBase (row, last) + line.start, line.length,
                               0, selFrom, selTo, selArgb);
        }

        FillHighlights (painter, theme, y, column, GetCellBase (row, last) + line.start, line.length, line.row, litEdge);
        DrawRun (text, theme, font, y, column, GetCellBase (row, last) + line.start, run, selected, selFrom, selTo, colors, lit, litText);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::DrawRun
//
//  A run of characters at `column` on a line, whose first is at `flatStart`
//  in the row's text. The selected part is drawn in the full foreground and
//  the rest in the preview's text color; the face is fixed-width, so each
//  part starts exactly where its first character's cell does.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::DrawRun (IDxuiTextRenderer    & text,
                            const IDxuiTheme     & theme,
                            const DxuiFontHandle & font,
                            int                    y,
                            int                    column,
                            int                    flatStart,
                            const std::wstring   & chars,
                            bool                   selected,
                            int                    selFrom,
                            int                    selTo,
                            const std::vector<uint32_t> & colors,
                            const std::vector<bool>     & lit,
                            uint32_t                      litArgb) const
{
    HRESULT   hr     = S_OK;
    int       left   = GetTextLeft();
    int       count  = (int) chars.size();
    int       first  = selected ? std::clamp (selFrom - flatStart, 0, count) : count;
    int       end    = selected ? std::clamp (selTo - flatStart, first, count) : count;
    uint32_t  normal = DxuiColor::Mix (theme.ContentBackground(), theme.Foreground(), m_textStrength);
    int       start  = 0;
    auto      colorAt = [&] (int i)
                        {
                            size_t  flat = (size_t) (flatStart + i);

                            if (flat < lit.size() && lit[flat])
                            {
                                return litArgb;
                            }

                            if (i >= first && i < end)
                            {
                                return theme.Foreground();
                            }

                            return (flat < colors.size() && colors[flat] != 0) ? colors[flat] : normal;
                        };



    //  Each run of one color is drawn on its own, from its first cell.
    while (start < count)
    {
        uint32_t  argb = colorAt (start);
        int       stop = start + 1;

        while (stop < count && colorAt (stop) == argb)
        {
            stop++;
        }

        hr = text.DrawString (chars.substr ((size_t) start, (size_t) (stop - start)).c_str(),
                              (float) (left + (column + start) * m_cellAdvance), (float) (y + m_textTopPx),
                              (float) ((stop - start + 1) * m_cellAdvance), (float) (m_cellHeightPx - m_textTopPx),
                              argb, font.sizeDip, font.face,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        start = stop;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetTextLeft
//
//  Where the text's first column starts: past the pad and the gutter.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetTextLeft() const
{
    return (int) m_boundsDip.left + GetPadLeftPx() + GetGutterPx();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetPadLeftPx
//
//  The space left of the gutter and the text: the pad every side has, or in
//  a view that fills a pane, the pane's text inset, so the text starts where
//  the pane's title does.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTextView::GetPadLeftPx() const
{
    return m_paneTextInset ? DxuiPaneMetrics::GetContentTextInsetPx (m_scaler) : m_scaler.ToPx (s_kPadDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::GetRowColors
//
////////////////////////////////////////////////////////////////////////////////

std::vector<uint32_t> DxuiTextView::GetRowColors (const Row & row) const
{
    std::vector<uint32_t>  colors;



    if (row.spans.empty())
    {
        return colors;
    }

    colors.assign ((size_t) GetRowLength (row), 0u);

    for (const Span & span : row.spans)
    {
        int  base   = GetCellBase (row, span.cell);
        int  length = (span.cell < (int) row.cells.size()) ? (int) row.cells[(size_t) span.cell].size() : 0;

        for (int i = (std::max) (0, span.start); i < span.start + span.length && i < length; i++)
        {
            colors[(size_t) (base + i)] = span.argb;
        }
    }

    return colors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::FillHighlights
//
//  Each highlighted match on the row that falls in the `count` characters
//  starting at `flatStart`, the way the selection is filled. The selected
//  match, the one the search is at, is also outlined in `edge`, unless it
//  has a fill of its own to set it apart.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::FillHighlights (IDxuiPainter & painter, const IDxuiTheme & theme, int y, int column, int flatStart, int count, int row, uint32_t edge) const
{
    int  left = GetTextLeft();



    for (const FindMatch & match : m_highlights)
    {
        int  first = (std::max) (match.start.offset, flatStart);
        int  end   = (std::min) (match.start.offset + match.length, flatStart + count);

        if (match.start.row != row || end <= first)
        {
            continue;
        }

        FillSelectedRange (painter, y, column, flatStart, count, 0, match.start.offset, match.start.offset + match.length, GetFindFillOf (match, theme));

        if (m_findCurrentArgb == 0 && IsCurrentMatch (match))
        {
            painter.OutlineRect ((float) (left + (column + first - flatStart) * m_cellAdvance), (float) y,
                                 (float) ((end - first) * m_cellAdvance), (float) m_cellHeightPx,
                                 m_scaler.ToPxf (2.0f), edge);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::FillSelectedRange
//
//  The selected part of `count` characters starting at `flatStart` in the
//  row's text and at `column` on the line, and the `trailCells` after them
//  when the separator that follows is selected too: the gap after a column,
//  or the end of a row whose selection goes on to the next.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextView::FillSelectedRange (IDxuiPainter & painter, int y, int column, int flatStart, int count, int trailCells, int selFrom, int selTo, uint32_t argb) const
{
    int  left      = GetTextLeft();
    int  first     = (std::max) (selFrom, flatStart);
    int  end       = (std::min) (selTo, flatStart + count);
    int  separator = flatStart + count;



    if (end > first)
    {
        painter.FillRect ((float) (left + (column + first - flatStart) * m_cellAdvance), (float) y,
                          (float) ((end - first) * m_cellAdvance), (float) m_cellHeightPx, argb);
    }

    if (trailCells > 0 && selFrom <= separator && separator < selTo)
    {
        painter.FillRect ((float) (left + (column + count) * m_cellAdvance), (float) y,
                          (float) (trailCells * m_cellAdvance), (float) m_cellHeightPx, argb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::OnMouse
//
//  A press places both ends of the selection, or moves only the caret with
//  Shift, and a drag moves the caret, scrolling past either edge. A second
//  press at the same place within the double-click time selects the word.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::OnMouse (const DxuiMouseEvent & ev)
{
    POINT     point   = ev.positionDip;
    int64_t   nowMs   = (int64_t) GetTickCount64();
    bool      handled = false;
    bool      twice   = false;
    Position  pos;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Wheel:
        ScrollLines ((int) (-ev.wheelDelta * (float) kWheelLines));
        handled = true;
        break;

    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Right)
        {
            if (m_onContextMenu)
            {
                m_onContextMenu (point);
            }

            handled = true;
        }
        else if (ev.button == DxuiMouseButton::Left && IsScrollbarVisible() && m_vertScroll.HitTest (point.x, point.y))
        {
            handled = m_vertScroll.OnMouseDown (point.x, point.y);
            SetTopLine (m_vertScroll.GetScrollPos());
        }
        else if (ev.button == DxuiMouseButton::Left)
        {
            pos   = HitTest (point);
            twice = (nowMs - m_lastClickMs) <= (int64_t) GetDoubleClickTime() && pos == m_lastClick;

            m_lastClickMs = twice ? 0 : nowMs;
            m_lastClick   = pos;

            m_wordDrag = twice;

            //  A double-click selects the word, and dragging on from it
            //  takes whole words until the button comes up.
            if (twice)
            {
                GetWordBounds (pos, m_wordFirst, m_wordLast);

                m_anchor   = m_wordFirst;
                m_caret    = m_wordLast;
                m_dragging = true;
            }
            else if (ev.shift)
            {
                m_caret    = pos;
                m_dragging = true;
            }
            else
            {
                m_anchor   = pos;
                m_caret    = pos;
                m_dragging = true;
            }

            handled = true;
        }

        break;

    case DxuiMouseEventKind::Move:
        if (m_vertScroll.IsDragging())
        {
            handled = m_vertScroll.OnMouseMove (point.x, point.y);
            SetTopLine (m_vertScroll.GetScrollPos());
        }
        else if (m_dragging)
        {
            if (point.y < m_boundsDip.top)
            {
                ScrollLines (-1);
            }
            else if (point.y >= m_boundsDip.bottom)
            {
                ScrollLines (1);
            }

            pos = HitTest (point);

            if (m_wordDrag)
            {
                Position  first;
                Position  last;

                GetWordBounds (pos, first, last);

                m_anchor = (pos < m_wordFirst) ? m_wordLast : m_wordFirst;
                m_caret  = (pos < m_wordFirst) ? first      : (std::max) (last, m_wordLast);
            }
            else
            {
                m_caret = pos;
            }

            handled = true;
        }

        break;

    case DxuiMouseEventKind::Up:
        if (m_vertScroll.IsDragging())
        {
            handled = m_vertScroll.OnMouseUp();
        }
        else if (m_dragging)
        {
            m_dragging = false;
            handled    = true;
        }

        break;

    default:
        break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::OnKey
//
//  The arrow and page keys scroll; there is no caret to move in read-only text.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::OnKey (const DxuiKeyEvent & ev)
{
    //  With Ctrl or Alt down the key is a shortcut, left to the host.
    bool  handled = ev.kind == DxuiKeyEventKind::Down && !ev.ctrl && !ev.alt;



    if (!handled)
    {
        return false;
    }

    switch (ev.vk)
    {
    case VK_UP:    ScrollLines (-1);             break;
    case VK_DOWN:  ScrollLines (1);              break;
    case VK_PRIOR: ScrollLines (-GetLineCap());  break;
    case VK_NEXT:  ScrollLines (GetLineCap());   break;
    case VK_HOME:  SetTopLine (0);               break;
    case VK_END:   SetTopLine (GetLineCount());  break;
    default:       handled = false;              break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::QueryCommand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::QueryCommand (DxuiStandardCommand command, bool & outEnabled) const
{
    bool  handled = false;



    if (command == DxuiStandardCommand::Copy)
    {
        outEnabled = HasSelection();
        handled    = true;
    }
    else if (command == DxuiStandardCommand::SelectAll)
    {
        outEnabled = !m_rows.empty();
        handled    = true;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextView::InvokeCommand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextView::InvokeCommand (DxuiStandardCommand command)
{
    bool  enabled = false;
    bool  handled = QueryCommand (command, enabled);



    if (handled && enabled && command == DxuiStandardCommand::Copy)
    {
        CopySelection();
    }
    else if (handled && enabled && command == DxuiStandardCommand::SelectAll)
    {
        SelectAll();
    }

    return handled;
}
