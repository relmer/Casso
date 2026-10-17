#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"

#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/GutterGlyph.h"
#include "Ui/Debugger/Panes/BasicPanes.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::CreateBasicPanes
//
//  The Applesoft program (FR-173), a list. Its variables are in the Watches
//  pane and its frames in the Call stack pane.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::CreateBasicPanes()
{
    m_basicProgramList = CreateChild<DxuiListView>();
    m_basicProgramPane = std::make_unique<BasicProgramPane> (m_basicProgramList, [this] (Word line) { ToggleBasicBreakpoint (line); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ConfigureBasicPanes
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ConfigureBasicPanes()
{
    m_basicProgramPane->Configure (kGutterColumnDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::AddBasicPanes
//
//  The list alone, with no toolbar: the program's breakpoints are set by
//  activating a row, and the steps are the window's own.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::AddBasicPanes()
{
    m_basicProgramFrame = std::make_unique<DebuggerPaneFrame> (L"BASIC program");
    m_basicProgramFrame->AddPart (m_basicProgramList);
    m_dockSite->AddPane (DebuggerLayout::kBasicProgram, L"BASIC program", m_basicProgramFrame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetBasicFrame
//
////////////////////////////////////////////////////////////////////////////////

DebuggerPaneFrame * DebuggerWindow::GetBasicFrame (const std::wstring & pane) const
{
    return (pane == DebuggerLayout::kBasicProgram) ? m_basicProgramFrame.get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetBasicList
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView * DebuggerWindow::GetBasicList (const std::wstring & pane) const
{
    return (pane == DebuggerLayout::kBasicProgram) ? m_basicProgramList : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ApplyBasicPanes
//
//  The program pane follows the line being run, a new line scrolled into
//  view.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ApplyBasicPanes()
{
    const std::optional<DebuggerViewSnapshot::BasicState>  & basic = m_snapshot->basic;
    BasicProgramPane::Style                                  style;



    if (!m_isBasicReading)
    {
        return;
    }

    style.pcIcon           = GetPcIcon (nullptr);
    style.enabledIcon      = GetBreakpointIcon (true);
    style.disabledIcon     = GetBreakpointIcon (false);
    style.pcOnEnabledIcon  = GetPcIcon (style.enabledIcon);
    style.pcOnDisabledIcon = GetPcIcon (style.disabledIcon);
    style.pcRowArgb        = GetPcRowArgb();
    style.syntax           = GetSyntaxColors();

    m_basicProgramPane->SetStyle (style);
    m_basicProgramPane->Apply    (basic, m_snapshot->breakpoints);

    if (m_basicPendingLine.has_value() && basic.has_value() && basic->hasProgram)
    {
        m_basicProgramPane->ShowLine (*m_basicPendingLine);
        m_basicPendingLine.reset();
    }

    if (basic.has_value() && basic->line != m_basicShownLine)
    {
        m_basicShownLine = basic->line;

        if (m_basicShownLine.has_value())
        {
            m_basicProgramPane->ShowLine (*m_basicShownLine);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::FollowBasicProgram
//
//  The program pane comes forward at the line when the machine stops in the
//  interpreter while a program runs, and at a BASIC line breakpoint's line
//  when one is set. At a stop in machine code the program called, the code
//  view stays in front.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::FollowBasicProgram()
{
    static constexpr Word  kInterpreterFirst = 0xD000;
    static constexpr Word  kInterpreterLast  = 0xF7FF;
    const std::optional<DebuggerViewSnapshot::BasicState>  & basic   = m_snapshot->basic;
    bool                                                     isStop  = m_snapshot->isPaused && !m_wasBasicPaused;
    std::set<int>                                            ids;
    std::optional<Word>                                      line;



    m_wasBasicPaused = m_snapshot->isPaused;

    if (isStop && basic.has_value() && basic->line.has_value() && m_snapshot->pc >= kInterpreterFirst && m_snapshot->pc <= kInterpreterLast)
    {
        line = basic->line;
    }

    for (const DebuggerViewSnapshot::BreakpointLine & bp : m_snapshot->breakpoints)
    {
        if (bp.info.kind != BreakpointKind::BasicLine)
        {
            continue;
        }

        ids.insert (bp.id);

        if (m_basicBreakpointIds.has_value() && !m_basicBreakpointIds->contains (bp.id))
        {
            line = bp.info.address;
        }
    }

    m_basicBreakpointIds = std::move (ids);

    if (line.has_value())
    {
        ShowBasicLine (*line);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SyncBasicReading
//
//  While the machine runs, its thread reads the program for the snapshots
//  only while the program pane is open and on screen; each change is sent
//  to the host.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SyncBasicReading()
{
    bool  isWanted = IsPaneShown (DebuggerLayout::kBasicProgram) && m_basicProgramList->IsVisible();


    if (isWanted == m_isBasicReading || m_host == nullptr)
    {
        return;
    }

    m_isBasicReading = isWanted;
    m_basicShownLine.reset();
    m_host->SetDebuggerBasicShown (isWanted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ToggleBasicBreakpoint
//
//  A line breakpoint there is cleared, or one is set, as one step the
//  Breakpoints pane can undo.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ToggleBasicBreakpoint (Word line)
{
    DebuggerAction      action;
    std::optional<int>  id;



    if (m_snapshot == nullptr)
    {
        return;
    }

    for (const DebuggerViewSnapshot::BreakpointLine & bp : m_snapshot->breakpoints)
    {
        id = (bp.info.kind == BreakpointKind::BasicLine && bp.info.address == line) ? std::optional<int> (bp.id) : id;
    }

    action = id.has_value() ? DebuggerActions::GetClearBreakpoint (*id, GetMode())
                            : DebuggerActions::GetDefineBreakpoint (std::format ("BPBASIC {}", line), GetMode());

    RunBreakpointStep ({ BreakpointStep::Kind::Actions, { action }, {} });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ShowBasicLine
//
//  The line, brought forward in the program pane, and scrolled to again once
//  the pane has the program.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::ShowBasicLine (Word line)
{
    ShowPane (DebuggerLayout::kBasicProgram);
    m_basicProgramPane->ShowLine (line);
    m_basicPendingLine = line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetBasicStep
//
//  With the keys in the program pane a step is the interpreter's (FR-174):
//  step into runs to the next statement, step over to the next line, and
//  step out to the statement after the newest GOSUB returns.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebuggerAction> DebuggerWindow::GetBasicStep (DebuggerKeySchemes::Action action) const
{
    using Action = DebuggerKeySchemes::Action;



    if (GetFocused() != m_basicProgramList)
    {
        return std::nullopt;
    }

    switch (action)
    {
    case Action::StepInto: return DebuggerActions::GetBasicStep (DebugVerb::BasicStep,     GetMode());
    case Action::StepOver: return DebuggerActions::GetBasicStep (DebugVerb::BasicStepLine, GetMode());
    case Action::StepOut:  return DebuggerActions::GetBasicStep (DebugVerb::BasicStepOut,  GetMode());
    default:               return std::nullopt;
    }
}
