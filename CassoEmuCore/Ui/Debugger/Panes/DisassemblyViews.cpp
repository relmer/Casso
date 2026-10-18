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
    m_bar         (host, DebuggerLayout::GetCodePaneId (view), L"Disassembly options")
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

std::wstring DisassemblyViewPane::GetId() const
{
    return DebuggerLayout::GetCodePaneId (m_view);
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
//  DisassemblyViewPane::CreateControls
//
//  The first view's history band has its place in history and a Go live
//  link.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyViewPane::CreateControls()
{
    DxuiPanel  & parent = m_host.GetControlParent();



    m_list = parent.CreateChild<DxuiListView>();
    m_bar.Create();

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
//  DisassemblyViewPane::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> DisassemblyViewPane::GetControls() const
{
    return { m_bar.GetSlot(), m_bar.GetBar(), m_list };
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
//  What was right-clicked leads (FR-084): the instruction's operand,
//  resolved through its addressing mode, or the line's own address; one
//  entry for each memory pane open.
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

    const DebuggerViewSnapshot::CodeLine  & line = m_group.GetCodeLines (m_view)[(size_t) index];
    Word                                    at   = line.address;



    if (column >= (int) DisassemblyCells::kInstructionColumn && !line.memoryOperand.empty())
    {
        m_host.AddShowInMemoryItems (TextEncoding::NarrowToWide (line.shownOperand), line.memoryOperand, items);
    }
    else
    {
        m_host.AddShowInMemoryItems (std::format (L"${:04X}", at), std::format ("{:04X}", at), items);
    }

    items.push_back ({ snapshot->code[(size_t) row].hasBreakpoint ? L"Remove breakpoint" : L"Insert breakpoint",
                       [this, at] { m_host.RunAction (DebuggerActions::GetToggleBreakpoint (*m_host.GetLatestSnapshot(), at, m_host.GetMode())); } });
    items.push_back ({ L"Run to cursor",       [this, at] { m_host.RunAction (DebuggerActions::GetRunToCursor (at)); } });

    if (snapshot->isPaused && DisassemblyOptions::CanTakeBreakpoint (line))
    {
        items.push_back ({ L"Set next statement", [this, at] { m_group.GetGutter().SetNextStatement (at); } });
    }

    items.push_back ({ L"Show next statement", [this]     { m_host.ShowPcCode(); } });
    items.push_back ({ L"Copy",                [this]     { DxuiClipboard::SetText (m_host.GetOwnerHwnd(), m_list->GetSelectionText()); } });

    m_group.GetEdits().AddItems (m_view, line, row, items);

    if (m_view > 0)
    {
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
            mark = DxuiTabGroup::LeadingMark { s_kpszTriangleRight, DxuiTheme::kMonoFace, m_host.GetPcMarkerArgb() };
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
//  columns. The viewing options hide the address, bytes and label columns.
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
    bool                                                 hasSourceRows = false;
    std::wstring                                         text;
    std::optional<HeatMapView::Palette>                  heatPalette;



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

    hasSourceRows = std::ranges::any_of (built, [] (const DisassemblyOptions::Row & row) { return row.codeLine < 0; });

    for (const DisassemblyOptions::Row & each : built)
    {
        std::vector<DxuiListView::Cell>          cells (DisassemblyCells::kColumnCount);
        uint32_t                                 fill    = 0;
        const DebuggerViewSnapshot::CodeLine   * line    = nullptr;
        bool                                     hasText = false;
        std::wstring                             rowTip;

        //  A source line: its number when on, then its text in the syntax
        //  colors, from the first column after the glyph margin.
        if (each.codeLine < 0)
        {
            hasText = TryGetSourceText (each.fileId, each.sourceLine, text);
            text    = hasText ? text : std::wstring();

            DisassemblyCells::FillSourceRow (cells, *list, each.sourceLine, text, numbers, textColors, (theme != nullptr) ? theme->ForegroundMuted() : 0u);

            if (theme != nullptr && DisassemblyCells::IsDimmed (m_options, hasSourceRows, true, false))
            {
                DisassemblyCells::DimRow (cells, theme->Foreground(), theme->TextViewBackground());
            }

            rows.push_back (std::move (cells));
            continue;
        }

        line = &lines[(size_t) each.codeLine];

        //  The glyph margin has a breakpoint's dot and the PC's arrow, the
        //  arrow drawn over the dot on a line with both, as Visual Studio
        //  draws them. A line that can take a breakpoint has a gray one under
        //  the pointer.
        if (line->hasBreakpoint)
        {
            cells[DisassemblyCells::kGutterColumn].icon = m_host.GetBreakpointIcon (line->isEnabled);
            cells[DisassemblyCells::kGutterColumn].tip  = ColorLegend::GetText (line->isEnabled ? ColorLegend::Meaning::BreakpointEnabled : ColorLegend::Meaning::BreakpointDisabled);
        }
        else if ((int) rows.size() == m_gutter.GetHoverRow (view) && DisassemblyOptions::CanTakeBreakpoint (*line))
        {
            cells[DisassemblyCells::kGutterColumn].icon = m_host.GetHoverBreakpointIcon();
            cells[DisassemblyCells::kGutterColumn].tip  = ColorLegend::GetText (ColorLegend::Meaning::BreakpointHover);
        }

        //  While the PC's arrow is dragged, it and the PC's row color are on
        //  the line under the pointer instead.
        current = line->isCurrent ? (int) rows.size() : current;

        if ((m_gutter.GetDragRow (view) >= 0) ? (int) rows.size() == m_gutter.GetDragRow (view) : line->isCurrent)
        {
            cells[DisassemblyCells::kGutterColumn].icon = m_host.GetPcMarker (cells[DisassemblyCells::kGutterColumn].icon);
            cells[DisassemblyCells::kGutterColumn].tip  = ColorLegend::JoinLines ({ ColorLegend::GetText (ColorLegend::Meaning::PcMarker), cells[DisassemblyCells::kGutterColumn].tip });
            fill                                        = m_host.GetPcRowArgb();
            rowTip                                      = ColorLegend::GetText (ColorLegend::Meaning::PcRow);
        }
        else if (view == m_navigatedView && m_navigatedTo.has_value() && *m_navigatedTo == line->address)
        {
            fill   = m_host.GetNavigatedRowArgb();
            rowTip = ColorLegend::GetText (ColorLegend::Meaning::NavigatedRow);
        }
        else if (target.has_value() && *target == line->address)
        {
            fill   = m_host.GetTargetRowArgb();
            rowTip = ColorLegend::GetText (ColorLegend::Meaning::TargetRow);
        }

        (void) DisassemblyCells::FillCodeRow (cells, *line, m_options, textColors);

        if (heatPalette.has_value() && snapshot->heatColumns != nullptr)
        {
            cells[DisassemblyCells::kHeatColumn] = HeatStrip::GetCell (*snapshot->heatColumns, line->address, std::max<size_t> (1, HeatStrip::CountBytes (line->bytes)), *heatPalette, false);
        }

        if (theme != nullptr && DisassemblyCells::IsDimmed (m_options, hasSourceRows, false, line->isCurrent))
        {
            DisassemblyCells::DimRow (cells, theme->Foreground(), theme->TextViewBackground());
        }

        //  A byte that changed is in the changed color, whether code or an
        //  edit changed it.
        for (const auto & [first, last] : m_changes.GetChangedRanges (line->address, line->bytes))
        {
            cells[DisassemblyCells::kBytesColumn].colorRanges.push_back ({ first, last, DebuggerTextColors::GetChangedOn (textColors, fill) });
        }

        //  What each color on the row means, for the tip over its cell: the
        //  row's fill on every cell, then a changed byte's color. The
        //  operand's and result's colors are the same on every row, so the
        //  column's heading has them.
        cells[DisassemblyCells::kAddressColumn].tip     = rowTip;
        cells[DisassemblyCells::kLabelColumn].tip       = rowTip;
        cells[DisassemblyCells::kInstructionColumn].tip = rowTip;
        cells[DisassemblyCells::kOperandColumn].tip     = rowTip;
        cells[DisassemblyCells::kBytesColumn].tip       = ColorLegend::JoinLines ({ cells[DisassemblyCells::kBytesColumn].colorRanges.empty() ? L"" : ColorLegend::GetText (ColorLegend::Meaning::Changed), rowTip });

        for (DxuiListView::Cell & cell : cells)
        {
            cell.background = fill;
        }

        rows.push_back (std::move (cells));
    }

    list->SetRows (std::move (rows));

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

    if (m_isOpen[(size_t) view])
    {
        m_host.ActivatePaneTab (DebuggerLayout::GetCodePaneId (view));
        m_host.RelayoutDock();
    }

    if (host != nullptr)
    {
        host->SetDebuggerCodeAddress (address, view);
    }
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



    m_active                  = view;
    m_selected[(size_t) view] = std::nullopt;

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
//  is loaded.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGroup::TryGetSourceText (int fileId, int line, std::wstring & text)
{
    const DebuggerViewSnapshot               * snapshot = m_host.GetLatestSnapshot();
    IDebuggerWindowHost                      * host     = m_host.GetWindowHost();
    const DebuggerViewSnapshot::SourceState  * source   = nullptr;
    SourceLookup                               lookup;



    if (snapshot == nullptr || !snapshot->source.has_value() || host == nullptr || line <= 0)
    {
        return false;
    }

    source = &*snapshot->source;

    if (m_sourceKey != source->debugFilePath)
    {
        m_sourceKey = source->debugFilePath;
        m_sourceText.clear();
    }

    if (!m_sourceText.contains (fileId))
    {
        for (const DebugSourceFile & record : source->files)
        {
            if (record.id == fileId)
            {
                lookup = host->FindDebuggerSource (record, source->debugFilePath, source->programKey);
            }
        }

        m_sourceText[fileId] = SourcePane::SplitLines (lookup.text);
    }

    if (line > (int) m_sourceText[fileId].size())
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
//  took to the PC. Either end scrolled out of view runs the line to that
//  edge of the rows, so the arrow is drawn while any part of it crosses
//  them. A view past the code views is a source document's, whose pane
//  places the PC's arrow alone.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGroup::GetBranchArrow (int view, BranchArrow::Input & input, Word & goesTo, bool & isTaken, bool isBranchedFrom) const
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
    DxuiListView                * list     = (view < DebuggerViewState::kMaxCodeViews) ? GetCodeList (view) : nullptr;



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

    if (isBranchedFrom && (snapshot == nullptr || !snapshot->branchedFrom.has_value()))
    {
        return false;
    }

    return DisassemblyCells::TryGetBranchArrow (*list, GetCodeLines (view), m_views[(size_t) view]->GetRows(), input, goesTo, isTaken,
                                                isBranchedFrom ? snapshot->branchedFrom : std::nullopt);
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
//  Whether the branch is taken, and why, from the instruction at the PC and
//  the flags now.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGroup::TryGetBranchTip (POINT clientPx, RECT & anchor, std::wstring & text) const
{
    BranchArrow::Input  input;
    Word                goesTo      = 0;
    bool                isTaken     = true;
    std::string         instruction;



    if (m_host.GetLatestSnapshot() == nullptr)
    {
        return false;
    }

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        for (const DebuggerViewSnapshot::CodeLine & line : GetCodeLines (view))
        {
            instruction = line.isCurrent ? line.instruction : instruction;
        }
    }

    for (int view = 0; view < DebuggerViewState::kMaxCodeViews + SourceDocuments::kMaxDocuments; view++)
    {
        input = BranchArrow::Input();

        if (!GetBranchArrow (view, input, goesTo, isTaken) ||
            !BranchArrow::HitTest (input, (float) clientPx.x, (float) clientPx.y))
        {
            continue;
        }

        anchor = RECT { clientPx.x, clientPx.y, clientPx.x + 1, clientPx.y + 1 };
        text   = ColorLegend::GetBranchTip (instruction, isTaken, m_host.GetRegisterByte ("P"));
        return true;
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
        return false;
    }

    const DebuggerViewSnapshot::CodeLine & line = GetCodeLines (view)[(size_t) index];

    if (column == (int) DisassemblyCells::kInstructionColumn - 1 && !line.label.empty())
    {
        name    = line.label;
        address = line.address;
    }
    else if (column == (int) DisassemblyCells::kInstructionColumn && line.shownOperand != line.memoryOperand)
    {
        for (char ch : line.shownOperand)
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

    if (name.empty() || !list->GetCellTextRectPx (row, (size_t) column, anchor))
    {
        return false;
    }

    about = SymbolDescriptions::Find (name);
    text  = std::format (L"{} = ${:04X}", TextEncoding::NarrowToWide (name), address);

    if (about != nullptr)
    {
        text += L"\n" + TextEncoding::NarrowToWide (about);
    }

    OffsetRect (&anchor, bounds.left, bounds.top);
    return true;
}
