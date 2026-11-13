#pragma once

#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Ui/Debugger/Panes/EventLogRows.h"
#include "Ui/Debugger/Panes/EventLogText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IEventLogFilter
//
//  A log's column filters, as File Explorer's: a column of a few values is
//  filtered by the values checked, and a column of numbers by a comparison.
//  A number is typed as the column shows it, the time of day included.
//
////////////////////////////////////////////////////////////////////////////////

class IEventLogFilter
{
public:
    enum class Kind { None, Values, Compare };
    enum class Op   { Less, Greater, Equal, Between };

    struct Comparison
    {
        Op        op    = Op::Equal;
        uint64_t  first = 0;
        uint64_t  last  = 0;    // Between's upper bound
    };

    virtual ~IEventLogFilter() = default;

    virtual Kind                       GetFilterKind     (size_t column) const = 0;
    virtual std::vector<std::wstring>  GetFilterValues   (size_t column) const = 0;
    virtual bool                       IsValueChecked    (size_t column, const std::wstring & value) const = 0;
    virtual void                       ToggleValue       (size_t column, const std::wstring & value) = 0;
    virtual void                       SetComparison     (size_t column, const Comparison & comparison) = 0;
    virtual void                       ClearFilter       (size_t column) = 0;
    virtual bool                       IsFiltered        (size_t column) const = 0;
    virtual std::optional<uint64_t>    ParseFilterNumber (size_t column, const std::wstring & text) const = 0;

    //  The filters as an options field, `filters=...`, and read back from an
    //  options text; a text without the field leaves no filter.
    virtual std::string                GetFilterText     () const = 0;
    virtual void                       SetFilterText     (const std::string & options) = 0;
};





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
//  While the list is at its newest row the pane has at most kMaxRows rows:
//  past that the oldest are dropped down to kRetainedRows, and the machine's
//  thread is sent where the rows start now, so scrolling up rebuilds them from
//  history. While the list is on older rows, the rows scrolling up brought in
//  are not dropped as they arrive: nothing is dropped until the list is back
//  at its newest row or the rows pass kHardMaxRows. Nothing is dropped while a
//  request for older rows is outstanding either, since the machine's thread
//  sends the rows before where the rows started then. A delta that only adds
//  newer rows extends the view with them, so a frame's work is the size of
//  the delta, not of the log; in a sorted view each newer row is put in its
//  place by the sort column's text, which is made once for each row.
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
class EventLogPane : public IEventLogFilter
{
public:
    using Row       = DebugLogRow<T>;
    using Cells     = std::vector<DxuiListView::Cell>;
    using RequestFn = std::function<void (const DebugLogRequest & request)>;

    //  A column's heading, alignment, starting width (0 to take the room
    //  left, kFit to fit the text NoteFitTexts notes for it), whether it is
    //  visible by default, and its heading's tip.
    struct ColumnSpec
    {
        const wchar_t   * title    = L"";
        DxuiTextHAlign    align    = DxuiTextHAlign::Left;
        int               widthDip = 0;
        bool              isShown  = true;
        const wchar_t   * tip      = L"";
    };

    static constexpr int     kTopRow       = -1;
    static constexpr int     kFit          = -1;
    static constexpr size_t  kTimeColumn   = 0;
    static constexpr size_t  kCycleColumn  = 1;
    static constexpr size_t  kMaxRows      = 200000;
    static constexpr size_t  kRetainedRows = 150000;
    static constexpr size_t  kHardMaxRows  = 2 * kMaxRows;

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
            columns.push_back ({ spec.title, (std::max) (spec.widthDip, 0), spec.widthDip == 0, spec.align, spec.isShown, spec.tip });
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

        m_isShown          = isShown;
        m_requestedAt      = UINT64_MAX;
        m_isEarlierPending = false;

        m_request ({ isShown ? DebugLogRequest::Kind::Open : DebugLogRequest::Kind::Close });
    }

    bool  IsShown () const { return m_isShown; }

    void  Apply (const DebugLogDelta<T> & delta)
    {
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

        //  The machine's thread has the request for older rows once it
        //  rebuilds them or sends them.
        m_isEarlierPending = m_isEarlierPending && !delta.isRebuilding && !delta.isReset && prepended == 0;

        dropped   = TrimRows (isAtTail);
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
            m_viewTop.reset();
            m_isTopKept = false;
        }

        std::erase_if (m_selected, [&] (size_t index) { return index + prepended < dropped || index + prepended - dropped >= count; });

        for (size_t & index : m_selected)
        {
            index = index + prepended - dropped;
        }

        moveOn (m_anchor);
        moveOn (m_keyboardRow);

        //  The top row is moved on through every delta since the view was
        //  built, as the selection is, and Refresh scrolls back to it when any
        //  of them put rows in front or dropped rows the list was not following.
        moveOn (m_viewTop);

        m_isTopKept = m_isTopKept || prepended > 0 || (dropped > 0 && !isAtTail);
        m_keepTop   = m_isTopKept ? m_viewTop : std::nullopt;

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

    //  The time and the cycle compare; a pane's other columns as it says.
    Kind  GetFilterKind (size_t column) const override
    {
        return (column == kTimeColumn || column == kCycleColumn) ? Kind::Compare : Kind::None;
    }

    //  The values the column's events show, sorted, with any checked that no
    //  event shows now.
    std::vector<std::wstring>  GetFilterValues (size_t column) const override
    {
        std::set<std::wstring>   values;
        Cells                    cells;
        const std::vector<Row> & rows   = m_rows.GetRows();
        auto                     filter = m_valueFilters.find (column);



        for (size_t i = 0; i < rows.size(); i++)
        {
            if (rows[i].kind == DebugLogRowKind::Event)
            {
                MakeRowCells ((int) i, cells);

                if (column < cells.size() && !cells[column].text.empty())
                {
                    values.insert (cells[column].text);
                }
            }
        }

        if (filter != m_valueFilters.end())
        {
            values.insert (filter->second.begin(), filter->second.end());
        }

        return { values.begin(), values.end() };
    }

    bool  IsValueChecked (size_t column, const std::wstring & value) const override
    {
        auto  filter = m_valueFilters.find (column);



        return filter != m_valueFilters.end() && filter->second.contains (value);
    }

    //  Checking a value shows only the checked values' rows; unchecking the
    //  last ends the column's filter.
    void  ToggleValue (size_t column, const std::wstring & value) override
    {
        std::set<std::wstring>  & checked = m_valueFilters[column];



        if (!checked.erase (value))
        {
            checked.insert (value);
        }

        if (checked.empty())
        {
            m_valueFilters.erase (column);
        }

        MarkDirty();
    }

    void  SetComparison (size_t column, const Comparison & comparison) override
    {
        m_compareFilters[column] = comparison;
        MarkDirty();
    }

    void  ClearFilter (size_t column) override
    {
        m_valueFilters.erase (column);
        m_compareFilters.erase (column);
        MarkDirty();
    }

    bool  IsFiltered (size_t column) const override
    {
        return m_valueFilters.contains (column) || m_compareFilters.contains (column);
    }

    //  The time of day in milliseconds, from the hours, minutes, seconds and
    //  milliseconds as the column shows them and an AM or PM; any other
    //  column's number, with its commas, or in hex after a $.
    std::optional<uint64_t>  ParseFilterNumber (size_t column, const std::wstring & text) const override
    {
        return (column == kTimeColumn) ? ParseTimeOfDay (text) : ParseCount (text);
    }

    //  Each column's filter, comma separated: `column:v:value|value` for its
    //  values, `column:c:op:first:last` for its comparison. The characters
    //  the text separates on are escaped in a value as `%` and two hex digits.
    std::string  GetFilterText () const override
    {
        std::string  text;



        for (const auto & [column, checked] : m_valueFilters)
        {
            text += std::format ("{}{}:v:", text.empty() ? "" : ",", column);

            for (const std::wstring & value : checked)
            {
                text += (value == *checked.begin() ? std::string() : std::string ("|")) + EscapeFilterValue (TextEncoding::WideToUtf8 (value));
            }
        }

        for (const auto & [column, comparison] : m_compareFilters)
        {
            text += std::format ("{}{}:c:{}:{}:{}", text.empty() ? "" : ",", column, (int) comparison.op, comparison.first, comparison.last);
        }

        return "filters=" + text;
    }

    void  SetFilterText (const std::string & options) override
    {
        size_t       at     = options.find ("filters=");
        std::string  text   = (at == std::string::npos) ? std::string() : options.substr (at + 8, options.find (';', at) - (at + 8));
        size_t       start  = 0;



        m_valueFilters.clear();
        m_compareFilters.clear();

        while (start < text.size())
        {
            size_t                    end    = std::min (text.find (',', start), text.size());
            std::vector<std::string>  fields = SplitFilterText (text.substr (start, end - start), ':');

            if (fields.size() >= 3 && fields[1] == "v")
            {
                for (const std::string & value : SplitFilterText (fields[2], '|'))
                {
                    m_valueFilters[(size_t) std::strtoull (fields[0].c_str(), nullptr, 10)].insert (TextEncoding::Utf8ToWide (UnescapeFilterValue (value)));
                }
            }
            else if (fields.size() >= 5 && fields[1] == "c")
            {
                m_compareFilters[(size_t) std::strtoull (fields[0].c_str(), nullptr, 10)] =
                    { (Op) std::atoi (fields[2].c_str()), std::strtoull (fields[3].c_str(), nullptr, 10), std::strtoull (fields[4].c_str(), nullptr, 10) };
            }

            start = end + 1;
        }

        MarkDirty();
    }

    static std::string  EscapeFilterValue (const std::string & value)
    {
        std::string  escaped;



        for (char ch : value)
        {
            escaped += (ch != '\0' && std::strchr (",|:;=%", ch) != nullptr) ? std::format ("%{:02X}", (unsigned char) ch) : std::string (1, ch);
        }

        return escaped;
    }

    static std::string  UnescapeFilterValue (const std::string & value)
    {
        std::string  text;



        for (size_t i = 0; i < value.size(); i++)
        {
            if (value[i] == '%' && i + 2 < value.size())
            {
                text += (char) std::strtoul (value.substr (i + 1, 2).c_str(), nullptr, 16);
                i    += 2;
            }
            else
            {
                text += value[i];
            }
        }

        return text;
    }

    static std::vector<std::string>  SplitFilterText (const std::string & text, char separator)
    {
        std::vector<std::string>  parts;
        size_t                    start = 0;



        while (start <= text.size())
        {
            size_t  end = std::min (text.find (separator, start), text.size());

            parts.push_back (text.substr (start, end - start));
            start = end + 1;
        }

        return parts;
    }

    static std::optional<uint64_t>  ParseCount (const std::wstring & text)
    {
        std::wstring  digits;
        bool          isHex  = false;
        uint64_t      value  = 0;



        for (wchar_t ch : text)
        {
            if (ch == L'$' && digits.empty())
            {
                isHex = true;
            }
            else if (ch != L',' && ch != L' ')
            {
                digits += ch;
            }
        }

        if (digits.empty() || digits.find_first_not_of (isHex ? L"0123456789ABCDEFabcdef" : L"0123456789") != std::wstring::npos)
        {
            return std::nullopt;
        }

        for (wchar_t ch : digits)
        {
            value = value * (isHex ? 16 : 10) + (uint64_t) (iswdigit (ch) ? ch - L'0' : (towupper (ch) - L'A' + 10));
        }

        return value;
    }

    static std::optional<uint64_t>  ParseTimeOfDay (const std::wstring & text)
    {
        static constexpr uint64_t  kScales[] = { 3600000, 60000, 1000, 1 };
        std::vector<uint64_t>      parts     = { 0 };
        bool                       isDigit   = false;
        bool                       isPm      = text.find (L"PM") != std::wstring::npos || text.find (L"pm") != std::wstring::npos;
        bool                       isAm      = text.find (L"AM") != std::wstring::npos || text.find (L"am") != std::wstring::npos;
        uint64_t                   ms        = 0;



        for (wchar_t ch : text)
        {
            if (iswdigit (ch))
            {
                parts.back() = parts.back() * 10 + (uint64_t) (ch - L'0');
                isDigit      = true;
            }
            else if (isDigit && parts.size() < std::size (kScales))
            {
                parts.push_back (0);
                isDigit = false;
            }
        }

        if (!isDigit && parts.size() > 1)
        {
            parts.pop_back();
        }

        if (parts.size() < 2 || parts[0] > 23)
        {
            return std::nullopt;
        }

        parts[0] = (isPm && parts[0] < 12) ? parts[0] + 12 : (isAm && parts[0] == 12) ? 0 : parts[0];

        for (size_t i = 0; i < parts.size(); i++)
        {
            ms += parts[i] * kScales[i];
        }

        return ms;
    }

    //  The list's rows again, when anything changed since the last call,
    //  with the selection on the same rows.
    void  Refresh()
    {
        std::vector<int>  rows;
        int               anchor      = -1;
        int               keyboardRow = -1;
        size_t            i           = 0;
        bool              isExtended  = false;



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

        TakeSelection();

        //  Only newer rows came to a sorted view: each goes in its place, and
        //  the row at the top stays at the top.
        isExtended = !m_isFullBuildDue && m_sortColumn >= 0 && m_shownBefore == UINT64_MAX && ExtendSortedView();

        if (isExtended && !m_keepTop.has_value())
        {
            m_keepTop = m_viewTop;
        }

        if (!isExtended)
        {
            BuildView();
        }

        m_isFullBuildDue = false;
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
                if (m_view[i] != kTopRow && (m_sortColumn < 0 ? (size_t) m_view[i] >= *m_keepTop : (size_t) m_view[i] == *m_keepTop))
                {
                    m_list->SetTopRow ((int) i);
                    break;
                }
            }

            m_keepTop.reset();
        }

        m_isTopKept = false;
    }

    //  In position order, the row above the oldest coming into view requests
    //  the rows before it, once for each oldest row.
    void  FollowScroll()
    {
        bool  isTopShown = m_sortColumn < 0 && m_list->IsVisible() && m_list->GetTopRow() == 0;



        if (isTopShown && !m_rows.IsAtStart() && !m_rows.IsRebuilding() && m_rows.GetHeldFrom() != m_requestedAt)
        {
            m_requestedAt      = m_rows.GetHeldFrom();
            m_isEarlierPending = true;
            m_request ({ DebugLogRequest::Kind::Earlier });
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
    const std::vector<int>  & GetView       () const { return m_view; }
    const EventLogRows<T>   & GetRows       () const { return m_rows; }
    size_t                    GetBuildCount () const { return m_buildCount; }

    void  ProvideRow (int viewRow, Cells & out) const
    {
        if (viewRow < 0 || (size_t) viewRow >= m_view.size())
        {
            out.assign (GetSpecs().size(), DxuiListView::Cell());
            return;
        }

        MakeRowCells (m_view[(size_t) viewRow], out);
    }

    //  The cells of the row at `index` in the pane's rows, or of the row
    //  above the oldest for kTopRow.
    void  MakeRowCells (int index, Cells & out) const
    {
        const ColumnSpecs  & specs = GetSpecs();
        const Row          * row   = nullptr;



        out.assign (specs.size(), DxuiListView::Cell());

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
    virtual void    PrepareView     (const std::vector<Row> & rows, size_t from) { (void) rows; (void) from; }
    virtual size_t  GetSettledCount (const std::vector<Row> & rows, size_t count) const { (void) rows; return count; }
    virtual bool    Matches         (const Row & row, size_t index) const = 0;
    virtual void    MakeCells       (const Row & row, Cells & out) const = 0;
    virtual size_t  GetLabelColumn  () const = 0;
    virtual size_t  GetTextColumn   () const = 0;

    //  The number a row sorts by in a column of numbers, or none to sort it
    //  by its text: "1,300" does not come before "300".
    virtual std::optional<uint64_t>  GetSortNumber (const Row & row, size_t index, size_t column) const { (void) row; (void) index; (void) column; return std::nullopt; }

private:
    const ColumnSpecs &  GetSpecs() const
    {
        if (m_specs.empty())
        {
            m_specs = GetColumnSpecs();
        }

        return m_specs;
    }

    //  An event the pane's filter and the column filters let through. A
    //  column's text and number come from the cells the list shows.
    bool  IsShownEvent (const Row & row, size_t index) const
    {
        Cells  cells;



        if (!Matches (row, index))
        {
            return false;
        }

        if (m_valueFilters.empty() && m_compareFilters.empty())
        {
            return true;
        }

        MakeRowCells ((int) index, cells);

        for (const auto & [column, checked] : m_valueFilters)
        {
            if (column >= cells.size() || !checked.contains (cells[column].text))
            {
                return false;
            }
        }

        for (const auto & [column, comparison] : m_compareFilters)
        {
            std::optional<uint64_t>  number = (column < cells.size()) ? ParseFilterNumber (column, cells[column].text) : std::nullopt;

            if (!number.has_value() || !IsInside (comparison, *number))
            {
                return false;
            }
        }

        return true;
    }

    static bool  IsInside (const Comparison & comparison, uint64_t number)
    {
        switch (comparison.op)
        {
        case Op::Less:    return number <  comparison.first;
        case Op::Greater: return number >  comparison.first;
        case Op::Between: return number >= comparison.first && number <= comparison.last;
        default:          return number == comparison.first;
        }
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
        m_buildCount++;

        if (m_sortColumn < 0)
        {
            m_view.push_back (kTopRow);
        }

        PrepareView (rows, 0);

        for (i = 0; i < rows.size() && rows[i].position < m_shownBefore; i++)
        {
            if (rows[i].kind != DebugLogRowKind::Event || IsShownEvent (rows[i], i))
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
            if (rows[i].kind != DebugLogRowKind::Event || IsShownEvent (rows[i], i))
            {
                m_view.push_back ((int) i);
            }
        }

        m_viewRowCount = rows.size();

        return m_view.size() >= kept + removed.size() && std::equal (removed.begin(), removed.end(), m_view.begin() + (ptrdiff_t) kept);
    }

    //  The sorted view with the rows added since it was built each put in its
    //  place, after the rows that sort the same, which are older. False when
    //  the new rows can change the filtering of a row already in the view,
    //  and a full build follows.
    bool  ExtendSortedView()
    {
        const std::vector<Row>  & rows    = m_rows.GetRows();
        size_t                    from    = GetSettledCount (rows, std::min (m_viewRowCount, rows.size()));
        size_t                    i       = 0;
        bool                      isMoved = std::ranges::any_of (m_view, [from] (int index) { return index != kTopRow && (size_t) index >= from; });



        if (isMoved)
        {
            return false;
        }

        PrepareView (rows, from);
        m_sortKeys.resize (rows.size());
        m_sortNumbers.resize (rows.size());

        for (i = from; i < rows.size(); i++)
        {
            if (rows[i].kind == DebugLogRowKind::Event && !IsShownEvent (rows[i], i))
            {
                continue;
            }

            m_sortKeys[i]    = GetSortKey ((int) i);
            m_sortNumbers[i] = GetSortNumber (rows[i], i, (size_t) m_sortColumn);
            m_view.insert (std::ranges::upper_bound (m_view, (int) i, [this] (int a, int b) { return IsSortedBefore (a, b); }), (int) i);
        }

        m_viewRowCount = rows.size();

        return true;
    }

    //  Past kMaxRows with the list at its newest row, or past kHardMaxRows
    //  wherever it is, the oldest rows dropped down to kRetainedRows, and a
    //  request to the machine's thread with where the rows start now. None
    //  while older rows are being rebuilt or a request for them is
    //  outstanding. The count of rows dropped.
    size_t  TrimRows (bool isAtTail)
    {
        size_t  most    = isAtTail ? kMaxRows : kHardMaxRows;
        size_t  dropped = 0;



        if (m_rows.GetRows().size() <= most || m_rows.IsRebuilding() || m_isEarlierPending)
        {
            return 0;
        }

        dropped       = m_rows.TrimFront (kRetainedRows);
        m_requestedAt = UINT64_MAX;

        m_request ({ DebugLogRequest::Kind::DropBefore, m_rows.GetHeldFrom() });

        return dropped;
    }

    //  The time and cycle columns sort by number, the others by their text;
    //  rows that tie stay in position order.
    void  SortView()
    {
        m_sortKeys.assign    (m_rows.GetRows().size(), std::wstring());
        m_sortNumbers.assign (m_rows.GetRows().size(), std::nullopt);

        for (int index : m_view)
        {
            m_sortKeys[(size_t) index]    = GetSortKey (index);
            m_sortNumbers[(size_t) index] = GetSortNumber (m_rows.GetRows()[(size_t) index], (size_t) index, (size_t) m_sortColumn);
        }

        std::ranges::stable_sort (m_view, [this] (int a, int b) { return IsSortedBefore (a, b); });
    }

    //  The text a row sorts by: its cell in the sort column, none for the
    //  time and cycle columns, which sort by number.
    std::wstring  GetSortKey (int index) const
    {
        size_t  column = (size_t) m_sortColumn;
        Cells   cells;



        if (column == kTimeColumn || column == kCycleColumn)
        {
            return {};
        }

        MakeRowCells (index, cells);

        return (column < cells.size()) ? cells[column].text : std::wstring();
    }

    //  Whether the row at index a sorts before the row at index b.
    bool  IsSortedBefore (int a, int b) const
    {
        const Row  & left   = m_rows.GetRows()[(size_t) a];
        const Row  & right  = m_rows.GetRows()[(size_t) b];
        size_t       column = (size_t) m_sortColumn;



        if (column == kCycleColumn)
        {
            return m_isSortDescending ? right.cycle < left.cycle : left.cycle < right.cycle;
        }

        if (column == kTimeColumn)
        {
            return m_isSortDescending ? right.wallTime < left.wallTime : left.wallTime < right.wallTime;
        }

        //  Rows with a number go first, by number, and the rest by text, so
        //  the order is one a sort can follow.
        if (m_sortNumbers[(size_t) a].has_value() != m_sortNumbers[(size_t) b].has_value())
        {
            return m_isSortDescending ? m_sortNumbers[(size_t) b].has_value() : m_sortNumbers[(size_t) a].has_value();
        }

        if (m_sortNumbers[(size_t) a].has_value())
        {
            return m_isSortDescending ? *m_sortNumbers[(size_t) b] < *m_sortNumbers[(size_t) a] : *m_sortNumbers[(size_t) a] < *m_sortNumbers[(size_t) b];
        }

        return m_isSortDescending ? m_sortKeys[(size_t) b] < m_sortKeys[(size_t) a] : m_sortKeys[(size_t) a] < m_sortKeys[(size_t) b];
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

        m_viewTop   = GetTopIndex();
        m_isTopKept = false;

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

    DxuiListView                          * m_list;
    RequestFn                               m_request;
    EventLogRows<T>                         m_rows;
    std::vector<int>                        m_view;
    std::vector<std::wstring>               m_sortKeys;   // the sort column's text of each row in a sorted view, by row index
    std::vector<std::optional<uint64_t>>    m_sortNumbers;   // the sort column's number of each row, where it has one
    size_t                                  m_buildCount  = 0;   // full builds of the view, for a test
    mutable ColumnSpecs                     m_specs;
    std::vector<size_t>                     m_selected;
    std::optional<size_t>                   m_anchor;
    std::optional<size_t>                   m_keyboardRow;
    std::optional<size_t>                   m_keepTop;
    std::optional<size_t>                   m_viewTop;   // the top row's index, moved on by each delta since the view was built
    bool                                    m_isTopKept   = false;   // a delta since the view was built put rows in front or dropped rows above
    bool                     m_isEarlierPending = false;   // "earlier" was sent and no delta has come from it yet
    uint64_t                 m_shownBefore      = UINT64_MAX;
    size_t                   m_viewRowCount     = 0;        // the rows the view was built over
    uint64_t                 m_requestedAt      = UINT64_MAX;
    int                      m_sortColumn       = -1;
    bool                     m_isSortDescending = false;
    bool                     m_isShown          = false;
    bool                     m_isDirty          = true;
    bool                     m_isSelectionTaken = false;
    bool                     m_isFullBuildDue   = true;

    //  The column filters: each column's checked values, and its comparison.
    std::map<size_t, std::set<std::wstring>>  m_valueFilters;
    std::map<size_t, Comparison>              m_compareFilters;
};
