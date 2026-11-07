#include "Pch.h"

#include "Ui/Debugger/Panes/BasicProgramController.h"

#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/CodeMargins.h"
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
    IDebuggerPane (host),
    m_bar         (host, DebuggerLayout::kBasicProgram, L"BASIC program toolbar")
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

const std::wstring & BasicProgramController::GetId() const
{
    static const std::wstring  s_kId = DebuggerLayout::kBasicProgram;



    return s_kId;
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

    m_bar.Create();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::Configure
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgramController::Configure()
{
    m_model->Configure (CodeMargins::kGlyphDip);
    m_list->SetOwnerWindow (m_host.GetOwnerHwnd());
    m_list->SetOnPaste     ([this] { m_host.PasteBasicProgram(); });

    m_bar.Configure();
    m_bar.GetBar()->SetEntries (MakeBarEntries());
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::MakeBarEntries
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbar::Entry> BasicProgramController::MakeBarEntries() const
{
    struct Row
    {
        const wchar_t          * label;
        const wchar_t          * glyph;
        const wchar_t          * tip;
        std::function<void()>    dispatch;
    };

    std::vector<Row>                 rows    =
    {
        { L"Load",  s_kpszMdl2OpenFile, L"Load an Applesoft listing from a file as the program in memory", [this] { m_host.OpenBasicProgram(); } },
        { L"Save",  s_kpszMdl2Save,     L"Save the program in memory to a file as a listing",              [this] { m_host.SaveBasicProgram(); } },
        { L"Clear", s_kpszMdl2Delete,   L"Clear the program in memory, as NEW does",                       [this] { m_host.ClearBasicProgram(); } },
    };
    std::vector<DxuiToolbar::Entry>  entries;



    for (Row & row : rows)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        DxuiToolbar::Entry            entry;

        command->id       = (int) entries.size() + 1;
        command->label    = row.label;
        command->glyph    = row.glyph;
        command->tip      = row.tip;
        command->dispatch = std::move (row.dispatch);

        entry.command  = command;
        entry.kind     = DxuiToolbar::Kind::Command;
        entry.iconOnly = true;
        entries.push_back (std::move (entry));
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::AddToDock
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgramController::AddToDock (DxuiDockSite & dock)
{
    m_frame = std::make_unique<DebuggerPaneFrame> (L"BASIC program");
    m_frame->AddPart (m_bar.GetSlot(), PaneBarSlot::GetHeightPx);
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
//  view stays in front. An instruction step or a run to the cursor doesn't
//  change which pane is in front, wherever it stops, and a breakpoint on an
//  address brings the code view forward over this pane.
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
    m_isStop    = isStop && !IsMachineStop (snapshot) && basic.has_value() && basic->line.has_value() &&
                  snapshot.pc >= kInterpreterFirst && snapshot.pc <= kInterpreterLast;

    if (m_isStop)
    {
        line = basic->line;
    }

    if (isStop && snapshot.stopReason != StopReason::Step && snapshot.stopReason != StopReason::RunTo && IsMachineStop (snapshot) && m_list->IsVisible())
    {
        m_host.OpenPane (DebuggerLayout::GetCodePaneId (snapshot.followView));
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
//  BasicProgramController::IsMachineStop
//
//  Whether the machine stopped for something at the instruction level, not
//  for a BASIC step, a BASIC error, a BASIC line's breakpoint or a pause.
//
////////////////////////////////////////////////////////////////////////////////

bool BasicProgramController::IsMachineStop (const DebuggerViewSnapshot & snapshot)
{
    auto  isBasicLine = [&snapshot] (const DebuggerViewSnapshot::BreakpointLine & bp)
    {
        return bp.id == snapshot.stopBreakpoint && bp.info.kind == BreakpointKind::BasicLine;
    };



    switch (snapshot.stopReason)
    {
        case StopReason::Breakpoint:
            return std::ranges::none_of (snapshot.breakpoints, isBasicLine);

        case StopReason::Watchpoint:
        case StopReason::Step:
        case StopReason::RunTo:
        case StopReason::Brk:
        case StopReason::InvalidOpcode:
        case StopReason::Disk:
            return true;

        default:
            return false;
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
    style.hoverIcon        = m_host.GetHoverBreakpointIcon();
    style.hoverRow         = m_model->GetStyle().hoverRow;
    style.pcRowArgb        = m_host.GetPcRowArgb();
    style.textArgb         = (m_host.GetTheme() != nullptr) ? m_host.GetTheme()->Foreground() : 0;
    style.syntax           = m_host.GetSyntaxColors();

    (void) UpdateWrap();

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
//  BasicProgramController::UpdateWrap
//
//  The listing wraps at two thirds of the list's width past the gutter and
//  the heat column, with the rest for the values, measured in the
//  list's face. True when the width changed.
//
////////////////////////////////////////////////////////////////////////////////

bool BasicProgramController::UpdateWrap()
{
    constexpr float        kListingShare = 2.0f / 3.0f;
    IDxuiTextRenderer    * text          = m_host.GetPaneTextRenderer();
    const DxuiDpiScaler  & scaler        = m_host.GetPaneScaler();
    const RECT           & bounds        = m_list->GetBounds();
    float                  fontPx        = scaler.ToPxf (m_list->GetFontSizeDip());
    const wchar_t        * face          = m_list->GetFace();
    float                  pads          = scaler.ToPxf ((float) (m_list->GetCellPadLeftDip() + m_list->GetCellPadRightDip()));
    float                  widthPx       = 0.0f;



    widthPx = (float) (bounds.right - bounds.left) - (float) m_list->GetColumnEffectiveWidthPx (BasicProgramPane::kGutterColumn) -
              (float) m_list->GetColumnEffectiveWidthPx (BasicProgramPane::kHeatColumn);
    widthPx = std::floor (std::max (0.0f, widthPx * kListingShare - pads));

    if (text == nullptr || widthPx <= 0.0f || widthPx == m_model->GetWrapPx())
    {
        return false;
    }

    m_model->SetWrap (widthPx, [text, fontPx, face] (const std::wstring & run)
    {
        float    w  = 0.0f;
        float    h  = 0.0f;
        HRESULT  hr = text->MeasureString (run.c_str(), fontPx, face, w, h);



        IGNORE_RETURN_VALUE (hr, S_OK);
        return w;
    });

    return true;
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
    bool                   isFading = m_isReading && m_model->Tick (nowMs);



    //  A new width wraps the listing again at once, with no snapshot to wait for.
    if (m_isReading && UpdateWrap() && m_host.GetLatestSnapshot() != nullptr)
    {
        ApplyRows (*m_host.GetLatestSnapshot());
    }

    if (isWanted == m_isReading || host == nullptr)
    {
        return isFading;
    }

    m_isReading = isWanted;
    m_shownLine.reset();
    host->SetDebuggerBasicShown (isWanted);
    return isFading;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::RouteEarlyMouse
//
//  The listing's gutter, ahead of the list, which would take a press there
//  as the start of a text selection.
//
////////////////////////////////////////////////////////////////////////////////

bool BasicProgramController::RouteEarlyMouse (const DxuiMouseEvent & ev)
{
    return m_list != nullptr && m_host.IsRoutable (m_list) && m_list->IsVisible() && m_model->OnGutterMouse (ev);
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
    std::optional<int>  id = FindBreakpoint (line);
    DebuggerAction      action;



    if (m_host.GetLatestSnapshot() == nullptr)
    {
        return;
    }

    action = id.has_value() ? DebuggerActions::GetClearBreakpoint (*id, m_host.GetMode())
                            : DebuggerActions::GetDefineBreakpoint (std::format ("BPBASIC {}", line), m_host.GetMode());

    m_host.RunAction (DebuggerActions::GetBreakpointStep ({ BreakpointStep::Kind::Actions, { action }, {} }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::FindBreakpoint
//
//  The id of the breakpoint on a line of the program.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<int> BasicProgramController::FindBreakpoint (Word line) const
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
    std::optional<int>            id;



    if (snapshot == nullptr)
    {
        return id;
    }

    for (const DebuggerViewSnapshot::BreakpointLine & bp : snapshot->breakpoints)
    {
        id = (bp.info.kind == BreakpointKind::BasicLine && bp.info.address == line) ? std::optional<int> (bp.id) : id;
    }

    return id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController::AddMenuItems
//
//  Copy for the text selected or the rows, the line's breakpoint, and the
//  bar's Load and Save.
//
////////////////////////////////////////////////////////////////////////////////

bool BasicProgramController::AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items)
{
    std::optional<Word>  line = m_model->GetLineOfRow (row);



    if (list != m_list)
    {
        return false;
    }

    items.push_back ({ L"Copy",  [this] { DxuiClipboard::SetText (m_host.GetOwnerHwnd(), m_list->GetSelectionText()); } });
    items.push_back ({ L"Paste", [this] { m_host.PasteBasicProgram(); } });

    //  The line's breakpoint rows, as the disassembly's are.
    if (line.has_value())
    {
        const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
        std::optional<int>            id       = FindBreakpoint (*line);
        std::optional<bool>           enabled;



        for (const DebuggerViewSnapshot::BreakpointLine & bp : (snapshot != nullptr && id.has_value()) ? snapshot->breakpoints : std::vector<DebuggerViewSnapshot::BreakpointLine>())
        {
            enabled = (bp.id == *id) ? std::optional<bool> (bp.enabled) : enabled;
        }

        items.push_back (PaneMenu::MakeSeparator());
        PaneMenu::AddBreakpointRows (items, enabled,
                                     [this, at = *line] { ToggleBreakpoint (at); },
                                     [this, bp = id.value_or (-1)] (bool on)
                                     {
                                         m_host.RunAction (DebuggerActions::GetBreakpointStep ({ BreakpointStep::Kind::Actions,
                                                                                                 { DebuggerActions::GetEnableBreakpoint (bp, on, m_host.GetMode()) }, {} }));
                                     },
                                     [this, at = *line] { ToggleBreakpoint (at); });
    }

    items.push_back (PaneMenu::MakeSeparator());
    items.push_back ({ L"Load BASIC program...", [this] { m_host.OpenBasicProgram(); } });
    items.push_back ({ L"Save BASIC program...", [this] { m_host.SaveBasicProgram(); } });
    items.push_back ({ L"Clear BASIC program...", [this] { m_host.ClearBasicProgram(); } });

    return true;
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
