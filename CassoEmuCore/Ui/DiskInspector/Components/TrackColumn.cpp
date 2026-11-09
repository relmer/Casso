#include "Pch.h"

#include "Ui/DiskInspector/Components/TrackColumn.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorMetrics.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/InspectorTables.h"
#include "Ui/DiskInspector/SectorRowView.h"
#include "Ui/DiskInspector/TrackHeaderView.h"





static constexpr int     s_kSectorRowDip = 32;
static constexpr int     s_kSelectorDip  = 26;
static constexpr int     s_kMinSegmentDip = 64;
static constexpr int     s_kSegmentPadDip = 24;

//  Every view in FR-160's order; the Fields table is the Nibbles view's
//  section headers (FR-042).
static const TrackColumn::View  s_kViews[] =
{
    { L"Issues",       true,  kTabFindings,    L"Anything unusual about the disk or its image file" },
    { L"Files",        true,  kTabFileMap,     L"The file each sector of the disk's volume is part of" },
    { L"Sectors",      false, kTabSectorData,  L"The selected sector's 256 bytes, under the details of its address and data fields" },
    { L"Tracks",       true,  kTabTracks,      L"Every quarter track: what is recorded there and how well it reads" },
    { L"Image",        true,  kTabImage,       L"What the image file itself contains" },
    { L"Nibbles",      false, kTabNibbles,     L"Every nibble on the track, as the disk controller reads them" },
    { L"Flux",         false, kTabFluxTiming,  L"How fast or slow each cell runs, on a track recorded as flux" },
    { L"Differences",  true,  kTabDifferences, L"Everything that differs between disk A and disk B" },
    { L"Find results", true,  kTabFindResults, L"Every hit of the find" },
};





////////////////////////////////////////////////////////////////////////////////
//
//  TrackColumn::TrackColumn
//
////////////////////////////////////////////////////////////////////////////////

TrackColumn::TrackColumn (InspectorSession & session, IInspectorComponentHost & host) :
    m_session (session),
    m_host    (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackColumn::Create
//
////////////////////////////////////////////////////////////////////////////////

void TrackColumn::Create()
{
    DxuiPanel &           parent = m_host.GetControlParent();
    vector<std::wstring>  labels;



    m_header    = parent.CreateChild<TrackHeaderView> (m_session.context);
    m_sectorRow = parent.CreateChild<SectorRowView>   (m_session.context);
    m_views     = parent.CreateChild<DxuiSegmentedControl>();
    m_shown     = GetShownViews();

    for (const View & view : m_shown)
    {
        labels.push_back (view.label);
    }

    m_views->SetSegments (std::move (labels));
    m_views->SetOnChange ([this] (int index) { Choose (index); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackColumn::GetShownViews
//
//  Issues only while the disk has any, Flux only for a disk with a flux
//  track, Differences only while comparing, and Find results only with hits
//  (FR-046, FR-048, FR-160).
//
////////////////////////////////////////////////////////////////////////////////

vector<TrackColumn::View> TrackColumn::GetShownViews() const
{
    vector<View>  shown;



    for (const View & view : s_kViews)
    {
        bool  isShown = true;

        switch (view.isDisk ? view.tab + kTrackTabCount : view.tab)
        {
            case kTabFindings + kTrackTabCount:    isShown = m_hasIssues;                                                  break;
            case kTabFluxTiming:                   isShown = m_session.analysis.summary.fluxTracks > 0;                     break;
            case kTabDifferences + kTrackTabCount: isShown = m_session.comparison.IsComparing();                            break;
            case kTabFindResults + kTrackTabCount: isShown = !m_session.find.IsEmpty();                                     break;
            default:                                                                                                      break;
        }

        if (isShown)
        {
            shown.push_back (view);
        }
    }

    return shown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackColumn::GetAllViews
//
////////////////////////////////////////////////////////////////////////////////

vector<TrackColumn::View> TrackColumn::GetAllViews()
{
    return vector<View> (std::begin (s_kViews), std::end (s_kViews));
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackColumn::GetSelectedIndex
//
//  The chosen view's place in the selector, or -1 when it is not present.
//
////////////////////////////////////////////////////////////////////////////////

int TrackColumn::GetSelectedIndex() const
{
    int  index = -1;
    int  i     = 0;



    for (i = 0; i < static_cast<int> (m_shown.size()); i++)
    {
        const View &  view = m_shown[static_cast<size_t> (i)];

        if (view.isDisk == m_session.isDiskGroup && view.tab == (view.isDisk ? m_session.diskTab : m_session.trackTab))
        {
            index = i;
        }
    }

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackColumn::Choose
//
////////////////////////////////////////////////////////////////////////////////

void TrackColumn::Choose (int index)
{
    if (index >= 0 && index < static_cast<int> (m_shown.size()))
    {
        const View &  view = m_shown[static_cast<size_t> (index)];

        if (view.isDisk)
        {
            m_host.ShowDiskTab (view.tab);
        }
        else
        {
            m_host.ShowTrackTab (view.tab);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackColumn::OnAnalysisChanged
//
////////////////////////////////////////////////////////////////////////////////

void TrackColumn::OnAnalysisChanged()
{
    std::array<int, InspectorTables::kCategoryCount>  counts = InspectorTables::CountCategories (m_session.analysis);



    m_hasIssues = m_session.context.hasDisk && std::any_of (counts.begin(), counts.end(), [] (int count) { return count > 0; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackColumn::Update
//
//  The selector's choices follow what is present. When the chosen view goes,
//  as Issues does after the last issue is edited away, Sectors is chosen
//  (FR-048).
//
////////////////////////////////////////////////////////////////////////////////

void TrackColumn::Update()
{
    vector<View>  shown     = GetShownViews();
    bool          isChanged = shown.size() != m_shown.size();
    size_t        i         = 0;



    for (i = 0; !isChanged && i < shown.size(); i++)
    {
        isChanged = shown[i].tab != m_shown[i].tab || shown[i].isDisk != m_shown[i].isDisk;
    }

    if (isChanged)
    {
        vector<std::wstring>  labels;

        m_shown = std::move (shown);

        for (const View & view : m_shown)
        {
            labels.push_back (view.label);
        }

        m_views->SetSegments (std::move (labels));
        m_session.isLayoutStale = true;
    }

    if (GetSelectedIndex() < 0 && m_session.context.hasDisk)
    {
        m_host.ShowTrackTab (kTabSectorData);
    }

    m_views->SetVisible  (m_session.context.hasDisk);
    m_views->SetSelected (std::max (0, GetSelectedIndex()), !isChanged);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackColumn::Tick
//
////////////////////////////////////////////////////////////////////////////////

void TrackColumn::Tick (int64_t nowMs)
{
    m_views->Tick (nowMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackColumn::IsLayoutStale
//
//  True when the sector row's count of rows at its width has changed.
//
////////////////////////////////////////////////////////////////////////////////

bool TrackColumn::IsLayoutStale() const
{
    RECT  row = m_sectorRow->GetBounds();



    return m_sectorRow->GetRowCount (row.right - row.left, m_session.scaler) != m_rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackColumn::Layout
//
//  The header, then rows of sector buttons, each row past the first moving
//  the selector down, then the selector, left-justified with equal segments
//  each as wide as the longest label (FR-159).
//
////////////////////////////////////////////////////////////////////////////////

int TrackColumn::Layout (const RECT & area, const DxuiDpiScaler & scaler)
{
    int     width   = area.right - area.left;
    int     header  = scaler.ToPx (TrackHeaderView::kLineDip * TrackHeaderView::kLines);
    int     margin  = scaler.ToPx (InspectorMetrics::kMarginDip);
    int     rowsH   = 0;
    int     segment = scaler.ToPx (s_kMinSegmentDip);
    int     count   = std::max (1, static_cast<int> (m_shown.size()));
    int     y       = area.top;



    m_header->Layout ({ area.left, y, area.right, y + header }, scaler);
    y += header;

    m_rows = m_sectorRow->GetRowCount (width, scaler);
    rowsH  = scaler.ToPx (s_kSectorRowDip) + (m_rows - 1) * scaler.ToPx (SectorRowView::kRowDip + SectorRowView::kGapDip);
    m_sectorRow->Layout ({ area.left, y, area.right, y + rowsH }, scaler);
    y += rowsH;

    for (const View & view : m_shown)
    {
        segment = std::max (segment, scaler.ToPx (static_cast<int> (std::ceil (wcslen (view.label) * InspectorMetrics::kCharWidthDip)) + s_kSegmentPadDip));
    }

    segment = std::min (segment, width / count);
    m_columnPx = { area.left, area.top, area.right, y };

    y += margin / 2;
    m_views->Layout ({ area.left, y, area.left + segment * count, y + scaler.ToPx (s_kSelectorDip) }, scaler);
    y += scaler.ToPx (s_kSelectorDip) + margin / 2;

    return y;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackColumn::GetTip
//
//  Each view's tip under the pointer's segment (FR-059).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TrackColumn::GetTip (POINT pointPx, RECT & outAnchorPx) const
{
    std::wstring  tip;
    int           index = m_views->IsVisible() ? m_views->HitTest (pointPx.x, pointPx.y) : -1;



    if (index >= 0 && index < static_cast<int> (m_shown.size()))
    {
        tip         = m_shown[static_cast<size_t> (index)].tip;
        outAnchorPx = m_views->GetBounds();
    }

    return tip;
}
