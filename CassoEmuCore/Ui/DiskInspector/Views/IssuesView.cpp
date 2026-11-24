#include "Pch.h"

#include "Ui/DiskInspector/Views/IssuesView.h"
#include "Devices/Disk/Inspector/FindingFormatter.h"
#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"





static constexpr int      s_kToggleDip    = 104;
static constexpr int      s_kToggleGapDip = 4;
static constexpr float    s_kToggleHDip   = 22.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::IssuesView
//
//  The finding columns, each as wide as its content measured: the severity
//  as an icon under an empty heading, and the finding on one line. The last
//  column takes any width left over, and the list scrolls sideways when the
//  columns are wider than the view. Copy keeps the severity's heading.
//
////////////////////////////////////////////////////////////////////////////////

IssuesView::IssuesView (InspectorViewContext & context) :
    InspectorTableView (context)
{
    vector<std::wstring>  titles  = InspectorTables::GetFindingColumns();
    vector<std::wstring>  columns = titles;
    vector<int>           widths  (columns.size(), 0);



    columns[0] = std::wstring();
    widths[0]  = kSeverityColumnDip;

    SetColumns    (columns, widths, kCellColumn);
    SetIconColumn (0);
    SetWrapsText  (false);
    SetOnSort     ([this] (int column) { OnSort (column); });

    m_titles = std::move (titles);
    m_list.SetPreciseAutoFit (true);

    m_banner.SetVisible  (false);
    m_banner.SetOnAction ([this] (size_t)
    {
        if (m_repair)
        {
            m_repair ({});
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void IssuesView::Refresh()
{
    if (m_context.analysis != nullptr && m_context.hasDisk)
    {
        m_counts = InspectorTables::CountCategories (*m_context.analysis);
        SetRows (InspectorTables::BuildFindings (*m_context.analysis, m_mask, m_sortColumn, m_isDescending));
    }
    else
    {
        m_counts = {};
        SetRows ({});
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::ShowFinding
//
////////////////////////////////////////////////////////////////////////////////

void IssuesView::ShowFinding (int finding)
{
    int  category = 0;



    if (m_context.analysis == nullptr || finding < 0 || finding >= static_cast<int> (m_context.analysis->findings.size()))
    {
        return;
    }

    category = static_cast<int> (m_context.analysis->findings[finding].category);

    if ((m_mask & (1u << category)) == 0)
    {
        m_mask |= (1u << category);
        Refresh();
    }

    SelectRowWhere ([finding] (const TableRow & row) { return row.finding == finding; });

    if (m_select)
    {
        for (const TableRow & row : m_rows)
        {
            if (row.finding == finding)
            {
                m_select (row);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::GetSeverityRect
//
//  The severity icon of a row in view, centered in its whole cell, not the
//  text's part of it, which starts past the column's padding; empty out of
//  view.
//
////////////////////////////////////////////////////////////////////////////////

RECT IssuesView::GetSeverityRect (int row) const
{
    RECT  cell = {};
    RECT  list = m_list.GetBounds();
    int   size = m_scaler.ToPx (kSeverityIconDip);
    int   left = 0;
    int   top  = 0;



    if (row < 0 || row >= static_cast<int> (m_rows.size()) || !m_list.GetCellRectPx (row, 0, cell))
    {
        return RECT {};
    }

    left = list.left + (cell.left + cell.right - size) / 2;
    top  = list.top + (cell.top + cell.bottom - size) / 2;

    if (top < list.top + m_list.GetHeaderHeightPx() || top + size > list.bottom)
    {
        return RECT {};
    }

    return RECT { left, top, left + size, top + size };
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::Layout
//
//  The infobar across the top while it's shown, and the filter and the list
//  under it.
//
////////////////////////////////////////////////////////////////////////////////

void IssuesView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    RECT  list   = boundsDip;
    int   banner = 0;



    UpdateBanner();

    if (m_banner.IsVisible())
    {
        banner = static_cast<int> (std::ceil (m_banner.GetPreferredHeightPx (static_cast<float> (boundsDip.right - boundsDip.left), scaler)));
    }

    m_banner.Layout (RECT { boundsDip.left, boundsDip.top, boundsDip.right, boundsDip.top + banner }, scaler);

    m_bannerPx = (banner > 0) ? banner + scaler.ToPx (kBannerGapDip) : 0;
    list.top   = (std::min) (list.bottom, list.top + m_bannerPx);

    InspectorTableView::Layout (list, scaler);
    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::Paint
//
//  The table, a severity icon on each row, and the infobar over them.
//
////////////////////////////////////////////////////////////////////////////////

void IssuesView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT  rect = {};



    InspectorTableView::Paint (painter, text, theme);

    for (int row = m_list.GetTopRow(); row < static_cast<int> (m_rows.size()); row++)
    {
        rect = GetSeverityRect (row);

        if (rect.right > rect.left && m_rows[row].severity >= 0)
        {
            DxuiSeverityBadge::Draw (painter, static_cast<DxuiSeverity> (m_rows[row].severity), static_cast<float> (rect.left), static_cast<float> (rect.top),
                                     static_cast<float> (rect.right - rect.left), theme, m_context.palette.colors.sectorBad);
        }
    }

    if (m_banner.IsVisible())
    {
        m_banner.Paint (painter, text, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::OnMouse
//
//  The infobar's Repair... first, then a click on a category toggles it in
//  the filter.
//
////////////////////////////////////////////////////////////////////////////////

bool IssuesView::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isHandled = m_banner.IsVisible() && m_banner.OnMouse (ev);
    int   c         = 0;
    RECT  r         = {};



    if (!isHandled && ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        for (c = 0; c < InspectorTables::kCategoryCount && !isHandled; c++)
        {
            r = GetToggleRect (c);

            if (m_counts[c] > 0 && PtInRect (&r, ev.positionDip))
            {
                m_mask   ^= (1u << c);
                isHandled = true;
                Refresh();
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
//  IssuesView::HasBanner
//
//  The infobar while the volume can be repaired or has a Bad or Suspect
//  verdict.
//
////////////////////////////////////////////////////////////////////////////////

bool IssuesView::HasBanner() const
{
    bool  isDamaged = m_context.analysis != nullptr && m_context.hasDisk &&
                      (m_context.analysis->verdict == FileSystemVerdict::Bad || m_context.analysis->verdict == FileSystemVerdict::Suspect);



    return m_plan != nullptr && (m_plan->canRepair || isDamaged);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::UpdateBanner
//
//  Bad as an error and anything else as a warning, with Repair... only while
//  Repair is available.
//
////////////////////////////////////////////////////////////////////////////////

void IssuesView::UpdateBanner()
{
    bool  isShown = HasBanner();



    m_banner.SetVisible (isShown);

    if (!isShown)
    {
        return;
    }

    m_banner.SetText     (FormatBanner (*m_context.analysis, *m_plan));
    m_banner.SetSeverity ((m_context.analysis->verdict == FileSystemVerdict::Bad) ? DxuiInfoBanner::Severity::Error : DxuiInfoBanner::Severity::Warning);
    m_banner.SetActions  (m_plan->canRepair ? vector<std::wstring> { L"Repair..." } : vector<std::wstring>());
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::FormatBanner
//
//  "Track 18, sector $E is used by HELLO but marked free.", and when
//  Repair isn't available, its reason after.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring IssuesView::FormatBanner (const DiskAnalysis & analysis, const RepairPlan & plan)
{
    const Finding  * finding  = FileSystemCheck::GetVerdictFinding (analysis);
    std::wstring     sentence = (finding != nullptr) ? FindingFormatter::Format (*finding) : std::wstring();
    std::wstring     text;



    if (!sentence.empty())
    {
        sentence[0] = static_cast<wchar_t> (towupper (sentence[0]));
    }

    if (!sentence.empty() && !sentence.ends_with (L'.'))
    {
        sentence += L".";
    }

    //  The infobar's color indicates the verdict, so the text is the finding
    //  alone, or the verdict when there is no finding.
    text = sentence.empty() ? FileSystemCheck::FormatVerdict (analysis.verdict) + L"." : sentence;

    if (!plan.canRepair)
    {
        text += L" Repair isn't available. " + plan.reason;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::GetTopHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int IssuesView::GetTopHeightPx() const
{
    return m_scaler.ToPx (kFilterDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::PaintTop
//
//  A toggle per category that has findings, with its count; one filtered
//  out is drawn without its fill. With no findings, the list has a line to
//  that effect.
//
////////////////////////////////////////////////////////////////////////////////

void IssuesView::PaintTop (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & top)
{
    int           c       = 0;
    int           total   = 0;
    RECT          r       = {};
    bool          isOn    = false;
    std::wstring  label;



    (void) top;

    for (c = 0; c < InspectorTables::kCategoryCount; c++)
    {
        total += m_counts[c];

        if (m_counts[c] == 0)
        {
            continue;
        }

        r     = GetToggleRect (c);
        isOn  = (m_mask & (1u << c)) != 0;
        label = std::format (L"{} {}", FindingFormatter::FormatCategory (static_cast<FindingCategory> (c)), m_counts[c]);

        painter.FillRoundedRect (static_cast<float> (r.left), static_cast<float> (r.top), static_cast<float> (r.right - r.left), static_cast<float> (r.bottom - r.top),
                                 static_cast<float> (r.bottom - r.top) / 2, isOn ? theme.SelectionBackground() : theme.Background());
        painter.OutlineRoundedRect (static_cast<float> (r.left), static_cast<float> (r.top), static_cast<float> (r.right - r.left), static_cast<float> (r.bottom - r.top),
                                    static_cast<float> (r.bottom - r.top) / 2, m_scaler.ToPxf (1.0f), theme.ButtonBorder());
        text.DrawString (label.c_str(), static_cast<float> (r.left), static_cast<float> (r.top), static_cast<float> (r.right - r.left), static_cast<float> (r.bottom - r.top),
                         isOn ? theme.Foreground() : theme.ForegroundMuted(), m_scaler.ToPxf (kSmallDip), DxuiTheme::kBodyFace,
                         DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }

    SetEmptyText ((total == 0 && m_context.hasDisk) ? L"No findings" : L"");
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::OnSort
//
//  A second click on the same column reverses it.
//
////////////////////////////////////////////////////////////////////////////////

void IssuesView::OnSort (int column)
{
    m_isDescending = (column == m_sortColumn) ? !m_isDescending : false;
    m_sortColumn   = column;

    SetSortIndicator (m_sortColumn, m_isDescending);
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::GetToggleRect
//
//  Toggles sit left to right in category order, skipping empty ones.
//
////////////////////////////////////////////////////////////////////////////////

RECT IssuesView::GetToggleRect (int category) const
{
    int  w     = m_scaler.ToPx (s_kToggleDip);
    int  gap   = m_scaler.ToPx (s_kToggleGapDip);
    int  h     = m_scaler.ToPx (static_cast<int> (s_kToggleHDip));
    int  shown = 0;
    int  c     = 0;
    int  left  = 0;
    int  top   = m_boundsDip.top + m_bannerPx + (GetTopHeightPx() - h) / 2;



    for (c = 0; c < category; c++)
    {
        shown += (m_counts[c] > 0) ? 1 : 0;
    }

    left = m_boundsDip.left + shown * (w + gap);

    return { left, top, left + w, top + h };
}
