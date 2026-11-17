#include "Pch.h"

#include "Ui/DiskInspector/Components/StatusBand.h"
#include "Ui/DiskInspector/ComparisonText.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"





static constexpr LPCWSTR  s_kpszAnalyzing = L"Analyzing";





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::StatusBand
//
////////////////////////////////////////////////////////////////////////////////

StatusBand::StatusBand (InspectorSession & session, IInspectorComponentHost & host) :
    m_session (session),
    m_host    (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::Create
//
////////////////////////////////////////////////////////////////////////////////

void StatusBand::Create()
{
    m_bar.Create (m_host.GetControlParent(), [this] (double zoom) { m_host.SetWindowZoom (zoom); }, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::Tick
//
////////////////////////////////////////////////////////////////////////////////

void StatusBand::Tick (int64_t nowMs)
{
    (void) nowMs;

    m_bar.Tick (m_host.GetOwnerHwnd());
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::PaintUnder
//
////////////////////////////////////////////////////////////////////////////////

void StatusBand::PaintUnder (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    m_bar.Paint (painter, text, theme, BuildLead(), m_session.context.palette.colors.sectorBad);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::BuildLead
//
//  The band's left end in order: the count of unsaved sectors, the
//  comparison's result while comparing (FR-121), the summary chips
//  (FR-018), a chip for custom decode settings, "Analyzing" while analysis
//  is pending, and the selection.
//
////////////////////////////////////////////////////////////////////////////////

vector<InspectorStatusBar::LeadItem> StatusBand::BuildLead() const
{
    using LeadKind = InspectorStatusBar::LeadKind;

    vector<InspectorStatusBar::LeadItem>  items;
    std::wstring                          selection = GetSelectionLine();
    int                                   unsaved   = m_session.edits.GetCounts().sectors;



    if (unsaved > 0)
    {
        items.push_back ({ InspectorText::FormatCount (unsaved, L"unsaved sector", L"unsaved sectors"), LeadKind::Result });
    }

    if (m_session.context.hasDisk)
    {
        if (m_session.comparison.IsComparing())
        {
            items.push_back ({ (m_session.comparison.IsBusy() || !m_session.comparison.HasResult()) ? std::wstring (L"Comparing")
                                                                                                   : ComparisonText::FormatResult (m_session.comparison.GetResult()),
                               LeadKind::Result });
        }

        for (const SummaryChip & chip : InspectorText::BuildChips (m_session.analysis.summary, m_session.analysis.verdict))
        {
            items.push_back ({ chip.text, chip.isBad ? LeadKind::BadChip : LeadKind::Chip });
            items.back().isLinked = chip.kind == SummaryChipKind::FileSystem && FileSystemCheck::GetVerdictFinding (m_session.analysis) != nullptr;
        }

        if (!m_session.analysis.settings.IsStandard())
        {
            items.push_back ({ L"Custom decode settings", LeadKind::Chip });
        }
    }

    if (m_session.pendingRequest != 0 || m_session.scheduler->HasPending())
    {
        items.push_back ({ s_kpszAnalyzing, LeadKind::Note });
    }

    if (!selection.empty())
    {
        items.push_back ({ selection, LeadKind::Selection });
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::GetSelectionLine
//
//  What is selected, or nothing with no disk.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring StatusBand::GetSelectionLine() const
{
    const InspectorViewModel &  model = m_session.model;



    return m_session.context.hasDisk ? InspectorText::FormatSelection (model.GetTrack(), model.GetQuarterTrack(), model.GetSectorIndex(), model.GetFirstNibble(),
                                                                       model.GetNibbleCount(), m_session.GetFileMap())
                                     : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::OnMouse
//
//  A press on "?" opens the walkthrough, and one on the file-system chip of
//  a Bad or Suspect volume opens its finding on the Issues tab.
//
////////////////////////////////////////////////////////////////////////////////

bool StatusBand::OnMouse (const DxuiMouseEvent & ev)
{
    bool                                   isDown  = ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left;
    bool                                   isHot   = ev.kind != DxuiMouseEventKind::Leave && m_bar.GetLinkedChipAt (ev.positionDip) != nullptr;
    bool                                   isHelp  = isDown && m_bar.IsOverHelp (ev.positionDip);
    const InspectorStatusBar::LeadItem   * chip    = isDown ? m_bar.GetLinkedChipAt (ev.positionDip) : nullptr;
    const Finding                        * finding = (chip != nullptr) ? FileSystemCheck::GetVerdictFinding (m_session.analysis) : nullptr;



    m_bar.SetLinkHot (isHot);

    if (isHelp)
    {
        m_host.OpenWalkthrough();
    }

    if (finding != nullptr)
    {
        m_host.ShowFinding (static_cast<int> (finding - m_session.analysis.findings.data()));
    }

    return isHelp || chip != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::GetTip
//
////////////////////////////////////////////////////////////////////////////////

std::wstring StatusBand::GetTip (POINT pointPx, RECT & outAnchorPx) const
{
    std::wstring  tip;



    const InspectorStatusBar::LeadItem  * chip = m_bar.GetLinkedChipAt (pointPx);



    if (m_bar.IsOverHelp (pointPx))
    {
        tip         = L"Open the walkthrough";
        outAnchorPx = m_bar.GetHelpRect();
    }
    else if (chip != nullptr)
    {
        tip         = L"Go to this issue on the Issues tab";
        outAnchorPx = RECT { pointPx.x, pointPx.y, pointPx.x + 1, pointPx.y + 1 };
    }

    return tip;
}
