#include "Pch.h"

#include "Ui/Debugger/Panes/DisassemblyViews.h"

#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Debugger/SymbolDescriptions.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/HeatStrip.h"
#include "Ui/Debugger/HistoryBand.h"
#include "Ui/Debugger/Panes/CodeEditCommands.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/DisassemblyGutter.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/ToolbarCheckEntry.h"





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
//  DisassemblyViewPane::~DisassemblyViewPane
//
////////////////////////////////////////////////////////////////////////////////

DisassemblyViewPane::~DisassemblyViewPane() = default;





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
    m_banner = parent.CreateChild<DxuiActionBanner>();
    m_bar.Create();

    m_banner->SetSeverity (DxuiInfoBanner::Severity::Info);
    m_banner->SetVisible  (false);

    if (m_view == 0)
    {
        m_historyBand = parent.CreateChild<HistoryBand>();
        m_historyBand->SetOnGoLive ([this] { m_host.GoLive(); });
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
    DxuiToolbar  * bar = m_bar.GetBar();



    bar->SetTextRenderer  (m_host.GetPaneTextRenderer());
    bar->SetPopupHost     (m_host.GetPanePopupHost());
    bar->SetCompact       (true);
    bar->SetPaneTextInset (true);
    bar->SetEntries       (m_group.BuildBarEntries());
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
//  open), then the line's edits.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyViewPane::AddMenuItems (DxuiListView * list, int row, PaneMenuItems & items)
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
    int                           index    = m_group.GetCodeLineOfRow (m_view, row);
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



    items.push_back ({ L"Run to cursor", [this, at] { m_host.RunAction (DebuggerActions::GetRunToCursor (at)); } });

    if (snapshot->isPaused && DisassemblyOptions::CanTakeBreakpoint (line))
    {
        items.push_back ({ L"Set next statement", [this, at] { m_group.GetGutter().SetNextStatement (at); } });
    }

    items.push_back ({ L"Show next statement", [this] { m_host.ShowPcCode(); } });
    items.push_back (PaneMenu::MakeSeparator());

    items.push_back ({ line.hasBreakpoint ? L"Remove breakpoint" : L"Set breakpoint",
                       [this, at] { m_host.RunAction (DebuggerActions::GetToggleBreakpoint (*m_host.GetLatestSnapshot(), at, m_host.GetMode())); } });

    if (line.hasBreakpoint && breakpoint != snapshot->breakpoints.end())
    {
        items.push_back ({ breakpoint->enabled ? L"Disable breakpoint" : L"Enable breakpoint",
                           [this, id = breakpoint->id, enable = !breakpoint->enabled] { m_host.RunAction (DebuggerActions::GetEnableBreakpoint (id, enable, m_host.GetMode())); } });
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





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::DisassemblyGroup
//
//  The options the user last left.
//
////////////////////////////////////////////////////////////////////////////////

DisassemblyGroup::DisassemblyGroup (IDebuggerPaneHost & host, DisassemblyGutter & gutter, CodeEditCommands & edits, SourceDocumentGroup & sources) :
    m_host    (host),
    m_gutter  (gutter),
    m_edits   (edits),
    m_sources (sources)
{
    IDebuggerWindowHost  * windowHost = host.GetWindowHost();



    m_options = DisassemblyOptions::FromText ((windowHost != nullptr) ? windowHost->GetDebuggerDisassemblyOptions() : std::string());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::~DisassemblyGroup
//
////////////////////////////////////////////////////////////////////////////////

DisassemblyGroup::~DisassemblyGroup() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::CreateViews
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::unique_ptr<IDebuggerPane>> DisassemblyGroup::CreateViews()
{
    std::vector<std::unique_ptr<IDebuggerPane>>  views;



    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        std::unique_ptr<DisassemblyViewPane>  pane = std::make_unique<DisassemblyViewPane> (m_host, *this, view);

        m_views[(size_t) view] = pane.get();
        views.push_back (std::move (pane));
    }

    return views;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::GetCodeLines
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<DebuggerViewSnapshot::CodeLine> & DisassemblyGroup::GetCodeLines (int view) const
{
    static const std::vector<DebuggerViewSnapshot::CodeLine>  kNone;
    const DebuggerViewSnapshot                              * snapshot = m_host.GetLatestSnapshot();



    if (snapshot == nullptr || view < 0 || view >= DebuggerViewState::kMaxCodeViews)
    {
        return kNone;
    }

    return snapshot->codeViews[(size_t) view];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::GetCodeList
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView * DisassemblyGroup::GetCodeList (int view) const
{
    return m_views[(size_t) view]->GetList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::GetCodeLineOfRow
//
////////////////////////////////////////////////////////////////////////////////

int DisassemblyGroup::GetCodeLineOfRow (int view, int row) const
{
    int  line = -1;



    if (view < 0 || view >= DebuggerViewState::kMaxCodeViews)
    {
        return -1;
    }

    line = DisassemblyOptions::GetLineOfRow (m_views[(size_t) view]->GetRows(), row);

    return (line < (int) GetCodeLines (view).size()) ? line : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::GetGutterLineOfRow
//
////////////////////////////////////////////////////////////////////////////////

int DisassemblyGroup::GetGutterLineOfRow (int view, int row) const
{
    int  line = -1;



    if (view < 0 || view >= DebuggerViewState::kMaxCodeViews)
    {
        return -1;
    }

    line = DisassemblyOptions::GetGutterLineOfRow (m_views[(size_t) view]->GetRows(), row);

    return (line < (int) GetCodeLines (view).size()) ? line : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::GetViewOf
//
////////////////////////////////////////////////////////////////////////////////

int DisassemblyGroup::GetViewOf (const IDxuiControl * control) const
{
    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        if (control != nullptr && control == m_views[(size_t) view]->GetList())
        {
            return view;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::GetOpenCount
//
////////////////////////////////////////////////////////////////////////////////

int DisassemblyGroup::GetOpenCount() const
{
    return (int) std::count (m_isOpen.begin(), m_isOpen.end(), true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::BuildBarEntries
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbar::Entry> DisassemblyGroup::BuildBarEntries()
{
    constexpr int                         kOptionEntry = 100;
    constexpr DisassemblyOptions::Option  kOrder[]     = { DisassemblyOptions::Option::Addresses,
                                                           DisassemblyOptions::Option::CodeBytes,
                                                           DisassemblyOptions::Option::Source,
                                                           DisassemblyOptions::Option::Symbols,
                                                           DisassemblyOptions::Option::LineNumbers,
                                                           DisassemblyOptions::Option::Heat };
    std::vector<DxuiToolbar::Entry>       entries;



    for (DisassemblyOptions::Option option : kOrder)
    {
        auto                command = std::make_shared<DxuiCommand>();
        DxuiToolbar::Entry  entry;

        command->id        = kOptionEntry + (int) option;
        command->label     = DisassemblyOptions::GetLabel (option);
        command->dispatch  = [this, option] { ToggleOption (option); };
        command->isChecked = [this, option] { return m_options.IsOn (option); };
        command->isEnabled = [this, option] { return IsOptionEnabled (option); };

        m_optionEntries.push_back (std::make_unique<ToolbarCheckEntry> (command));

        if (option == DisassemblyOptions::Option::Source)
        {
            m_optionEntries.back()->SetTipSource ([this] { return DisassemblyOptions::GetSourceTip (IsOptionEnabled (DisassemblyOptions::Option::Source)); });
        }
        else if (option == DisassemblyOptions::Option::LineNumbers)
        {
            m_optionEntries.back()->SetTipSource ([this]
            {
                const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();

                return DisassemblyOptions::GetLineNumbersTip (snapshot != nullptr && snapshot->source.has_value(), m_options.IsOn (DisassemblyOptions::Option::Source));
            });
        }
        else if (option == DisassemblyOptions::Option::Symbols)
        {
            m_optionEntries.back()->SetTipSource ([this]
            {
                const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();

                return DisassemblyOptions::GetSymbolsTip ((snapshot != nullptr) ? snapshot->symbolSources : std::vector<std::string>());
            });
        }

        //  A command, not a toggle: the check box has the state, so the
        //  entry draws no pressed box around a checked one.
        entry.command = command;
        entry.kind    = DxuiToolbar::Kind::Command;
        entry.custom  = m_optionEntries.back().get();
        entries.push_back (std::move (entry));
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::ToggleOption
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGroup::ToggleOption (DisassemblyOptions::Option option)
{
    IDebuggerWindowHost  * host = m_host.GetWindowHost();



    if (!IsOptionEnabled (option))
    {
        return;
    }

    m_options.Toggle (option);

    if (host != nullptr)
    {
        host->SetDebuggerDisassemblyOptions (m_options.ToText());
    }

    m_host.SyncHeatRecording();

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews && m_host.GetLatestSnapshot() != nullptr; view++)
    {
        ApplyView (view);
    }

    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::IsOptionEnabled
//
//  Source and line numbers only once a debug file maps the code; the line
//  numbers only with the source they number.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGroup::IsOptionEnabled (DisassemblyOptions::Option option) const
{
    const DebuggerViewSnapshot  * snapshot     = m_host.GetLatestSnapshot();
    bool                          hasDebugFile = snapshot != nullptr && snapshot->source.has_value();



    if (!DisassemblyOptions::NeedsDebugFile (option))
    {
        return true;
    }

    if (option == DisassemblyOptions::Option::LineNumbers)
    {
        return hasDebugFile && m_options.IsOn (DisassemblyOptions::Option::Source);
    }

    return hasDebugFile;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::FitLabels
//
//  Each load of symbols or a debug file changes the labels the rows have, so
//  every view's Label column is fitted to them again, in place of a width
//  the user or a project set: a saved width made for other labels cut the
//  new ones short.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGroup::FitLabels (const DebuggerViewSnapshot & snapshot)
{
    DxuiListView  * list = nullptr;



    if (snapshot.symbols == m_fittedSymbols)
    {
        return;
    }

    m_fittedSymbols = snapshot.symbols;

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        list = GetCodeList (view);

        if (list != nullptr)
        {
            list->SetColumnOverrideWidthPx (DisassemblyCells::kLabelColumn, -1);
            list->ResetAutoFit();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::Apply
//
//  Each disassembly view open, and which follows the PC: once a second view
//  is open, the follower's tab has the PC's yellow dot and a tip with that.
//  Every view's open state is taken before any dot is set, so the first
//  view's dot reflects a second view opened in this snapshot.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGroup::Apply()
{
    const DebuggerViewSnapshot  & snapshot = *m_host.GetLatestSnapshot();



    UpdateChanges (snapshot);
    FitLabels     (snapshot);

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        bool  open = snapshot.codeOpen[(size_t) view];

        if (m_isOpen[(size_t) view] != open)
        {
            m_isOpen[(size_t) view] = open;

            //  The keys and the command bar act on the view last used, and a
            //  closed one has nothing to act on, so the first view takes over.
            if (!open && view == m_active)
            {
                m_active = 0;
            }

            //  A view open as the window starts was open when it closed, and
            //  its group's front tab is the one in front then.
            if (open && view > 0 && !m_host.IsRestoringViews())
            {
                m_host.ActivatePaneTab (DebuggerLayout::GetCodePaneId (view));
            }

            m_host.RelayoutDock();
            UpdateLines();
        }
    }

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        bool                       follows = GetOpenCount() > 1 && snapshot.followView == view;
        DxuiTabGroup::LeadingMark  mark;

        //  The tab of the view following the PC has the PC's own marker: the
        //  same character, face and color as the arrow on the PC's line, so
        //  the two read as one sign.
        if (follows)
        {
            mark = DxuiTabGroup::LeadingMark { s_kpszTriangleRight, DxuiTheme::kMonoFace, m_host.GetPcTabMarkArgb() };
        }

        m_host.SetPaneTabMark (DebuggerLayout::GetCodePaneId (view), mark, follows ? L"This disassembly follows the PC" : L"");

        if (m_isOpen[(size_t) view])
        {
            ApplyView (view);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::UpdateChanges
//
//  The bytes on every view's rows, by address, so a view that scrolls
//  compares a byte with the last one any view had there. Another machine's bytes did
//  not change; they are someone else's.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGroup::UpdateChanges (const DebuggerViewSnapshot & snapshot)
{
    ByteChanges::Seen  seen;



    if (snapshot.machine != m_machine)
    {
        m_changes.Clear();
        m_machine = snapshot.machine;
    }

    for (const std::vector<DebuggerViewSnapshot::CodeLine> & lines : snapshot.codeViews)
    {
        for (const DebuggerViewSnapshot::CodeLine & line : lines)
        {
            ByteChanges::Seen  row = ByteChanges::ParseRowBytes (line.address, line.bytes);

            seen.insert (seen.end(), row.begin(), row.end());
        }
    }

    m_changes.Update (snapshot.isPaused, seen);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::ApplyView
//
//  One view's rows: the gutter's breakpoints, the PC's arrow and row
//  wherever the PC is on its lines, the row another pane brought into view,
//  and a branch's destination. With source on, each source line is on a row
//  of its own above the code it produced, its text running across the
//  columns, and the row has the marks of the first instruction under it. The
//  viewing options hide the address, bytes and label columns.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGroup::ApplyView (int view)
{
    const DebuggerViewSnapshot                         * snapshot      = m_host.GetLatestSnapshot();
    const DxuiTheme                                    * theme         = m_host.GetTheme();
    DxuiListView                                       * list          = GetCodeList (view);
    const std::vector<DebuggerViewSnapshot::CodeLine>  & lines         = GetCodeLines (view);
    std::vector<std::vector<DxuiListView::Cell>>         rows;
    std::vector<DisassemblyOptions::Row>                 built;
    int                                                  current       = -1;
    int                                                  selected      = -1;
    std::optional<Word>                                  target;
    DebuggerTextColors::Set                              textColors    = m_host.GetTextColors();
    bool                                                 source        = m_options.IsOn (DisassemblyOptions::Option::Source) &&
                                                                         IsOptionEnabled (DisassemblyOptions::Option::Source);
    bool                                                 numbers       = m_options.IsOn (DisassemblyOptions::Option::LineNumbers) &&
                                                                         IsOptionEnabled (DisassemblyOptions::Option::LineNumbers);
    std::wstring                                         text;
    std::optional<HeatMapView::Palette>                  heatPalette;
    int                                                  pcSourceRow   = -1;
    int                                                  marked        = -1;
    bool                                                 operandMoved  = false;



    DisassemblyCells::ShowColumns (*list, m_options);

    if (m_options.IsOn (DisassemblyOptions::Option::Heat))
    {
        heatPalette = m_host.GetHeatPalette();
    }

    for (const DebuggerViewSnapshot::CodeLine & line : lines)
    {
        if (line.isCurrent && line.target.has_value())
        {
            target = line.target;
        }
    }

    built = DisassemblyOptions::BuildRows (lines, source, [this, &text] (int fileId, int line) { return TryGetSourceText (fileId, line, text); });
    m_views[(size_t) view]->SetRows (built);
    SendShownLines (view, built);
    m_views[(size_t) view]->SetStaleText (DisassemblyViewPane::GetStaleText (snapshot->staleFileNames, snapshot->source.has_value() && snapshot->source->isStale));

    //  With source lines on, a source row represents the first instruction
    //  its line produced: the instruction's breakpoint, PC marker
    //  and row color are on the source row, and the instruction is dimmed
    //  under it as the others are. While the PC's arrow is dragged, the
    //  marker and the PC's row color are on the row under the pointer.
    pcSourceRow = FindPcSourceRow (lines, built);
    current     = (pcSourceRow >= 0) ? pcSourceRow : -1;

    for (size_t i = 0; i < lines.size() && current < 0; i++)
    {
        current = lines[i].isCurrent ? DisassemblyOptions::GetRowOfLine (built, (int) i) : current;
    }

    marked = (m_gutter.GetDragRow (view) >= 0) ? m_gutter.GetDragRow (view) : current;


    for (const DisassemblyOptions::Row & each : built)
    {
        std::vector<DxuiListView::Cell>          cells (DisassemblyCells::kColumnCount);
        int                                      at      = (int) rows.size();
        int                                      gutter  = DisassemblyOptions::GetGutterLineOfRow (built, at);
        uint32_t                                 fill    = 0;
        const DebuggerViewSnapshot::CodeLine   * line    = nullptr;
        bool                                     hasText = false;
        std::wstring                             rowTip;
        Word                                     first   = 0;
        size_t                                   count   = 0;
        std::vector<DxuiListView::Cell>          scratch (DisassemblyCells::kColumnCount);

        if (gutter >= 0)
        {
            MarkRow (view, at, lines[(size_t) gutter], target, at == marked, cells, fill, rowTip);
        }

        //  A source line: its number when on, then its text in the syntax
        //  colors, from the first column after the glyph margin. A line that
        //  produced one instruction has the instruction's operand and result
        //  too, and the instruction's row doesn't.
        if (each.codeLine < 0)
        {
            hasText       = TryGetSourceText (each.fileId, each.sourceLine, text);
            text          = hasText ? text : std::wstring();
            operandMoved  = gutter >= 0 && DisassemblyOptions::IsSingleInstruction (built, at) && SourcePane::IsInstructionLine (text);

            if (operandMoved)
            {
                (void) DisassemblyCells::FillCodeRow (scratch, lines[(size_t) gutter], m_options, textColors);
            }

            DisassemblyCells::FillSourceRow (cells, *list, each.sourceLine, text, numbers, textColors, (theme != nullptr) ? theme->ForegroundMuted() : 0u,
                                             operandMoved ? &scratch[DisassemblyCells::kOperandColumn] : nullptr);

            //  The line's heat is its instructions' heat.
            if (heatPalette.has_value() && snapshot->heatColumns != nullptr && DisassemblyCells::TryGetSourceRowSpan (built, rows.size(), lines, first, count))
            {
                cells[DisassemblyCells::kHeatColumn] = HeatStrip::GetCell (*snapshot->heatColumns, first, count, *heatPalette, false);
            }

            if (theme != nullptr && DisassemblyCells::IsDimmed (m_options, source, true, at == current))
            {
                DisassemblyCells::DimRow (cells, theme->Foreground(), theme->TextViewBackground());
            }
        }
        else
        {
            line = &lines[(size_t) each.codeLine];

            (void) DisassemblyCells::FillCodeRow (cells, *line, m_options, textColors);

            if (operandMoved && DisassemblyOptions::IsFronted (built, at))
            {
                cells[DisassemblyCells::kOperandColumn] = DxuiListView::Cell();
            }

            operandMoved = false;

            if (heatPalette.has_value() && snapshot->heatColumns != nullptr)
            {
                cells[DisassemblyCells::kHeatColumn] = HeatStrip::GetCell (*snapshot->heatColumns, line->address, std::max<size_t> (1, HeatStrip::CountBytes (line->bytes)), *heatPalette, false);
            }

            //  An instruction with a source line is dimmed under it, whatever else
            //  is in view, so the dimming doesn't change as the view scrolls.
            if (theme != nullptr && DisassemblyCells::IsDimmed (m_options, source && line->sourceFileId >= 0, false, at == current))
            {
                DisassemblyCells::DimRow (cells, theme->Foreground(), theme->TextViewBackground());
            }

            //  A byte that changed is in the changed color, whether code or an
            //  edit changed it.
            for (const auto & [changedFirst, changedLast] : m_changes.GetChangedRanges (line->address, line->bytes))
            {
                cells[DisassemblyCells::kBytesColumn].colorRanges.push_back ({ changedFirst, changedLast, DebuggerTextColors::GetChangedOn (textColors, fill) });
            }
        }

        //  What each color on the row means, for the tip over its cell: the
        //  row's fill on every cell, then a changed byte's color. The
        //  operand's and result's colors are the same on every row, so the
        //  column's heading has them.
        cells[DisassemblyCells::kAddressColumn].tip     = rowTip;
        cells[DisassemblyCells::kLabelColumn].tip       = rowTip;
        cells[DisassemblyCells::kInstructionColumn].tip = (line != nullptr && line->dataOrigin != DataOrigin::None)
                                                              ? ColorLegend::JoinLines ({ DisassemblyCells::GetDataTip (line->dataOrigin), rowTip })
                                                              : rowTip;
        cells[DisassemblyCells::kOperandColumn].tip     = rowTip;
        cells[DisassemblyCells::kBytesColumn].tip       = ColorLegend::JoinLines ({ cells[DisassemblyCells::kBytesColumn].colorRanges.empty() ? L"" : ColorLegend::GetText (ColorLegend::Meaning::Changed), rowTip });

        //  The heat cell's color is drawn over the row's.
        for (DxuiListView::Cell & cell : cells)
        {
            cell.background = (cell.background != 0) ? cell.background : fill;
        }

        rows.push_back (std::move (cells));
    }

    list->SetRows (std::move (rows));

    if (SelectScrolledEdge (view, built))
    {
        m_host.CheckOperandTip (view);
        return;
    }

    //  The selection is an instruction, not a row: new rows move it to the
    //  row with its address, or clear it when none has it, so F9 and run to
    //  cursor act on what the user selected.
    if (list->GetSelectedRow() >= 0)
    {
        selected = -1;

        for (size_t i = 0; i < lines.size(); i++)
        {
            if (m_selected[(size_t) view] == lines[i].address)
            {
                selected = DisassemblyOptions::GetRowOfLine (built, (int) i);
                break;
            }
        }

        if (selected != list->GetSelectedRow())
        {
            list->SetSelectedRow (selected);
        }
    }

    if (current >= 0 && list->GetSelectedRow() < 0)
    {
        list->EnsureVisible (current);
    }

    m_host.CheckOperandTip (view);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::MarkRow
//
//  The gutter and row color of a row representing an instruction: its
//  breakpoint or the gray one under the pointer, the PC's marker and color
//  when the row is marked, and else the color of the row another pane
//  brought into view or of the branch's destination.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGroup::MarkRow (int view, int row, const DebuggerViewSnapshot::CodeLine & line, std::optional<Word> target, bool isMarked,
                                std::vector<DxuiListView::Cell> & cells, uint32_t & fill, std::wstring & rowTip)
{
    DxuiListView::Cell  & gutter = cells[DisassemblyCells::kGutterColumn];



    //  The glyph margin has a breakpoint's dot and the PC's arrow, the
    //  arrow drawn over the dot on a line with both, as Visual Studio
    //  draws them. A line that can take a breakpoint has a gray one under
    //  the pointer.
    if (line.hasBreakpoint)
    {
        gutter.icon = m_host.GetBreakpointIcon (line.isEnabled);
        gutter.tip  = ColorLegend::GetText (line.isEnabled ? ColorLegend::Meaning::BreakpointEnabled : ColorLegend::Meaning::BreakpointDisabled);
    }
    else if (row == m_gutter.GetHoverRow (view) && DisassemblyOptions::CanTakeBreakpoint (line))
    {
        gutter.icon = m_host.GetHoverBreakpointIcon();
        gutter.tip  = ColorLegend::GetText (ColorLegend::Meaning::BreakpointHover);
    }

    if (isMarked)
    {
        gutter.icon = m_host.GetPcMarker (gutter.icon);
        gutter.tip  = ColorLegend::JoinLines ({ ColorLegend::GetText (ColorLegend::Meaning::PcMarker), gutter.tip });
        fill        = m_host.GetPcRowArgb();
        rowTip      = ColorLegend::GetText (ColorLegend::Meaning::PcRow);
    }
    else if (view == m_navigatedView && m_navigatedTo.has_value() && *m_navigatedTo == line.address)
    {
        fill   = m_host.GetNavigatedRowArgb();
        rowTip = ColorLegend::GetText (ColorLegend::Meaning::NavigatedRow);
    }
    else if (target.has_value() && *target == line.address)
    {
        fill   = m_host.GetTargetRowArgb();
        rowTip = ColorLegend::GetText (ColorLegend::Meaning::TargetRow);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::ApplyHistory
//
//  The first view is laid out again when its band appears or goes.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGroup::ApplyHistory (const HistoryStatus & status)
{
    HistoryBand  * band     = m_views[0]->GetHistoryBand();
    bool           wasShown = HistoryBand::IsShown (band->GetStatus());
    bool           isShown  = HistoryBand::IsShown (status);



    band->SetStatus (status);

    if (wasShown != isShown)
    {
        m_views[0]->GetFrame()->Relayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::SendShownLines
//
//  With source rows, a view's lines run past its rows, and the CPU thread
//  uses how many are in view to bring the PC on screen and to center it;
//  only a change is sent.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGroup::SendShownLines (int view, const std::vector<DisassemblyOptions::Row> & built)
{
    IDebuggerWindowHost  * host  = m_host.GetWindowHost();
    int                    shown = DisassemblyOptions::CountShownLines (built, GetCodeList (view)->GetVisibleRowCapacity());



    if (host != nullptr && shown > 0 && shown != m_shownSentTo[(size_t) view])
    {
        m_shownSentTo[(size_t) view] = shown;
        host->SetDebuggerCodeShown (shown, view);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::UpdateLines
//
//  Each view has as many lines as it has room for, so a pane is full
//  whatever height it is dragged to. The count crosses to the CPU thread,
//  which builds the snapshot again, so only a change is sent. A view out of
//  sight measures nothing, and its count is not sent until it is visible.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGroup::UpdateLines()
{
    IDebuggerWindowHost  * host = m_host.GetWindowHost();



    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        int  fits = GetCodeList (view)->GetVisibleRowCapacity();

        if (m_isOpen[(size_t) view] && fits > 0 && fits != m_linesSentTo[(size_t) view] && host != nullptr)
        {
            m_linesSentTo[(size_t) view] = fits;
            host->SetDebuggerCodeLines (fits, view);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::ShowCode
//
//  An address goes to the view last used; the PC, to the one following it.
//  That view comes to the front, so the address or the PC is in view rather
//  than drawn on a tab behind another.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGroup::ShowCode (std::optional<Word> address)
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
    IDebuggerWindowHost         * host     = m_host.GetWindowHost();
    int                           view     = address.has_value() ? m_active : ((snapshot != nullptr) ? snapshot->followView : 0);



    m_navigatedTo   = address;
    m_navigatedView = view;

    //  A view brought forward from behind another tab sends its line count
    //  ahead of the address, which is centered for the count the view has.
    if (m_isOpen[(size_t) view])
    {
        m_host.ActivatePaneTab (DebuggerLayout::GetCodePaneId (view));
        m_host.RelayoutDock();
        UpdateLines();
    }

    if (host != nullptr)
    {
        host->SetDebuggerCodeAddress (address, view);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::FindPcSourceRow
//
//  The source row representing the PC's instruction, the first its line
//  produced, or -1 when the PC's instruction has no source row above it.
//
////////////////////////////////////////////////////////////////////////////////

int DisassemblyGroup::FindPcSourceRow (
    const std::vector<DebuggerViewSnapshot::CodeLine>  & lines,
    const std::vector<DisassemblyOptions::Row>         & built)
{
    int  row = -1;



    for (size_t i = 0; i < lines.size(); i++)
    {
        if (lines[i].isCurrent)
        {
            row = DisassemblyOptions::GetFrontRowOfLine (built, (int) i);
            return (row >= 0 && built[(size_t) row].codeLine < 0) ? row : -1;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::SelectEdgeAfterScroll
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGroup::SelectEdgeAfterScroll (int view, bool isTop)
{
    const std::vector<DebuggerViewSnapshot::CodeLine>  & lines = GetCodeLines (view);



    m_edgeAfterScroll[(size_t) view]  = isTop;
    m_edgeScrolledFrom[(size_t) view] = lines.empty() ? std::nullopt : std::optional<Word> (lines.front().address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::SelectScrolledEdge
//
//  Once a view's rows start at another line than when a key scrolled it,
//  the instruction at that edge is selected: the first, or the last one in
//  view. Rows from before the scroll don't change the selection.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGroup::SelectScrolledEdge (int view, const std::vector<DisassemblyOptions::Row> & built)
{
    const std::vector<DebuggerViewSnapshot::CodeLine>  & lines = GetCodeLines (view);
    DxuiListView                                       * list  = GetCodeList (view);
    std::optional<bool>                                  isTop = m_edgeAfterScroll[(size_t) view];
    int                                                  row   = 0;
    int                                                  line  = -1;



    if (!isTop.has_value() || lines.empty() || built.empty() || m_edgeScrolledFrom[(size_t) view] == lines.front().address)
    {
        return false;
    }

    m_edgeAfterScroll[(size_t) view] = std::nullopt;

    row = *isTop ? 0 : std::min ((int) built.size(), std::max (1, list->GetVisibleRowCapacity())) - 1;

    //  A source row has no instruction; the nearest code row toward the
    //  middle is selected.
    while (row >= 0 && row < (int) built.size() && built[(size_t) row].codeLine < 0)
    {
        row += *isTop ? 1 : -1;
    }

    line = (row >= 0 && row < (int) built.size()) ? built[(size_t) row].codeLine : -1;

    if (line < 0)
    {
        return false;
    }

    m_selected[(size_t) view] = lines[(size_t) line].address;
    list->SetSelectedRow (row);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::OnRowSelected
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGroup::OnRowSelected (int view, int row)
{
    const std::vector<DebuggerViewSnapshot::CodeLine>  & lines = GetCodeLines (view);
    int                                                  index = GetCodeLineOfRow (view, row);



    m_active                         = view;
    m_selected[(size_t) view]        = std::nullopt;
    m_edgeAfterScroll[(size_t) view] = std::nullopt;

    if (index >= 0)
    {
        m_selected[(size_t) view] = lines[(size_t) index].address;
        m_sources.ShowLine (lines[(size_t) index].sourceFileId, lines[(size_t) index].sourceLine);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::TryGetSourceText
//
//  A source line's text for the source rows, read through the host as a
//  source document reads its file, once per file while the same debug file
//  and program are loaded. A file not found is looked up again with the next
//  snapshot, since a dropped or located source can be found then.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGroup::TryGetSourceText (int fileId, int line, std::wstring & text)
{
    const DebuggerViewSnapshot               * snapshot = m_host.GetLatestSnapshot();
    IDebuggerWindowHost                      * host     = m_host.GetWindowHost();
    const DebuggerViewSnapshot::SourceState  * source   = nullptr;
    std::wstring                               key;
    SourceLookup                               lookup;



    if (snapshot == nullptr || !snapshot->source.has_value() || host == nullptr || line <= 0)
    {
        return false;
    }

    source = &*snapshot->source;
    key    = source->debugFilePath + TextEncoding::NarrowToWide (source->programKey);

    if (m_sourceKey != key)
    {
        m_sourceKey = key;
        m_sourceText.clear();
        m_sourceMisses.clear();
    }

    if (m_missesFor != snapshot)
    {
        m_missesFor = snapshot;
        m_sourceMisses.clear();
    }

    if (!m_sourceText.contains (fileId) && !m_sourceMisses.contains (fileId))
    {
        for (const DebugSourceFile & record : source->files)
        {
            if (record.id == fileId)
            {
                lookup = host->FindDebuggerSource (record, source->debugFilePath, source->programKey);
            }
        }

        if (lookup.match != SourceMatch::NotFound && !lookup.text.empty())
        {
            m_sourceText[fileId] = SourcePane::SplitLines (lookup.text);
        }
        else
        {
            m_sourceMisses.insert (fileId);
        }
    }

    if (!m_sourceText.contains (fileId) || line > (int) m_sourceText[fileId].size())
    {
        return false;
    }

    text = m_sourceText[fileId][(size_t) (line - 1)];
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::GetBranchArrow
//
//  Where the PC's branch, jump or call arrow lies in a view, in pixels, and
//  where it goes, or with isBranchedFrom, the arrow of the one a step just
//  took to the PC. Where both are drawn, they are set apart at the PC's row.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGroup::GetBranchArrow (int view, BranchArrow::Input & input, Word & goesTo, bool & isTaken, bool isBranchedFrom) const
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
//  DisassemblyGroup::GetOneBranchArrow
//
//  One of the arrows, from the snapshot's addresses, so either end scrolled
//  out of view runs the line to that edge of the rows, and the arrow is
//  drawn while any part of it crosses them. Without the PC's target in the
//  snapshot, as while running, the PC's arrow is drawn from its line. A
//  view past the code views is a source document's, whose pane places the
//  PC's arrow alone.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGroup::GetOneBranchArrow (int view, BranchArrow::Input & input, Word & goesTo, bool & isTaken, bool isBranchedFrom) const
{
    const DebuggerViewSnapshot   * snapshot = m_host.GetLatestSnapshot();
    DxuiListView                 * list     = (view < DebuggerViewState::kMaxCodeViews) ? GetCodeList (view) : nullptr;
    DisassemblyCells::ArrowEnds    ends;



    if (!m_options.IsOn (DisassemblyOptions::Option::BranchArrow))
    {
        return false;
    }

    if (view >= DebuggerViewState::kMaxCodeViews)
    {
        const SourceDocumentPane & document = m_sources.GetDocument (view - DebuggerViewState::kMaxCodeViews);

        return !isBranchedFrom && document.IsShown() && m_host.IsRoutable (document.GetView()) && document.GetModel().GetBranchArrow (input, goesTo, isTaken);
    }

    if (list == nullptr || !m_isOpen[(size_t) view] || !list->IsVisible() || !m_host.IsRoutable (list))
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
    else if (!DisassemblyCells::TryGetPcArrowEnds (GetCodeLines (view), ends))
    {
        return false;
    }

    goesTo  = ends.goesTo;
    isTaken = ends.isTaken;

    return DisassemblyCells::TryGetBranchArrow (*list, GetCodeLines (view), m_views[(size_t) view]->GetRows(), ends, input);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::PaintBranchArrow
//
//  The arrow a step just took, dotted, and the PC's, in the PC marker's
//  color, or in the disabled color for a branch that the current flags do
//  not take.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGroup::PaintBranchArrow (IDxuiPainter & painter, int view)
{
    const DxuiTheme     * theme   = m_host.GetTheme();
    BranchArrow::Input    input;
    Word                  goesTo  = 0;
    bool                  isTaken = true;



    for (bool isBranchedFrom : { true, false })
    {
        input = BranchArrow::Input();

        if (GetBranchArrow (view, input, goesTo, isTaken, isBranchedFrom))
        {
            DisassemblyCells::PaintBranchArrow (painter, input, isTaken, m_host.GetPcMarkerArgb(), (theme != nullptr) ? theme->ForegroundDisabled() : 0xFF808080,
                                                isBranchedFrom);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::ClickBranchArrow
//
//  A click on a view's branch arrow goes to where it points, as Go to
//  would; the next click on it goes back to the PC.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGroup::ClickBranchArrow (POINT pointPx)
{
    BranchArrow::Input  input;
    Word                goesTo  = 0;
    bool                isTaken = true;



    for (int view = 0; view < DebuggerViewState::kMaxCodeViews + SourceDocuments::kMaxDocuments; view++)
    {
        input = BranchArrow::Input();

        if (!GetBranchArrow (view, input, goesTo, isTaken) ||
            !BranchArrow::HitTest (input, (float) pointPx.x, (float) pointPx.y))
        {
            continue;
        }

        m_active = (view < DebuggerViewState::kMaxCodeViews) ? view : m_active;

        if (m_isArrowAtTarget)
        {
            m_host.ShowPcCode();
        }
        else
        {
            m_host.GoToCode (goesTo);
        }

        m_isArrowAtTarget = !m_isArrowAtTarget;
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::TryGetBranchTip
//
//  Over the PC's arrow, whether its branch will be taken, and why, from the
//  instruction at the PC and the flags now; over the dotted arrow, the
//  branch the last step took to the PC.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGroup::TryGetBranchTip (POINT clientPx, RECT & anchor, std::wstring & text) const
{
    const DebuggerViewSnapshot  * snapshot    = m_host.GetLatestSnapshot();
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
        for (const DebuggerViewSnapshot::CodeLine & line : GetCodeLines (view))
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
//  DisassemblyGroup::TryGetSymbolTip
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGroup::TryGetSymbolTip (POINT clientPx, RECT & anchor, std::wstring & text) const
{
    DxuiListView * list    = nullptr;
    RECT           bounds  = {};
    int            view    = -1;
    int            row     = -1;
    int            column  = -1;
    int            index   = -1;
    std::string    name;
    Word           address = 0;
    const char   * about   = nullptr;
    std::string    comment;



    for (int each = 0; each < DebuggerViewState::kMaxCodeViews; each++)
    {
        DxuiListView  * code = GetCodeList (each);

        if (m_host.IsRoutable (code) && code->IsVisible() && DxuiDockSite::Contains (code->GetBounds(), clientPx))
        {
            list   = code;
            view   = each;
            bounds = code->GetBounds();
        }
    }

    if (m_host.GetLatestSnapshot() == nullptr || list == nullptr)
    {
        return false;
    }

    row    = list->HitTestRow (clientPx.x - bounds.left, clientPx.y - bounds.top);
    column = m_host.GetListColumnAt (list, clientPx.x - bounds.left);
    index  = GetCodeLineOfRow (view, row);

    if (index < 0)
    {
        return TryGetSourceSymbolTip (*list, clientPx, anchor, text);
    }

    const DebuggerViewSnapshot::CodeLine & line = GetCodeLines (view)[(size_t) index];

    if (column == (int) DisassemblyCells::kInstructionColumn - 1 && !line.label.empty())
    {
        name    = line.label;
        address = line.address;
    }
    else if (column == (int) DisassemblyCells::kInstructionColumn)
    {
        //  The operand text differs from the address only for a symbol.
        for (char ch : (line.shownOperand != line.memoryOperand) ? line.shownOperand : std::string())
        {
            if (std::isalnum ((unsigned char) ch) || ch == '_')
            {
                name += ch;
            }
            else if (!name.empty())
            {
                break;
            }
        }

        for (char ch : line.memoryOperand)
        {
            if (std::isxdigit ((unsigned char) ch))
            {
                address = (Word) ((address << 4) | (Word) std::stoi (std::string (1, ch), nullptr, 16));
            }
            else if (ch == ',' || ch == ')')
            {
                break;
            }
        }
    }

    if (!list->GetCellTextRectPx (row, (size_t) column, anchor))
    {
        return false;
    }

    //  An operand with no symbol at a card's or port's register has the
    //  register's description.
    if (name.empty())
    {
        OffsetRect (&anchor, bounds.left, bounds.top);
        return column == (int) DisassemblyCells::kInstructionColumn && TryGetIoRegisterTip (m_host.GetLatestSnapshot()->ioRegisters, address, text);
    }

    about = SymbolDescriptions::Find (name);
    text  = std::format (L"{} = ${:04X}", TextEncoding::NarrowToWide (name), address);

    if (about != nullptr)
    {
        text += L"\n" + TextEncoding::NarrowToWide (about);
    }

    comment = m_host.GetSymbolComment (address, name);

    if (!comment.empty())
    {
        text += L"\n" + TextEncoding::NarrowToWide (comment);
    }

    OffsetRect (&anchor, bounds.left, bounds.top);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::TryGetSourceSymbolTip
//
//  Over a word of a source row, the symbol it is.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGroup::TryGetSourceSymbolTip (const DxuiListView & list, POINT clientPx, RECT & anchor, std::wstring & text) const
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
    RECT                          bounds   = list.GetBounds();
    int                           row      = -1;
    size_t                        column   = 0;
    int                           ch       = 0;



    if (snapshot == nullptr || snapshot->symbols == nullptr ||
        !list.TryHitTestCellChar (POINT { clientPx.x - bounds.left, clientPx.y - bounds.top }, row, column, ch) ||
        !SourcePane::TryGetSymbolText (list.GetCellsOfRow (row)[column].text, ch, snapshot->symbols->symbols, text))
    {
        return false;
    }

    anchor = RECT { clientPx.x, clientPx.y, clientPx.x + 1, clientPx.y + 1 };
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::TryGetIoRegisterTip
//
//  "$C0A9" and the register's description on the line below, for an
//  address with one.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGroup::TryGetIoRegisterTip (const std::vector<DiagnosticsIoRegister> & registers, Word address, std::wstring & text)
{
    for (const DiagnosticsIoRegister & each : registers)
    {
        if (each.address == address)
        {
            text = std::format (L"${:04X}\n{}", address, TextEncoding::NarrowToWide (each.description));
            return true;
        }
    }

    return false;
}
