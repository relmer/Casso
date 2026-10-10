#include "Pch.h"

#include "Ui/DiskInspector/DifferencesTab.h"





static constexpr int      s_kToggleDip    = 96;
static constexpr int      s_kOptionDip    = 196;
static constexpr int      s_kToggleGapDip = 4;
static constexpr float    s_kToggleHDip   = 22.0f;
static constexpr LPCWSTR  s_kpszOptions[] = { L"Ignore sync widths and counts", L"Ignore volume numbers", L"Ignore dates" };





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesTab::DifferencesTab
//
////////////////////////////////////////////////////////////////////////////////

DifferencesTab::DifferencesTab (InspectorViewContext & context, ComparisonSession & session) :
    InspectorTableView (context),
    m_session          (session)
{
    SetColumns (ComparisonText::GetColumns());
    SetOnSort  ([this] (int column) { OnSort (column); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesTab::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void DifferencesTab::Refresh()
{
    m_counts = ComparisonText::CountGroups (m_session.GetListed());
    SetRows (ComparisonText::BuildRows (m_session.GetListed(), m_mask, m_sortColumn, m_isDescending));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesTab::GetDifference
//
////////////////////////////////////////////////////////////////////////////////

const Difference * DifferencesTab::GetDifference (const TableRow & row) const
{
    const vector<Difference> &  listed = m_session.GetListed();



    return (row.finding >= 0 && row.finding < static_cast<int> (listed.size())) ? &listed[row.finding] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesTab::OnMouse
//
//  A click on a group toggles it in the filter; a click on an option turns
//  it on or off.
//
////////////////////////////////////////////////////////////////////////////////

bool DifferencesTab::OnMouse (const DxuiMouseEvent & ev)
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
//  DifferencesTab::GetTopHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DifferencesTab::GetTopHeightPx() const
{
    return m_scaler.ToPx (2 * kRowDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesTab::PaintTop
//
//  A toggle per group that has differences, with its count, one filtered
//  out drawn without its fill; with none, a line says so. Below, the three
//  options, each filled while on.
//
////////////////////////////////////////////////////////////////////////////////

void DifferencesTab::PaintTop (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & top)
{
    int           i     = 0;
    int           total = 0;
    RECT          r     = {};
    bool          isOn  = false;
    std::wstring  label;



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
            label = std::format (L"{} {}", ComparisonText::FormatGroup (i), m_counts[i]);
            paintToggle (GetToggleRect (i), label, isOn);
        }
    }

    if (total == 0)
    {
        r = { top.left, top.top, top.right, top.top + m_scaler.ToPx (kRowDip) };
        text.DrawString (m_session.IsBusy() ? L"Comparing" : L"No differences", static_cast<float> (r.left), static_cast<float> (r.top),
                         static_cast<float> (r.right - r.left), static_cast<float> (r.bottom - r.top), theme.ForegroundMuted(), m_scaler.ToPxf (kTextDip),
                         DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }

    for (i = 0; i < kOptionCount; i++)
    {
        paintToggle (GetOptionRect (i), s_kpszOptions[i], IsOptionOn (i));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesTab::OnSort
//
//  A second click on the same column reverses it.
//
////////////////////////////////////////////////////////////////////////////////

void DifferencesTab::OnSort (int column)
{
    m_isDescending = (column == m_sortColumn) ? !m_isDescending : false;
    m_sortColumn   = column;

    SetSortIndicator (m_sortColumn, m_isDescending);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesTab::GetToggleRect
//
//  Toggles sit left to right in group order, skipping empty ones.
//
////////////////////////////////////////////////////////////////////////////////

RECT DifferencesTab::GetToggleRect (int group) const
{
    int  w     = m_scaler.ToPx (s_kToggleDip);
    int  gap   = m_scaler.ToPx (s_kToggleGapDip);
    int  h     = m_scaler.ToPx (static_cast<int> (s_kToggleHDip));
    int  shown = 0;
    int  i     = 0;
    int  left  = 0;
    int  top   = m_boundsDip.top + (m_scaler.ToPx (kRowDip) - h) / 2;



    for (i = 0; i < group; i++)
    {
        shown += (m_counts[i] > 0) ? 1 : 0;
    }

    left = m_boundsDip.left + shown * (w + gap);

    return { left, top, left + w, top + h };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesTab::GetOptionRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DifferencesTab::GetOptionRect (int option) const
{
    int  w    = std::min (m_scaler.ToPx (s_kOptionDip), static_cast<int> (m_boundsDip.right - m_boundsDip.left) / kOptionCount - m_scaler.ToPx (s_kToggleGapDip));
    int  gap  = m_scaler.ToPx (s_kToggleGapDip);
    int  h    = m_scaler.ToPx (static_cast<int> (s_kToggleHDip));
    int  top  = m_boundsDip.top + m_scaler.ToPx (kRowDip) + (m_scaler.ToPx (kRowDip) - h) / 2;
    int  left = m_boundsDip.left + option * (w + gap);



    return { left, top, left + w, top + h };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesTab::IsOptionOn
//
////////////////////////////////////////////////////////////////////////////////

bool DifferencesTab::IsOptionOn (int option) const
{
    const ComparisonOptions &  options = m_session.GetOptions();



    return (option == 0) ? options.isIgnoringSync : (option == 1) ? options.isIgnoringVolumes : options.isIgnoringDates;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DifferencesTab::ToggleOption
//
////////////////////////////////////////////////////////////////////////////////

void DifferencesTab::ToggleOption (int option)
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
