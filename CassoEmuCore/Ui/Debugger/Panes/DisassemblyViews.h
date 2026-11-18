#pragma once

#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/ByteChanges.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/DisassemblyOptions.h"
#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/DisassemblyBranchArrows.h"
#include "Ui/Debugger/Panes/DisassemblyViewPane.h"
#include "Ui/Debugger/Panes/ICodeViews.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"

class CodeEditCommands;
class DisassemblyGutter;
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
    DisassemblyGroup (IDebuggerPaneHost & host, DisassemblyGutter & gutter, CodeEditCommands & edits, SourceDocumentGroup & sources);
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

    //  A source line's text, as it is drawn in the source rows; false where
    //  the file isn't found.
    bool                             GetSourceText  (int fileId, int line, std::wstring & text) { return TryGetSourceText (fileId, line, text); }

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

    //  The PC's branch arrow in a view, a view past the code views being a
    //  source document's; drawn, clicked to go where it points and back, and
    //  its tip.
    DisassemblyBranchArrows  & GetArrows () { return m_arrows; }

    bool  GetBranchArrow       (int view, BranchArrow::Input & input, Word & goesTo, bool & isTaken, bool isBranchedFrom = false) const { return m_arrows.GetBranchArrow (view, input, goesTo, isTaken, isBranchedFrom); }
    void  PaintBranchArrow     (IDxuiPainter & painter, int view)                                                                    { m_arrows.PaintBranchArrow (painter, view); }
    bool  ClickBranchArrow     (POINT pointPx)                                                                                       { return m_arrows.ClickBranchArrow (pointPx); }
    bool  TryGetBranchTip      (POINT clientPx, RECT & anchor, std::wstring & text) const                                            { return m_arrows.TryGetBranchTip (clientPx, anchor, text); }
    bool  IsOverArrowsOrGutter (POINT clientPx) const                                                                                { return m_arrows.IsOverArrowsOrGutter (clientPx); }

    //  The code list under a point, and which view it is.
    DxuiListView  * FindCodeListAt (POINT clientPx, int & view) const;

    //  Over a name in a view -- a line's label, or the symbol an operand is
    //  written with -- its address and what it is.
    bool  TryGetSymbolTip       (POINT clientPx, RECT & anchor, std::wstring & text) const;
    bool  TryGetSourceSymbolTip (const DxuiListView & list, POINT clientPx, RECT & anchor, std::wstring & text) const;
    bool  TryGetSymbolAt        (POINT clientPx, std::string & name, Word & address) const;

    //  The symbol a row's operand is written with, as "name = $value": an
    //  instruction's, or the first a source row's code has after its
    //  mnemonic. False for a row with none.
    bool                             TryGetOperandSymbol (int view, int row, std::wstring & text) const;

    //  Where a source row's parts are in its cells; false for an
    //  instruction's row. And whether the symbol under a point is the one
    //  its row's operand is written with.
    bool                             TryGetSourceParts   (int view, int row, DisassemblyCells::SourceParts & parts) const;
    bool                             IsOperandSymbolAt   (POINT clientPx) const;
    static bool                      TryGetIoRegisterTip (const std::vector<DiagnosticsIoRegister> & registers, Word address, std::wstring & text);

    //  The source row representing the PC's instruction, the first its line
    //  produced, which has the PC's marker with source lines on; -1 when the
    //  PC's instruction has no source row above it.
    static int                       FindPcSourceRow     (const std::vector<DebuggerViewSnapshot::CodeLine> & lines, const std::vector<DisassemblyOptions::Row> & built);

private:
    void            UpdateChanges    (const DebuggerViewSnapshot & snapshot);
    void            FitLabels        (const DebuggerViewSnapshot & snapshot);
    void            SendShownLines   (int view, const std::vector<DisassemblyOptions::Row> & built);
    void            SetAddressScroll (int view, const std::vector<DebuggerViewSnapshot::CodeLine> & lines, const std::vector<DisassemblyOptions::Row> & built);
    static void     GetLineSymbol    (const DebuggerViewSnapshot::CodeLine & line, int column, std::string & name, Word & address);
    static void     AddOperandSymbol (DxuiListView::Cell & cell, const DebuggerViewSnapshot::CodeLine & line, uint32_t symbolArgb);
    static int      GetCodeEnd       (const DisassemblyCells::SourceParts & parts);
    void                             MarkRow        (int view, int row, const DebuggerViewSnapshot::CodeLine & line, std::optional<Word> target, bool isMarked,
                                                     std::vector<DxuiListView::Cell> & cells, uint32_t & fill, std::wstring & rowTip);
    bool                             SelectScrolledEdge (int view, const std::vector<DisassemblyOptions::Row> & built);
    void                             SelectAsked        (int view, const DebuggerViewSnapshot & snapshot, const std::vector<DebuggerViewSnapshot::CodeLine> & lines,
                                                         const std::vector<DisassemblyOptions::Row> & built);
    bool  TryGetSourceText  (int fileId, int line, std::wstring & text);

    IDebuggerPaneHost                                   & m_host;
    DisassemblyGutter                                   & m_gutter;
    CodeEditCommands                                    & m_edits;
    SourceDocumentGroup                                 & m_sources;
    std::array<DisassemblyViewPane *, DebuggerViewState::kMaxCodeViews>   m_views        = {};
    DisassemblyOptions                                    m_options;
    std::vector<std::unique_ptr<ToolbarCheckEntry>>       m_optionEntries;
    int                                                   m_active       = 0;
    std::array<bool, DebuggerViewState::kMaxCodeViews>    m_isOpen       = { true };
    std::shared_ptr<const DebuggerViewSnapshot::SymbolsState>  m_fittedSymbols;
    std::array<int, DebuggerViewState::kMaxCodeViews>     m_linesSentTo  = {};
    std::array<int, DebuggerViewState::kMaxCodeViews>     m_shownSentTo  = {};
    std::array<std::optional<Word>, DebuggerViewState::kMaxCodeViews>     m_selected;
    std::array<std::optional<Word>, DebuggerViewState::kMaxCodeViews>     m_findSelect;
    std::array<std::optional<bool>, DebuggerViewState::kMaxCodeViews>     m_edgeAfterScroll;     // true for the top row
    std::array<std::optional<Word>, DebuggerViewState::kMaxCodeViews>     m_edgeScrolledFrom;    // the first line when the key scrolled
    std::optional<Word>                                   m_navigatedTo;
    int                                                   m_navigatedView = 0;
    std::wstring                                          m_sourceKey;                // the debug file's path and program key the text was read for
    std::map<int, std::vector<std::wstring>>              m_sourceText;
    std::set<int>                                         m_sourceMisses;             // files not found for m_missesFor
    const DebuggerViewSnapshot                          * m_missesFor = nullptr;
    ByteChanges                                           m_changes;
    DisassemblyBranchArrows                               m_arrows { m_host, *this, m_sources };
    std::string                                           m_machine;

    //  The serial of the request each view last applied, so a rebuild of the
    //  view doesn't select the row again over the user's selection.
    std::array<uint32_t, DebuggerViewState::kMaxCodeViews>  m_askedSerial = {};
};
