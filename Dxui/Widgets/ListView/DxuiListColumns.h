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

    //  As FitColumnToContent, measuring only the rows on screen.
    void  FitColumnToVisibleContent (size_t idx);

    //  As FitColumnToVisibleContent, but only ever wider: a column narrower
    //  than the rows on screen need widens, and a wider one is not changed.
    //  GrowColumnToContent measures every row.
    void  GrowColumnToVisibleContent (size_t idx);
    void  GrowColumnToContent        (size_t idx);

    //  As FitColumnToContent, with no report of the width to the host, as it
    //  isn't a width the user chose.
    void  AutoFitColumnToContent     (size_t idx);

    void  FitAllColumnsToContent    ();
    int   GetColumnOverrideWidthPx  (size_t idx) const;
    int   GetColumnEffectiveWidthPx (size_t idx) const;
    int   GetTotalMeasuredWidthPx   () const;
    void  MeasureColumnsPx          (IDxuiTextRenderer & text) const;

    void  UpdateAutoFitFromRows    ();
    void  ResetAutoFit             ();
    int   GetVisibleColumnCount    () const;
    int   GetNthVisibleColumnIndex (int n) const;
    int   GetVisibleIndexOfColumn  (size_t absCol) const;
    int   GetContentWidthPx        () const;

    bool  GetCellTextRectPx   (int row, size_t column, RECT & outRect) const;
    static constexpr float  s_kExplorerIconPadDip       = 4.67f;    // the name's icon, into its column
    static constexpr float  s_kExplorerCellPadDip       = 6.67f;    // every other column's text
    static constexpr float  s_kExplorerNameTitleDip     = 16.67f;   // the name column's title
    static constexpr float  s_kExplorerSelectAllRoomDip = 17.33f;   // how far the header's select-all box moves that title
    int   GetCellPadLeftPx    (size_t column) const;
    int   GetCellPadRightPx   () const;
    int   GetHeaderTitlePadPx (size_t column) const;
    static constexpr int    s_kCellPadLeftDip        = 12;
    static constexpr int    s_kCellIconGapDip        = 6;
    static constexpr int    s_kCellSwatchDip         = 10;
    static constexpr int    s_kHeaderPadMinDip       = 16;   // a title's room either side, however dense the cells

    //  The room ahead of a cell's text, and that with the room after it.
    int   GetCellLeadPx       (const DxuiListViewTypes::Cell & cell, size_t column) const;
    int   GetCellExtraPx      (const DxuiListViewTypes::Cell & cell, size_t column) const;

    //  The space before the first column of a list that fills a pane.
    int   GetPaneLeadPx       () const;

    //  The width the content auto-fit sets for a cell of `chars` characters.
    int   GetFittedWidthPx    (size_t chars) const;

    //  Text a fitted column must have room for, measured at the next paint;
    //  RequeueFitTexts measures every noted string again.
    void  NoteFitText         (size_t idx, const std::wstring & text);
    void  RequeueFitTexts     ();

    // Per-character width estimate for content auto-fit, as a fraction
    // of the font em. Segoe UI averages well under this for the digit /
    // punctuation / short-label columns the debug panels auto-fit;
    // erring high trades a little extra column width for a guarantee
    // that values never clip.
    static constexpr float  s_kAutoCharWidthEm   = 0.62f;

    // Grow the monotonic auto-fit glyph counts from one row's cells (the
    // per-row half of UpdateAutoFitFromRows, used for the visible window in
    // virtual mode where m_list.m_rowData is empty).
    void  NoteAutoFitRow          (const std::vector<DxuiListViewTypes::Cell> & cells) const;
    int   GetColumnNaturalWidthPx (size_t c) const;

    //  What a column's content wants, ignoring any override already on it, so
    //  fitting a column that has been dragged still measures the content.
    //  The rows measured are [firstRow, endRow).
    int          GetColumnContentWidthPx (IDxuiTextRenderer & text, size_t c, int firstRow = 0, int endRow = INT_MAX) const;

    //  Applies a width a fit asked for, once the paint pass has measured it.
    void  ApplyPendingFit     (IDxuiTextRenderer & text);

    //  With grow-to-visible-rows on, every fitted column is grown to the rows
    //  on screen at a paint whose rows or scroll position changed: a column
    //  without a set width, not stretched, and not sized by the user.
    //  The grown width only raises the fitted width (m_grownWPx); it is not
    //  an override, which stays the user's.
    void  QueueGrowsToVisibleRows ();
    void  ApplyAutoGrows          (IDxuiTextRenderer & text);
    void  ComputeColumnLayout     (float fullW, std::vector<int> & xs, std::vector<int> & ws) const;

private:
    //  The room past a heading's title.
    int   GetTitleExtraPx     (size_t column, int padPx) const;

    //  Whether a cell can be left unmeasured: see the definition.
    bool  TrySkipMonoMeasure  (size_t column, const DxuiListViewTypes::Cell & cell) const;

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
    std::vector<size_t>       m_pendingAutoFits;
    std::vector<size_t>       m_pendingVisibleFits;
    std::vector<size_t>       m_pendingVisibleGrows;
    std::vector<size_t>       m_pendingGrows;
    std::vector<size_t>       m_pendingAutoGrows;   // QueueGrowsToVisibleRows' columns, measured at the next paint
    std::vector<int>          m_grownWPx;           // each column's widest visible rows, as measured

    //  For each column, the longest printable-ASCII text in the monospaced
    //  face measured at each lead, at the font size in pixels it was
    //  measured at. See TrySkipMonoMeasure.
    mutable std::vector<std::map<int, size_t>>  m_monoMeasuredChars;
    mutable float                               m_monoMeasuredFontPx = 0.0f;

    //  NoteFitText's strings: those noted for each column, and those not yet
    //  measured.
    std::vector<std::unordered_set<std::wstring>>         m_notedFitTexts;
    mutable std::vector<std::pair<size_t, std::wstring>>  m_pendingFitTexts;
};
