#include "Pch.h"

#include "Ui/Debugger/OperandResultTip.h"
#include "Ui/Debugger/DebuggerTextColors.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::TryMakeLayout
//
//  Only where the view cuts text off: at its edge, or at a column's edge
//  where the column has been made narrower than its text, or where the
//  operand and result lie wholly outside the view. The tip extends past the
//  view to the end of its text and wraps only at the screen's right edge.
//
//  The tip lies inside the work area it is given without moving off the
//  row: a popup moved onto the screen would no longer line its text up with
//  the row's.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandResultTip::TryMakeLayout (
    const Placement    & placement,
    IDxuiTextRenderer  & text,
    Layout             & out)
{
    HRESULT  hr         = S_OK;
    Layout   layout;
    Pieces   pieces;
    bool     isMade     = false;
    bool     isUsable   = false;
    bool     isOnScreen = false;
    bool     hasPieces  = false;
    bool     doesFit    = false;
    bool     hasRuns    = false;
    int      rowHeight  = (int) (placement.row.bottom - placement.row.top);
    float    rightPx    = 0.0f;



    isUsable = placement.fontPx > 0.0f && placement.face != nullptr && rowHeight > 0 && !placement.segments.empty();
    CBR (isUsable);

    //  The row's start, where its highlight starts, on the screen the tip is
    //  laid out on.
    isOnScreen = placement.row.left >= placement.workArea.left && placement.row.left < placement.workArea.right &&
                 placement.row.top  >= placement.workArea.top  && placement.row.top  < placement.workArea.bottom;
    CBR (isOnScreen);

    layout.fontPx     = placement.fontPx;
    layout.face       = placement.face;
    layout.lineHeight = rowHeight;
    layout.rowTop     = (int) placement.row.top;
    layout.fill       = placement.fill;

    hasPieces = TryGetPieces (placement, text, pieces, layout.hasSymbol);
    CBR (hasPieces);

    doesFit = TryWrapFirst (placement, text, pieces.first, layout);
    CBR (doesFit);

    for (const Piece & piece : pieces.below)
    {
        WrapPiece (placement, text, piece, layout);
    }

    hasRuns = !layout.runs.empty();
    CBR (hasRuns);

    rightPx = MeasureRightPx (text, layout);
    CBR (rightPx <= (float) (placement.workArea.right - placement.padRightPx));

    layout.rect = PlaceRect (placement, layout, rightPx);
    CBR (layout.rect.bottom <= placement.workArea.bottom);

    MakeReadable (layout);

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
    return EqualRect (&a.rect, &b.rect) != FALSE &&
           a.rowTop     == b.rowTop     &&
           a.firstLeft  == b.firstLeft  &&
           a.lineHeight == b.lineHeight &&
           a.lineCount  == b.lineCount  &&
           a.fontPx     == b.fontPx     &&
           a.face       == b.face       &&
           a.fill       == b.fill       &&
           a.hasSymbol  == b.hasSymbol  &&
           a.runs       == b.runs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::Paint
//
//  The tip's popup is cleared to transparent before this runs. The row's
//  line is filled from its first redrawn text, and the view's drawing of the
//  row is visible left of that; every line below it is filled across the
//  whole row. No outline.
//
////////////////////////////////////////////////////////////////////////////////

void OperandResultTip::Paint (
    const Layout       & layout,
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text)
{
    float  width    = (float) (layout.rect.right - layout.rect.left);
    float  height   = (float) layout.lineHeight;
    float  rowY     = (float) (layout.rowTop - layout.rect.top);
    bool   hasFirst = std::ranges::any_of (layout.runs, [] (const Run & run) { return run.line == 0; });



    if (hasFirst)
    {
        painter.FillRect ((float) (layout.firstLeft - layout.rect.left), rowY, (float) (layout.rect.right - layout.firstLeft), height, layout.fill);
    }

    if (layout.lineCount > 1)
    {
        painter.FillRect (0.0f, rowY + height, width, height * (float) (layout.lineCount - 1), layout.fill);
    }

    for (const Run & run : layout.runs)
    {
        PaintRun (layout, run, run.leftPx - (float) layout.rect.left, rowY + height * (float) run.line, text);
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
//  OperandResultTip::GetClip
//
//  Measured against the box the view clips the part's text to, so a part is
//  cut exactly when the view's drawing of it is clipped. A part past the
//  view's right edge, or whose drawing ends left of its left edge, is out of
//  view; one starting left of that edge has its start out of sight.
//
////////////////////////////////////////////////////////////////////////////////

OperandResultTip::Clip OperandResultTip::GetClip (
    const Segment    & segment,
    float              textPx,
    const Placement  & placement)
{
    float  drawnEnd = (std::min) (segment.leftPx + textPx, segment.clipRightPx);



    if (segment.leftPx >= (float) placement.row.right)
    {
        return Clip::PastRight;
    }

    if (drawnEnd <= (float) placement.visibleLeft)
    {
        return Clip::PastLeft;
    }

    if (segment.leftPx < (float) placement.visibleLeft)
    {
        return Clip::StartHidden;
    }

    return (textPx > segment.clipRightPx - segment.leftPx) ? Clip::Cut : Clip::Whole;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::TryGetPieces
//
//  The row's line runs from the first part the view cuts off or draws
//  past its edge to the row's end, each part where the view draws it unless
//  the part ahead of it, drawn whole, runs into it; it then follows that
//  part, a padding's width apart, as columns are. False where every part is
//  whole in view, apart from parts scrolled out of sight to the left, or a
//  part cannot be measured.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandResultTip::TryGetPieces (
    const Placement    & placement,
    IDxuiTextRenderer  & text,
    Pieces             & out,
    bool               & hasSymbol)
{
    HRESULT             hr        = S_OK;
    size_t              count     = placement.segments.size();
    std::vector<Clip>   clips     (count, Clip::Whole);
    int                 first     = -1;
    int                 operand   = (placement.operand >= 0 && placement.operand < (int) count) ? placement.operand : -1;
    float               gapPx     = (float) (placement.padLeftPx + placement.padRightPx);
    float               rightPx   = -FLT_MAX;
    float               leftPx    = 0.0f;
    float               textPx    = 0.0f;
    bool                isHidden  = false;
    bool                isMade    = false;
    Piece               piece;



    for (size_t i = 0; i < count; i++)
    {
        const Segment  & segment = placement.segments[i];

        if (segment.text.empty())
        {
            continue;
        }

        textPx = MeasurePx (text, segment.text, placement.fontPx, placement.face);
        CBR (textPx > 0.0f);

        clips[i] = GetClip (segment, textPx, placement);

        if (first < 0 && (clips[i] == Clip::Cut || (clips[i] == Clip::PastRight && (int) i != operand)))
        {
            first = (int) i;
        }
    }

    isHidden = operand >= 0 && !placement.segments[(size_t) operand].text.empty() &&
               (clips[(size_t) operand] == Clip::PastRight || clips[(size_t) operand] == Clip::PastLeft);
    CBR (first >= 0 || isHidden);

    hasSymbol = operand >= 0 && !placement.symbol.empty();

    for (int i = (first >= 0) ? first : (int) count; i < (int) count; i++)
    {
        const Segment  & segment = placement.segments[(size_t) i];

        if (segment.text.empty() || clips[(size_t) i] == Clip::PastLeft || clips[(size_t) i] == Clip::StartHidden)
        {
            continue;
        }

        leftPx = (std::max) (segment.leftPx, rightPx + gapPx);

        if (i == operand)
        {
            AddOperand (placement, segment, clips[(size_t) i], leftPx, clips[(size_t) i] != Clip::Whole || hasSymbol, out);
            continue;
        }

        piece = MakePiece (segment, 0, (int) segment.text.size(), leftPx);
        out.first.push_back (piece);

        rightPx = leftPx + MeasurePx (text, piece.text, placement.fontPx, placement.face);
    }

    //  The operand and result out of view, with nothing ahead of them cut
    //  off, lie wholly below the row.
    if (isHidden && (first < 0 || clips[(size_t) operand] == Clip::PastLeft))
    {
        AddOperand (placement, placement.segments[(size_t) operand], clips[(size_t) operand], placement.segments[(size_t) operand].leftPx, true, out);
    }

    isMade = !out.first.empty() || !out.below.empty();

Error:
    return isMade;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::AddOperand
//
//  The operand and result as a whole where the view draws them whole and
//  the operand has no symbol to add. Otherwise apart: a source line's
//  comment in their place on the row's line, the operand with its symbol
//  and the result each on a line below, under the row's code; with no
//  comment, an operand in view stays where it is, and its result goes below
//  it, from its left. An operand out of view lies below under the code, as
//  its result does.
//
////////////////////////////////////////////////////////////////////////////////

void OperandResultTip::AddOperand (
    const Placement  & placement,
    const Segment    & segment,
    Clip               clip,
    float              inPlacePx,
    bool               isSplit,
    Pieces           & pieces)
{
    int     size       = (int) segment.text.size();
    int     commentAt  = (segment.commentAt >= 0 && segment.commentAt < size) ? segment.commentAt : size;
    int     resultAt   = (segment.resultAt  >= 0 && segment.resultAt  < commentAt) ? segment.resultAt : commentAt;
    int     operandEnd = resultAt;
    int     resultEnd  = commentAt;
    bool    isHidden   = clip == Clip::PastRight || clip == Clip::PastLeft;
    bool    hasComment = commentAt < size;
    float   belowPx    = (std::max) (placement.codeLeftPx, (float) (placement.visibleLeft + placement.padLeftPx));
    Piece   operand;
    Piece   result;
    Piece   comment;



    if (!isSplit)
    {
        pieces.first.push_back (MakePiece (segment, 0, size, inPlacePx));
        return;
    }

    while (operandEnd > 0 && segment.text[(size_t) (operandEnd - 1)] == L' ')
    {
        operandEnd--;
    }

    while (resultEnd > resultAt && segment.text[(size_t) (resultEnd - 1)] == L' ')
    {
        resultEnd--;
    }

    operand = MakePiece (segment, 0, operandEnd, belowPx);
    result  = MakePiece (segment, resultAt, resultEnd, belowPx);
    comment = MakePiece (segment, commentAt, size, inPlacePx);

    comment.isWholeLine = true;

    //  The symbol is another of the operand's values, a space after them.
    if (!placement.symbol.empty())
    {
        operand.text += operand.text.empty() ? L"" : L" ";
        operand.text += placement.symbol;
        operand.colors.resize (operand.text.size(), placement.symbolInk);
    }

    //  Scrolled out of sight to the left, the comment has no place on the
    //  row's line, so it goes below too.
    if (hasComment && clip == Clip::PastLeft)
    {
        comment.leftPx = belowPx;
        pieces.below.push_back (comment);
    }
    else if (hasComment)
    {
        pieces.first.push_back (comment);
    }
    else if (!isHidden)
    {
        operand.leftPx = inPlacePx;
        result.leftPx  = inPlacePx;
        pieces.first.push_back (operand);
        operand.text.clear();
    }

    for (const Piece * each : { &operand, &result })
    {
        if (!each->text.empty())
        {
            pieces.below.push_back (*each);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::MakePiece
//
////////////////////////////////////////////////////////////////////////////////

OperandResultTip::Piece OperandResultTip::MakePiece (
    const Segment  & segment,
    int              first,
    int              end,
    float            leftPx)
{
    Piece  piece;



    piece.text        = segment.text.substr ((size_t) first, (size_t) (std::max) (0, end - first));
    piece.leftPx      = leftPx;
    piece.isItalic    = segment.isItalic;
    piece.isWholeLine = segment.isWholeLine;

    for (int k = first; k < end; k++)
    {
        piece.colors.push_back ((k < (int) segment.colors.size()) ? segment.colors[(size_t) k] : 0);
    }

    return piece;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::TryWrapFirst
//
//  The row's line: every part but the last where it is, and the last run
//  on to its end, wrapped where it is wider than the screen's room. False
//  where a part ahead of the last runs off the screen.
//
////////////////////////////////////////////////////////////////////////////////

bool OperandResultTip::TryWrapFirst (
    const Placement      & placement,
    IDxuiTextRenderer    & text,
    std::vector<Piece>   & pieces,
    Layout               & layout)
{
    HRESULT  hr        = S_OK;
    bool     doesFit   = true;
    bool     hasPieces = false;
    float    widthPx   = 0.0f;



    layout.lineCount = 1;
    layout.firstLeft = (int) placement.row.right;

    hasPieces = !pieces.empty();
    BAIL_OUT_IF (!hasPieces, S_OK);

    layout.firstLeft = (std::max) ((int) std::floor (pieces.front().leftPx) - placement.padLeftPx, (int) placement.row.left);

    for (size_t i = 0; i + 1 < pieces.size(); i++)
    {
        widthPx = MeasurePx (text, pieces[i].text, placement.fontPx, placement.face);
        doesFit = pieces[i].leftPx + widthPx <= (float) (placement.workArea.right - placement.padRightPx);
        CBR (doesFit);

        layout.runs.push_back ({ 0, pieces[i].leftPx, pieces[i].text, pieces[i].colors, pieces[i].isItalic });
    }

    layout.lineCount = 0;
    WrapPiece (placement, text, pieces.back(), layout);

Error:
    return doesFit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::WrapPiece
//
//  A piece from the next line, or from the row's line for the last piece
//  on it: a source or data line by its comment, anything else from its left.
//
////////////////////////////////////////////////////////////////////////////////

void OperandResultTip::WrapPiece (
    const Placement    & placement,
    IDxuiTextRenderer  & text,
    const Piece        & piece,
    Layout             & layout)
{
    float   roomPx = GetRoomPx (placement, piece.leftPx);
    Indent  indent;



    if (piece.isWholeLine)
    {
        indent = MakeIndent (piece, text, layout, roomPx);
    }

    WrapRun (text, piece, layout, roomPx, indent);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::GetRoomPx
//
////////////////////////////////////////////////////////////////////////////////

float OperandResultTip::GetRoomPx (
    const Placement  & placement,
    float              leftPx)
{
    return (float) (placement.workArea.right - placement.padRightPx) - leftPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::MakeIndent
//
//  A line that starts in the comment starts under the comment's first
//  character, after the ; and the spaces past it; a line that starts before
//  the comment starts four characters right of the code's start. An indent
//  with less than the least room after it is dropped.
//
////////////////////////////////////////////////////////////////////////////////

OperandResultTip::Indent OperandResultTip::MakeIndent (
    const Piece        & piece,
    IDxuiTextRenderer  & text,
    const Layout       & layout,
    float                roomPx)
{
    int     size      = (int) piece.text.size();
    int     codeAt    = 0;
    float   minRoomPx = MeasurePx (text, std::wstring (kMinIndentRoomChars, L'0'), layout.fontPx, layout.face);
    float   stepPx    = MeasurePx (text, std::wstring (kCodeIndentChars,    L'0'), layout.fontPx, layout.face);
    Indent  indent;



    while (codeAt < size && iswspace (piece.text[(size_t) codeAt]))
    {
        codeAt++;
    }

    indent.commentAt = FindCommentText (piece.text, 0);
    indent.codePx    = MeasurePx (text, piece.text.substr (0, (size_t) codeAt), layout.fontPx, layout.face) + stepPx;
    indent.commentPx = (indent.commentAt < size) ? MeasurePx (text, piece.text.substr (0, (size_t) indent.commentAt), layout.fontPx, layout.face) : 0.0f;

    indent.codePx    = (roomPx - indent.codePx    < minRoomPx) ? 0.0f : indent.codePx;
    indent.commentPx = (roomPx - indent.commentPx < minRoomPx) ? 0.0f : indent.commentPx;

    return indent;
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
//  A piece broken across lines, from the line after the last one laid out,
//  each broken at its last space where it has one, and mid-word only where a
//  word is wider than a line. The spaces at a break go with it. The lines
//  after the first start at the indent for where they start, with that much
//  less room.
//
////////////////////////////////////////////////////////////////////////////////

void OperandResultTip::WrapRun (
    IDxuiTextRenderer  & text,
    const Piece        & piece,
    Layout             & layout,
    float                maxWidthPx,
    const Indent       & indent)
{
    int    end      = (int) piece.text.size();
    int    start    = 0;
    int    stop     = 0;
    int    next     = 0;
    int    line     = layout.lineCount;
    bool   isShort  = false;
    float  indentPx = 0.0f;



    while (start < end)
    {
        indentPx = (start == 0) ? 0.0f : (start >= indent.commentAt) ? indent.commentPx : indent.codePx;
        stop     = FindFitEnd (text, layout, piece.text, start, end, maxWidthPx - indentPx);
        isShort  = stop < end;

        for (int k = stop; isShort && k > start; k--)
        {
            if (piece.text[(size_t) k] == L' ')
            {
                stop = k;
                break;
            }
        }

        next = stop;

        while (stop > start && piece.text[(size_t) (stop - 1)] == L' ')
        {
            stop--;
        }

        if (stop > start)
        {
            layout.runs.push_back ({ line,
                                     piece.leftPx + indentPx,
                                     piece.text.substr ((size_t) start, (size_t) (stop - start)),
                                     std::vector<uint32_t> (piece.colors.begin() + start, piece.colors.begin() + stop),
                                     piece.isItalic });
            line++;
        }

        start = next;

        while (start < end && piece.text[(size_t) start] == L' ')
        {
            start++;
        }
    }

    layout.lineCount = (std::max) (layout.lineCount, line);
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
    IDxuiTextRenderer    & text,
    const Layout         & layout,
    const std::wstring   & chars,
    int                    first,
    int                    end,
    float                  maxWidthPx)
{
    int    low   = first + 1;
    int    high  = end;
    int    mid   = 0;
    float  width = 0.0f;



    while (low < high)
    {
        mid   = low + (high - low + 1) / 2;
        width = MeasurePx (text, chars.substr ((size_t) first, (size_t) (mid - first)), layout.fontPx, layout.face);

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
//  OperandResultTip::MeasureRightPx
//
//  Where the run that ends furthest right ends.
//
////////////////////////////////////////////////////////////////////////////////

float OperandResultTip::MeasureRightPx (
    IDxuiTextRenderer  & text,
    const Layout       & layout)
{
    float  rightPx = 0.0f;



    for (const Run & run : layout.runs)
    {
        rightPx = (std::max) (rightPx, run.leftPx + MeasurePx (text, run.text, layout.fontPx, layout.face));
    }

    return rightPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::PlaceRect
//
//  From the row's start, or for a tip of the row's line alone from its
//  first redrawn text, to the end of the run that ends furthest right or to
//  the view's edge, whichever is further, and no further than the screen's
//  edge. From the row's top, or the line below it where the row's line
//  has nothing redrawn.
//
//  Its width and height are ones its popup comes up at exactly, larger where
//  the screen has room and smaller where it does not.
//
////////////////////////////////////////////////////////////////////////////////

RECT OperandResultTip::PlaceRect (
    const Placement  & placement,
    Layout           & layout,
    float              rightPx)
{
    bool  hasFirst = std::ranges::any_of (layout.runs, [] (const Run & run) { return run.line == 0; });
    int   textEnd  = (int) std::ceil (rightPx) + placement.padRightPx;
    RECT  rect     = {};



    rect.left   = (layout.lineCount > 1) ? (int) placement.row.left : layout.firstLeft;
    rect.top    = layout.rowTop + (hasFirst ? 0 : layout.lineHeight);
    rect.right  = (std::min) ((std::max) ((int) placement.row.right, textEnd), (int) placement.workArea.right);
    rect.bottom = layout.rowTop + layout.lineCount * layout.lineHeight;

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
//  OperandResultTip::MakeReadable
//
//  Every character's color moved until it reads on the fill, as the view
//  moves them.
//
////////////////////////////////////////////////////////////////////////////////

void OperandResultTip::MakeReadable (Layout & layout)
{
    for (Run & run : layout.runs)
    {
        for (uint32_t & each : run.colors)
        {
            each = DxuiColor::ComputeInkForContrast (each, layout.fill, DebuggerTextColors::s_kMinTextContrast);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip::PaintRun
//
//  A stretch at a time in each color, each placed by measuring the run
//  before it, as the list draws a cell of several colors, in the italic of
//  a row drawn in italics.
//
////////////////////////////////////////////////////////////////////////////////

void OperandResultTip::PaintRun (
    const Layout       & layout,
    const Run          & run,
    float                x,
    float                y,
    IDxuiTextRenderer  & text)
{
    HRESULT  hr     = S_OK;
    int      pos    = 0;
    int      end    = (int) run.text.size();
    int      runEnd = 0;
    float    offset = 0.0f;
    float    width  = (float) (layout.rect.right - layout.rect.left) - x;



    while (pos < end)
    {
        runEnd = pos + 1;

        while (runEnd < end && run.colors[(size_t) runEnd] == run.colors[(size_t) pos])
        {
            runEnd++;
        }

        offset = (pos > 0) ? MeasurePx (text, run.text.substr (0, (size_t) pos), layout.fontPx, layout.face) : 0.0f;

        hr = text.DrawString (run.text.substr ((size_t) pos, (size_t) (runEnd - pos)).c_str(),
                              x + offset,
                              y,
                              width - offset,
                              (float) layout.lineHeight,
                              run.colors[(size_t) pos],
                              layout.fontPx,
                              layout.face,
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::CenterOnCapHeight,
                              run.isItalic ? DxuiFontWeight::Italic : DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        pos = runEnd;
    }
}
