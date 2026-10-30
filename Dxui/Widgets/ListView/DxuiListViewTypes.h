#pragma once

#include "Pch.h"

struct DxuiIconImage;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewTypes
//
//  The types DxuiListView and its parts share. DxuiListView derives from it, so
//  each is also DxuiListView::<type>.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiListViewTypes
{
    struct Column
    {
        std::wstring    title;
        int             widthDip = 0;   // 0 = auto-fit content (or stretch if stretch=true)
        bool            stretch  = false;   // when true, column absorbs any remaining width after fixed/auto
        DxuiTextHAlign  align    = DxuiTextHAlign::Left;
        bool            visible  = true;

        //  A column the list can be filtered on has File Explorer's chevron
        //  on its header while hovered, and always while filtered.
        bool            filterable = false;
        bool            filtered   = false;
    };

    struct Cell
    {
        std::wstring  text;
        bool          dim = false;     // muted color (e.g. "(Download)" hint)

        // Half-open character ranges [first, second) within `text` to paint
        // as search-match highlights. Empty = none. Supplied sorted and
        // non-overlapping; honored only for left-aligned columns.
        std::vector<std::pair<int, int>>  matches;

        //  Half-open character ranges drawn in the muted color within text
        //  that is otherwise not. Sorted, non-overlapping, left-aligned
        //  columns only, as matches are.
        std::vector<std::pair<int, int>>  dimRanges;

        //  Drawn at the start of the cell, the text moved along past it.
        std::shared_ptr<const DxuiIconImage>  icon;

        //  The icon drawn at half opacity, as Explorer draws a hidden item's.
        bool          iconGhosted = false;

        //  A red badge with a cross at the icon's lower left, for an item
        //  that cannot be opened.
        bool          iconBroken  = false;

        //  The text's color in place of the theme's; zero keeps the theme's.
        uint32_t      argb        = 0;

        //  A usage bar drawn in place of the text, filled to this fraction;
        //  negative draws none. Explorer's drive tiles have one.
        float         meter       = -1.0f;

        //  The lines Tiles and Content draw under the name in place of the
        //  other columns, for a row whose tile shows something else.
        std::vector<Cell>  tileLines;

        //  Tiles shows the name alone, as Explorer's does for a folder; the
        //  Content view still shows the other columns.
        bool          tileNameOnly = false;

        //  The Content view's lines, as File Explorer lays them out: one under
        //  the name, two in a middle column, and two in a column at the right,
        //  each top to bottom. A line with no text is left blank, keeping the
        //  lines below it in place.
        std::vector<Cell>  contentLeft;
        std::vector<Cell>  contentMiddle;
        std::vector<Cell>  contentRight;

        //  The Content view's name for the value in `text`, as "Size:", drawn
        //  muted ahead of it.
        std::wstring       label;

        //  A search result's folder, ahead of the first line under the name
        //  and cut short to leave that line room, as Explorer shows it.
        std::wstring       contentPath;
    };

    // Geometry of every interactive scrollbar region, in coordinates
    // relative to the widget rect's top-left. arrowH == 0 means the bar
    // is too short for arrow buttons.
    struct ScrollbarMetrics
    {
        bool   visible      = false;
        int    barX         = 0;
        int    barW         = 0;
        int    arrowH       = 0;
        int    upArrowTop   = 0;
        int    downArrowTop = 0;
        int    trackTop     = 0;
        int    trackH       = 0;
        float  thumbTop     = 0.0f;
        float  thumbH       = 0.0f;
    };

    // Horizontal-scroll counterpart to ScrollbarMetrics, in coordinates
    // relative to the widget rect's top-left. arrowW == 0 means the bar
    // is too short for arrow buttons.
    struct HorzScrollbarMetrics
    {
        bool   visible     = false;
        int    barY        = 0;
        int    barH        = 0;
        int    arrowW      = 0;
        int    leftArrowX  = 0;
        int    rightArrowX = 0;
        int    trackLeft   = 0;
        int    trackW      = 0;
        float  thumbLeft   = 0.0f;
        float  thumbW      = 0.0f;
    };

    //  Groups over the rows, as Explorer's Group by draws them: a header line
    //  above each group's first row, which the rows must already be ordered
    //  for. Row indices everywhere stay the rows' own; only the lines they
    //  are drawn on move. Details view only; empty for none.
    struct Group
    {
        std::wstring  label;
        int           firstRow  = 0;
        bool          collapsed = false;   // its rows hidden, the header kept
    };

    // Virtual (provider) row model. Instead of materializing every row up
    // front via SetRows, the host supplies a total row count plus a callback
    // that fills one row's cells on demand. Paint pulls only the rows in the
    // visible window, so a 100k-row live log costs O(visible) per frame
    // instead of O(total) allocations (GH #88). Mutually exclusive with
    // SetRows/AppendRows: installing a provider drops any pushed rows, and
    // SetRows/AppendRows clears provider mode. The provider is invoked during
    // Paint (const) for rows in [GetTopRow(), +capacity); it must be a pure
    // read of host state valid for the ListView's lifetime.
    using RowProvider = std::function<void (int row, std::vector<Cell> & out)>;

    // A row source is a provider that hands back the host's own cells by
    // reference, for a host that keeps them: nothing is copied per paint, and
    // the host builds a row only when the list first asks for it. The
    // reference must stay valid until the next call.
    using RowSource = std::function<const std::vector<Cell> & (int row)>;

    // Each row's name alone, for a host that builds its rows' cells only as
    // they show: Small icons and List measure every name to size their
    // columns, and read it from here rather than building every row.
    using RowNameSource = std::function<std::wstring (int row)>;

    //  Explorer's eight views. Details is the table of columns; the others
    //  lay the rows out as items with the first cell's icon and text, and
    //  Tiles and Content add the next cells on the lines below. Selection,
    //  keyboard movement, hit testing and the callbacks mean the same in each.
    enum class View { Details, ExtraLargeIcons, LargeIcons, MediumIcons, SmallIcons, List, Tiles, Content };

    struct ItemMetrics
    {
        int   cellWDip   = 0;
        int   cellHDip   = 0;
        int   iconDip    = 0;
        bool  columns    = false;
        bool  labelBelow = false;
        int   textLines  = 1;

        //  The pitch of the lines beside the icon.
        int   lineDip    = 18;
    };

    //  Milliseconds since some fixed point, for type-ahead's reset. Defaults
    //  to the system tick count; a test supplies its own.
    using ClockFn = std::function<int64_t()>;

    // Which held scrollbar element is auto-repeating (None when idle).
    enum class ScrollRepeat
    {
        None,
        VertArrowUp,
        VertArrowDown,
        VertTrack,
        HorzArrowLeft,
        HorzArrowRight,
        HorzTrack,
    };

    // Per-element ARGB colors derived once per paint from the theme.
    struct Palette
    {
        uint32_t  fg       = 0;
        uint32_t  fgDim    = 0;
        uint32_t  hdrFg    = 0;
        uint32_t  bgRow    = 0;
        uint32_t  bgHover  = 0;
        uint32_t  bgSel    = 0;
        uint32_t  edgeSel  = 0;   // outline on the focused selected row; zero draws none
        uint32_t  bgHeader = 0;
        uint32_t  border   = 0;
        uint32_t  matchBg  = 0;
    };

    // Resolved scrollbar state for the current rect, columns, and rows.
    // vBar / hBar account for one another (the horizontal bar steals row
    // capacity; the vertical bar steals viewport width), so they are
    // resolved together. viewportW is the width available to columns
    // (full width minus the vertical bar); contentW is the natural total
    // column width (no stretch fill).
    struct ScrollLayout
    {
        bool  vBar      = false;
        bool  hBar      = false;
        int   rowCap    = 0;
        int   viewportW = 0;
        int   contentW  = 0;
        int   partialPx = 0;   // the part of a row the body shows past its last whole one
    };

    //  A group opening or closing slides its rows out from under its header,
    //  or back under it, and everything below moves with them, as Explorer's
    //  do: an ease in and out over a quarter second.
    struct GroupSlide
    {
        int      group      = -1;
        int64_t  startMs    = 0;
    };

    //  Where Details draws a line: a row, or a group's header. A slid row is
    //  one of the sliding group's, drawn inside the slide's clip.
    struct LineSpot
    {
        int    row   = -1;
        int    group = -1;
        float  y     = 0.0f;
        bool   slid  = false;
    };

    //  A page sliding into view from where the view was drawn; see BeginPageSlide.
    struct PageSlide
    {
        bool     active   = false;
        int      fromPx   = 0;    // from the list's top, or from fromLine's top
        int      fromLine = -1;   // an item view's line the start is kept against, or -1
        int64_t  startMs  = 0;
    };

    //  The item views' layout in pixels: cell size, items to a line, lines in
    //  all, and lines in sight.
    struct ItemGrid
    {
        int  cellW   = 0;   // the pitch from one item to the next along a line
        int  boxW    = 0;   // the item itself, narrower when a row is spread
        int  cellH   = 0;
        int  perLine = 1;
        int  lines   = 0;
        int  visible = 1;
    };

    //  Grouped item views: each line a group's header or a row of its items,
    //  scrolled by lines, as Details is.
    struct ItemLine
    {
        int  group  = -1;   // the header's group; -1 for a row of items
        int  first  = 0;    // the row's first item
        int  count  = 0;    // items on the row
        int  top    = 0;    // in the layout's own pixels
        int  height = 0;
    };

    struct ItemLayout
    {
        std::vector<ItemLine>  lines;
        std::vector<int>       rowLine;   // each item's line; -1 under a collapsed header
        int                    totalH  = 0;
    };

    //  A grouped item view mid-slide paints from a layout with the sliding
    //  group open: the lines past its header lifted by `lift`, every line
    //  moved by `shift` to the scroll of the layout the list keeps, and the
    //  group's items clipped to [clipTop, clipTop + clipH).
    struct ItemSlide
    {
        ItemLayout  layout;
        bool        active  = false;
        int         header  = -1;
        int         lift    = 0;
        int         shift   = 0;
        int         clipTop = 0;
        int         clipH   = 0;
        int         start   = 0;
        int         end     = 0;
    };

    //  Grouped List: a block of columns per group, side by side.
    struct ListBlock
    {
        int  group = -1;
        int  first = 0;
        int  count = 0;    // items shown; none under a collapsed header
        int  left  = 0;    // in the content's own pixels
        int  width = 0;

        //  Where each of its columns starts past the indent, and where the
        //  last ends: each as wide as the widest name in it, as Explorer's.
        std::vector<int>  colLefts;
    };

    struct ListLayout
    {
        std::vector<ListBlock>  blocks;
        std::vector<int>        blockOf;   // each item's block; -1 under a collapsed header
        int                     headerH = 0;
        int                     indent  = 0;
        int                     perCol  = 1;
        int                     totalW  = 0;
    };

    //  A column of List, grouped or not, as its page keys move across them.
    struct ListColumn
    {
        int  left  = 0;   // the item box, in the content's own pixels
        int  right = 0;
        int  first = 0;   // the items it holds
        int  count = 0;
    };

    //  What a group layout is built from. The layouts are kept until one of
    //  these changes, so a hit test or a paint reads them rather than
    //  rebuilding them, which walks every item.
    struct LayoutKey
    {
        int                               cellW   = 0;
        int                               cellH   = 0;
        int                               perLine = 0;
        int                               fullH   = 0;
        int                               barW    = 0;
        int                               headerH = 0;
        int                               indent  = 0;
        int                               rows    = 0;
        int                               names   = 0;   // the item name measurements' version
        int                               caption = 0;   // the caption line pitch
        std::vector<std::pair<int, bool>> groups;    // each group's first row and whether it is collapsed

        bool operator== (const LayoutKey &) const = default;
    };
};
