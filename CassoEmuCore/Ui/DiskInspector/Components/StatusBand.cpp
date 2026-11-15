#include "Pch.h"

#include "Ui/DiskInspector/Components/StatusBand.h"
#include "Ui/DiskInspector/ComparisonText.h"
#include "Ui/DiskInspector/Components/IInspectorComponentHost.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/InspectorText.h"





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
//  The band's left end in order: the comparison's result while comparing
//  (FR-121), the summary chips (FR-018), a chip for custom decode settings,
//  "Analyzing" while analysis is pending, and the selection.
//
////////////////////////////////////////////////////////////////////////////////

vector<InspectorStatusBar::LeadItem> StatusBand::BuildLead() const
{
    using LeadKind = InspectorStatusBar::LeadKind;

    vector<InspectorStatusBar::LeadItem>  items;
    std::wstring                          selection = GetSelectionLine();



    if (m_session.context.hasDisk)
    {
        if (m_session.comparison.IsComparing())
        {
            items.push_back ({ (m_session.comparison.IsBusy() || !m_session.comparison.HasResult()) ? std::wstring (L"Comparing")
                                                                                                   : ComparisonText::FormatResult (m_session.comparison.GetResult()),
                               LeadKind::Result });
        }

        for (const SummaryChip & chip : InspectorText::BuildChips (m_session.analysis.summary))
        {
            items.push_back ({ chip.text, chip.isBad ? LeadKind::BadChip : LeadKind::Chip });
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
//  A press on "?" opens the walkthrough (FR-153).
//
////////////////////////////////////////////////////////////////////////////////

bool StatusBand::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isPress = ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && m_bar.IsOverHelp (ev.positionDip);



    if (isPress)
    {
        m_host.OpenWalkthrough();
    }

    return isPress;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StatusBand::GetTip
//
////////////////////////////////////////////////////////////////////////////////

std::wstring StatusBand::GetTip (POINT pointPx, RECT & outAnchorPx) const
{
    std::wstring  tip;



    if (m_bar.IsOverHelp (pointPx))
    {
        tip         = L"Open the walkthrough";
        outAnchorPx = m_bar.GetHelpRect();
    }

    return tip;
}
