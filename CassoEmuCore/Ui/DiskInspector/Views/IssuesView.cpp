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
    ShowColumns();
    SetIconColumn (0);
    SetWrapsText  (false);
    SetOnSort     ([this] (int column) { OnSort (column); });

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
    vector<TableRow>  rows;
    vector<TableRow>  others;



    m_counts = {};

    if (m_context.analysis != nullptr && m_context.hasDisk && IsComparing())
    {
        //  Each category's count is of the lines, so a finding both images
        //  have counts once.
        rows   = InspectorTables::BuildFindings (*m_context.analysis, InspectorTables::GetAllCategories(), m_sortColumn, m_isDescending);
        others = InspectorTables::BuildFindings (*m_otherAnalysis, InspectorTables::GetAllCategories(), m_sortColumn, m_isDescending);
        InspectorTables::AlignFindings (rows, others);

        for (size_t i = 0; i < rows.size(); i++)
        {
            const TableRow &      side     = (rows[i].finding >= 0) ? rows[i] : others[i];
            const DiskAnalysis &  analysis = (rows[i].finding >= 0) ? *m_context.analysis : *m_otherAnalysis;
            int                   category = static_cast<int> (analysis.findings[static_cast<size_t> (side.finding)].category);

            m_counts[category]++;

            if ((m_mask & (1u << category)) == 0)
            {
                rows[i].finding   = -2;
            }
        }

        for (size_t i = rows.size(); i-- > 0;)
        {
            if (rows[i].finding == -2)
            {
                rows.erase   (rows.begin() + static_cast<ptrdiff_t> (i));
                others.erase (others.begin() + static_cast<ptrdiff_t> (i));
            }
        }

        //  Each line has both sides' cells. The other image's side goes to
        //  its track and sector, since its field numbers aren't this
        //  image's, and its severity is drawn as an icon in place of text.
        for (size_t i = 0; i < rows.size(); i++)
        {
            others[i].field = -1;
            rows[i].cells.insert (rows[i].cells.end(), others[i].cells.begin(), others[i].cells.end());
            rows[i].cells[others[i].cells.size()] = std::wstring();
        }

        m_otherRows = std::move (others);
        SetRows (std::move (rows));
    }
    else if (m_context.analysis != nullptr && m_context.hasDisk)
    {
        m_counts = InspectorTables::CountCategories (*m_context.analysis);
        m_otherRows.clear();
        SetRows (InspectorTables::BuildFindings (*m_context.analysis, m_mask, m_sortColumn, m_isDescending));
    }
    else
    {
        m_otherRows.clear();
        SetRows ({});
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::SetComparedWith
//
////////////////////////////////////////////////////////////////////////////////

void IssuesView::SetComparedWith (const DiskAnalysis * other, const std::wstring & name, const std::wstring & otherName)
{
    bool  isChanged = (other != nullptr) != m_isCompared || name != m_names[0] || otherName != m_names[1];



    m_otherAnalysis = other;
    m_names[0]      = name;
    m_names[1]      = otherName;

    if (isChanged)
    {
        m_isCompared = other != nullptr;
        ShowColumns();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::ShowColumns
//
//  The finding columns, each as wide as its content measured: the severity
//  as an icon under an empty heading, and the finding on one line. While
//  comparing, the other image's columns follow this image's, and each
//  Finding heading has its image's name.
//
////////////////////////////////////////////////////////////////////////////////

void IssuesView::ShowColumns()
{
    vector<std::wstring>  titles  = InspectorTables::GetFindingColumns();
    vector<std::wstring>  columns = titles;
    vector<int>           widths  (columns.size(), 0);
    size_t                count   = titles.size();



    columns[0] = std::wstring();
    widths[0]  = kSeverityColumnDip;

    if (m_isCompared)
    {
        columns[kFindingColumn] = L"Finding in " + m_names[0];
        titles[kFindingColumn]  = columns[kFindingColumn];

        for (size_t c = 0; c < count; c++)
        {
            columns.push_back (columns[c]);
            titles.push_back  (titles[c]);
            widths.push_back  (widths[c]);
        }

        columns[count + kFindingColumn] = L"Finding in " + m_names[1];
        titles[count + kFindingColumn]  = columns[count + kFindingColumn];
    }

    SetColumns (columns, widths, m_isCompared ? count + kCellColumn : kCellColumn);
    m_titles = std::move (titles);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::GetRowToSelect
//
//  While comparing, a line's side the press was on, or the other side when
//  that one is blank.
//
////////////////////////////////////////////////////////////////////////////////

const TableRow & IssuesView::GetRowToSelect (int index) const
{
    size_t  i         = static_cast<size_t> (index);
    bool    isOther   = IsComparing() && i < m_otherRows.size() && (m_isOtherSide ? m_otherRows[i].finding >= 0 : m_rows[i].finding < 0);



    return isOther ? m_otherRows[i] : m_rows[i];
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
    return GetBadgeRect (row, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::GetBadgeRect
//
//  Where the severity icon in a column of a row goes, empty out of view.
//
////////////////////////////////////////////////////////////////////////////////

RECT IssuesView::GetBadgeRect (int row, size_t column) const
{
    const DxuiListView &  view   = m_list;
    RECT                  cell   = {};
    RECT                  list   = view.GetBounds();
    int                   sizePx = m_scaler.ToPx (kSeverityIconDip);
    int                   left   = 0;
    int                   top    = 0;



    if (row < 0 || row >= static_cast<int> (m_rows.size()) || !view.GetCellRectPx (row, static_cast<int> (column), cell))
    {
        return RECT {};
    }

    left = list.left + (cell.left + cell.right - sizePx) / 2;
    top  = list.top + (cell.top + cell.bottom - sizePx) / 2;

    if (top < list.top + view.GetHeaderHeightPx() || top + sizePx > list.bottom)
    {
        return RECT {};
    }

    return RECT { left, top, left + sizePx, top + sizePx };
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
    RECT    rect   = {};
    size_t  second = InspectorTables::GetFindingColumns().size();



    InspectorTableView::Paint (painter, text, theme);

    //  Each side's severity, the other image's in that image's severity column.
    for (int row = m_list.GetTopRow(); row < static_cast<int> (m_rows.size()); row++)
    {
        for (int side = 0; side < (m_isCompared ? 2 : 1); side++)
        {
            int  severity = (side == 0) ? m_rows[row].severity : (static_cast<size_t> (row) < m_otherRows.size() ? m_otherRows[row].severity : -1);

            rect = GetBadgeRect (row, (side == 0) ? 0 : second);

            if (rect.right > rect.left && severity >= 0)
            {
                DxuiSeverityBadge::Draw (painter, static_cast<DxuiSeverity> (severity), static_cast<float> (rect.left), static_cast<float> (rect.top),
                                         static_cast<float> (rect.right - rect.left), theme, m_context.palette.colors.sectorBad);
            }
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

    //  A press on the other image's columns goes to its finding.
    if (!isHandled && ev.kind == DxuiMouseEventKind::Down && m_isCompared)
    {
        RECT  cell  = {};
        RECT  list  = m_list.GetBounds();
        int   first = std::max (m_list.GetTopRow(), 0);

        m_isOtherSide = !m_rows.empty() && m_list.GetCellRectPx (first, static_cast<int> (InspectorTables::GetFindingColumns().size()), cell) &&
                        ev.positionDip.x >= list.left + cell.left;
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
    m_banner.SetSeverity ((m_context.analysis->verdict == FileSystemVerdict::Bad) ? DxuiInfoBar::Severity::Error : DxuiInfoBar::Severity::Warning);
    m_banner.SetActions  (m_plan->canRepair ? vector<std::wstring> { L"Repair..." } : vector<std::wstring>());
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::FormatBanner
//
//  "Track 18, sector $2 is used by HELLO but marked free.", and when
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
    return m_scaler.ToPx (kFilterDip) + (m_caption.empty() ? 0 : m_scaler.ToPx (kCaptionDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  IssuesView::PaintTop
//
//  A toggle per category that has findings, with its count; one filtered
//  out is drawn without its fill, and any caption is under them. With no
//  findings, the list has a line to that effect.
//
////////////////////////////////////////////////////////////////////////////////

void IssuesView::PaintTop (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & top)
{
    int           c       = 0;
    int           total   = 0;
    RECT          r       = {};
    bool          isOn    = false;
    std::wstring  label;



    //  The caption, as while comparing, under the toggles.
    if (!m_caption.empty())
    {
        InspectorTableView::PaintTop (painter, text, theme, { top.left, top.top + m_bannerPx + m_scaler.ToPx (kFilterDip), top.right, top.top + m_bannerPx + GetTopHeightPx() });
    }

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
    column = m_isCompared ? column % static_cast<int> (InspectorTables::GetFindingColumns().size()) : column;
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
    int  top   = m_boundsDip.top + m_bannerPx + (m_scaler.ToPx (kFilterDip) - h) / 2;



    for (c = 0; c < category; c++)
    {
        shown += (m_counts[c] > 0) ? 1 : 0;
    }

    left = m_boundsDip.left + shown * (w + gap);

    return { left, top, left + w, top + h };
}
