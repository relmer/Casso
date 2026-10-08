#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  OperandResultTip
//
//  A disassembly row's operand and result shown whole where the pane cuts
//  them off, laid over their cell as Explorer lays a cut-off name over its
//  row. The text starts exactly where the cell's does, in the cell's face,
//  size and colors on the cell's own fill, so the tip reads as the cell grown
//  to fit: the operand wrapped over as many lines as it needs, then the
//  result from the start of a line of its own, wrapped the same way, each
//  line a row high.
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
    };

    //  One line of the tip: a run of the cell's text.
    struct Line
    {
        int  start  = 0;
        int  length = 0;

        bool operator== (const Line & other) const = default;
    };

    //  The tip in the window's pixels, and all its paint needs.
    struct Layout
    {
        RECT                     rect       = {};
        POINT                    textOrigin = {};
        int                      lineHeight = 0;
        float                    fontPx     = 0.0f;
        const wchar_t          * face       = nullptr;
        uint32_t                 fill       = 0;
        uint32_t                 border     = 0;
        std::wstring             text;
        std::vector<uint32_t>    colors;
        std::vector<Line>        lines;
    };

    //  The tip is never narrower than this many characters, past the pane if
    //  it must be, so a cell scrolled nearly out of sight is not wrapped a
    //  character or two to a line; only the edge of the screen makes it
    //  narrower.
    static constexpr int  kMinWrapChars = 24;
    static constexpr int  kBorderPx     = 1;

    //  False where the cell's text fits where the list draws it, and where
    //  there is no text, its start is out of sight or off the screen, it
    //  cannot be measured, or it does not fit on the screen below its row.
    static bool  TryMakeLayout (const DxuiListView::Cell & cell, const Placement & placement, IDxuiTextRenderer & text, Layout & out);

    //  Whether two layouts draw the same thing in the same place.
    static bool  IsSame        (const Layout & a, const Layout & b);

    //  The tip's outline and lines, its top left at the origin, as a popup's
    //  content is drawn.
    static void  Paint         (const Layout & layout, IDxuiPainter & painter, IDxuiTextRenderer & text);

    //  The size, in pixels, of a popup sized `px` pixels once that size has
    //  been through DIPs and back at `dpi`, as an in-place tip's is.
    static int   GetPopupPx    (int px, UINT dpi);

private:
    static float                  MeasurePx       (IDxuiTextRenderer & text, const std::wstring & chars, float fontPx, const wchar_t * face);
    static float                  MeasureWidestPx (IDxuiTextRenderer & text, const Layout & layout);
    static int                    GetBottomPx     (const Layout & layout);
    static bool                   TryWrapOnScreen (IDxuiTextRenderer & text, const Placement & placement, float paneRoomPx, Layout & layout, float & widestPx);
    static RECT                   PlaceRect       (const Placement & placement, const Layout & layout, int cellRight, float widestPx);
    static int                    FitPopupPx      (int px, int limitPx, UINT dpi);
    static uint32_t               MakeFill        (const DxuiListView::Cell & cell, uint32_t rowFill, uint32_t contentBackground);
    static std::vector<uint32_t>  MakeColors      (const DxuiListView::Cell & cell, uint32_t ink, uint32_t fill);
    static void                   WrapCell        (IDxuiTextRenderer & text, Layout & layout, float maxWidthPx);
    static void                   WrapRun         (IDxuiTextRenderer & text, Layout & layout, int first, int end, float maxWidthPx);
    static int                    FindFitEnd      (IDxuiTextRenderer & text, const Layout & layout, int first, int end, float maxWidthPx);
    static void                   PaintLine       (const Layout & layout, const Line & line, float x, float y, float widthPx, IDxuiTextRenderer & text);
};
