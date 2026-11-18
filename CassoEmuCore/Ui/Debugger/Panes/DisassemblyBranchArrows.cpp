#include "Pch.h"

#include "Ui/Debugger/Panes/DisassemblyBranchArrows.h"
#include "Ui/Debugger/Panes/DisassemblyViewPane.h"
#include "Ui/Debugger/Panes/DisassemblyViews.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"
#include "Ui/Debugger/Panes/SourcePane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyBranchArrows::DisassemblyBranchArrows
//
////////////////////////////////////////////////////////////////////////////////

DisassemblyBranchArrows::DisassemblyBranchArrows (
    const DebuggerPaneParts  & parts,
    DisassemblyGroup     & group,
    SourceDocumentGroup  & sources) :
    m_paneRouting (parts.routing),
    m_paneNavigator (parts.navigator),
    m_latest (parts.latest),
    m_look (parts.look),
    m_group   (group),
    m_sources (sources)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyBranchArrows::GetBranchArrow
//
//  Where the PC's branch, jump or call arrow lies in a view, in pixels, and
//  where it goes, or with isBranchedFrom, the arrow of the one a step just
//  took to the PC. Where both are drawn, they are set apart at the PC's row.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyBranchArrows::GetBranchArrow (int view, BranchArrow::Input & input, Word & goesTo, bool & isTaken, bool isBranchedFrom) const
{
    BranchArrow::Input  other;
    Word                otherGoesTo = 0;
    bool                otherTaken  = true;



    if (!GetOneBranchArrow (view, input, goesTo, isTaken, isBranchedFrom))
    {
        return false;
    }

    if (!GetOneBranchArrow (view, other, otherGoesTo, otherTaken, !isBranchedFrom))
    {
        return true;
    }

    if (isBranchedFrom)
    {
        DisassemblyCells::SeparateArrows (input, other);
    }
    else
    {
        DisassemblyCells::SeparateArrows (other, input);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyBranchArrows::GetOneBranchArrow
//
//  One of the arrows, from the snapshot's addresses, so either end scrolled
//  out of view runs the line to that edge of the rows, and the arrow is
//  drawn while any part of it crosses them. Without the PC's target in the
//  snapshot, as while running, the PC's arrow is drawn from its line. A
//  view past the code views is a source document's, whose pane places the
//  PC's arrow alone.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyBranchArrows::GetOneBranchArrow (int view, BranchArrow::Input & input, Word & goesTo, bool & isTaken, bool isBranchedFrom) const
{
    const DebuggerViewSnapshot   * snapshot = m_latest.Get();
    DxuiListView                 * list     = (view < DebuggerViewState::kMaxCodeViews) ? m_group.GetCodeList (view) : nullptr;
    DisassemblyCells::ArrowEnds    ends;



    if (!m_group.GetOptions().IsOn (DisassemblyOptions::Option::BranchArrow))
    {
        return false;
    }

    if (view >= DebuggerViewState::kMaxCodeViews)
    {
        const SourceDocumentPane & document = m_sources.GetDocument (view - DebuggerViewState::kMaxCodeViews);

        return !isBranchedFrom && document.IsShown() && m_paneRouting.IsRoutable (document.GetView()) && document.GetModel().GetBranchArrow (input, goesTo, isTaken);
    }

    if (list == nullptr || !m_group.IsOpen (view) || !list->IsVisible() || !m_paneRouting.IsRoutable (list))
    {
        return false;
    }

    if (isBranchedFrom)
    {
        if (snapshot == nullptr || !snapshot->branchedFrom.has_value())
        {
            return false;
        }

        ends = { *snapshot->branchedFrom, snapshot->pc, true };
    }
    else if (snapshot != nullptr && snapshot->pcTarget.has_value())
    {
        ends = { snapshot->pc, *snapshot->pcTarget, snapshot->isPcTargetTaken };
    }
    else if (!DisassemblyCells::TryGetPcArrowEnds (m_group.GetCodeLines (view), ends))
    {
        return false;
    }

    goesTo  = ends.goesTo;
    isTaken = ends.isTaken;

    return DisassemblyCells::TryGetBranchArrow (*list, m_group.GetCodeLines (view), m_group.GetView (view).GetRows(), ends, input);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyBranchArrows::PaintBranchArrow
//
//  The arrow a step just took, dotted, and the PC's, in the PC marker's
//  color, or in the disabled color for a branch that the current flags do
//  not take.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyBranchArrows::PaintBranchArrow (IDxuiPainter & painter, int view)
{
    const DxuiTheme     * theme   = m_look.GetTheme();
    BranchArrow::Input    input;
    Word                  goesTo  = 0;
    bool                  isTaken = true;



    for (bool isBranchedFrom : { true, false })
    {
        input = BranchArrow::Input();

        if (GetBranchArrow (view, input, goesTo, isTaken, isBranchedFrom))
        {
            DisassemblyCells::PaintBranchArrow (painter, input, isTaken, m_look.GetPcMarkerArgb(), (theme != nullptr) ? theme->ForegroundDisabled() : 0xFF808080,
                                                isBranchedFrom);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyBranchArrows::ClickBranchArrow
//
//  A click on the PC's branch arrow goes to where it points, as Go to
//  would, and a click on the dotted arrow goes to the branch taken by the
//  last step; the next click on either goes back to the PC. In a source
//  document the document scrolls, rather than the disassembly coming to the
//  front.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyBranchArrows::ClickBranchArrow (POINT pointPx)
{
    const DebuggerViewSnapshot  * snapshot = m_latest.Get();
    BranchArrow::Input            input;
    Word                          goesTo   = 0;
    bool                          isTaken  = true;



    //  The dotted arrow is hit-tested first, so a click where it lies over
    //  the PC's arrow moves the view to the branch taken.
    for (bool isBranchedFrom : { true, false })
    {
        for (int view = 0; view < DebuggerViewState::kMaxCodeViews + SourceDocuments::kMaxDocuments; view++)
        {
            input = BranchArrow::Input();

            if (!GetBranchArrow (view, input, goesTo, isTaken, isBranchedFrom) ||
                !BranchArrow::HitTest (input, (float) pointPx.x, (float) pointPx.y))
            {
                continue;
            }

            if (view >= DebuggerViewState::kMaxCodeViews)
            {
                ClickSourceArrow (m_sources.GetDocument (view - DebuggerViewState::kMaxCodeViews).GetModel());
                return true;
            }

            m_group.SetActive (view);

            if (m_isArrowAtTarget)
            {
                m_paneNavigator.GoToCode (std::nullopt);
            }
            else
            {
                m_paneNavigator.GoToCode ((isBranchedFrom && snapshot != nullptr && snapshot->branchedFrom.has_value()) ? *snapshot->branchedFrom : goesTo);
            }

            m_isArrowAtTarget = !m_isArrowAtTarget;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyBranchArrows::ClickSourceArrow
//
//  The document scrolls to the line the PC's branch goes to, and the next
//  click scrolls back to the PC's line.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyBranchArrows::ClickSourceArrow (SourcePane & document)
{
    if (m_isArrowAtTarget)
    {
        document.ShowMarkedLine();
        m_isArrowAtTarget = false;
    }
    else
    {
        //  A target in no line of this file doesn't scroll the document, so
        //  the next click tries it again.
        m_isArrowAtTarget = document.ShowArrowTarget();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyBranchArrows::TryGetBranchTip
//
//  Over the PC's arrow, whether its branch will be taken, and why, from the
//  instruction at the PC and the flags now; over the dotted arrow, the
//  branch the last step took to the PC.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyBranchArrows::TryGetBranchTip (POINT clientPx, RECT & anchor, std::wstring & text) const
{
    const DebuggerViewSnapshot  * snapshot    = m_latest.Get();
    BranchArrow::Input            input;
    Word                          goesTo      = 0;
    bool                          isTaken     = true;
    std::string                   instruction;
    std::string                   stepped;



    if (snapshot == nullptr)
    {
        return false;
    }

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        for (const DebuggerViewSnapshot::CodeLine & line : m_group.GetCodeLines (view))
        {
            instruction = line.isCurrent ? line.instruction : instruction;
            stepped     = (snapshot->branchedFrom == line.address) ? line.instruction : stepped;
        }
    }

    //  The PC's arrow is drawn over the dotted one, so it is found first.
    for (bool isBranchedFrom : { false, true })
    {
        for (int view = 0; view < DebuggerViewState::kMaxCodeViews + SourceDocuments::kMaxDocuments; view++)
        {
            input = BranchArrow::Input();

            if (!GetBranchArrow (view, input, goesTo, isTaken, isBranchedFrom) ||
                !BranchArrow::HitTest (input, (float) clientPx.x, (float) clientPx.y))
            {
                continue;
            }

            anchor = RECT { clientPx.x, clientPx.y, clientPx.x + 1, clientPx.y + 1 };
            text   = isBranchedFrom ? ColorLegend::GetTakenBranchTip (stepped) : ColorLegend::GetBranchTip (instruction, isTaken);
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyBranchArrows::IsOverArrowsOrGutter
//
//  In a view, the lanes span the arrow's distance left of the mnemonics,
//  with a second arrow a lane further out.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyBranchArrows::IsOverArrowsOrGutter (POINT clientPx) const
{
    const float      kLanesDip = BranchArrow::Input().marginPx + BranchArrow::Input().stubPx + DisassemblyCells::kArrowLaneDip;
    int              view      = -1;
    DxuiListView   * list      = m_group.FindCodeListAt (clientPx, view);
    RECT             bounds    = {};
    RECT             cell      = {};
    int              row       = -1;
    int              x         = 0;
    float            scale     = 1.0f;



    for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        const SourceDocumentPane  & document = m_sources.GetDocument (slot);

        if (document.IsShown() && document.GetView()->IsVisible() && m_paneRouting.IsRoutable (document.GetView()) &&
            (document.GetView()->IsInGutter (clientPx) || document.GetModel().IsOverArrowLanes (clientPx)))
        {
            return true;
        }
    }

    if (list == nullptr)
    {
        return false;
    }

    bounds = list->GetBounds();
    x      = clientPx.x - bounds.left;
    row    = list->HitTestRow (x, clientPx.y - bounds.top);

    if (row < 0)
    {
        return false;
    }

    if (x + list->GetLeftPx() < list->GetPaneLeadPx() + list->GetColumnEffectiveWidthPx (DisassemblyCells::kGutterColumn))
    {
        return true;
    }

    if (!list->GetCellTextRectPx (row, DisassemblyCells::kInstructionColumn, cell))
    {
        return false;
    }

    scale = (float) (cell.bottom - cell.top) / (float) (std::max) (1, list->GetRowHeightDip());

    return (float) x >= (float) cell.left - kLanesDip * scale && x < cell.left;
}
