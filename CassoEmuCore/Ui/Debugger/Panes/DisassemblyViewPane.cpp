#include "Pch.h"

#include "Ui/Debugger/Panes/DisassemblyViewPane.h"
#include "Ui/Debugger/BreakpointActions.h"
#include "Ui/Debugger/RunActions.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/HistoryBand.h"
#include "Ui/Debugger/Panes/CodeEditCommands.h"
#include "Ui/Debugger/Panes/CodeViewFind.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/DisassemblyGutter.h"
#include "Ui/Debugger/Panes/DisassemblyViews.h"

#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::DisassemblyViewPane
//
////////////////////////////////////////////////////////////////////////////////

DisassemblyViewPane::DisassemblyViewPane (IDebuggerPaneHost & host, DisassemblyGroup & group, int view) :
    IDebuggerPane (host),
    m_group       (group),
    m_view        (view),
    m_id          (DebuggerLayout::GetCodePaneId (view)),
    m_bar         (host, m_id, L"Disassembly options")
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::GetId
//
////////////////////////////////////////////////////////////////////////////////

const std::wstring & DisassemblyViewPane::GetId() const
{
    return m_id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::GetTitle
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DisassemblyViewPane::GetTitle() const
{
    return std::format (L"Disassembly {}", m_view + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::GetTabTitle
//
//  The first view's tab has no number, as it is the only view by default.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DisassemblyViewPane::GetTabTitle() const
{
    return (m_view == 0) ? std::wstring (L"Disassembly") : GetTitle();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::GetStopFocusRank
//
//  At a stop the focus moves to the active view, unless another pane has a
//  higher rank, such as a BASIC program stopped in.
//
////////////////////////////////////////////////////////////////////////////////

int DisassemblyViewPane::GetStopFocusRank() const
{
    return (m_group.GetActive() == m_view) ? 1 : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::CreateControls
//
//  The first view's history band has its place in history and a Go live
//  link.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyViewPane::CreateControls()
{
    DxuiPanel  & parent = m_host.GetControlParent();



    m_list   = parent.CreateChild<DxuiListView>();
    m_banner = parent.CreateChild<DxuiInfoBanner>();
    m_bar.Create();

    m_banner->SetSeverity (DxuiInfoBanner::Severity::Info);
    m_banner->SetVisible  (false);

    m_find = std::make_unique<CodeViewFind> (*m_list, m_group.GetOptions(),
                                             [this] { return GetFindLines(); },
                                             [this] (bool isTop) { return GetFindStart (isTop); },
                                             [this] (Word address) { SelectFound (address); },
                                             [this] (int fileId, int line, std::wstring & text) { return m_group.GetSourceText (fileId, line, text); });

    if (m_view == 0)
    {
        m_historyBand = parent.CreateChild<HistoryBand>();
        m_historyBand->SetOnGoLive ([this] { m_host.GoLive(); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::GetFindView
//
////////////////////////////////////////////////////////////////////////////////

IDxuiFindTarget * DisassemblyViewPane::GetFindView() const
{
    return m_find.get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::GetFindLines
//
//  The paused machine's lines for the whole address space; none while it
//  runs, when the find searches the rows on screen.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const std::vector<DebuggerViewSnapshot::CodeLine>> DisassemblyViewPane::GetFindLines() const
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();



    return (snapshot != nullptr && snapshot->isPaused) ? snapshot->codeFind : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::GetFindStart
//
//  The selected instruction's address, or the top line's with isTop set;
//  none for a view with no lines.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> DisassemblyViewPane::GetFindStart (bool isTop) const
{
    const std::vector<DebuggerViewSnapshot::CodeLine>  & lines = m_group.GetCodeLines (m_view);
    int                                                  line  = isTop ? 0 : m_group.GetCodeLineOfRow (m_view, m_list->GetSelectedRow());



    if (line < 0 || (size_t) line >= lines.size())
    {
        return lines.empty() ? std::nullopt : std::optional<Word> (lines.front().address);
    }

    return lines[(size_t) line].address;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::SelectFound
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyViewPane::SelectFound (Word address)
{
    IDebuggerWindowHost  * host = m_host.GetWindowHost();



    m_group.SetActive     (m_view);
    m_group.SetFindSelect (m_view, address);

    if (host != nullptr)
    {
        host->SelectDebuggerCode (m_view, address);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::Configure
//
//  The bar is the viewing options as check boxes, as Visual Studio's are. A
//  breakpoint is set from the gutter (DisassemblyGutter), as in an editor;
//  the rest of the view is TEXT (FR-076): a drag selects characters, a
//  double-click selects the word under the pointer, and Ctrl+C copies what
//  is selected -- a double-click never touches a breakpoint.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyViewPane::Configure()
{
    DxuiToolbar                      * bar     = m_bar.GetBar();
    std::vector<DxuiToolbar::Entry>    entries = m_group.BuildBarEntries();



    entries.push_back (m_host.GetFindEntry (GetId()));

    bar->SetTextRenderer  (m_host.GetPaneTextRenderer());
    bar->SetPopupHost     (m_host.GetPanePopupHost());
    bar->SetCompact       (true);
    bar->SetPaneTextInset (true);
    bar->SetEntries       (std::move (entries));
    bar->SetVisible       (false);

    //  A code row selected selects its line in the source pane (FR-054).
    m_list->SetOnSelectionChanged ([this] (int row) { m_group.OnRowSelected (m_view, row); });

    DisassemblyCells::Configure (*m_list);

    m_list->SetActivateOnDoubleClick (true);
    m_list->SetTextSelection         (true);
    m_list->SetOwnerWindow           (m_host.GetOwnerHwnd());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::TryGetOpened
//
//  The first view is always open.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyViewPane::TryGetOpened (bool & isOpened) const
{
    if (m_view == 0)
    {
        return false;
    }

    isOpened = m_group.IsOpen (m_view);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::RequestOpen
//
//  A view opened from the View menu starts at the PC.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyViewPane::RequestOpen()
{
    IDebuggerWindowHost         * host     = m_host.GetWindowHost();
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();



    if (m_view > 0 && host != nullptr)
    {
        host->SetDebuggerCodeAddress ((snapshot != nullptr) ? snapshot->pc : (Word) 0, m_view);
    }

    return m_view > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::RequestClose
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyViewPane::RequestClose()
{
    IDebuggerWindowHost  * host = m_host.GetWindowHost();



    if (m_view > 0 && host != nullptr)
    {
        host->CloseDebuggerCodeView (m_view);
    }

    return m_view > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::AddToDock
//
//  The toolbar, a place PlaceBar fills, over the lines; the first view has
//  its history band between them while the machine is back in history.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyViewPane::AddToDock (DxuiDockSite & dock)
{
    m_frame = std::make_unique<DebuggerPaneFrame> (GetTitle());
    m_frame->AddPart (m_bar.GetSlot(), PaneBarSlot::GetHeightPx);

    if (m_historyBand != nullptr)
    {
        m_frame->AddPart (m_historyBand, [this] (int, const DxuiDpiScaler & scaler) { return scaler.ToPx (HistoryBand::GetHeightDip (m_host.GetPaneRowDip())); },
                          [this] { return HistoryBand::IsShown (m_historyBand->GetStatus()); });
    }

    m_frame->AddPart (m_banner,
                      [this] (int width, const DxuiDpiScaler & scaler) { return (int) m_banner->GetPreferredHeightPx ((float) width, scaler); },
                      [this] { return m_isBannerShown; });
    m_frame->AddPart (m_list);

    dock.AddPane (GetId(), GetTitle(), m_frame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::GetContent
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * DisassemblyViewPane::GetContent() const
{
    return m_frame.get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::SetStaleText
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyViewPane::SetStaleText (const std::wstring & text)
{
    bool  isShown = !text.empty();



    if (text == m_banner->GetText() && isShown == m_isBannerShown)
    {
        return;
    }

    m_banner->SetText (text);
    m_isBannerShown = isShown;

    if (m_frame != nullptr)
    {
        m_frame->Relayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::GetStaleText
//
//  A debug file has source lines and labels; a symbol file has labels
//  alone.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DisassemblyViewPane::GetStaleText (const std::vector<std::string> & names, bool isDebugFileStale)
{
    std::vector<std::string>  symbols  = names;
    std::string               text;
    std::string               joined;



    if (isDebugFileStale && !symbols.empty())
    {
        text = std::format ("The program in memory doesn't match {}, so its source lines and labels are hidden.", symbols.front());
        symbols.erase (symbols.begin());
    }

    for (size_t i = 0; i < symbols.size(); i++)
    {
        joined += (i == 0) ? "" : (i + 1 == symbols.size()) ? " or " : ", ";
        joined += symbols[i];
    }

    if (!symbols.empty())
    {
        text += text.empty() ? "" : "\n";
        text += std::format ("The program in memory doesn't match {}, so {} labels are hidden.", joined, (symbols.size() == 1) ? "its" : "their");
    }

    return TextEncoding::NarrowToWide (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> DisassemblyViewPane::GetControls() const
{
    return { m_bar.GetSlot(), m_bar.GetBar(), m_banner, m_list };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::GetColorKey
//
////////////////////////////////////////////////////////////////////////////////

std::optional<PaneColorKeySpec> DisassemblyViewPane::GetColorKey() const
{
    return PaneColorKeySpec { m_bar.GetSlot(), ColorLegend::Pane::Disassembly, nullptr };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::RouteBarMouse
//
//  The bar, then the history band, whose Go live link takes a press.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyViewPane::RouteBarMouse (const DxuiMouseEvent & ev)
{
    if (m_bar.RouteMouse (ev))
    {
        return true;
    }

    return m_historyBand != nullptr && m_historyBand->IsVisible() && DxuiDockSite::Contains (m_historyBand->GetBounds(), ev.positionDip) &&
           m_host.IsBarRouted (GetId()) && m_historyBand->OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::AddMenuItems
//
//  In groups: running to the line, its breakpoint, copying and showing it in
//  memory (FR-084: the instruction's operand, resolved through its
//  addressing mode, or the line's address, one row for each memory pane
//  open), then the line's edits. A source or label row's menu is the menu of
//  the instruction below it.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyViewPane::AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items)
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
    int                           index    = m_group.GetMenuLineOfRow (m_view, row);
    RECT                          bounds   = m_list->GetBounds();
    int                           column   = m_host.GetListColumnAt (m_list, m_host.GetLastPressPx().x - bounds.left);



    if (list != m_list)
    {
        return false;
    }

    if (index < 0)
    {
        return true;
    }

    const DebuggerViewSnapshot::CodeLine  & line       = m_group.GetCodeLines (m_view)[(size_t) index];
    Word                                    at         = line.address;
    auto                                    breakpoint = std::ranges::find_if (snapshot->breakpoints, [at] (const DebuggerViewSnapshot::BreakpointLine & each) { return each.address == at; });



    items.push_back ({ L"Run to cursor", [this, at] { m_host.RunAction (RunActions::GetRunToCursor (at)); } });

    if (snapshot->isPaused && DisassemblyOptions::CanTakeBreakpoint (line))
    {
        items.push_back ({ L"Set next statement", [this, at] { m_group.GetGutter().SetNextStatement (at); } });
    }

    items.push_back ({ L"Show next statement", [this] { m_host.ShowPcCode(); } });
    items.push_back (PaneMenu::MakeSeparator());

    if (line.hasBreakpoint)
    {
        PaneMenu::AddBreakpointRows (items, line.isEnabled, nullptr,
                                     (breakpoint == snapshot->breakpoints.end()) ? std::function<void (bool)>()
                                         : [this, id = breakpoint->id] (bool enable) { m_host.RunAction (BreakpointActions::GetEnableBreakpoint (id, enable, m_host.GetMode())); },
                                     [this, at] { m_host.RunAction (BreakpointActions::GetToggleBreakpoint (*m_host.GetLatestSnapshot(), at, m_host.GetMode())); });
    }
    else if (DisassemblyOptions::CanTakeBreakpoint (line))
    {
        PaneMenu::AddBreakpointRows (items, std::nullopt,
                                     [this, at] { m_host.RunAction (BreakpointActions::GetToggleBreakpoint (*m_host.GetLatestSnapshot(), at, m_host.GetMode())); },
                                     nullptr, nullptr);
    }

    items.push_back (PaneMenu::MakeSeparator());
    items.push_back ({ L"Copy", [this] { DxuiClipboard::SetText (m_host.GetOwnerHwnd(), m_list->GetSelectionText()); } });

    if (column >= (int) DisassemblyCells::kInstructionColumn && !line.memoryOperand.empty())
    {
        m_host.AddShowInMemoryItems (TextEncoding::NarrowToWide (line.shownOperand), line.memoryOperand, items);
    }
    else
    {
        m_host.AddShowInMemoryItems (std::format (L"${:04X}", at), std::format ("{:04X}", at), items);
    }

    items.push_back (PaneMenu::MakeSeparator());
    m_group.GetEdits().AddItems (m_view, line, row, items);

    if (m_view > 0)
    {
        items.push_back (PaneMenu::MakeSeparator());
        items.push_back ({ std::format (L"Close Disassembly {}", m_view + 1), [this] { (void) RequestClose(); } });
    }

    return true;
}
