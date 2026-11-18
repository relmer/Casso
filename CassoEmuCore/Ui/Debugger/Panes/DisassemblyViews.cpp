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
#include "Ui/Debugger/SourceLineText.h"
#include "Ui/Debugger/Panes/CodeEditCommands.h"
#include "Ui/Debugger/Panes/CodeViewFind.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/DisassemblyGutter.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/SourceRows.h"
#include "Ui/Debugger/ToolbarCheckEntry.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane::~DisassemblyViewPane
//
////////////////////////////////////////////////////////////////////////////////

DisassemblyViewPane::~DisassemblyViewPane() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::DisassemblyGroup
//
//  The options the user last left.
//
////////////////////////////////////////////////////////////////////////////////

DisassemblyGroup::DisassemblyGroup (IDebuggerPaneHost & host, DisassemblyGutter & gutter, CodeEditCommands & edits, SourceDocumentGroup & sources) :
    m_host       (host),
    m_gutter     (gutter),
    m_edits      (edits),
    m_sources    (sources),
    m_sourceText (host),
    m_symbols    (host, *this)
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
//  DisassemblyGroup::GetMenuLineOfRow
//
////////////////////////////////////////////////////////////////////////////////

int DisassemblyGroup::GetMenuLineOfRow (int view, int row) const
{
    int  line = -1;



    if (view < 0 || view >= DebuggerViewState::kMaxCodeViews)
    {
        return -1;
    }

    line = DisassemblyOptions::GetLineAtOrBelow (m_views[(size_t) view]->GetRows(), row);

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
    RowContext                                    context      = MakeRowContext (view);
    std::vector<std::vector<DxuiListView::Cell>>  rows;
    std::vector<DisassemblyCells::SourceParts>    parts;
    bool                                          operandMoved = false;



    for (const DisassemblyOptions::Row & each : context.built)
    {
        rows.push_back (BuildRow (view, context, each, (int) rows.size(), operandMoved, parts));
    }

    context.list->SetRows (std::move (rows));
    m_views[(size_t) view]->SetSourceParts (std::move (parts));
    SelectAsked (view, *context.snapshot, context.lines, context.built);

    if (!SelectScrolledEdge (view, context.built))
    {
        KeepSelection (view, context);
    }

    m_host.CheckOperandTip (view);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::MakeRowContext
//
//  What every row of a view is drawn with: the rows the options build from
//  its lines, the PC's row, the row marked as the PC's, and the branch's
//  destination. The view's rows, visible line count and address scroll are
//  set here, before any row is drawn.
//
//  With source lines on, a source row represents the first instruction its
//  line produced: the instruction's breakpoint, PC marker and row color are
//  on the source row, and the instruction is dimmed under it as the others
//  are. While the PC's arrow is dragged, the marker and the PC's row color
//  are on the row under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

DisassemblyGroup::RowContext DisassemblyGroup::MakeRowContext (int view)
{
    RowContext    context     { m_host.GetLatestSnapshot(), m_host.GetTheme(), GetCodeList (view), GetCodeLines (view) };
    std::wstring  text;
    int           pcSourceRow = -1;



    context.textColors = m_host.GetTextColors();
    context.source     = m_options.IsOn (DisassemblyOptions::Option::Source) && IsOptionEnabled (DisassemblyOptions::Option::Source);
    context.numbers    = m_options.IsOn (DisassemblyOptions::Option::LineNumbers) && IsOptionEnabled (DisassemblyOptions::Option::LineNumbers);

    DisassemblyCells::ShowColumns (*context.list, m_options);

    if (m_options.IsOn (DisassemblyOptions::Option::Heat))
    {
        context.heatPalette = m_host.GetHeatPalette();
    }

    for (const DebuggerViewSnapshot::CodeLine & line : context.lines)
    {
        if (line.isCurrent && line.target.has_value())
        {
            context.target = line.target;
        }
    }

    context.built = DisassemblyOptions::BuildRows (context.lines, context.source,
                                                   [this, &text] (int fileId, int line) { return m_sourceText.TryGet (fileId, line, text); },
                                                   [this, &text] (int fileId, int line) { return m_sourceText.TryGet (fileId, line, text) && SourceLineText::IsLabelLine (text); });
    m_views[(size_t) view]->SetRows (context.built);
    SendShownLines (view, context.built);
    SetAddressScroll (view, context.lines, context.built);
    m_views[(size_t) view]->SetStaleText (DisassemblyViewPane::GetStaleText (context.snapshot->staleFileNames, context.snapshot->source.has_value() && context.snapshot->source->isStale));

    pcSourceRow     = FindPcSourceRow (context.lines, context.built);
    context.current = (pcSourceRow >= 0) ? pcSourceRow : -1;

    for (size_t i = 0; i < context.lines.size() && context.current < 0; i++)
    {
        context.current = context.lines[i].isCurrent ? DisassemblyOptions::GetRowOfLine (context.built, (int) i) : context.current;
    }

    context.marked = (m_gutter.GetDragRow (view) >= 0) ? m_gutter.GetDragRow (view) : context.current;

    return context;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::BuildRow
//
//  One row's cells: its marks, a source line's or an instruction's cells,
//  the tips with what its colors mean, and its fill. ioOperandMoved is set
//  by a source row that took its one instruction's operand, and the
//  instruction's row below it then leaves its operand cell empty.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Cell> DisassemblyGroup::BuildRow (
    int                                           view,
    const RowContext                            & context,
    const DisassemblyOptions::Row               & each,
    int                                           at,
    bool                                        & ioOperandMoved,
    std::vector<DisassemblyCells::SourceParts>  & parts)
{
    std::vector<DxuiListView::Cell>          cells (DisassemblyCells::kColumnCount);
    int                                      gutter = DisassemblyOptions::GetGutterLineOfRow (context.built, at);
    uint32_t                                 fill   = 0;
    const DebuggerViewSnapshot::CodeLine   * line   = nullptr;
    std::wstring                             rowTip;



    parts.emplace_back();

    if (gutter >= 0)
    {
        MarkRow (view, at, context.lines[(size_t) gutter], context.target, at == context.marked, cells, fill, rowTip);
    }

    if (each.codeLine < 0)
    {
        parts.back()   = FillSourceRow (context, each, at, gutter, cells, ioOperandMoved);
    }
    else
    {
        line           = &context.lines[(size_t) each.codeLine];
        ioOperandMoved = FillCodeRow (context, *line, at, fill, ioOperandMoved, cells);
    }

    SetRowTips (cells, line, rowTip);

    //  The heat cell's color is drawn over the row's.
    for (DxuiListView::Cell & cell : cells)
    {
        cell.background = (cell.background != 0) ? cell.background : fill;
    }

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::FillSourceRow
//
//  A source line: its number when on, then its text in the syntax colors,
//  from the first column after the glyph margin. A line that produced one
//  instruction has the instruction's operand and result too, and the
//  instruction's row doesn't; outOperandMoved is set for that. The line's
//  heat is its instructions' heat. Returns where the row's parts are.
//
////////////////////////////////////////////////////////////////////////////////

DisassemblyCells::SourceParts DisassemblyGroup::FillSourceRow (
    const RowContext                 & context,
    const DisassemblyOptions::Row    & each,
    int                                at,
    int                                gutter,
    std::vector<DxuiListView::Cell>  & cells,
    bool                             & outOperandMoved)
{
    std::vector<DxuiListView::Cell>  scratch (DisassemblyCells::kColumnCount);
    DisassemblyCells::SourceParts    parts;
    std::wstring                     text;
    bool                             hasText = m_sourceText.TryGet (each.fileId, each.sourceLine, text);
    Word                             first   = 0;
    size_t                           count   = 0;



    text            = hasText ? text : std::wstring();
    outOperandMoved = gutter >= 0 && DisassemblyOptions::IsSingleInstruction (context.built, at) && SourceLineText::IsInstructionLine (text);

    if (outOperandMoved)
    {
        (void) DisassemblyCells::FillCodeRow (scratch, context.lines[(size_t) gutter], m_options, context.textColors);
        DisassemblySymbols::AddOperandSymbol (scratch[DisassemblyCells::kOperandColumn], context.lines[(size_t) gutter], context.textColors.syntax.symbol);
    }

    parts = DisassemblyCells::FillSourceRow (cells, *context.list, each.sourceLine, text, context.numbers, context.textColors,
                                             (context.theme != nullptr) ? context.theme->ForegroundMuted() : 0u,
                                             outOperandMoved ? &scratch[DisassemblyCells::kOperandColumn] : nullptr);

    if (context.heatPalette.has_value() && context.snapshot->heatColumns != nullptr &&
        DisassemblyCells::TryGetSourceRowSpan (context.built, (size_t) at, context.lines, first, count))
    {
        cells[DisassemblyCells::kHeatColumn] = HeatStrip::GetCell (*context.snapshot->heatColumns, first, count, *context.heatPalette, false);
    }

    if (context.theme != nullptr && DisassemblyCells::IsDimmed (m_options, context.source, true, at == context.current))
    {
        DisassemblyCells::DimRow (cells, context.theme->Foreground(), context.theme->TextViewBackground());
    }

    return parts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::FillCodeRow
//
//  An instruction's row. Its operand is left off when the source row above
//  took it. An instruction with a source line is dimmed under it, whatever
//  else is in view, so the dimming doesn't change as the view scrolls; a
//  line whose file isn't found has no source row, so its instruction isn't
//  dimmed. A byte that changed is in the changed color, whether code or an
//  edit changed it. Returns false, for the next row's operandMoved.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGroup::FillCodeRow (
    const RowContext                       & context,
    const DebuggerViewSnapshot::CodeLine   & line,
    int                                      at,
    uint32_t                                 fill,
    bool                                     isOperandMoved,
    std::vector<DxuiListView::Cell>        & cells)
{
    std::wstring  text;
    bool          isDimmable = context.theme != nullptr;
    bool          hasSource  = isDimmable && context.source && line.sourceFileId >= 0 && m_sourceText.TryGet (line.sourceFileId, line.sourceLine, text);



    (void) DisassemblyCells::FillCodeRow (cells, line, m_options, context.textColors);
    DisassemblySymbols::AddOperandSymbol (cells[DisassemblyCells::kOperandColumn], line, context.textColors.syntax.symbol);

    if (isOperandMoved && DisassemblyOptions::IsFronted (context.built, at))
    {
        cells[DisassemblyCells::kOperandColumn] = DxuiListView::Cell();
    }

    if (context.heatPalette.has_value() && context.snapshot->heatColumns != nullptr)
    {
        cells[DisassemblyCells::kHeatColumn] = HeatStrip::GetCell (*context.snapshot->heatColumns, line.address, std::max<size_t> (1, HeatStrip::CountBytes (line.bytes)), *context.heatPalette, false);
    }

    if (isDimmable && DisassemblyCells::IsDimmed (m_options, hasSource, false, at == context.current))
    {
        DisassemblyCells::DimRow (cells, context.theme->Foreground(), context.theme->TextViewBackground());
    }

    for (const auto & [changedFirst, changedLast] : m_changes.GetChangedRanges (line.address, line.bytes))
    {
        cells[DisassemblyCells::kBytesColumn].colorRanges.push_back ({ changedFirst, changedLast, DebuggerTextColors::GetChangedOn (context.textColors, fill) });
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::SetRowTips
//
//  What each color on the row means, for the tip over its cell: the row's
//  fill on every cell, then a changed byte's color. The operand's and
//  result's colors are the same on every row, so the column's heading has
//  them.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGroup::SetRowTips (
    std::vector<DxuiListView::Cell>        & cells,
    const DebuggerViewSnapshot::CodeLine   * line,
    const std::wstring                     & rowTip)
{
    cells[DisassemblyCells::kAddressColumn].tip     = rowTip;
    cells[DisassemblyCells::kLabelColumn].tip       = rowTip;
    cells[DisassemblyCells::kInstructionColumn].tip = (line != nullptr && line->dataOrigin != DataOrigin::None)
                                                          ? ColorLegend::JoinLines ({ DisassemblyCells::GetDataTip (line->dataOrigin), rowTip })
                                                          : rowTip;
    cells[DisassemblyCells::kOperandColumn].tip     = rowTip;
    cells[DisassemblyCells::kBytesColumn].tip       = ColorLegend::JoinLines ({ cells[DisassemblyCells::kBytesColumn].colorRanges.empty() ? L"" : ColorLegend::GetText (ColorLegend::Meaning::Changed), rowTip });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::KeepSelection
//
//  The selection is an instruction, not a row: new rows move it to the row
//  with its address, or clear it when none has it, so F9 and run to cursor
//  act on what the user selected. With nothing selected, the PC's row is
//  scrolled into view.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGroup::KeepSelection (int view, const RowContext & context)
{
    int  selected = -1;



    if (context.list->GetSelectedRow() >= 0)
    {
        for (size_t i = 0; i < context.lines.size(); i++)
        {
            if (m_selected[(size_t) view] == context.lines[i].address)
            {
                selected = DisassemblyOptions::GetRowOfLine (context.built, (int) i);
                break;
            }
        }

        if (selected != context.list->GetSelectedRow())
        {
            context.list->SetSelectedRow (selected);
        }
    }

    if (context.current >= 0 && context.list->GetSelectedRow() < 0)
    {
        context.list->EnsureVisible (context.current);
    }
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

    //  A bookmark's outline goes around the dot's place, so a breakpoint on
    //  the line is seen inside it.
    if (m_host.GetLatestSnapshot() != nullptr &&
        std::ranges::any_of (m_host.GetLatestSnapshot()->bookmarks, [&line] (const DebuggerViewSnapshot::WatchLine & each) { return each.address == line.address; }))
    {
        gutter.icon = m_host.GetBookmarkMark (gutter.icon);
        gutter.tip  = ColorLegend::JoinLines ({ gutter.tip, L"Bookmark" });
    }

    if (isMarked)
    {
        gutter.icon = m_host.GetPcMarker (gutter.icon);
        gutter.tip  = ColorLegend::JoinLines ({ ColorLegend::GetText (ColorLegend::Meaning::PcMarker), gutter.tip });
        fill        = m_host.GetTextColors().pcRow;
        rowTip      = ColorLegend::GetText (ColorLegend::Meaning::PcRow);
    }
    else if (view == m_navigatedView && m_navigatedTo.has_value() && *m_navigatedTo == line.address)
    {
        fill   = m_host.GetTextColors().navigatedRow;
        rowTip = ColorLegend::GetText (ColorLegend::Meaning::NavigatedRow);
    }
    else if (target.has_value() && *target == line.address)
    {
        fill   = m_host.GetTextColors().targetRow;
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
//  DisassemblyGroup::SetAddressScroll
//
//  A view's scroll bar is the whole address space: the puck's top is the
//  first address in view and its length the addresses in view, whatever
//  the rows built from them. A click on an arrow scrolls an instruction,
//  a click on the track scrolls a view's worth of them, and a drag of the
//  puck centers the view on the address it is dragged to.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGroup::SetAddressScroll (int view, const std::vector<DebuggerViewSnapshot::CodeLine> & lines, const std::vector<DisassemblyOptions::Row> & built)
{
    constexpr int     kAddressSpace = 0x10000;
    DxuiListView    * list          = GetCodeList (view);
    DxuiScrollInfo    model;
    int               shown         = 0;
    int               first         = 0;
    int               last          = 0;



    if (list == nullptr || lines.empty())
    {
        return;
    }

    shown = (std::max) (1, (std::min) (DisassemblyOptions::CountShownLines (built, list->GetVisibleRowCapacity()), (int) lines.size()));
    first = lines.front().address;
    last  = lines[(size_t) shown - 1].address + (int) (lines[(size_t) shown - 1].bytes.size() + 1) / 3;

    model.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    model.nMin  = 0;
    model.nMax  = kAddressSpace;
    model.nPage = (UINT) (std::max) (1, last - first);
    model.nPos  = first;

    list->SetVertScrollModel (model, [this, view, shown, page = (int) model.nPage] (int code, int pos)
    {
        IDebuggerWindowHost  * host = m_host.GetWindowHost();



        if (host == nullptr)
        {
            return;
        }

        switch (code)
        {
        case SB_LINEUP:     host->ScrollDebuggerCode (-1, view);     break;
        case SB_LINEDOWN:   host->ScrollDebuggerCode (1, view);      break;
        case SB_PAGEUP:     host->ScrollDebuggerCode (-shown, view); break;
        case SB_PAGEDOWN:   host->ScrollDebuggerCode (shown, view);  break;
        case SB_THUMBTRACK: host->SetDebuggerCodeAddress ((Word) (std::min) (pos + page / 2, kAddressSpace - 1), view); break;
        default:                                                     break;
        }
    });
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
//  DisassemblyGroup::SelectAsked
//
//  Selects the instruction a snapshot specifies (codeSelect), once per
//  request (codeSelectSerial). Its source line is selected in its document,
//  and the view's tab is brought to the front and the view is focused, so the
//  row is drawn in the focused selection color. A
//  request whose line is not in the view yet is applied from a later
//  snapshot that contains it.
//
////////////////////////////////////////////////////////////////////////////////

void DisassemblyGroup::SelectAsked (
    int                                                   view,
    const DebuggerViewSnapshot                          & snapshot,
    const std::vector<DebuggerViewSnapshot::CodeLine>   & lines,
    const std::vector<DisassemblyOptions::Row>          & built)
{
    std::optional<Word>  asked  = snapshot.codeSelect[(size_t) view];
    uint32_t             serial = snapshot.codeSelectSerial[(size_t) view];



    if (!asked.has_value() || serial == m_askedSerial[(size_t) view])
    {
        return;
    }

    for (size_t i = 0; i < lines.size(); i++)
    {
        if (lines[i].address == *asked)
        {
            m_askedSerial[(size_t) view] = serial;
            m_selected[(size_t) view]    = *asked;
            GetCodeList (view)->SetSelectedRow (DisassemblyOptions::GetRowOfLine (built, (int) i));
            m_sources.ShowLine (lines[i].sourceFileId, lines[i].sourceLine);
            m_host.ActivatePaneTab (DebuggerLayout::GetCodePaneId (view));

            //  A find's match is selected with the focus left in the find box,
            //  so typing goes on there.
            if (m_findSelect[(size_t) view] != asked)
            {
                m_host.FocusControl (GetCodeList (view));
            }

            m_findSelect[(size_t) view].reset();
            return;
        }
    }
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
//  DisassemblyGroup::FindCodeListAt
//
//  The code view under a point, and its number; null for none.
//
////////////////////////////////////////////////////////////////////////////////

DxuiListView * DisassemblyGroup::FindCodeListAt (POINT clientPx, int & view) const
{
    DxuiListView  * list = nullptr;



    for (int each = 0; each < DebuggerViewState::kMaxCodeViews; each++)
    {
        DxuiListView  * code = GetCodeList (each);

        if (m_host.IsRoutable (code) && code->IsVisible() && DxuiDockSite::Contains (code->GetBounds(), clientPx))
        {
            list = code;
            view = each;
        }
    }

    return list;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::TryGetSourceParts
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGroup::TryGetSourceParts (int view, int row, DisassemblyCells::SourceParts & parts) const
{
    const std::vector<DisassemblyCells::SourceParts>  * all = nullptr;



    if (view < 0 || view >= DebuggerViewState::kMaxCodeViews || m_views[(size_t) view] == nullptr || GetCodeLineOfRow (view, row) >= 0)
    {
        return false;
    }

    all = &m_views[(size_t) view]->GetSourceParts();

    if (row < 0 || row >= (int) all->size() || (*all)[(size_t) row].codeColumn == DisassemblyCells::SourceParts::kNone)
    {
        return false;
    }

    parts = (*all)[(size_t) row];
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::ScrollCodeAtEdge
//
//  A disassembly view's rows are only the lines in view, so an arrow at its first
//  or last visible line, or Page Up or Page Down there, scrolls the view
//  through memory rather than stopping; the selected row stays at the edge,
//  on the line scrolled into view. True when the key scrolled a view.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGroup::ScrollCodeAtEdge (
    WPARAM           vk,
    IDxuiControl   * focused)
{
    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        DxuiListView  * code  = GetCodeList (view);
        int             row   = 0;
        int             last  = 0;
        int             page  = 0;
        int             lines = 0;



        if (code == nullptr || code != focused || m_host.GetWindowHost() == nullptr)
        {
            continue;
        }

        row   = code->GetSelectedRow();
        page  = std::max (1, code->GetVisibleRowCapacity());
        last  = std::min (code->GetRowCount(), page) - 1;
        lines = (vk == VK_UP    && row <= 0)    ? -1
              : (vk == VK_DOWN  && row >= last) ?  1
              : (vk == VK_PRIOR && row <= 0)    ? -page
              : (vk == VK_NEXT  && row >= last) ?  page
              :                                    0;

        if (lines == 0 || row < 0)
        {
            return false;
        }

        SelectEdgeAfterScroll (view, lines < 0);
        m_host.GetWindowHost()->ScrollDebuggerCode (lines, view);
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup::SelectCodeViewEdge
//
//  Ctrl+Page Up and Ctrl+Page Down select a disassembly view's top and
//  bottom row in view, as they move to the top and bottom line in view in a
//  source document and in Visual Studio's editor. True when a key moved a
//  view's selection.
//
////////////////////////////////////////////////////////////////////////////////

bool DisassemblyGroup::SelectCodeViewEdge (
    WPARAM           vk,
    IDxuiControl   * focused)
{
    for (int view = 0; view < DebuggerViewState::kMaxCodeViews; view++)
    {
        DxuiListView  * code = GetCodeList (view);
        int             top  = 0;
        int             row  = 0;



        if (code == nullptr || code != focused || (vk != VK_PRIOR && vk != VK_NEXT))
        {
            continue;
        }

        top = code->GetTopRow();
        row = (vk == VK_PRIOR) ? top : (std::min) (code->GetRowCount(), top + (std::max) (1, code->GetVisibleRowCapacity())) - 1;

        if (row < 0)
        {
            return false;
        }

        code->SetSelectedRow (row);
        OnRowSelected (view, row);
        return true;
    }

    return false;
}
