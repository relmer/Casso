#include "Pch.h"

#include "Ui/DiskInspector/Components/InspectorTooltips.h"
#include "Ui/DiskInspector/Components/IInspectorComponent.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/Components/PlatterCorners.h"
#include "Ui/DiskInspector/InspectorTip.h"
#include "Ui/DiskInspector/Views/FluxView.h"
#include "Ui/DiskInspector/InspectorTableView.h"
#include "Ui/DiskInspector/PlatterView.h"
#include "Ui/DiskInspector/SectorRowView.h"
#include "Ui/DiskInspector/TrackHeaderView.h"
#include "Ui/DiskInspector/TrackStripView.h"
#include "Ui/DiskInspector/Views/DifferencesView.h"
#include "Ui/DiskInspector/Views/FilesView.h"
#include "Ui/DiskInspector/Views/IssuesView.h"
#include "Ui/DiskInspector/Views/NibblesView.h"
#include "Ui/DiskInspector/Views/SectorsView.h"





static constexpr int  s_kStripTipOffsetDip = 16;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTooltips::InspectorTooltips
//
////////////////////////////////////////////////////////////////////////////////

InspectorTooltips::InspectorTooltips (InspectorSession & session, DxuiTooltip & tooltip, const Sources & sources) :
    m_session (session),
    m_tooltip (tooltip),
    m_sources (sources)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTooltips::Update
//
//  The first view with a tooltip for the point supplies it; over the file
//  name, the tip has the name in full. Over the platter, the strips and the
//  File map grid, the tip appears at once and follows the pointer.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTooltips::Update (POINT pointPx)
{
    std::wstring                          tip;
    RECT                                  anchor    = {};
    int64_t                               now       = static_cast<int64_t> (GetTickCount64());
    vector<DxuiTooltipSwatch>             swatches;
    vector<uint32_t>                      lineColors;
    vector<vector<std::wstring>>          table;
    bool                                  isInstant = false;
    bool                                  isOnStrip = false;
    const std::array<InspectorView *, 13> views     = { m_session.views.platter, m_session.views.strip, m_session.views.stripB, m_session.views.sectorRow, m_session.views.sectors, m_session.views.nibbles, m_session.views.flux, m_session.views.header, m_session.views.image, m_session.views.tracks, m_session.views.issues, m_session.views.diffs, m_session.views.findResults };



    for (InspectorView * view : views)
    {
        if (tip.empty() && view != nullptr && view->IsVisible() && IsInside (view->GetBounds(), pointPx) && !(view == m_session.views.platter && m_sources.corners->Contains (pointPx)) &&
            view->GetTooltip (pointPx, tip, anchor))
        {
            view->GetTooltipSwatches   (pointPx, swatches);
            view->GetTooltipLineColors (pointPx, lineColors);
            isInstant = view == m_session.views.platter || view == m_session.views.strip || view == m_session.views.stripB;
            isOnStrip = view == m_session.views.strip || view == m_session.views.stripB;
        }
    }

    //  While comparing, a strip's tip has both images' nibbles at the cursor.
    if (isOnStrip && m_session.comparison.IsComparing())
    {
        BuildComparedTip (pointPx, tip, swatches, table);
    }

    //  The File map grid's tip appears at once, as the platter's does.
    if (tip.empty() && m_sources.files->GetGridTip (pointPx, tip, anchor, swatches))
    {
        isInstant = true;
    }

    if (tip.empty() && m_session.context.hasDisk)
    {
        tip = GetButtonTip (pointPx, anchor);
    }

    for (IInspectorComponent * component : *m_sources.components)
    {
        tip = tip.empty() ? component->GetTip (pointPx, anchor) : tip;
    }

    if (tip.empty())
    {
        m_tooltip.RequestHide (now);
    }
    else if (isInstant || tip != m_text || (!m_tooltip.IsShown() && !m_tooltip.IsPending()))
    {
        m_tooltip.SetSwatches   (swatches);
        m_tooltip.SetLineColors (lineColors);
        m_tooltip.SetTable      (table);

        //  A strip's tip sits to the right of the pointer, so the cursor
        //  line under it stays in sight.
        m_tooltip.SetPointerOffsetDip (isOnStrip ? s_kStripTipOffsetDip : 0);

        isInstant ? m_tooltip.RequestShowNow (anchor, tip, now) : m_tooltip.RequestShow (anchor, tip, now);
    }

    m_text = tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTooltips::BuildComparedTip
//
//  The lines both strips' tips start with, then a column for each image
//  headed by its file name with the rest of its lines. The swatches of the
//  shared lines are kept. If either strip has no tip, the tip is not changed.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTooltips::BuildComparedTip (POINT pointPx, std::wstring & inOutTip, vector<DxuiTooltipSwatch> & inOutSwatches, vector<vector<std::wstring>> & outTable) const
{
    InspectorTip                  tips[2];
    vector<std::wstring>          lines[2];
    const DiskComparison        * comparison = m_session.comparison.HasResult() ? &m_session.comparison.GetResult() : nullptr;
    SideNames                     names      = (comparison != nullptr) ? comparison->names : SideNames();
    size_t                        shared     = 0;
    size_t                        rows       = 0;
    std::wstring                  text;
    vector<DxuiTooltipSwatch>     kept;



    if (m_session.views.strip == nullptr || m_session.views.stripB == nullptr ||
        !m_session.views.strip->BuildTipAtX (pointPx.x, tips[0]) || !m_session.views.stripB->BuildTipAtX (pointPx.x, tips[1]))
    {
        return;
    }

    for (int side = 0; side < 2; side++)
    {
        std::wstringstream  stream (tips[side].GetText());
        std::wstring        line;

        while (std::getline (stream, line))
        {
            lines[side].push_back (line);
        }
    }

    while (shared < lines[0].size() && shared < lines[1].size() && lines[0][shared] == lines[1][shared])
    {
        shared++;
    }

    for (size_t i = 0; i < shared; i++)
    {
        text += (i > 0 ? L"\n" : L"") + lines[0][i];
    }

    for (const DxuiTooltipSwatch & swatch : inOutSwatches)
    {
        if (swatch.line >= 0 && static_cast<size_t> (swatch.line) < shared)
        {
            kept.push_back (swatch);
        }
    }

    rows = std::max (lines[0].size(), lines[1].size()) - shared;
    outTable.clear();

    if (rows > 0)
    {
        outTable.push_back ({ names.a, names.b });
    }

    for (size_t i = 0; i < rows; i++)
    {
        size_t  at = shared + i;

        outTable.push_back ({ at < lines[0].size() ? lines[0][at] : std::wstring(), at < lines[1].size() ? lines[1][at] : std::wstring() });
    }

    inOutTip      = text.empty() ? std::wstring (L" ") : text;
    inOutSwatches = std::move (kept);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTooltips::GetButtonTip
//
//  Every button's tooltip (FR-059), with the key that does the same, or
//  for one that is unavailable, why.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorTooltips::GetButtonTip (POINT pointPx, RECT & outAnchorPx) const
{
    //  Each control's tip, and for one that can be unavailable, why it is.
    struct ButtonTip
    {
        const IDxuiControl  * control;
        LPCWSTR               text;
        LPCWSTR               whyUnavailable;
    };

    const ButtonTip  tips[] =
    {
        { m_sources.copySector, L"Copy the whole sector as a hex dump (Ctrl+Shift+C)", L"The selected sector has no data field to copy" },
    };


    std::wstring  tip;



    for (const ButtonTip & entry : tips)
    {
        if (tip.empty() && entry.control != nullptr && entry.control->IsVisible() && IsInside (entry.control->GetBounds(), pointPx))
        {
            tip         = (entry.control->IsEnabled() || entry.whyUnavailable == nullptr) ? entry.text : entry.whyUnavailable;
            outAnchorPx = entry.control->GetBounds();
        }
    }

    return tip;
}
