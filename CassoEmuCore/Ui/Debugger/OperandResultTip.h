#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip
//
//  A code row's text drawn whole where its view cuts it off, laid over the
//  row as Explorer lays a cut-off name over its row. Each line of the tip
//  is filled with the row's highlight from the row's start to the tip's
//  right edge, so the tip reads as the row and more rows below it, all
//  highlighted.
//
//  The first line is the row's text, each part where the view draws
//  it, run on past the view's edge to the end of whatever the view cuts
//  off. The lines below it are the operand and then the result, under the
//  row's code. An operand in view but cut off stays on the first line, and
//  its result then starts under it. A source line's comment, at the end of
//  its operand and result, stays on the first line in their place.
//
//  Text wraps only at the screen's right edge: a source or data line under
//  its comment's first character where the break is in the comment, and
//  four characters right of its code's start otherwise.
//
//  Pure layout in the window's pixels, so the window only shows what this
//  gives it.
//
////////////////////////////////////////////////////////////////////////////////

class OperandResultTip
{
public:
    //  A part of the row's text as the view draws it: its characters and
    //  their colors, where the view starts them and where its drawing of
    //  them is clipped. The operand and result part has the result's start
    //  in its text and the comment's, the length of the text where it has
    //  none.
    struct Segment
    {
        std::wstring           text;
        std::vector<uint32_t>  colors;
        float                  leftPx      = 0.0f;
        float                  clipRightPx = 0.0f;
        bool                   isItalic    = false;
        bool                   isWholeLine = false;   // a source or data line, which wraps by its comment
        bool                   isBelowLine = false;   // an operand and result after a source line in one cell, which lie below the line
        int                    resultAt    = -1;
        int                    commentAt   = -1;
    };

    //  The row in the window's pixels and how its view draws it.
    struct Placement
    {
        RECT                    row          = {};        // left is where the row's highlight starts, right the view's edge
        int                     visibleLeft  = 0;         // the view's text area's left, which a part starting before is out of sight of
        float                   codeLeftPx   = 0.0f;      // where the row's code starts, which the lines below line up under
        std::vector<Segment>    segments;
        int                     operand      = -1;        // the operand and result part, or -1 for none
        std::wstring            symbol;                   // the operand's symbol as "name = $value", drawn as another operand value
        uint32_t                symbolInk    = 0;         // the operand's color, which the symbol is drawn in
        bool                    isInline     = false;     // the operand belongs on the row's line, as in a source document, and goes below only for a comment
        int                     padLeftPx    = 0;         // the room either side of a part's text
        int                     padRightPx   = 0;
        float                   fontPx       = 0.0f;
        const wchar_t         * face         = nullptr;
        uint32_t                fill         = 0;         // the row's highlight over the view's fill, opaque
        RECT                    workArea     = {};        // the work area of the monitor the row starts on, in the same pixels
        UINT                    dpi          = DxuiDpiScaler::kBaseDpi;
    };

    //  A run of text on one of the tip's lines, line 0 being the row's line,
    //  starting at `leftPx` in the window's pixels.
    struct Run
    {
        int                    line     = 0;
        float                  leftPx   = 0.0f;
        std::wstring           text;
        std::vector<uint32_t>  colors;
        bool                   isItalic = false;

        bool operator== (const Run & other) const = default;
    };

    //  The tip in the window's pixels. The row's line is filled from
    //  `firstLeft`, and every line below it from the rect's left, the row's
    //  start, to the rect's right.
    struct Layout
    {
        RECT                   rect       = {};
        int                    rowTop     = 0;
        int                    firstLeft  = 0;
        int                    lineHeight = 0;
        int                    lineCount  = 0;
        float                  fontPx     = 0.0f;
        const wchar_t        * face       = nullptr;
        uint32_t               fill       = 0;
        bool                   hasSymbol  = false;
        std::vector<Run>       runs;
    };

    //  A wrapped source line's lines after its first start this many
    //  characters right of its code, where the break is not in a comment.
    static constexpr int  kCodeIndentChars     = 4;

    //  An indented line has at least this many characters of room; with
    //  less, it starts at the part's left.
    static constexpr int  kMinIndentRoomChars  = 8;

    //  False where everything fits where the view draws it, and where there
    //  is no text, it cannot be measured, the row's start is off its screen,
    //  or the tip does not fit on that screen.
    static bool  TryMakeLayout (const Placement & placement, IDxuiTextRenderer & text, Layout & out);

    //  Whether two layouts draw the same thing in the same place.
    static bool  IsSame        (const Layout & a, const Layout & b);

    //  The tip's fills and text, its top left at the origin, as a popup's
    //  content is drawn on a popup cleared to transparent.
    static void  Paint         (const Layout & layout, IDxuiPainter & painter, IDxuiTextRenderer & text);

    //  The size, in pixels, of a popup sized `px` pixels once that size has
    //  been through DIPs and back at `dpi`, as an in-place tip's is.
    static int   GetPopupPx    (int px, UINT dpi);

private:
    //  How much of a part the view draws.
    enum class Clip
    {
        Whole,
        Cut,
        PastRight,
        PastLeft,
        StartHidden,
    };

    //  Where the lines after a run's first start: past `commentAt`, the
    //  comment's indent, and before it the code's.
    struct Indent
    {
        int    commentAt = INT_MAX;
        float  codePx    = 0.0f;
        float  commentPx = 0.0f;
    };

    //  A run before it is wrapped: its text and colors, its left, where the
    //  view draws its part, and whether it wraps as a source line does.
    struct Piece
    {
        std::wstring           text;
        std::vector<uint32_t>  colors;
        float                  leftPx      = 0.0f;
        float                  viewLeftPx  = 0.0f;
        bool                   isItalic    = false;
        bool                   isWholeLine = false;
    };

    //  The tip's pieces: those on the row's line, and those below it.
    struct Pieces
    {
        std::vector<Piece>  first;
        std::vector<Piece>  below;
    };

    static float   MeasurePx       (IDxuiTextRenderer & text, const std::wstring & chars, float fontPx, const wchar_t * face);
    static Clip    GetClip         (const Segment & segment, float textPx, const Placement & placement);
    static bool    TryGetPieces    (const Placement & placement, IDxuiTextRenderer & text, Pieces & out, bool & hasSymbol);
    static void    AddOperand      (const Placement & placement, const Segment & segment, Clip clip, float inPlacePx, bool isSplit, Pieces & pieces);
    static Piece   MakePiece       (const Segment & segment, int first, int end, float leftPx);
    static void    WrapFirst       (const Placement & placement, IDxuiTextRenderer & text, std::vector<Piece> & pieces, Layout & layout);
    static void    WrapPiece       (const Placement & placement, IDxuiTextRenderer & text, const Piece & piece, Layout & layout);
    static void    WrapPieceAt     (const Placement & placement, IDxuiTextRenderer & text, Piece piece, int line, Layout & layout);
    static float   GetRoomPx       (const Placement & placement, float leftPx);
    static Indent  MakeIndent      (const Piece & piece, IDxuiTextRenderer & text, const Layout & layout, float roomPx);
    static int     FindCommentText (const std::wstring & chars, int first);
    static void    WrapRun         (IDxuiTextRenderer & text, const Piece & piece, Layout & layout, float maxWidthPx, const Indent & indent);
    static int     FindFitEnd      (IDxuiTextRenderer & text, const Layout & layout, const std::wstring & chars, int first, int end, float maxWidthPx);
    static float   MeasureRightPx  (IDxuiTextRenderer & text, const Layout & layout);
    static RECT    PlaceRect       (const Placement & placement, Layout & layout, float rightPx);
    static int     FitPopupPx      (int px, int limitPx, UINT dpi);
    static void    MakeReadable    (Layout & layout);
    static void    PaintRun        (const Layout & layout, const Run & run, float x, float y, IDxuiTextRenderer & text);
};
