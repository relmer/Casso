#include "Pch.h"

#include "Ui/Debugger/Panes/BasicProgramController.h"

#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/Panes/BasicPanes.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::BasicProgramController
//
////////////////////////////////////////////////////////////////////////////////

BasicProgramController::BasicProgramController (IDebuggerPaneHost & host) :
    IDebuggerPane (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::~BasicProgramController
//
////////////////////////////////////////////////////////////////////////////////

BasicProgramController::~BasicProgramController() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::GetId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BasicProgramController::GetId() const
{
    return DebuggerLayout::kBasicProgram;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::CreateControls
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgramController::CreateControls()
{
    m_list  = m_host.GetControlParent().CreateChild<DxuiListView>();
    m_model = std::make_unique<BasicProgramPane> (m_list, [this] (Word line) { ToggleBreakpoint (line); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::Configure
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgramController::Configure()
{
    m_model->Configure (DisassemblyCells::kGutterColumnDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::AddToDock
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgramController::AddToDock (DxuiDockSite & dock)
{
    m_frame = std::make_unique<DebuggerPaneFrame> (L"BASIC program");
    m_frame->AddPart (m_list);

    dock.AddPane (DebuggerLayout::kBasicProgram, L"BASIC program", m_frame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::GetContent
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * BasicProgramController::GetContent() const
{
    return m_frame.get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::Apply
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgramController::Apply (const DebuggerViewSnapshot & snapshot)
{
    Follow    (snapshot);
    ApplyRows (snapshot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::Follow
//
//  The pane comes forward at the line when the machine stops in the
//  interpreter while a program runs, and at a BASIC line breakpoint's line
//  when one is set. At a stop in machine code the program called, the code
//  view stays in front.
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgramController::Follow (const DebuggerViewSnapshot & snapshot)
{
    static constexpr Word                                    kInterpreterFirst = 0xD000;
    static constexpr Word                                    kInterpreterLast  = 0xF7FF;
    const std::optional<DebuggerViewSnapshot::BasicState>  & basic             = snapshot.basic;
    bool                                                     isStop            = snapshot.isPaused && !m_wasPaused;
    std::set<int>                                            ids;
    std::optional<Word>                                      line;



    m_wasPaused = snapshot.isPaused;
    m_isStop    = isStop && basic.has_value() && basic->line.has_value() && snapshot.pc >= kInterpreterFirst && snapshot.pc <= kInterpreterLast;

    if (m_isStop)
    {
        line = basic->line;
    }

    for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot.breakpoints)
    {
        if (bp.info.kind != BreakpointKind::BasicLine)
        {
            continue;
        }

        ids.insert (bp.id);

        if (m_breakpointIds.has_value() && !m_breakpointIds->contains (bp.id))
        {
            line = bp.info.address;
        }
    }

    m_breakpointIds = std::move (ids);

    if (line.has_value())
    {
        ShowLine (*line);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::ApplyRows
//
//  The pane follows the line being run, a new line scrolled into view.
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgramController::ApplyRows (const DebuggerViewSnapshot & snapshot)
{
    const std::optional<DebuggerViewSnapshot::BasicState>  & basic = snapshot.basic;
    BasicProgramPane::Style                                  style;



    if (!m_isReading)
    {
        return;
    }

    style.pcIcon           = m_host.GetPcMarker (nullptr);
    style.enabledIcon      = m_host.GetBreakpointIcon (true);
    style.disabledIcon     = m_host.GetBreakpointIcon (false);
    style.pcOnEnabledIcon  = m_host.GetPcMarker (style.enabledIcon);
    style.pcOnDisabledIcon = m_host.GetPcMarker (style.disabledIcon);
    style.pcRowArgb        = m_host.GetPcRowArgb();
    style.syntax           = m_host.GetSyntaxColors();

    m_model->SetStyle (style);
    m_model->SetHeat  (snapshot.heatColumns, m_host.IsCodeHeatOn() ? std::optional<HeatMapView::Palette> (m_host.GetHeatPalette()) : std::nullopt);
    m_model->Apply    (basic, snapshot.breakpoints);

    if (m_pendingLine.has_value() && basic.has_value() && basic->hasProgram)
    {
        m_model->ShowLine (*m_pendingLine);
        m_pendingLine.reset();
    }

    if (basic.has_value() && basic->line != m_shownLine)
    {
        m_shownLine = basic->line;

        if (m_shownLine.has_value())
        {
            m_model->ShowLine (*m_shownLine);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::Tick
//
//  Whether the machine's thread reads the program follows whether the pane
//  is open and on screen; each change is sent to the host.
//
////////////////////////////////////////////////////////////////////////////////

bool BasicProgramController::Tick (int64_t nowMs)
{
    IDebuggerWindowHost  * host     = m_host.GetWindowHost();
    bool                   isWanted = m_host.IsPaneOpen (DebuggerLayout::kBasicProgram) && m_list->IsVisible();



    (void) nowMs;

    if (isWanted == m_isReading || host == nullptr)
    {
        return false;
    }

    m_isReading = isWanted;
    m_shownLine.reset();
    host->SetDebuggerBasicShown (isWanted);
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::ToggleBreakpoint
//
//  A line breakpoint there is cleared, or one is set, as one step the
//  Breakpoints pane can undo.
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgramController::ToggleBreakpoint (Word line)
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
    DebuggerAction                action;
    std::optional<int>            id;



    if (snapshot == nullptr)
    {
        return;
    }

    for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot->breakpoints)
    {
        id = (bp.info.kind == BreakpointKind::BasicLine && bp.info.address == line) ? std::optional<int> (bp.id) : id;
    }

    action = id.has_value() ? DebuggerActions::GetClearBreakpoint (*id, m_host.GetMode())
                            : DebuggerActions::GetDefineBreakpoint (std::format ("BPBASIC {}", line), m_host.GetMode());

    m_host.RunAction (DebuggerActions::GetBreakpointStep ({ BreakpointStep::Kind::Actions, { action }, {} }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::ShowLine
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgramController::ShowLine (Word line)
{
    m_host.OpenPane (DebuggerLayout::kBasicProgram);
    m_model->ShowLine (line);
    m_pendingLine = line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::OnMappedAction
//
//  With the keys in the list a step is the interpreter's (FR-174): step into
//  runs to the next statement, step over to the next line, and step out to
//  the statement after the newest GOSUB returns.
//
////////////////////////////////////////////////////////////////////////////////

bool BasicProgramController::OnMappedAction (DebuggerKeySchemes::Action action, IDxuiControl * focused)
{
    std::optional<DebugVerb>  verb;



    if (focused != m_list)
    {
        return false;
    }

    switch (action)
    {
    case DebuggerKeySchemes::Action::StepInto: verb = DebugVerb::BasicStep;     break;
    case DebuggerKeySchemes::Action::StepOver: verb = DebugVerb::BasicStepLine; break;
    case DebuggerKeySchemes::Action::StepOut:  verb = DebugVerb::BasicStepOut;  break;
    default:                                                                    break;
    }

    if (!verb.has_value())
    {
        return false;
    }

    m_host.RunAction (DebuggerActions::GetBasicStep (*verb, m_host.GetMode()));
    return true;
}
