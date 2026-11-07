#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip
//
//  A disassembly row's operand and result shown whole where the pane cuts
//  them off, laid over their cell as Explorer lays a cut-off name over its
//  row. The text starts exactly where the cell's does, in the cell's face,
//  size and colors on the cell's fill, so the tip reads as the cell grown
//  to fit: the operand on its first line and the result on the next, both
//  from the same left and as wide as the wider of the two, each wrapped only
//  at the screen's right edge, each line a row high.
//
//  A source or data line that runs on across the row is drawn on one line
//  from the line's start, wrapped only at the screen's right edge: under
//  the comment's first character where the break is in a comment, and
//  four characters right of the code's start otherwise.
//
//  Pure layout in the window's pixels, so the window only shows what this
//  gives it.
//
////////////////////////////////////////////////////////////////////////////////

class OperandResultTip
{
public:
    //  Where the list draws the cell, and how.
    struct Placement
    {
        RECT              textRect          = {};        // left is where the text starts, right the column's edge; top and bottom are its row's
        int               visibleLeft       = 0;         // the pane's edges, past which the list draws nothing
        int               visibleRight      = 0;
        float             clipLeftPx        = 0.0f;      // where the view starts the cell's text, and where its drawing of the text is clipped
        float             clipRightPx       = 0.0f;
        int               padLeftPx         = 0;         // the cell's room either side of its text
        int               padRightPx        = 0;
        float             fontPx            = 0.0f;
        const wchar_t   * face              = nullptr;
        uint32_t          contentBackground = 0;         // the list's fill
        uint32_t          rowFill           = 0;         // the row's own over it: the selection's, the hover's, or none
        uint32_t          ink               = 0;         // the text's color where the cell gives none
        uint32_t          border            = 0;
        RECT              workArea          = {};        // the monitor's work area, in the same pixels
        UINT              dpi               = DxuiDpiScaler::kBaseDpi;   // the scale the tip's popup is sized at

        //  A cell that is a whole line running on across the row, a source
        //  line's or a data line's, and the part of that line drawn in a
        //  column ahead of the cell, such as its label, with where that
        //  part's text starts. The tip starts at the line's start.
        bool                  isWholeLine       = false;
        DxuiListView::Cell    lead;
        int                   leadLeft          = 0;
    };

    //  One line of the tip: a run of the text, drawn `indentPx` right of the
    //  tip's text origin.
    struct Line
    {
        int    start    = 0;
        int    length   = 0;
        float  indentPx = 0.0f;

        bool operator== (const Line & other) const = default;
    };

    //  The tip in the window's pixels, and everything Paint uses. A rectangle
    //  from the line's start to the end of its widest line. The text starts
    //  with the lead's characters, drawn from `leadLeft` on the first line;
    //  the lines index the text after them.
    struct Layout
    {
        RECT                     rect       = {};
        POINT                    textOrigin = {};
        int                      leadLeft   = 0;
        int                      leadLength = 0;
        int                      lineHeight = 0;
        float                    fontPx     = 0.0f;
        const wchar_t          * face       = nullptr;
        uint32_t                 fill       = 0;
        uint32_t                 border     = 0;
        std::wstring             text;
        std::vector<uint32_t>    colors;
        std::vector<Line>        lines;
    };

    static constexpr int  kBorderPx            = 1;

    //  A wrapped source line's lines after its first start this many
    //  characters right of its code, where the break is not in a comment.
    static constexpr int  kCodeIndentChars     = 4;

    //  An indented line has at least this many characters of room; with
    //  less, it starts at the tip's text origin.
    static constexpr int  kMinIndentRoomChars  = 8;

    //  False where the cell's text fits where the list draws it, and where
    //  there is no text, its start is out of sight or off the screen, it
    //  cannot be measured, or it does not fit on the screen below its row.
    //  A cell wholly outside the pane has all its lines below its row.
    static bool  TryMakeLayout (const DxuiListView::Cell & cell, const Placement & placement, IDxuiTextRenderer & text, Layout & out);

    //  Whether the cell's text lies wholly outside the pane: starting past its
    //  right edge, or ending left of its left edge.
    static bool  IsCellHidden  (const DxuiListView::Cell & cell, const Placement & placement, IDxuiTextRenderer & text);

    //  Whether two layouts draw the same thing in the same place.
    static bool  IsSame        (const Layout & a, const Layout & b);

    //  The tip's outline and lines, its top left at the origin, as a popup's
    //  content is drawn on a popup cleared to the layout's fill.
    static void  Paint         (const Layout & layout, IDxuiPainter & painter, IDxuiTextRenderer & text);

    //  The size, in pixels, of a popup sized `px` pixels once that size has
    //  been through DIPs and back at `dpi`, as an in-place tip's is.
    static int   GetPopupPx    (int px, UINT dpi);

private:
    //  Where the lines after a run's first start: past `commentAt`, the
    //  comment's indent, and before it the code's.
    struct Indent
    {
        int    commentAt = INT_MAX;
        float  codePx    = 0.0f;
        float  commentPx = 0.0f;
    };

    static float                  MeasurePx       (IDxuiTextRenderer & text, const std::wstring & chars, float fontPx, const wchar_t * face);
    static float                  MeasureWidestPx (IDxuiTextRenderer & text, const Layout & layout);
    static int                    GetBottomPx     (const Layout & layout);
    static bool                   TryWrapOnScreen (IDxuiTextRenderer & text, const Placement & placement, Layout & layout, float & widestPx);
    static RECT                   PlaceRect       (const Placement & placement, const Layout & layout, int cellRight, float widestPx);
    static int                    FitPopupPx      (int px, int limitPx, UINT dpi);
    static uint32_t               MakeFill        (const DxuiListView::Cell & cell, uint32_t rowFill, uint32_t contentBackground);
    static std::vector<uint32_t>  MakeColors      (const DxuiListView::Cell & cell, uint32_t ink, uint32_t fill);
    static void                   SetLead         (const Placement & placement, IDxuiTextRenderer & text, Layout & layout);
    static void                   WrapCell        (IDxuiTextRenderer & text, Layout & layout, float maxWidthPx);
    static void                   WrapLine        (IDxuiTextRenderer & text, Layout & layout, float maxWidthPx);
    static int                    FindCommentText (const std::wstring & chars, int first);
    static void                   WrapRun         (IDxuiTextRenderer & text, Layout & layout, int first, int end, float maxWidthPx, const Indent & indent);
    static int                    FindFitEnd      (IDxuiTextRenderer & text, const Layout & layout, int first, int end, float maxWidthPx);
    static void                   PaintLine       (const Layout & layout, const Line & line, float x, float y, float widthPx, IDxuiTextRenderer & text);
};
