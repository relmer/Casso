#include "Pch.h"

#include "Ui/DiskInspector/Views/DifferencesView.h"





static constexpr float    s_kPillPadDip   = 12.0f;
static constexpr float    s_kToggleGapDip = 6.0f;
static constexpr float    s_kToggleHDip   = 22.0f;
static constexpr LPCWSTR  s_kpszShow      = L"Show";
static constexpr LPCWSTR  s_kpszIgnore    = L"Ignore";
static constexpr LPCWSTR  s_kpszOptions[] = { L"Sync widths and counts", L"Volume numbers", L"Dates" };





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesView::DifferencesView
//
////////////////////////////////////////////////////////////////////////////////

DifferencesView::DifferencesView (InspectorViewContext & context, ComparisonSession & session) :
    InspectorTableView (context),
    m_session          (session)
{
    SetColumns (ComparisonText::GetColumns());
    SetOnSort  ([this] (int column) { OnSort (column); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesView::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void DifferencesView::Refresh()
{
    m_counts = ComparisonText::CountGroups (m_session.GetListed());
    SetRows (ComparisonText::BuildRows (m_session.GetListed(), m_mask, m_sortColumn, m_isDescending,
                                        m_session.HasResult() ? m_session.GetResult().names : SideNames()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesView::GetDifference
//
////////////////////////////////////////////////////////////////////////////////

const Difference * DifferencesView::GetDifference (const TableRow & row) const
{
    const vector<Difference> &  listed = m_session.GetListed();



    return (row.finding >= 0 && row.finding < static_cast<int> (listed.size())) ? &listed[row.finding] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesView::OnMouse
//
//  A click on a group toggles it in the filter; a click on an option turns
//  it on or off.
//
////////////////////////////////////////////////////////////////////////////////

bool DifferencesView::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isHandled = false;
    int   i         = 0;
    RECT  r         = {};



    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        for (i = 0; i < ComparisonText::kGroupCount && !isHandled; i++)
        {
            r = GetToggleRect (i);

            if (m_counts[i] > 0 && PtInRect (&r, ev.positionDip))
            {
                m_mask   ^= (1u << i);
                isHandled = true;
                Refresh();
            }
        }

        for (i = 0; i < kOptionCount && !isHandled; i++)
        {
            r = GetOptionRect (i);

            if (PtInRect (&r, ev.positionDip))
            {
                ToggleOption (i);
                isHandled = true;
            }
        }
    }

    if (!isHandled)
    {
        isHandled = InspectorTableView::OnMouse (ev);
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesView::GetTopHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DifferencesView::GetTopHeightPx() const
{
    return (m_topHeightPx > 0) ? m_topHeightPx : m_scaler.ToPx (2 * kRowDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesView::PaintTop
//
//  After "Show", a filter pill per group that has differences, with its
//  count, one filtered out drawn without its fill; with none, a line in
//  their place. After "Ignore", the three options, each filled while on.
//
////////////////////////////////////////////////////////////////////////////////

void DifferencesView::PaintTop (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & top)
{
    int           i     = 0;
    int           total = 0;
    RECT          r     = {};
    bool          isOn  = false;
    std::wstring  label;



    FlowPills (text);

    auto  paintLabel = [&] (const RECT & rect, LPCWSTR caption)
    {
        text.DrawString (caption, static_cast<float> (rect.left), static_cast<float> (rect.top), static_cast<float> (rect.right - rect.left),
                         static_cast<float> (rect.bottom - rect.top), theme.ForegroundMuted(), m_scaler.ToPxf (kSmallDip), DxuiTheme::kBodyFace,
                         DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    };

    auto  paintToggle = [&] (const RECT & rect, const std::wstring & caption, bool isFilled)
    {
        painter.FillRoundedRect (static_cast<float> (rect.left), static_cast<float> (rect.top), static_cast<float> (rect.right - rect.left),
                                 static_cast<float> (rect.bottom - rect.top), static_cast<float> (rect.bottom - rect.top) / 2,
                                 isFilled ? theme.SelectionBackground() : theme.Background());
        painter.OutlineRoundedRect (static_cast<float> (rect.left), static_cast<float> (rect.top), static_cast<float> (rect.right - rect.left),
                                    static_cast<float> (rect.bottom - rect.top), static_cast<float> (rect.bottom - rect.top) / 2, m_scaler.ToPxf (1.0f),
                                    theme.ButtonBorder());
        text.DrawString (caption.c_str(), static_cast<float> (rect.left), static_cast<float> (rect.top), static_cast<float> (rect.right - rect.left),
                         static_cast<float> (rect.bottom - rect.top), isFilled ? theme.Foreground() : theme.ForegroundMuted(), m_scaler.ToPxf (kSmallDip),
                         DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    };

    for (i = 0; i < ComparisonText::kGroupCount; i++)
    {
        total += m_counts[i];

        if (m_counts[i] > 0)
        {
            isOn  = (m_mask & (1u << i)) != 0;
            label = ComparisonText::FormatGroup (i, m_counts[i]);
            paintToggle (GetToggleRect (i), label, isOn);
        }
    }

    if (total > 0)
    {
        paintLabel (m_showLabel, s_kpszShow);
    }
    else
    {
        r = { top.left, top.top, top.right, top.top + m_scaler.ToPx (kRowDip) };
        text.DrawString (m_session.IsBusy() ? L"Comparing" : L"No differences", static_cast<float> (r.left), static_cast<float> (r.top),
                         static_cast<float> (r.right - r.left), static_cast<float> (r.bottom - r.top), theme.ForegroundMuted(), m_scaler.ToPxf (kTextDip),
                         DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }

    paintLabel (m_ignoreLabel, s_kpszIgnore);

    for (i = 0; i < kOptionCount; i++)
    {
        paintToggle (GetOptionRect (i), s_kpszOptions[i], IsOptionOn (i));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesView::OnSort
//
//  A second click on the same column reverses it.
//
////////////////////////////////////////////////////////////////////////////////

void DifferencesView::OnSort (int column)
{
    m_isDescending = (column == m_sortColumn) ? !m_isDescending : false;
    m_sortColumn   = column;

    SetSortIndicator (m_sortColumn, m_isDescending);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesView::FlowPills
//
//  Each pill as wide as its caption, left to right after its row's label,
//  wrapping under the first pill when the next doesn't fit. The labels
//  share a width, so both rows of pills start at one edge. When the rows
//  take a different height, the list is laid out again below them.
//
////////////////////////////////////////////////////////////////////////////////

void DifferencesView::FlowPills (IDxuiTextRenderer & text)
{
    int    rowH   = m_scaler.ToPx (kRowDip);
    int    h      = m_scaler.ToPx (static_cast<int> (s_kToggleHDip));
    int    gap    = m_scaler.ToPx (static_cast<int> (s_kToggleGapDip));
    int    pad    = m_scaler.ToPx (static_cast<int> (s_kPillPadDip));
    float  textPx = m_scaler.ToPxf (kSmallDip);
    float  w      = 0;
    float  unused = 0;
    int    indent = 0;
    int    x      = 0;
    int    row    = 0;
    int    height = 0;



    auto  measure = [&] (const std::wstring & caption)
    {
        w = 0;
        text.MeasureString (caption.c_str(), textPx, DxuiTheme::kBodyFace, w, unused);
        return static_cast<int> (std::ceil (w));
    };

    auto  place = [&] (int width)
    {
        if (x > indent && x + width > m_boundsDip.right)
        {
            x = indent;
            row++;
        }

        RECT  r = { x, m_boundsDip.top + row * rowH + (rowH - h) / 2, x + width, m_boundsDip.top + row * rowH + (rowH - h) / 2 + h };

        x += width + gap;
        return r;
    };

    indent = m_boundsDip.left + std::max (measure (s_kpszShow), measure (s_kpszIgnore)) + 2 * gap;

    x            = indent;
    m_showLabel  = { m_boundsDip.left, m_boundsDip.top, indent, m_boundsDip.top + rowH };
    m_groupRects = {};

    for (int i = 0; i < ComparisonText::kGroupCount; i++)
    {
        if (m_counts[i] > 0)
        {
            m_groupRects[i] = place (measure (ComparisonText::FormatGroup (i, m_counts[i])) + 2 * pad);
        }
    }

    x = indent;
    row++;
    m_ignoreLabel = { m_boundsDip.left, m_boundsDip.top + row * rowH, indent, m_boundsDip.top + (row + 1) * rowH };

    for (int i = 0; i < kOptionCount; i++)
    {
        m_optionRects[i] = place (measure (s_kpszOptions[i]) + 2 * pad);
    }

    height = (row + 1) * rowH;

    if (height != m_topHeightPx)
    {
        m_topHeightPx = height;
        InspectorTableView::Layout (m_boundsDip, m_scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesView::IsOptionOn
//
////////////////////////////////////////////////////////////////////////////////

bool DifferencesView::IsOptionOn (int option) const
{
    const ComparisonOptions &  options = m_session.GetOptions();



    return (option == 0) ? options.isIgnoringSync : (option == 1) ? options.isIgnoringVolumes : options.isIgnoringDates;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesView::ToggleOption
//
////////////////////////////////////////////////////////////////////////////////

void DifferencesView::ToggleOption (int option)
{
    ComparisonOptions  options = m_session.GetOptions();



    switch (option)
    {
        case 0:  options.isIgnoringSync    = !options.isIgnoringSync;    break;
        case 1:  options.isIgnoringVolumes = !options.isIgnoringVolumes; break;
        default: options.isIgnoringDates   = !options.isIgnoringDates;   break;
    }

    m_session.SetOptions (options);
    Refresh();
}
