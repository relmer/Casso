#pragma once

#include "Pch.h"

#include "Ui/Debugger/Panes/EventLogRows.h"
#include "Ui/Debugger/Panes/EventLogText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogPane
//
//  What the Disk II event log and the input log share: the rows from the
//  machine's deltas, filtered, in position order or sorted by a column, in a
//  virtual list. In position order a row is above the oldest: where history
//  begins, or a note that older rows are being rebuilt, or that scrolling
//  up rebuilds them; when that row comes into view the pane requests the
//  rows before its oldest. While the machine is behind live the rows from
//  its position on are hidden. The selection, its anchor and the keyboard's
//  row, and the row at the top when older rows go in front, stay the same
//  through each change.
//
//  The pane has at most kMaxRows rows: past that the oldest are dropped down
//  to kRetainedRows, and the machine's thread is sent where the rows start now,
//  so scrolling up rebuilds them from history. A delta that only adds newer
//  rows extends the view with them, so a frame's work is the size of the
//  delta, not of the log.
//
//  A pane for one kind of event defines its columns, its filter and its
//  event cells, and may look over every row before the filter runs. Its
//  first column is the time and its second the cycle; its label and text
//  columns have the rows that are not events.
//
//  UI thread only.
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
class EventLogPane
{
public:
    using Row       = DebugLogRow<T>;
    using Cells     = std::vector<DxuiListView::Cell>;
    using RequestFn = std::function<void (const std::string & request)>;

    //  A column's heading, alignment, starting width (0 to take the room
    //  left, kFit to fit the text NoteFitTexts notes for it) and whether it
    //  is visible by default.
    struct ColumnSpec
    {
        const wchar_t   * title    = L"";
        DxuiTextHAlign    align    = DxuiTextHAlign::Left;
        int               widthDip = 0;
        bool              isShown  = true;
    };

    static constexpr int     kTopRow       = -1;
    static constexpr int     kFit          = -1;
    static constexpr size_t  kTimeColumn   = 0;
    static constexpr size_t  kCycleColumn  = 1;
    static constexpr size_t  kMaxRows      = 200000;
    static constexpr size_t  kRetainedRows = 150000;

    EventLogPane (DxuiListView * list, RequestFn request) :
        m_list    (list),
        m_request (std::move (request))
    {
    }

    virtual ~EventLogPane() = default;

    EventLogPane             (const EventLogPane &) = delete;
    EventLogPane & operator= (const EventLogPane &) = delete;

    DxuiListView *  GetList () const { return m_list; }

    //  The list's columns, its rows from this pane, a header press that
    //  sorts, and the text the fitted columns must have room for: the rows
    //  come from a provider, so the list never measures them.
    void  Configure()
    {
        std::vector<DxuiListView::Column>  columns;



        for (const ColumnSpec & spec : GetColumnSpecs())
        {
            columns.push_back ({ spec.title, (std::max) (spec.widthDip, 0), spec.widthDip == 0, spec.align, spec.isShown });
        }

        m_list->SetColumns           (std::move (columns));
        m_list->SetRowProvider       (0, [this] (int row, Cells & out) { ProvideRow (row, out); });
        m_list->EnableStickyTail     (true);
        m_list->SetPreciseAutoFit    (true);
        m_list->SetMultiSelect       (true);
        m_list->SetKeyboardColumnNav (true);
        m_list->SetOnSortColumn      ([this] (int column) { SortBy (column); });

        for (const wchar_t * label : { EventLogText::GetGapLabel(), EventLogText::GetUnavailableLabel(),
                                       EventLogText::GetResetLabel(), EventLogText::GetPowerCycleLabel() })
        {
            m_list->NoteFitText (GetLabelColumn(), label);
        }

        for (const std::wstring & time : EventLogText::GetTimeFitTexts())
        {
            m_list->NoteFitText (kTimeColumn, time);
        }

        NoteFitTexts();
    }

    //  The pane made visible or hidden: the machine's thread sends its deltas
    //  only while it is visible.
    void  SetShown (bool isShown)
    {
        if (isShown == m_isShown)
        {
            return;
        }

        m_isShown     = isShown;
        m_requestedAt = UINT64_MAX;

        m_request (isShown ? "open" : "close");
    }

    bool  IsShown () const { return m_isShown; }

    void  Apply (const DebugLogDelta<T> & delta)
    {
        std::optional<size_t>  top       = GetTopIndex();
        bool                   isAtTail  = m_list->IsAtBottom();
        bool                   isCut     = false;
        size_t                 prepended = 0;
        size_t                 dropped   = 0;
        size_t                 count     = 0;
        auto                   moveOn    = [&] (std::optional<size_t> & index)
        {
            index = (index.has_value() && *index + prepended >= dropped && *index + prepended - dropped < count)
                  ? std::optional<size_t> (*index + prepended - dropped) : std::nullopt;
        };



        TakeSelection();

        //  Each live keyframe sends a cut at the machine's position, where no
        //  row is yet; only a cut that drops rows changes the view's rows.
        isCut     = delta.cutFrom != DebugLogDelta<T>::kNoCut && !m_rows.GetRows().empty() && m_rows.GetRows().back().position >= delta.cutFrom;
        prepended = m_rows.Apply (delta);
        dropped   = TrimRows();
        count     = m_rows.GetRows().size();

        if (delta.isReset || isCut || prepended > 0 || dropped > 0)
        {
            m_isFullBuildDue = true;
        }

        if (delta.isReset)
        {
            m_selected.clear();
            m_anchor.reset();
            m_keyboardRow.reset();
            top.reset();
        }

        std::erase_if (m_selected, [&] (size_t index) { return index + prepended < dropped || index + prepended - dropped >= count; });

        for (size_t & index : m_selected)
        {
            index = index + prepended - dropped;
        }

        moveOn (m_anchor);
        moveOn (m_keyboardRow);

        m_keepTop.reset();

        if (top.has_value() && (prepended > 0 || (dropped > 0 && !isAtTail)))
        {
            moveOn (top);
            m_keepTop = top;
        }

        m_isDirty = true;
    }

    //  The machine's position while it is behind live, or UINT64_MAX.
    void  SetShownBefore (uint64_t position)
    {
        if (position != m_shownBefore)
        {
            m_shownBefore    = position;
            m_isDirty        = true;
            m_isFullBuildDue = true;
        }
    }

    //  A filter or the sort changed.
    void  MarkDirty () { m_isDirty = true; m_isFullBuildDue = true; }

    //  The list's rows again, when anything changed since the last call,
    //  with the selection on the same rows.
    void  Refresh()
    {
        std::vector<int>  rows;
        int               anchor      = -1;
        int               keyboardRow = -1;
        size_t            i           = 0;



        if (!m_isDirty)
        {
            return;
        }

        m_isDirty = false;

        //  Only newer rows came, at the end of the list in position order:
        //  the view's rows stay where they were, and so does the selection.
        if (!m_isFullBuildDue && m_sortColumn < 0 && m_shownBefore == UINT64_MAX && ExtendView())
        {
            m_list->SetVirtualRowCount ((int) m_view.size());
            m_isSelectionTaken = false;
            return;
        }

        m_isFullBuildDue = false;

        TakeSelection();
        BuildView();
        m_list->SetVirtualRowCount ((int) m_view.size());

        for (i = 0; i < m_view.size(); i++)
        {
            if (m_view[i] == kTopRow)
            {
                continue;
            }

            if (std::ranges::binary_search (m_selected, (size_t) m_view[i]))
            {
                rows.push_back ((int) i);
            }

            anchor      = (m_anchor      == (size_t) m_view[i]) ? (int) i : anchor;
            keyboardRow = (m_keyboardRow == (size_t) m_view[i]) ? (int) i : keyboardRow;
        }

        m_list->RestoreSelectedRows (std::move (rows), anchor, keyboardRow);
        m_isSelectionTaken = false;

        if (m_keepTop.has_value())
        {
            for (i = 0; i < m_view.size(); i++)
            {
                if (m_view[i] != kTopRow && (size_t) m_view[i] >= *m_keepTop)
                {
                    m_list->SetTopRow ((int) i);
                    break;
                }
            }

            m_keepTop.reset();
        }
    }

    //  In position order, the row above the oldest coming into view requests
    //  the rows before it, once for each oldest row.
    void  FollowScroll()
    {
        bool  isTopShown = m_sortColumn < 0 && m_list->IsVisible() && m_list->GetTopRow() == 0;



        if (isTopShown && !m_rows.IsAtStart() && !m_rows.IsRebuilding() && m_rows.GetHeldFrom() != m_requestedAt)
        {
            m_requestedAt = m_rows.GetHeldFrom();
            m_request ("earlier");
        }
    }

    //  A header press: sorts by the column, then the other way, then back to
    //  position order.
    void  SortBy (int column)
    {
        if (column != m_sortColumn)
        {
            m_sortColumn       = column;
            m_isSortDescending = false;
        }
        else if (!m_isSortDescending)
        {
            m_isSortDescending = true;
        }
        else
        {
            m_sortColumn = -1;
        }

        m_list->SetSortIndicator (m_sortColumn, m_isSortDescending);
        m_isDirty        = true;
        m_isFullBuildDue = true;
    }

    int   GetSortColumn     () const { return m_sortColumn; }
    bool  IsSortDescending  () const { return m_isSortDescending; }

    void  SetColumnShown (size_t column, bool isShown) { m_list->SetColumnVisible (column, isShown); }
    bool  IsColumnShown  (size_t column) const         { return m_list->IsColumnVisible (column); }

    //  The visible columns of the selected rows, or of every row listed, a tab
    //  between cells and a line for each row, with no heading.
    std::wstring  GetText (bool isSelectedOnly) const
    {
        std::wstring  text;
        Cells         cells;
        size_t        i     = 0;
        size_t        c     = 0;
        bool          first = true;



        for (i = 0; i < m_view.size(); i++)
        {
            if (m_view[i] == kTopRow || (isSelectedOnly && !m_list->IsRowSelected ((int) i)))
            {
                continue;
            }

            ProvideRow ((int) i, cells);
            first = true;

            for (c = 0; c < cells.size(); c++)
            {
                if (!m_list->IsColumnVisible (c))
                {
                    continue;
                }

                text += first ? L"" : L"\t";
                text += cells[c].text;
                first = false;
            }

            text += L"\r\n";
        }

        return text;
    }

    //  Which columns are visible, as 1 or 0 each, and the widths the user
    //  dragged, in pixels, 0 for none: "11110 0,0,140,0,0".
    std::string  GetColumnsText() const
    {
        std::string  shown;
        std::string  widths;
        size_t       c      = 0;



        for (c = 0; c < m_list->GetColumnCount(); c++)
        {
            shown  += m_list->IsColumnVisible (c) ? '1' : '0';
            widths += std::format ("{}{}", (c > 0) ? "," : "", m_list->GetColumnOverrideWidthPx (c));
        }

        return shown + " " + widths;
    }

    void  SetColumnsText (const std::string & text)
    {
        size_t       space  = text.find (' ');
        std::string  shown  = text.substr (0, space);
        std::string  widths = (space == std::string::npos) ? std::string() : text.substr (space + 1);
        size_t       c      = 0;
        size_t       start  = 0;
        size_t       comma  = 0;
        int          width  = 0;



        //  A choice saved with another set of columns does not fit this one.
        if (shown.size() != m_list->GetColumnCount())
        {
            return;
        }

        for (c = 0; c < shown.size(); c++)
        {
            m_list->SetColumnVisible (c, shown[c] == '1');
        }

        for (c = 0; c < m_list->GetColumnCount() && start < widths.size(); c++)
        {
            comma = widths.find (',', start);
            width = std::atoi (widths.substr (start, comma - start).c_str());

            if (width > 0)
            {
                m_list->SetColumnOverrideWidthPx (c, width);
            }

            start = (comma == std::string::npos) ? widths.size() : comma + 1;
        }
    }

    //  The bytes the row in the list's order has, as a sector's; null for
    //  none.
    std::shared_ptr<const std::vector<Byte>>  GetRowData (int viewRow) const
    {
        if (viewRow < 0 || (size_t) viewRow >= m_view.size() || m_view[(size_t) viewRow] == kTopRow)
        {
            return nullptr;
        }

        return m_rows.GetRows()[(size_t) m_view[(size_t) viewRow]].data;
    }

    //  The rows behind the list, in the list's order: indices into the
    //  pane's rows, kTopRow for the row above the oldest.
    const std::vector<int>         &  GetView () const { return m_view; }
    const EventLogRows<T>          &  GetRows () const { return m_rows; }

    void  ProvideRow (int viewRow, Cells & out) const
    {
        const ColumnSpecs  & specs = GetSpecs();
        const Row          * row   = nullptr;
        int                  index = 0;



        out.assign (specs.size(), DxuiListView::Cell());

        if (viewRow < 0 || (size_t) viewRow >= m_view.size())
        {
            return;
        }

        index = m_view[(size_t) viewRow];

        if (index == kTopRow)
        {
            out[GetTextColumn()].text = m_rows.IsRebuilding() ? EventLogText::GetRebuildingText()
                                      : m_rows.IsAtStart()    ? EventLogText::GetStartText()
                                                              : EventLogText::GetEarlierText();
            out[GetTextColumn()].dim  = true;
            return;
        }

        row = &m_rows.GetRows()[(size_t) index];

        out[kTimeColumn].text  = EventLogText::FormatTime (row->wallTime);
        out[kCycleColumn].text = (row->kind == DebugLogRowKind::Event || row->cycle != 0) ? EventLogText::FormatCycle (row->cycle) : std::wstring();

        switch (row->kind)
        {
            case DebugLogRowKind::Event:
                MakeCells (*row, out);
                break;

            case DebugLogRowKind::Gap:
                SetMarkCells (out, EventLogText::GetGapLabel(), EventLogText::GetGapText());
                break;

            case DebugLogRowKind::Unavailable:
                SetMarkCells (out, EventLogText::GetUnavailableLabel(), EventLogText::GetUnavailableText());
                break;

            case DebugLogRowKind::Reset:
                SetMarkCells (out, EventLogText::GetResetLabel(), L"");
                break;

            case DebugLogRowKind::PowerCycle:
                SetMarkCells (out, EventLogText::GetPowerCycleLabel(), L"");
                break;
        }
    }

protected:
    using ColumnSpecs = std::vector<ColumnSpec>;

    virtual ColumnSpecs  GetColumnSpecs () const = 0;

    //  Notes every text a fitted column can have, when that is a fixed set,
    //  with the list's NoteFitText.
    virtual void         NoteFitTexts   () {}

    //  Looks over the rows from `from` on before the filter runs on them,
    //  the rows before it looked over already. GetSettledCount is the count of
    //  the first `count` rows whose filtering no newer row can change.
    virtual void         PrepareView     (const std::vector<Row> & rows, size_t from) { (void) rows; (void) from; }
    virtual size_t       GetSettledCount (const std::vector<Row> & rows, size_t count) const { (void) rows; return count; }
    virtual bool         Matches        (const Row & row, size_t index) const = 0;
    virtual void         MakeCells      (const Row & row, Cells & out) const = 0;
    virtual size_t       GetLabelColumn () const = 0;
    virtual size_t       GetTextColumn  () const = 0;

private:
    const ColumnSpecs &  GetSpecs() const
    {
        if (m_specs.empty())
        {
            m_specs = GetColumnSpecs();
        }

        return m_specs;
    }

    void  SetMarkCells (Cells & out, const wchar_t * label, const wchar_t * text) const
    {
        out[GetLabelColumn()].text = label;
        out[GetTextColumn()].text  = text;
        out[GetLabelColumn()].dim  = true;
        out[GetTextColumn()].dim   = true;
    }

    //  The rows that pass the filter, before the machine's position, in
    //  position order with the row above the oldest first, or sorted.
    void  BuildView()
    {
        const std::vector<Row>  & rows = m_rows.GetRows();
        size_t                    i    = 0;



        m_view.clear();

        if (m_sortColumn < 0)
        {
            m_view.push_back (kTopRow);
        }

        PrepareView (rows, 0);

        for (i = 0; i < rows.size() && rows[i].position < m_shownBefore; i++)
        {
            if (rows[i].kind != DebugLogRowKind::Event || Matches (rows[i], i))
            {
                m_view.push_back ((int) i);
            }
        }

        m_viewRowCount = rows.size();

        if (m_sortColumn >= 0)
        {
            SortView();
        }
    }

    //  The view extended with the rows added since it was built, in position
    //  order. The rows whose filtering the new rows can change are filtered
    //  again; false when that moves a row already in the view, and a full
    //  build follows.
    bool  ExtendView()
    {
        const std::vector<Row>  & rows    = m_rows.GetRows();
        size_t                    from    = GetSettledCount (rows, std::min (m_viewRowCount, rows.size()));
        size_t                    i       = 0;
        size_t                    kept    = 0;
        std::vector<int>          removed;



        while (!m_view.empty() && m_view.back() != kTopRow && (size_t) m_view.back() >= from)
        {
            removed.insert (removed.begin(), m_view.back());
            m_view.pop_back();
        }

        kept = m_view.size();

        PrepareView (rows, from);

        for (i = from; i < rows.size(); i++)
        {
            if (rows[i].kind != DebugLogRowKind::Event || Matches (rows[i], i))
            {
                m_view.push_back ((int) i);
            }
        }

        m_viewRowCount = rows.size();

        return m_view.size() >= kept + removed.size() && std::equal (removed.begin(), removed.end(), m_view.begin() + (ptrdiff_t) kept);
    }

    //  Past kMaxRows, the oldest rows dropped down to kRetainedRows, and a
    //  request to the machine's thread with where the rows start now. None
    //  while older rows are being rebuilt. The count of rows dropped.
    size_t  TrimRows()
    {
        size_t  dropped = 0;



        if (m_rows.GetRows().size() <= kMaxRows || m_rows.IsRebuilding())
        {
            return 0;
        }

        dropped       = m_rows.TrimFront (kRetainedRows);
        m_requestedAt = UINT64_MAX;

        m_request (std::format ("from {}", m_rows.GetHeldFrom()));

        return dropped;
    }

    //  The time and cycle columns sort by number, the others by their text;
    //  rows that tie stay in position order.
    void  SortView()
    {
        const std::vector<Row>     & rows    = m_rows.GetRows();
        size_t                       column  = (size_t) m_sortColumn;
        bool                         isTime  = column == kTimeColumn;
        bool                         isCycle = column == kCycleColumn;
        std::vector<std::wstring>    keys;
        std::vector<size_t>          order;
        Cells                        cells;
        std::vector<int>             sorted;
        size_t                       i       = 0;



        if (!isCycle && !isTime)
        {
            keys.resize (m_view.size());

            for (i = 0; i < m_view.size(); i++)
            {
                ProvideRow ((int) i, cells);
                keys[i] = (column < cells.size()) ? cells[column].text : std::wstring();
            }
        }

        for (i = 0; i < m_view.size(); i++)
        {
            order.push_back (i);
        }

        std::ranges::stable_sort (order, [&] (size_t a, size_t b)
        {
            const Row  & left  = rows[(size_t) m_view[a]];
            const Row  & right = rows[(size_t) m_view[b]];
            bool         less  = false;

            if (isCycle)
            {
                less = m_isSortDescending ? right.cycle < left.cycle : left.cycle < right.cycle;
            }
            else if (isTime)
            {
                less = m_isSortDescending ? right.wallTime < left.wallTime : left.wallTime < right.wallTime;
            }
            else
            {
                less = m_isSortDescending ? keys[b] < keys[a] : keys[a] < keys[b];
            }

            return less;
        });

        for (size_t each : order)
        {
            sorted.push_back (m_view[each]);
        }

        m_view = std::move (sorted);
    }

    //  The rows behind the list's selection, its anchor and its keyboard
    //  row, read through the view they were made in, once between two
    //  refreshes: a delta applied after the first changes the rows the view
    //  indexes.
    void  TakeSelection()
    {
        if (m_isSelectionTaken)
        {
            return;
        }

        m_selected.clear();

        for (int row : m_list->GetSelectedRows())
        {
            if (std::optional<size_t> index = GetIndexOf (row); index.has_value())
            {
                m_selected.push_back (*index);
            }
        }

        std::ranges::sort (m_selected);

        m_anchor           = GetIndexOf (m_list->GetAnchorRow());
        m_keyboardRow      = GetIndexOf (m_list->GetSelectedRow());
        m_isSelectionTaken = true;
    }

    std::optional<size_t>  GetIndexOf (int viewRow) const
    {
        if (viewRow < 0 || (size_t) viewRow >= m_view.size() || m_view[(size_t) viewRow] == kTopRow)
        {
            return std::nullopt;
        }

        return (size_t) m_view[(size_t) viewRow];
    }

    std::optional<size_t>  GetTopIndex() const
    {
        int  top = m_list->GetTopRow();



        for (; top >= 0 && (size_t) top < m_view.size(); top++)
        {
            if (m_view[(size_t) top] != kTopRow)
            {
                return (size_t) m_view[(size_t) top];
            }
        }

        return std::nullopt;
    }

    DxuiListView           * m_list;
    RequestFn                m_request;
    EventLogRows<T>          m_rows;
    std::vector<int>         m_view;
    mutable ColumnSpecs      m_specs;
    std::vector<size_t>      m_selected;
    std::optional<size_t>    m_anchor;
    std::optional<size_t>    m_keyboardRow;
    std::optional<size_t>    m_keepTop;
    uint64_t                 m_shownBefore      = UINT64_MAX;
    size_t                   m_viewRowCount     = 0;        // the rows the view was built over
    uint64_t                 m_requestedAt      = UINT64_MAX;
    int                      m_sortColumn       = -1;
    bool                     m_isSortDescending = false;
    bool                     m_isShown          = false;
    bool                     m_isDirty          = true;
    bool                     m_isSelectionTaken = false;
    bool                     m_isFullBuildDue   = true;
};
