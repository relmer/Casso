#pragma once

#include "Pch.h"

#include "Widgets/ListView/DxuiListViewTypes.h"

class DxuiListView;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListColumns
//
//  The details view's columns: which are visible, their widths as set,
//  measured or fitted to their content, where each starts, and the rect
//  of a cell's text.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListColumns
{
public:
    explicit DxuiListColumns (DxuiListView & list);

    void  SetColumnVisible          (size_t idx, bool visible);
    void  SetColumnOverrideWidthPx  (size_t idx, int px);

    void  FitColumnToContent        (size_t idx);

    void  FitAllColumnsToContent    ();
    int   GetColumnOverrideWidthPx  (size_t idx) const;
    int   GetColumnEffectiveWidthPx (size_t idx) const;
    int   GetTotalMeasuredWidthPx   () const;
    void  MeasureColumnsPx          (IDxuiTextRenderer & text) const;

    void  UpdateAutoFitFromRows     ();
    void  ResetAutoFit              ();
    int   GetVisibleColumnCount    () const;
    int   GetNthVisibleColumnIndex (int n) const;
    int   GetVisibleIndexOfColumn  (size_t absCol) const;
    int   GetContentWidthPx            () const;

    bool  GetCellTextRectPx   (int row, size_t column, RECT & outRect) const;
    static constexpr float  s_kExplorerIconPadDip    = 4.67f;    // the name's icon, into its column
    static constexpr float  s_kExplorerCellPadDip    = 6.67f;    // every other column's text
    static constexpr float  s_kExplorerNameTitleDip  = 16.67f;   // the name column's title
    int   GetCellPadLeftPx    (size_t column) const;
    int   GetHeaderTitlePadPx (size_t column) const;
    static constexpr int    s_kCellPadLeftDip        = 12;
    static constexpr int    s_kCellIconGapDip        = 6;

    // Per-character width estimate for content auto-fit, as a fraction
    // of the font em. Segoe UI averages well under this for the digit /
    // punctuation / short-label columns the debug panels auto-fit;
    // erring high trades a little extra column width for a guarantee
    // that values never clip.
    static constexpr float  s_kAutoCharWidthEm   = 0.62f;

    // Grow the monotonic auto-fit glyph counts from one row's cells (the
    // per-row half of UpdateAutoFitFromRows, used for the visible window in
    // virtual mode where m_list.m_rowData is empty).
    void         NoteAutoFitRow      (const std::vector<DxuiListViewTypes::Cell> & cells) const;
    int          GetColumnNaturalWidthPx (size_t c) const;

    //  What a column's content wants, ignoring any override already on it, so
    //  fitting a column that has been dragged still measures the content.
    int          GetColumnContentWidthPx (IDxuiTextRenderer & text, size_t c) const;

    //  Applies a width a fit asked for, once the paint pass has measured it.
    void         ApplyPendingFit         (IDxuiTextRenderer & text);
    void    ComputeColumnLayout (float fullW, std::vector<int> & xs, std::vector<int> & ws) const;

private:
    friend class DxuiListView;

    DxuiListView  & m_list;

    mutable std::vector<int>                           m_measuredWPx;
    std::vector<int>                                   m_overrideWPx;

    // Monotonic max glyph count per auto column (header + widest cell);
    // the cheap fallback used when no DWrite measurement exists (e.g. the
    // debug panels). ComputeColumnLayout turns it into a pixel width at the
    // current DPI; persists across SetRows.
    // Mutable: grown from the visible window during const Paint in virtual
    // mode (mirrors m_measuredWPx, which is likewise refreshed from Paint).
    mutable std::vector<int>  m_autoMaxChars;
    bool                      m_preciseAutoFit = false;
    mutable bool              m_measureDirty   = false;
    std::vector<size_t>       m_pendingFits;
};
