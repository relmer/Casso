#pragma once

#include "Pch.h"

#include "Widgets/ListView/DxuiListViewTypes.h"

class DxuiListView;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListItemLayout
//
//  The icon, tile, content and list views' layouts: item metrics, the
//  grid of items and its lines, list columns, each item's rect and hit
//  test, page slides, and the selection band.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListItemLayout
{
public:
    explicit DxuiListItemLayout (DxuiListView & list);

    static DxuiListViewTypes::ItemMetrics  GetItemMetrics   (DxuiListViewTypes::View view);
    int                 GetItemBoxPx     () const;
    static constexpr int    s_kItemsTopDip         = 6;
    static constexpr float  s_kItemsBarDip         = 17.33f;
    static constexpr int    s_kMeasureAllItemsMax  = 500;

    //  Measured at 100%, 125% and 150%. Medium's and Large's boxes are widths
    //  in dip, a part pixel dropped, and Large's is wider from 150% up; Extra large's is its icon, which stops at 256 px, and a margin
    //  part pixels and part dip. A box is as tall as its icon, its caption
    //  lines and 5.33 dip, and a row is a dip taller than its boxes.
    static constexpr float  s_kMediumBoxDip        = 74.0f;
    static constexpr float  s_kLargeBoxDip         = 107.33f;
    static constexpr float  s_kLargeBoxLowDpiDip   = 105.0f;   // Large's box below 150%
    static constexpr int    s_kXLargeBoxPadPx      = 9;
    static constexpr float  s_kXLargeBoxPadDip     = 6.0f;
    static constexpr float  s_kItemBoxPadDip       = 5.33f;

    //  An item's icon in pixels: the view's size, scaled, but never past the
    //  256 pixels of the shell's largest icon, as Explorer draws Extra large.
    static constexpr int  s_kMaxItemIconPx = 256;
    static int          GetItemIconPx    (DxuiListViewTypes::View view, UINT dpi);
    bool                GetItemRectPx    (int item, RECT & outRect) const;


    //  Explorer's Content, measured at 100, 125 and 150% over a range of
    //  widths: the icon 33.33 dip into the row and the name 8 past it. The
    //  second column, from the list's left, is 60% of the way to the
    //  scrollbar's column and 32.67 dip more, but never more than 303.33 dip
    //  short of that column nor past 685 dip. Rows are 52.67 dip apart, and
    //  the rule between them 4 dip in from the left and 6.67 short of the
    //  right.
    static constexpr float  s_kContentIconLeftDip    = 33.33f;
    static constexpr float  s_kContentNameGapDip     = 8.0f;
    static constexpr float  s_kContentRowDip         = 52.67f;
    static constexpr float  s_kContentRowLowDpiDip   = 48.0f;   // the row below 150%
    static constexpr float  s_kContentEndGapDip      = 2.0f;

    void                    BeginPageSlide     (const DxuiListViewTypes::PageSlide & running, int fromPx, int restBeforePx);
    int                     GetPageSlidePx     () const;
    bool                    UsesPageSlide      () const;
    int                     GetRestScrollPx    () const;
    int                     GetScrollViewPx    () const;
    DxuiListViewTypes::ItemGrid      GetItemGrid             () const;
    DxuiListViewTypes::ScrollLayout  ComputeItemScrollLayout () const;
    void          SpreadItemBoxes         (DxuiListViewTypes::ItemGrid & grid, int fullW, int fullH, int rows) const;
    int           HitTestItem             (int xPx, int yPx) const;
    void          EnsureItemVisible       (int item);
    int           GetItemIconLeftPx       () const;
    RECT          GetItemLabelRectPx      (const RECT & cell) const;
    POINT         GetItemScrollOffsetPx   () const;

    DxuiListViewTypes::ItemSlide     PlaceItemSlide          () const;
    void          CollectItemsToMeasure   (std::vector<std::pair<int, RECT>> & out) const;
    void          CollectLineItemsInSight (const DxuiListViewTypes::ItemLayout & layout, const DxuiListViewTypes::ItemLine & line, const DxuiListViewTypes::ItemSlide & slide, int pageY,
                                           std::vector<std::pair<int, RECT>> & out) const;
    bool          HasItemGroups           () const;
    bool          UsesItemLayout          () const;
    bool          IsFirstOfGroup          (int item) const;
    const DxuiListViewTypes::ItemLayout &  GetItemLayout     () const;
    DxuiListViewTypes::ItemLayout    BuildItemLayout         (int openGroup = -1) const;
    static int    FindItemLine            (const DxuiListViewTypes::ItemLayout & layout, int y);
    int           GetItemTopPx            (const DxuiListViewTypes::ItemLayout & layout) const;
    bool          GetGroupedItemRectPx    (const DxuiListViewTypes::ItemLayout & layout, int item, RECT & outRect) const;
    int           GetMaxItemTopLine       (const DxuiListViewTypes::ItemLayout & layout) const;
    void          EnsureItemLineVisible   (const DxuiListViewTypes::ItemLayout & layout, int line);
    static constexpr float  s_kMediumLabelWDip     = 60.67f;
    static constexpr float  s_kLargeLabelWDip      = 92.0f;
    static constexpr int    s_kXLargeLabelInsetPx  = 1;   // Extra large's names, as wide as its icon less this, at any scale
    static constexpr int    s_kLargeLabelInsetDip  = 18;

    struct ItemTextKey
    {
        int  view         = -1;
        int  cellW        = 0;
        int  rows         = 0;
        int  rowsVersion  = 0;
        int  fontCentiDip = 0;
        int  dpi          = 0;

        bool operator== (const ItemTextKey &) const = default;
    };

    void          SyncItemTextLines       () const;

    //  Small icons and List size their columns to the names, as Explorer's
    //  do: List each column to its own widest name, Small icons every column
    //  to the widest of all, up to a cap past which a name is cut short. The
    //  box around them Explorer's, measured at 150%: the icon well into the
    //  box, the name just past it, a little room after the name, and a gap
    //  between one box and the next.
    static constexpr float  s_kNameIconLeftDip  = 32.67f;
    static constexpr float  s_kNameRightPadDip  = 5.33f;
    static constexpr float  s_kSmallIconsGapDip = 1.0f;   // between Small icons' boxes once a name reaches the cap

    //  Explorer's tiles, measured at 100, 125 and 150%: a box 250 dip wide on
    //  the pitch, the icon 4 dip into it and the text 5.33 past the icon. The
    //  text is the name on one or two lines, then the details while the three
    //  lines last, every line one pixel more than the captions' pitch. A row
    //  is its icon or its text, whichever is taller, and 14 dip less 6 pixels
    //  more, which no single unit gives at all three scales; the box is 6 dip
    //  short of the row.
    static constexpr float  s_kTileGapDip        = 4.0f;
    static constexpr float  s_kTileIconLeftDip   = 4.0f;
    static constexpr float  s_kTileTextGapDip    = 5.33f;
    static constexpr float  s_kTilePadDip        = 14.0f;
    static constexpr int    s_kTilePadTrimPx     = 6;
    int           GetSmallRowPx           () const;
    int           GetItemLinesHPx         (const DxuiListViewTypes::ItemGrid & grid, int rows) const;
    static int    GetFirstItemLine        (const DxuiListViewTypes::ItemLayout & layout);
    int           GetTileLinePx           () const;
    int           GetTileRowPx            (int lines) const;
    static constexpr int  s_kSmallIconsMaxWDip = 309;

    void          MeasureItemNames        (IDxuiTextRenderer & text) const;
    int           GetNameCellWPx          (int textPx) const;
    bool          MeasureItemTextLines    (IDxuiTextRenderer & text, const std::vector<std::pair<int, RECT>> & onScreen) const;
    int           GetItemExtraPx          (int item) const;
    int           GetCaptionLinePx        () const;

    DxuiListViewTypes::LayoutKey     GetLayoutKey            () const;

    static inline const DxuiListViewTypes::ItemLayout  s_kNoItemLayout {};


    //  The line an item view's scroll is held at, let go when the view scrolls
    //  any other way; and the page sliding into view, which IsPageSliding ends
    //  once its time is up.
    void                                 ReleaseAnchor () { m_itemAnchorLine = -1; }
    void                                 EndPageSlide  () { m_pageSlide = {}; }
    const DxuiListViewTypes::PageSlide & GetPageSlide  () const { return m_pageSlide; }
    bool                                 IsPageSliding ();

private:
    friend class DxuiListView;
    friend class DxuiListFlowLayout;
    friend class DxuiListItemKeys;
    friend class DxuiListItemPainter;

    DxuiListView  & m_list;

    bool                                   m_pageSlideEnabled = false;
    mutable DxuiListViewTypes::ItemLayout  m_itemLayout;   // see LayoutKey
    mutable DxuiListViewTypes::LayoutKey   m_itemLayoutKey;
    mutable bool                           m_itemLayoutBuilt  = false;
    mutable std::vector<uint8_t>           m_itemTextLines;   // each item's name lines; 0 until measured
    mutable ItemTextKey                    m_itemTextKey;   // what the measurements were taken for
    mutable std::array<int, 5>             m_linesHKey        = { -1, -1, -1, -1, -1 };   // perLine, cellH, rows, rows version, text version
    mutable int                            m_linesHPx         = 0;
    DxuiListViewTypes::RowNameSource       m_rowNames;
    mutable int                            m_itemNameWidestPx = 0;

    int                                    m_itemAnchorLine   = -1;      // an item view's scroll held at this line's edge; -1 for the top row's
    bool                                   m_itemAnchorBottom = false;   // its bottom edge rather than its top
    mutable int                            m_itemTextVersion  = 0;
    mutable int                            m_captionLinePx    = 0;       // GDI's line height for captions; 0 until a paint measures it
    mutable std::vector<int>               m_itemNameWPx;                // each item's name width; empty until measured
    mutable ItemTextKey                    m_itemNameKey;                // what the widths were measured for
    DxuiListViewTypes::PageSlide           m_pageSlide;
};
