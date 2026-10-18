#include "Pch.h"

#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DebuggerMenuCommand.h"
#include "Ui/Debugger/DroppedFiles.h"
#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerStatusText.h"
#include "Ui/Debugger/GutterGlyph.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/Source/SourcePathList.h"
#include "Config/WindowPlacementProfile.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/FlagsDialog.h"
#include "Ui/Debugger/BreakpointDialog.h"
#include "Debugger/SymbolDescriptions.h"
#include "Cassque/CassquePromptDialog.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/Panes/TracePaneController.h"
#include "Ui/Debugger/MemoryBarCommands.h"
#include "Ui/Debugger/Panes/MeterBar.h"
#include "Ui/Debugger/MemoryAddressEntry.h"
#include "Ui/Debugger/Panes/BasicProgramController.h"
#include "Ui/Debugger/Panes/BreakpointsPane.h"
#include "Ui/Debugger/Panes/CodeEditCommands.h"
#include "Ui/Debugger/Panes/ConsolePanes.h"
#include "Ui/Debugger/ViewMenuRows.h"
#include "Ui/Debugger/Panes/DisassemblyGutter.h"
#include "Ui/Debugger/Panes/DisassemblyViews.h"
#include "Ui/Debugger/Panes/FindWidgets.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"
#include "Ui/Debugger/Panes/ToolPanes.h"
#include "Ui/Debugger/Panes/HeatMapPane.h"
#include "Ui/Debugger/Panes/MemoryPane.h"
#include "Ui/Debugger/Panes/MemoryWindows.h"
#include "Ui/Debugger/Panes/OperandTipController.h"
#include "Ui/Debugger/Panes/UndoListPanes.h"
#include "Ui/Debugger/Panes/WatchesPane.h"
#include "Ui/Debugger/DebuggerCommands.h"
#include "Ui/Debugger/HistoryBand.h"
#include "Ui/Debugger/HeatStrip.h"
#include "Ui/Debugger/Panes/FlameGraphView.h"
#include "Ui/Debugger/Panes/MemoryMapBar.h"
#include "Ui/Debugger/Panes/ProfileTimeline.h"
#include "Ui/Debugger/Panes/DiagnosticsPane.h"
#include "Ui/Debugger/ColorKeyButton.h"
#include "Ui/Debugger/HeatMapBarCommands.h"
#include "Ui/Debugger/HeatMapRangeBarCommands.h"
#include "Ui/Debugger/ToolbarLabelEntry.h"
#include "Ui/Debugger/ToolbarCheckEntry.h"
#include "Ui/Debugger/UndoBarCommands.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetLists
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView *> DebuggerWindow::GetLists() const
{
    std::vector<DxuiListView *>  lists;



    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        std::ranges::copy (pane->GetLists(), std::back_inserter (lists));
    }

    return lists;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetTextLists
//
//  Every list but the disassemblies, whose columns are code laid out as
//  Visual Studio lays out its own. Each column is aligned on its own, so a
//  column lines up in either face.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView *> DebuggerWindow::GetTextLists() const
{
    std::vector<DxuiListView *>  lists;



    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        if (pane->HasTextLists())
        {
            std::ranges::copy (pane->GetLists(), std::back_inserter (lists));
        }
    }

    return lists;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPressTargets
//
//  Every control a mouse press is offered to before the lists.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> DebuggerWindow::GetPressTargets() const
{
    std::vector<IDxuiControl *>  targets;



    targets.push_back (m_consolePane->GetCommandLine().GetBox());

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        std::ranges::copy (pane->GetPressTargets(), std::back_inserter (targets));
    }

    std::ranges::copy (m_finds->GetPressTargets(), std::back_inserter (targets));

    return targets;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::NoteViewFocus
//
//  Steps follow the view: the source pane steps by source line, the
//  disassembly by instruction, in the words of the session's mode. Sent only
//  when it changes, so a click does not fill the console.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::NoteViewFocus (bool isSource)
{
    if (m_host == nullptr || m_snapshot == nullptr || !m_snapshot->source.has_value())
    {
        return;
    }

    if (m_snapshot->source->stepBySource != isSource)
    {
        RunAction (DebuggerActions::GetSourceStepping (isSource, m_snapshot->mode));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::NoteTabFocus
//
//  A press on a pane's tab counts as a press inside that pane: the pane takes
//  the focus, so its group draws the focus outline, and a disassembly or
//  source tab sets the step mode.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::NoteTabFocus (POINT pointDip)
{
    RECT            tab    = {};
    std::wstring    tip;
    std::wstring    pane   = m_dockSite->GetTabAt (pointDip, tab, tip);
    int             slot   = GetSourceSlotOf (pane);
    IDxuiControl  * target = nullptr;



    if (pane.empty())
    {
        return;
    }

    if (GetPaneOfFocus() != pane)
    {
        target = (pane == DebuggerLayout::kConsole) ? m_consolePane->GetCommandLine().GetBox() : nullptr;

        for (IDxuiControl * part : GetPaneControls (pane))
        {
            target = (target != nullptr) ? target : FindFirstFocusable (part);
        }

        if (target != nullptr)
        {
            SetFocusedControl (target);
        }
    }

    if (slot >= 0)
    {
        m_sources->SetActive (slot);
        NoteViewFocus (true);
        return;
    }

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        if (pane == DebuggerLayout::GetCodePaneId (view))
        {
            m_code->SetActive (view);
            NoteViewFocus (false);
            return;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::FindFirstFocusable
//
//  The first focusable control in a pane's tree, the pane's content first.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * DebuggerWindow::FindFirstFocusable (IDxuiControl * node)
{
    IDxuiControl  * found = nullptr;



    if (node == nullptr)
    {
        return nullptr;
    }

    if (node->IsFocusable() && node->IsEnabled())
    {
        return node;
    }

    for (size_t i = 0; i < node->GetChildCount() && found == nullptr; i++)
    {
        found = FindFirstFocusable (node->GetChild (i));
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnFilesDropped
//
//  A debug or symbol file loads as SYM LOAD would, and a source opens as File
//  > Open source file opens it. Any other file loads as symbols when it reads
//  as a symbol file; otherwise it is shown as text, or as a hex dump when it
//  is binary. One that cannot be read goes to SYM LOAD, which reports why.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::OnFilesDropped (const std::vector<std::wstring> & paths)
{
    SourceLookup  lookup;
    int           index  = -1;



    if (paths.empty() || m_host == nullptr)
    {
        return false;
    }

    for (const std::wstring & path : paths)
    {
        DroppedFiles::Kind     kind    = DroppedFiles::GetKind (path);
        DroppedFiles::Opening  opening = DroppedFiles::Opening::Symbols;

        if (kind == DroppedFiles::Kind::Source)
        {
            m_sources->OpenSourcePath (path);
            continue;
        }

        if (kind == DroppedFiles::Kind::Other)
        {
            lookup  = m_host->MatchDroppedDebuggerSource ({}, path, std::string(), index);
            opening = lookup.path.empty() ? DroppedFiles::Opening::Symbols : DroppedFiles::GetOpening (lookup.text);
        }

        if (opening == DroppedFiles::Opening::Symbols)
        {
            RunAction (DebuggerActions::GetLoadSymbols (SourcePathList::WideToUtf8 (path), GetMode()));
            continue;
        }

        m_sources->OpenLooseFile (path, opening == DroppedFiles::Opening::Hex ? DroppedFiles::FormatHexDump (lookup.text) : lookup.text, false);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsSymbolFile
//
//  A debug file or one of the symbol tables SYM LOAD reads, by extension.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsSymbolFile (const std::wstring & path)
{
    return DroppedFiles::GetKind (path) == DroppedFiles::Kind::Symbols;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFocusedBox
//
////////////////////////////////////////////////////////////////////////////////

DxuiTextInput * DebuggerWindow::GetFocusedBox() const
{
    IDxuiControl  * focused = GetFocused();



    if (focused != nullptr && focused == m_consolePane->GetCommandLine().GetBox())
    {
        return m_consolePane->GetCommandLine().GetBox();
    }

    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        for (DxuiTextInput * box : pane->GetTextBoxes())
        {
            if (focused != nullptr && focused == box)
            {
                return box;
            }
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnMappedCommand
//
//  Every action is the command its button sends, except Pause, which is the
//  channel's pause. A cursor action with no line to act on still consumes the
//  key, so it does not fall through to anything else.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::OnMappedCommand (int commandId)
{
    DebuggerKeySchemes::Action     action = (DebuggerKeySchemes::Action) commandId;
    std::optional<DebuggerAction>  taken;
    int                            row    = GetCodeLineOfRow (m_code->GetActive(), m_code->GetCodeList (m_code->GetActive())->GetSelectedRow());
    DebuggerViewSnapshot           active;



    //  The bookmark keys (FR-189).
    if (action == DebuggerKeySchemes::Action::ToggleBookmark || action == DebuggerKeySchemes::Action::NextBookmark ||
        action == DebuggerKeySchemes::Action::PreviousBookmark)
    {
        if (m_snapshot != nullptr && action == DebuggerKeySchemes::Action::ToggleBookmark)
        {
            ToggleBookmarkAtCursor();
        }
        else if (m_snapshot != nullptr)
        {
            GoToBookmark (action == DebuggerKeySchemes::Action::PreviousBookmark);
        }

        return true;
    }

    //  A pane's key opens it, or brings it forward, and focuses it.
    if (!DebuggerKeySchemes::GetPaneOfAction (action).empty())
    {
        ShowPane  (DebuggerKeySchemes::GetPaneOfAction (action));
        FocusPane (DebuggerKeySchemes::GetPaneOfAction (action));
        return true;
    }

    //  A pane's own use of an action while it has the focus, as a memory
    //  window's Find, which searches memory, and F3 and Shift+F3, which walk
    //  the matches.
    for (const std::unique_ptr<IDebuggerPane> & pane : m_panes)
    {
        if (pane->OnMappedAction (action, GetFocused()))
        {
            return true;
        }
    }

    //  Find acts on the window, not the machine.
    if (action == DebuggerKeySchemes::Action::Find)
    {
        OpenFind();
        return true;
    }

    if (action == DebuggerKeySchemes::Action::FindNext || action == DebuggerKeySchemes::Action::FindPrevious)
    {
        FindInPane (action == DebuggerKeySchemes::Action::FindNext);
        return true;
    }

    if (action == DebuggerKeySchemes::Action::Pause)
    {
        if (m_host != nullptr)
        {
            TryStopReplay();
            m_host->PauseDebugger();
        }

        return true;
    }

    //  The cursor actions read the disassembly view last used.
    if (m_snapshot != nullptr)
    {
        active      = *m_snapshot;
        active.code = GetCodeLines (m_code->GetActive());
    }

    taken = DebuggerActions::GetForKey (action, (m_snapshot != nullptr) ? &active : nullptr, row, GetMode());

    if (taken.has_value())
    {
        RunAction (*taken);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteBoxKey
//
//  A key-down in a focused text box that the box does not keep goes to the
//  scheme first. A memory window and the Address box count as boxes that are
//  never empty: every character typed into the one is an edit, and the
//  other is no command line. When the scheme took a Space, the character WM_CHAR delivers
//  for it is swallowed, or an empty box that stepped would be left holding a
//  space. Returns true when the key was decided here.
//
//  In GSSquared mode the empty command line has GSSquared's own keys first:
//  Space and F10 step and Return resumes, whatever the scheme. In Monitor
//  mode Return on the empty command line is the Monitor's own empty line,
//  which shows the next row of bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteBoxKey (const DxuiKeyEvent & ev, bool & handled)
{
    DxuiTextInput                               * box           = GetFocusedBox();
    bool                                          inMemory      = std::ranges::any_of (m_panes, [this] (const std::unique_ptr<IDebuggerPane> & pane) { return pane->TakesTyping (GetFocused()); });
    bool                                          decided       = false;
    CommandMode                                   mode          = (m_snapshot != nullptr) ? m_snapshot->mode : CommandMode::AppleWin;
    std::optional<DebuggerKeySchemes::Action>     consoleAction;
    bool                                          boxEmpty      = false;
    bool                                          boxKeeps      = false;



    //  While the line assembler takes the box's lines, Return and Space are
    //  the box's, so an empty line ends the assembly.
    if (ev.kind == DxuiKeyEventKind::Down && box != nullptr && box == m_consolePane->GetCommandLine().GetBox() &&
        DebuggerViewState::DoesAssemblerKeepKey (m_snapshot.get(), ev.vk, ev.ctrl, ev.alt))
    {
        return false;
    }

    consoleAction = (ev.kind == DxuiKeyEventKind::Down && box != nullptr && box == m_consolePane->GetCommandLine().GetBox())
                  ? DebuggerViewState::GetConsoleKeyAction (mode, ev.vk, ev.ctrl, ev.alt, ev.shift, box->GetText().empty())
                  : std::nullopt;

    //  Only the command line steps from an empty line; the Address box is no
    //  command line, so it keeps Space and Return however empty it is.
    boxEmpty = box != nullptr && box == m_consolePane->GetCommandLine().GetBox() && box->GetText().empty() &&
               !DebuggerViewState::DoesConsoleKeepKey (mode, ev.vk, ev.ctrl, ev.alt);

    boxKeeps = DebuggerKeySchemes::DoesBoxKeepKey (ev.vk, ev.ctrl, ev.alt, true, boxEmpty,
                                                   box != nullptr && box == m_consolePane->GetCommandLine().GetBox() && mode == CommandMode::Monitor);

    if (consoleAction.has_value())
    {
        OnMappedCommand ((int) *consoleAction);
        m_swallowSpace = ev.vk == VK_SPACE;
        handled        = true;
        decided        = true;
    }
    else if (ev.kind == DxuiKeyEventKind::Char)
    {
        decided        = m_swallowSpace && ev.vk == L' ';
        handled        = decided;
        m_swallowSpace = false;
    }
    else if (ev.kind == DxuiKeyEventKind::Down && (box != nullptr || inMemory) && !boxKeeps && RouteMappedKey (ev))
    {
        m_swallowSpace = ev.vk == VK_SPACE;
        handled        = true;
        decided        = true;
    }
    else if (ev.kind == DxuiKeyEventKind::Down && (box != nullptr || inMemory) && boxKeeps && ev.vk != VK_TAB &&
             !(box != nullptr && ev.vk == VK_RETURN))
    {
        //  A key the box keeps stays with it even when the box does nothing
        //  with the key-down, so it never reaches the scheme after all.
        (void) GetFocused()->OnKey (ev);
        handled = true;
        decided = true;
    }

    return decided;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::ForwardToList
//
//  A list takes coordinates relative to itself.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::ForwardToList (DxuiListView * list, const DxuiMouseEvent & ev)
{
    DxuiMouseEvent  local  = ev;
    RECT            bounds = list->GetBounds();



    local.positionDip = { ev.positionDip.x - bounds.left, ev.positionDip.y - bounds.top };
    return list->OnMouse (local);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OfferPress
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::OfferPress (IDxuiControl * control, const DxuiMouseEvent & ev, bool & handled)
{
    if (!handled && control != nullptr && IsRoutable (control) && control->IsVisible() && control->OnMouse (ev))
    {
        SetFocusedControl (control);
        handled = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteCommandBarMouse
//
//  The strip takes the pointer over itself and everything while one of its
//  menus is open, so a click meant for a menu row never reaches the pane
//  behind it.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteCommandBarMouse (const DxuiMouseEvent & ev)
{
    int   x    = ev.positionDip.x;
    int   y    = ev.positionDip.y;
    RECT  bars = m_commandBar->GetBounds();
    bool  over = x >= bars.left && x < bars.right && y >= bars.top && y < bars.bottom;



    if (!over && !m_commandBar->IsMenuOpen())
    {
        m_commandBar->OnToolbarMouseLeave();
        return false;
    }

    //  The tip follows the hover and goes the moment a press opens a menu,
    //  as Casso Explorer's toolbar tips do.
    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        UpdateTooltip (ev.positionDip);
        return m_commandBar->OnToolbarMouseMove (x, y);

    case DxuiMouseEventKind::Down:
        GetRoutedTooltip().HideImmediate();
        return m_commandBar->OnToolbarLButtonDown (x, y);

    case DxuiMouseEventKind::Up:
        return m_commandBar->OnToolbarLButtonUp (x, y);

    default:
        return m_commandBar->IsMenuOpen();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteFloatingBarMouse
//
//  Input over the floating bar's window, less its grab handle, which that
//  window takes itself, routed as the docked bar's is, with the window's
//  own tooltip.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteFloatingBarMouse (const DxuiMouseEvent & ev)
{
    FloatingPanes::RouteScope  scope (*m_floating, kBarFloatKey);



    return RouteCommandBarMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::OnMouse (const DxuiMouseEvent & ev)
{
    int           x       = ev.positionDip.x;
    int           y       = ev.positionDip.y;
    bool          lbDown  = (GetKeyState (VK_LBUTTON) & 0x8000) != 0;
    bool          handled = false;
    std::wstring  pane;



    if (ev.kind == DxuiMouseEventKind::Down)
    {
        m_openingFocus.OnUserInput();
        m_stopFocus.OnInput ((m_floating->GetRoutingPane().empty() ? m_dockSite->GetPaneAt (ev.positionDip) : m_floating->GetRoutingPane()) == DebuggerLayout::kConsole);
        m_lastPressPx = POINT { x, y };
    }

    //  A pane's drag takes every event until the button comes up, wherever
    //  the pointer is, so the menu bar, toolbars and status bar it crosses
    //  neither take its moves nor show their tips. Off the panes, its cross
    //  and its preview go at once.
    if (m_floating->GetRoutingPane().empty() && m_dockSite->HasPointerDrag())
    {
        GetRoutedTooltip().HideImmediate();
        (void) m_dockSite->OnMouse (ev);
        return true;
    }

    //  A disassembly row's operand and result that the pane cuts off show
    //  whole over their cell while the pointer is on it. This runs ahead of
    //  everything that lies over the panes, so IsOperandTipCovered tests for
    //  each of them. A move event records whether the left button was down
    //  when the pointer moved; that, not the button's state at this moment,
    //  marks a drag over the cell.
    m_operandTips->Update (ev, ev.button == DxuiMouseButton::Left);

    //  A pane's own box being edited, such as a breakpoint's name, a watch, a
    //  register, a heat map range or its set's name.
    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        if (each->RouteEditMouse (ev))
        {
            return true;
        }
    }

    //  A pane's own use of the mouse ahead of every other control, such as
    //  Ctrl+wheel over the heat map, which zooms the map rather than the text.
    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        if (each->RouteEarlyMouse (ev))
        {
            return true;
        }
    }

    //  Ctrl+wheel sizes the panes' text as Ctrl+Plus and Ctrl+Minus do, over
    //  any pane, before a view that would take the wheel for itself.
    if (ev.kind == DxuiMouseEventKind::Wheel && ev.ctrl && !ev.wheelHorizontal && ev.wheelDelta != 0.0f)
    {
        StepTextZoom (ev.wheelDelta > 0.0f ? 1 : -1);
        return true;
    }

    //  The status bar's zoom popup lies over everything while it is open, and
    //  the status bar owns its band.
    if (m_floating->GetRoutingPane().empty() && RouteStatusBarMouse (ev))
    {
        return true;
    }

    //  The menu bar first, then the command bar: each owns its strip and
    //  whatever menu it has open.
    if (m_floating->GetRoutingPane().empty() && RouteMenuBarMouse (ev))
    {
        return true;
    }

    if (m_floating->GetRoutingPane().empty() && !m_timelineHost.IsFloating() && RouteTimelineMouse (ev))
    {
        return true;
    }

    if (m_floating->GetRoutingPane().empty() && !m_barHost.IsFloating() && RouteCommandBarMouse (ev))
    {
        return true;
    }

    //  The panes' info buttons first: they sit at the ends of the toolbars.
    if (m_colorKeys->RouteMouse (ev))
    {
        return true;
    }

    //  Then each pane's on the pane interface.
    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        if (each->RouteBarMouse (ev))
        {
            return true;
        }
    }

    //  A press on a disassembly or source tab sets the step mode as a press
    //  inside that pane does.
    if (m_floating->GetRoutingPane().empty() && ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        NoteTabFocus (ev.positionDip);
    }

    //  A click on a branch arrow goes to where it points, and the next back.
    if (m_floating->GetRoutingPane().empty() && ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left &&
        ClickBranchArrow (ev.positionDip))
    {
        return true;
    }

    //  Then the site: its strips, its sashes and a drag in progress lie over
    //  the panes. A tip up as a drag starts goes, as none shows during one.
    if (m_floating->GetRoutingPane().empty() && m_dockSite->OnMouse (ev))
    {
        if (m_dockSite->IsDragging())
        {
            GetRoutedTooltip().HideImmediate();
        }

        return true;
    }

    //  A pane slid out from an edge lies over others, so its area is its own.
    if (m_floating->GetRoutingPane().empty() && !m_dockSite->GetSlidPane().empty() && ev.kind != DxuiMouseEventKind::Move &&
        DxuiDockSite::Contains (m_dockSite->GetSlidRect(), ev.positionDip))
    {
        return m_floating->RouteMouse (m_dockSite->GetSlidPane(), ev);
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Right)
    {
        pane = m_floating->GetRoutingPane().empty() ? m_dockSite->GetPaneAt (ev.positionDip) : m_floating->GetRoutingPane();

        //  Over a pane's content, the content's own menu (FR-084); over its
        //  tab or title, the menu of what can be done with the pane.
        if (!pane.empty() && !ShowContentMenu (pane, ev.positionDip))
        {
            ShowDockToMenu (pane, ev.positionDip);
        }

        return true;
    }

    //  The find widget floats over its pane's text, so a press on it is the
    //  widget's, not the text's.
    if (!m_finds->IsOverPlate (ev.positionDip) &&
        (m_sources->RouteMouse (ev) || m_consolePane->GetOutput().RouteMouse (ev, m_consolePane->GetCommandLine().GetBox())))
    {
        return true;
    }

    //  What a pane draws, such as the heat map and the flame graph.
    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        if (each->RouteContentMouse (ev))
        {
            return true;
        }
    }

    for (DxuiListView * list : GetLists())
    {
        if (IsRoutable (list) && list->IsInteracting() && ev.kind != DxuiMouseEventKind::Down)
        {
            ForwardToList (list, ev);

            //  A click on a register's value edits it once the click is
            //  over, so the list is not left mid-press under the box.
            if (ev.kind == DxuiMouseEventKind::Up)
            {
                NoteListReleased (list, ev.positionDip);
            }

            return true;
        }
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        //  A floating window's tips show from its own tooltip.
        UpdateTooltip (ev.positionDip);
        m_gutter->Hover (ev.positionDip);
        m_gutter->Drag  (ev.positionDip);

        if (IsRoutable (m_consolePane->GetCommandLine().GetBox()))
        {
            m_consolePane->GetCommandLine().GetBox()->SetMouseHover (x, y);
        }

        for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
        {
            for (DxuiTextInput * box : each->GetTextBoxes())
            {
                if (IsRoutable (box))
                {
                    box->SetMouseHover (x, y);
                }
            }
        }

        //  Each find widget moves with its pane.
        m_finds->OnMouseMove (ev, lbDown);

        return true;

    case DxuiMouseEventKind::Down:
        if (ev.button != DxuiMouseButton::Left)
        {
            return true;
        }

        if (m_gutter->Click (ev))
        {
            return true;
        }

        for (IDxuiControl * control : GetPressTargets())
        {
            OfferPress (control, ev, handled);
        }

        for (DxuiListView * list : GetLists())
        {
            RECT  bounds = list->GetBounds();

            if (!handled && IsRoutable (list) && list->IsVisible() && x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom)
            {
                handled = ForwardToList (list, ev);
                SetFocusedControl (list);
                handled = true;

                if (m_code->GetViewOf (list) >= 0)
                {
                    m_code->SetActive (m_code->GetViewOf (list));
                    NoteViewFocus (false);
                }
            }
        }

        return true;

    case DxuiMouseEventKind::Up:
        if (ev.button == DxuiMouseButton::Left && m_gutter->Drop (ev))
        {
            return true;
        }

        //  Only to what is shown: the disassembly views' radios share a rect,
        //  and a press on the one in front must not reach those behind it.
        for (IDxuiControl * control : GetPressTargets())
        {
            if (IsRoutable (control) && control->IsVisible())
            {
                control->OnMouse (ev);
            }
        }

        //  A release goes to the list UNDER THE POINTER, the same as a press. A
        //  list mid-drag already had it above, whatever the pointer is over.
        //  Sending it to every list reached the ones hidden behind a tab as
        //  well: the Stack and Call Stack panes share a rect, so a click on the
        //  fifth Stack row released onto the fifth Call Stack row -- which
        //  activated it and moved the code pane to that frame's call site.
        for (DxuiListView * list : GetLists())
        {
            RECT  bounds = list->GetBounds();

            if (IsRoutable (list) && list->IsVisible() &&
                x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom)
            {
                ForwardToList (list, ev);
                NoteListReleased (list, ev.positionDip);
            }
        }

        return true;

    case DxuiMouseEventKind::Wheel:
        //  The code pane holds only the lines it shows, so the wheel scrolls
        //  the disassembly itself, through all of memory (FR-073).
        for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
        {
            DxuiListView  * code = m_code->GetCodeList (view);

            if (IsRoutable (code) && code->IsVisible() && DxuiDockSite::Contains (code->GetBounds(), ev.positionDip) && m_host != nullptr)
            {
                m_host->ScrollDebuggerCode ((int) std::lround (-ev.wheelDelta * (float) code->GetWheelLinesPerNotch()), view);
                return true;
            }
        }

        for (DxuiListView * list : GetLists())
        {
            RECT  bounds = list->GetBounds();

            if (IsRoutable (list) && list->IsVisible() && x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom)
            {
                ForwardToList (list, ev);
            }
        }

        return true;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetPaneOfControl
//
//  Empty for a control that belongs to no pane: the toolbar, which never
//  leaves this window. The memory bar and its box belong to the memory pane
//  they sit in.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerWindow::GetPaneOfControl (const IDxuiControl * control) const
{
    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        std::wstring  shared = each->GetPaneOfSharedControl (control);



        if (!shared.empty())
        {
            return shared;
        }
    }

    //  A find widget is the pane's it searches, and goes with it.
    if (!m_finds->GetPaneOfControl (control).empty())
    {
        return m_finds->GetPaneOfControl (control);
    }

    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        for (const IDxuiControl * part : GetPaneControls (each->GetId()))
        {
            if (part == control)
            {
                return each->GetId();
            }
        }
    }

    for (const std::wstring & pane : DebuggerLayout::GetPaneIds())
    {
        for (const IDxuiControl * part : GetPaneControls (pane))
        {
            if (part == control)
            {
                return pane;
            }
        }
    }

    return L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::IsRoutable
//
//  Whether an event now being routed may reach a control: an event from this
//  window reaches the controls still here, one from a floating window only
//  the controls of its pane.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::IsRoutable (const IDxuiControl * control) const
{
    return m_floating->IsRoutable (GetPaneOfControl (control));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetFocused
//
//  A floating window keeps its own focus: the control last pressed in it.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * DebuggerWindow::GetFocused() const
{
    return m_floating->IsRoutedToFloat() ? m_floating->GetRoutedFocus() : m_focusMgr.GetFocusedControl();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::SetFocusedControl
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerWindow::SetFocusedControl (IDxuiControl * control)
{
    if (!m_floating->IsRoutedToFloat())
    {
        m_focusMgr.SetFocused (control);
        return;
    }

    m_floating->SetRoutedFocus (control);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetRoutingHwnd
//
////////////////////////////////////////////////////////////////////////////////

HWND DebuggerWindow::GetRoutingHwnd() const
{
    HWND  hwnd = m_floating->GetRoutedHwnd();



    return (hwnd != nullptr) ? hwnd : GetHwnd();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::RouteDockKey
//
//  Docking from the keyboard (FR-042): Shift+F10 or the menu key opens Dock To
//  for the focused pane, and Alt+Shift with an arrow moves it into the group
//  that way.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::RouteDockKey (const DxuiKeyEvent & ev)
{
    std::wstring     pane    = GetPaneOfFocus();
    IDxuiControl   * focused = GetFocused();
    RECT             bounds  = {};
    DxuiDockSide     side    = DxuiDockSide::Left;



    if (pane.empty())
    {
        return false;
    }

    //  The menu opens at the focused control of the window the key came from,
    //  a floating pane's own when it came from one.
    if (ev.vk == VK_APPS || (ev.vk == VK_F10 && ev.shift && !ev.ctrl && !ev.alt))
    {
        bounds = (focused != nullptr) ? focused->GetBounds() : RECT {};
        ShowDockToMenu (pane, POINT { bounds.left, bounds.top });
        return true;
    }

    if (!ev.alt || !ev.shift || ev.ctrl)
    {
        return false;
    }

    switch (ev.vk)
    {
    case VK_LEFT:  side = DxuiDockSide::Left;   break;
    case VK_RIGHT: side = DxuiDockSide::Right;  break;
    case VK_UP:    side = DxuiDockSide::Top;    break;
    case VK_DOWN:  side = DxuiDockSide::Bottom; break;
    default:       return false;
    }

    (void) m_dockSite->MovePaneByArrow (pane, side);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::OnKey
//
//  Enter in a box submits it. The text input has no submit event of its own,
//  so the window catches Enter before the focused box would ignore it.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerWindow::OnKey (const DxuiKeyEvent & ev)
{
    IDxuiControl  * focused = GetFocused();
    bool            handled = false;



    if (ev.kind == DxuiKeyEventKind::Down || ev.kind == DxuiKeyEventKind::Char)
    {
        m_openingFocus.OnUserInput();
        m_stopFocus.OnInput (GetPaneOfFocus() == DebuggerLayout::kConsole);
    }

    //  A key can scroll, step or resize what the operand tip lies over, so
    //  the tip goes; the next move of the pointer brings it back.
    if (ev.kind == DxuiKeyEventKind::Down)
    {
        m_operandTips->Hide();
    }

    //  Escape cancels a pane's drag, as in Visual Studio, and every pane
    //  stays where it was.
    if (m_floating->GetRoutingPane().empty() && m_dockSite->OnDragKey (ev))
    {
        return true;
    }

    //  The second key of a chord goes to the key scheme before any box or
    //  list, so the 1 of Ctrl+Alt+M, 1 opens Memory 1 rather than typing,
    //  and the character it would type goes too.
    if (ev.kind == DxuiKeyEventKind::Down && IsKeyPrefixPending())
    {
        m_swallowChordChar = ev.vk != VK_CONTROL && ev.vk != VK_MENU && ev.vk != VK_SHIFT;
        (void) RouteMappedKey (ev);
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Char && m_swallowChordChar)
    {
        m_swallowChordChar = false;
        return true;
    }

    //  The menu bar takes the keys while one of its menus is open, and Alt
    //  with a letter opens the menu it marks.
    if (RouteMenuBarKey (ev, handled))
    {
        return handled;
    }

    //  A pane's color key, while it is up, closes on Escape.
    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_ESCAPE && m_colorKeys->IsShown())
    {
        m_colorKeys->Hide();
        return true;
    }

    //  The zoom popup, while open, takes Escape to close and the keys its
    //  slider moves by.
    if (m_zoomOpen && ev.kind == DxuiKeyEventKind::Down)
    {
        if (ev.vk == VK_ESCAPE)
        {
            CloseZoomPopup();
            return true;
        }

        if (m_zoomSlider.OnKey (ev))
        {
            Invalidate();
            return true;
        }
    }

    //  While a replay runs, Escape stops it where it has reached.
    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_ESCAPE && TryStopReplay())
    {
        return true;
    }

    //  A pane's own box being edited, such as a breakpoint's name, a watch, a
    //  register or a heat map range, takes every key; and a pane's list with
    //  the focus takes its own keys, as F2 and Ctrl+Z in the registers, stack,
    //  watches and breakpoints.
    for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
    {
        if (each->RouteEditKey (ev, focused))
        {
            return true;
        }
    }

    //  Shift+Esc closes the pane that has the focus, as in Visual Studio.
    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_ESCAPE && ev.shift && !ev.ctrl && !ev.alt && ClosePaneOfFocus())
    {
        return true;
    }

    if (m_finds->RouteKey (ev, handled))
    {
        return handled;
    }

    if (RouteBoxKey (ev, handled))
    {
        return handled;
    }

    if (ev.kind == DxuiKeyEventKind::Down && RouteDockKey (ev))
    {
        return true;
    }

    if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_RETURN)
    {
        for (const std::unique_ptr<IDebuggerPane> & each : m_panes)
        {
            if (each->SubmitTextBox (dynamic_cast<DxuiTextInput *> (focused)))
            {
                return true;
            }
        }
    }

    //  A floating window types into its own focused control.
    if (ev.kind == DxuiKeyEventKind::Char && !m_floating->GetRoutingPane().empty())
    {
        return focused != nullptr && focused->OnKey (ev);
    }

    //  Only the box this window's focus is on types. A box focused in a
    //  floating window keeps its own focused flag after a click back here.
    if (ev.kind == DxuiKeyEventKind::Char)
    {
        return IsTextBox (focused) && focused->OnKey (ev);
    }

    //  Ctrl+Plus, Ctrl+Minus and Ctrl+0 size the panes' text (FR-083).
    if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt)
    {
        switch (ev.vk)
        {
        case VK_OEM_PLUS:  case VK_ADD:       StepTextZoom (+1);                 return true;
        case VK_OEM_MINUS: case VK_SUBTRACT:  StepTextZoom (-1);                 return true;
        case '0':          case VK_NUMPAD0:   ApplyTextZoom (1.0f);              return true;
        default:                                                                 break;
        }
    }

    if (ev.kind == DxuiKeyEventKind::Down)
    {
        handled = (focused != nullptr) && focused->OnKey (ev);

        //  Ctrl+C and Ctrl+A copy and select in a pane whose own keys do not
        //  take them, as its Edit menu does.
        if (!handled && focused != nullptr && ev.ctrl && !ev.alt && !ev.shift && (ev.vk == 'C' || ev.vk == 'A'))
        {
            handled = focused->InvokeCommand (ev.vk == 'C' ? DxuiStandardCommand::Copy : DxuiStandardCommand::SelectAll);
        }

        if (!handled && ev.vk == VK_TAB && m_floating->GetRoutingPane().empty())
        {
            //  Panes show, hide and move between one Tab and the next, so the
            //  order is taken from the layout as it stands now.
            m_focusMgr.Rebuild();
            m_focusMgr.HandleKey (ev.shift ? DxuiFocusKey::ShiftTab : DxuiFocusKey::Tab);
            handled = true;
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DebuggerWindow::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  sash = (m_dockSite != nullptr) ? m_dockSite->GetCursorForPoint (clientPx) : nullptr;



    if (sash != nullptr)
    {
        return sash;
    }

    //  The find widget floats over its pane's text: the I-beam over its box,
    //  and the arrow over its buttons and the rest of it, not the text's
    //  I-beam beneath.
    if (LPCWSTR cursor = m_finds->GetCursor (clientPx); cursor != nullptr)
    {
        return cursor;
    }

    for (DxuiListView * list : GetLists())
    {
        if (!list->IsVisible() || !IsRoutable (list))
        {
            continue;
        }

        RECT     bounds = list->GetBounds();
        POINT    local  = { clientPx.x - bounds.left, clientPx.y - bounds.top };
        LPCWSTR  cursor = list->GetCursorForPoint (local);

        if (cursor != nullptr)
        {
            return cursor;
        }
    }

    return DxuiWindow::GetCursorForPoint (clientPx);
}
