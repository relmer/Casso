#pragma once

#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/ByteChanges.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/DisassemblyOptions.h"
#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/DisassemblyBranchArrows.h"
#include "Ui/Debugger/Panes/DisassemblySourceText.h"
#include "Ui/Debugger/Panes/DisassemblySymbols.h"
#include "Ui/Debugger/Panes/DisassemblyViewPane.h"
#include "Ui/Debugger/Panes/ICodeViews.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"

class CodeEditCommands;
class DisassemblyGutter;
class MemoryWindowGroup;
class SourceDocumentGroup;
class SourcePane;
class ToolbarCheckEntry;





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyGroup
//
//  What the disassembly views share: the viewing options, which apply to
//  every view, and their check boxes on each view's bar; the view last
//  used, which the keys and the command bar act on; each view's open state,
//  the line count it last sent the machine and the instruction selected in
//  it; the row another pane brought into view; the source lines read for
//  the source rows; and the bytes on every view's rows, for the changed
//  color.
//  The PC's branch arrow is drawn over the views and the source documents.
//  The views are in the pane list; the group itself is not.
//
////////////////////////////////////////////////////////////////////////////////

class DisassemblyGroup : public ICodeViews
{
public:
    DisassemblyGroup (const DebuggerPaneParts & parts, DisassemblyGutter & gutter, CodeEditCommands & edits, SourceDocumentGroup & sources, MemoryWindowGroup & memory);
    ~DisassemblyGroup() override;

    DisassemblyGroup (const DisassemblyGroup &)             = delete;
    DisassemblyGroup & operator= (const DisassemblyGroup &) = delete;

    //  The views, for the pane list.
    std::vector<std::unique_ptr<IDebuggerPane>>  CreateViews ();

    DisassemblyViewPane            & GetView        (int view) const { return *m_views[(size_t) view]; }
    CodeEditCommands               & GetEdits       () const { return m_edits; }
    DisassemblyGutter              & GetGutter      () const { return m_gutter; }

    //  ICodeViews.
    const std::vector<DebuggerViewSnapshot::CodeLine>  & GetCodeLines       (int view) const override;
    DxuiListView                                       * GetCodeList        (int view) const override;
    int                                                  GetCodeLineOfRow   (int view, int row) const override;
    int                                                  GetMenuLineOfRow   (int view, int row) const;
    int                                                  GetGutterLineOfRow (int view, int row) const override;

    //  The view a list is, or -1; and how many views are open.
    int                              GetViewOf      (const IDxuiControl * control) const;
    int                              GetOpenCount   () const;
    bool                             IsOpen         (int view) const { return m_isOpen[(size_t) view]; }

    //  The view last used, which the keys and the command bar act on.
    int                              GetActive      () const { return m_active; }
    void                             SetActive      (int view) { m_active = view; }

    //  The address a view's find requested, which is selected without
    //  moving the focus out of the find box.
    void                             SetFindSelect  (int view, Word address) { m_findSelect[(size_t) view] = address; }

    //  The source lines the source rows are drawn from.
    DisassemblySourceText          & GetSourceText  () { return m_sourceText; }

    //  The viewing options, and all of them replaced, as the options sheet
    //  does; each check box's entries for a view's bar, a check box switched,
    //  every view drawn again with it and the choice saved; and whether an
    //  option can be switched now.
    const DisassemblyOptions         & GetOptions      () const { return m_options; }
    void                               SetOptions      (const DisassemblyOptions & options) { m_options = options; }
    std::vector<DxuiToolbar::Entry>    BuildBarEntries ();
    void                               ToggleOption    (DisassemblyOptions::Option option);
    bool                               IsOptionEnabled (DisassemblyOptions::Option option) const;

    //  Each view's open state and rows from the snapshot; one view's rows;
    //  and the first view's history band.
    void                             Apply          ();
    void                             ApplyView      (int view);
    void                             ApplyHistory   (const HistoryStatus & status);

    //  Each frame, the lines each view has room for, only a change sent.
    void                             UpdateLines    ();

    //  The view last used moved to an address another pane chose, marking
    //  the row as the one brought into view; no address follows the PC
    //  again, which marks nothing.
    void                             ShowCode       (std::optional<Word> address);

    //  A code row selected: the view becomes the one last used, and the
    //  row's source line is selected in its document.
    void                             OnRowSelected  (int view, int row);

    //  The view was scrolled by a key at its first or last row: once rows
    //  from the new place come in, the row at that edge is selected, so the
    //  selection moves onto the line scrolled into view.
    void                             SelectEdgeAfterScroll (int view, bool isTop);

    //  A key at a view's first or last row in view: an arrow or a page key
    //  there scrolls the view through memory, and Ctrl+Page Up or Down selects
    //  the top or bottom row in view. True when the key moved a view.
    bool  ScrollCodeAtEdge   (WPARAM vk, IDxuiControl * focused);
    bool  SelectCodeViewEdge (WPARAM vk, IDxuiControl * focused);

    //  The PC's branch arrow over the views and the source documents.
    DisassemblyBranchArrows  & GetArrows () { return m_arrows; }

    //  The code list under a point, and which view it is.
    DxuiListView  * FindCodeListAt (POINT clientPx, int & view) const;

    //  The symbols in the views: symbol tips and the operand's symbol.
    const DisassemblySymbols  & GetSymbols () const { return m_symbols; }

    //  Where a source row's parts are in its cells; false for an
    //  instruction's row.
    bool                             TryGetSourceParts   (int view, int row, DisassemblyCells::SourceParts & parts) const;

    //  The source row representing the PC's instruction, the first its line
    //  produced, which has the PC's marker with source lines on; -1 when the
    //  PC's instruction has no source row above it.
    static int                       FindPcSourceRow     (const std::vector<DebuggerViewSnapshot::CodeLine> & lines, const std::vector<DisassemblyOptions::Row> & built);

private:
    //  What every row of a view is drawn with, made once per ApplyView.
    struct RowContext
    {
        const DebuggerViewSnapshot                         * snapshot = nullptr;
        const DxuiTheme                                    * theme    = nullptr;
        DxuiListView                                       * list     = nullptr;
        const std::vector<DebuggerViewSnapshot::CodeLine>  & lines;
        std::vector<DisassemblyOptions::Row>                 built;
        DebuggerTextColors::Set                              textColors;
        bool                                                 source   = false;
        bool                                                 numbers  = false;
        std::optional<HeatMapView::Palette>                  heatPalette;
        std::optional<Word>                                  target;
        int                                                  current  = -1;
        int                                                  marked   = -1;
    };

    RowContext                       MakeRowContext (int view);
    std::vector<DxuiListView::Cell>  BuildRow       (int view, const RowContext & context, const DisassemblyOptions::Row & each, int at, bool & ioOperandMoved,
                                                     std::vector<DisassemblyCells::SourceParts> & parts);
    DisassemblyCells::SourceParts    FillSourceRow  (const RowContext & context, const DisassemblyOptions::Row & each, int at, int gutter,
                                                     std::vector<DxuiListView::Cell> & cells, bool & outOperandMoved);
    bool                             FillCodeRow    (const RowContext & context, const DebuggerViewSnapshot::CodeLine & line, int at, uint32_t fill, bool isOperandMoved,
                                                     std::vector<DxuiListView::Cell> & cells);
    static void                      SetRowTips     (std::vector<DxuiListView::Cell> & cells, const DebuggerViewSnapshot::CodeLine * line, const std::wstring & rowTip);
    void                             KeepSelection  (int view, const RowContext & context);

    void            UpdateChanges    (const DebuggerViewSnapshot & snapshot);
    void            FitLabels        (const DebuggerViewSnapshot & snapshot);
    void            SendShownLines   (int view, const std::vector<DisassemblyOptions::Row> & built);
    void            SetAddressScroll (int view, const std::vector<DebuggerViewSnapshot::CodeLine> & lines, const std::vector<DisassemblyOptions::Row> & built);
    void                             MarkRow        (int view, int row, const DebuggerViewSnapshot::CodeLine & line, std::optional<Word> target, bool isMarked,
                                                     std::vector<DxuiListView::Cell> & cells, uint32_t & fill, std::wstring & rowTip);
    bool                             SelectScrolledEdge (int view, const std::vector<DisassemblyOptions::Row> & built);
    void                             SelectAsked        (int view, const DebuggerViewSnapshot & snapshot, const std::vector<DebuggerViewSnapshot::CodeLine> & lines,
                                                         const std::vector<DisassemblyOptions::Row> & built);

    IDebuggerWindowHost * const                                          & m_windowHost;
    IPaneViews                                                           & m_paneViews;
    IPaneLayout                                                          & m_paneLayout;
    IPaneTips                                                            & m_paneTips;
    IPaneRouting                                                         & m_paneRouting;
    IPaneFocus                                                           & m_paneFocus;
    IPaneTabs                                                            & m_paneTabs;
    MemoryWindowGroup                                                    & m_memory;
    const LatestSnapshot                                                 & m_latest;
    const DebuggerPaneParts                                              & m_parts;
    const DebuggerPaneLook                                               & m_look;
    DisassemblyGutter                                                    & m_gutter;
    CodeEditCommands                                                     & m_edits;
    SourceDocumentGroup                                                  & m_sources;
    std::array<DisassemblyViewPane *, DebuggerViewState::kMaxCodeViews>    m_views            = {};
    DisassemblyOptions                                                     m_options;
    std::vector<std::unique_ptr<ToolbarCheckEntry>>                        m_optionEntries;
    int                                                                    m_active           = 0;
    std::array<bool, DebuggerViewState::kMaxCodeViews>                     m_isOpen           = { true };
    std::shared_ptr<const DebuggerViewSnapshot::SymbolsState>              m_fittedSymbols;
    std::array<int, DebuggerViewState::kMaxCodeViews>                      m_linesSentTo      = {};
    std::array<int, DebuggerViewState::kMaxCodeViews>                      m_shownSentTo      = {};
    std::array<std::optional<Word>, DebuggerViewState::kMaxCodeViews>      m_selected;
    std::array<std::optional<Word>, DebuggerViewState::kMaxCodeViews>      m_findSelect;
    std::array<std::optional<bool>, DebuggerViewState::kMaxCodeViews>      m_edgeAfterScroll;   // true for the top row
    std::array<std::optional<Word>, DebuggerViewState::kMaxCodeViews>      m_edgeScrolledFrom;   // the first line when the key scrolled
    std::optional<Word>                                                    m_navigatedTo;
    int                                                                    m_navigatedView    = 0;
    ByteChanges                                                            m_changes;
    DisassemblySourceText                                                  m_sourceText;
    DisassemblySymbols                                                     m_symbols;
    DisassemblyBranchArrows                                                m_arrows           { m_parts, *this, m_sources };
    std::string                                                            m_machine;

    //  The serial of the request each view last applied, so a rebuild of the
    //  view doesn't select the row again over the user's selection.
    std::array<uint32_t, DebuggerViewState::kMaxCodeViews>  m_askedSerial = {};
};
