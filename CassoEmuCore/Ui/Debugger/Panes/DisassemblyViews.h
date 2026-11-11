#pragma once

#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/ByteChanges.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/DisassemblyOptions.h"
#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/ICodeViews.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"

class CodeEditCommands;
class CodeViewFind;
class DebuggerPaneFrame;
class DisassemblyGroup;
class DisassemblyGutter;
class DxuiInfoBanner;
class HistoryBand;
class SourceDocumentGroup;
class SourcePane;
class ToolbarCheckEntry;





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyViewPane
//
//  One disassembly view: its toolbar of viewing options over its lines, and
//  for the first view, the history band between them. The machine opens and
//  closes the views after the first, as the snapshot lists them.
//
////////////////////////////////////////////////////////////////////////////////

class DisassemblyViewPane : public IDebuggerPane
{
public:
    DisassemblyViewPane (IDebuggerPaneHost & host, DisassemblyGroup & group, int view);
    ~DisassemblyViewPane() override;

    DxuiListView                   * GetList        () const { return m_list; }
    DxuiToolbar                    * GetBar         () const { return m_bar.GetBar(); }
    HistoryBand                    * GetHistoryBand () const { return m_historyBand; }
    DebuggerPaneFrame              * GetFrame       () const { return m_frame.get(); }

    //  The view's rows, each a code line or a source line above it.
    const std::vector<DisassemblyOptions::Row>  & GetRows () const { return m_rows; }
    void                                          SetRows (std::vector<DisassemblyOptions::Row> rows) { m_rows = std::move (rows); }

    //  Where each row's parts are in its cells, as its source row was filled;
    //  the default for an instruction's row.
    const std::vector<DisassemblyCells::SourceParts>  & GetSourceParts () const { return m_sourceParts; }
    void                                                SetSourceParts (std::vector<DisassemblyCells::SourceParts> parts) { m_sourceParts = std::move (parts); }

    //  The banner over the lines while a loaded file's program isn't in
    //  memory: its text, or empty for none.
    void              SetStaleText  (const std::wstring & text);
    DxuiInfoBanner  * GetBanner     () const { return m_banner; }
    bool              IsBannerShown () const { return m_isBannerShown; }

    //  What the banner reads for the files whose program isn't in memory,
    //  the debug file first when isDebugFileStale is set; empty for none.
    static std::wstring              GetStaleText   (const std::vector<std::string> & names, bool isDebugFileStale);

    const std::wstring               & GetId            () const override;
    std::wstring                       GetTitle         () const override;
    std::wstring                       GetTabTitle      () const override;
    int                                GetStopFocusRank () const override;
    void                               CreateControls   () override;
    void                               Configure        () override;
    bool                               TryGetOpened     (bool & isOpened) const override;
    bool                               RequestOpen      () override;
    bool                               RequestClose     () override;
    bool                               IsFixed          () const override { return m_view == 0; }
    void                               AddToDock        (DxuiDockSite & dock) override;
    IDxuiControl                     * GetContent       () const override;
    IDxuiFindTarget                  * GetFindView      () const override;
    DebuggerPaneFrame                * GetFindFrame     () const override { return m_frame.get(); }
    std::vector<IDxuiControl *>        GetControls      () const override;
    std::vector<DxuiListView *>        GetLists         () const override { return { m_list }; }
    std::vector<DxuiToolbar *>         GetBars          () const override { return { m_bar.GetBar() }; }
    bool                               HasTextLists     () const override { return false; }
    std::optional<PaneColorKeySpec>    GetColorKey      () const override;
    void                               PlaceBar         () override { m_bar.Place(); }
    bool                               RouteBarMouse    (const DxuiMouseEvent & ev) override;
    bool                               AddMenuItems     (DxuiListView * list, int row, PaneMenuItems & items) override;

private:
    std::shared_ptr<const std::vector<DebuggerViewSnapshot::CodeLine>>  GetFindLines () const;
    std::optional<Word>                                                 GetFindStart (bool isTop) const;
    void                                                                SelectFound  (Word address);

    DisassemblyGroup                            & m_group;
    int                                           m_view          = 0;
    std::wstring                                  m_id;
    PaneBarSlot                                   m_bar;
    DxuiListView                                * m_list          = nullptr;
    HistoryBand                                 * m_historyBand   = nullptr;
    DxuiInfoBanner                              * m_banner        = nullptr;
    bool                                          m_isBannerShown = false;
    std::unique_ptr<DebuggerPaneFrame>            m_frame;
    std::unique_ptr<CodeViewFind>                 m_find;
    std::vector<DisassemblyOptions::Row>          m_rows;
    std::vector<DisassemblyCells::SourceParts>    m_sourceParts;
};





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

    //  The PC's branch arrow in a view, a view past the code views being a
    //  source document's; drawn, clicked to go where it points and back, and
    //  its tip.
    bool  GetBranchArrow   (int view, BranchArrow::Input & input, Word & goesTo, bool & isTaken, bool isBranchedFrom = false) const;
    void  PaintBranchArrow (IDxuiPainter & painter, int view);
    bool  ClickBranchArrow (POINT pointPx);
    bool  TryGetBranchTip  (POINT clientPx, RECT & anchor, std::wstring & text) const;

    //  Whether a point is on a code view's glyph margin or the lanes its
    //  branch arrows run in, left of the code, in a view or a source
    //  document, where the pointer is an arrow rather than the I-beam.
    bool                             IsOverArrowsOrGutter (POINT clientPx) const;

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
    void            ClickSourceArrow (SourcePane & document);
    DxuiListView  * FindCodeListAt   (POINT clientPx, int & view) const;
    static void     GetLineSymbol    (const DebuggerViewSnapshot::CodeLine & line, int column, std::string & name, Word & address);
    static void     AddOperandSymbol (DxuiListView::Cell & cell, const DebuggerViewSnapshot::CodeLine & line, uint32_t symbolArgb);
    static int      GetCodeEnd       (const DisassemblyCells::SourceParts & parts);
    void                             MarkRow        (int view, int row, const DebuggerViewSnapshot::CodeLine & line, std::optional<Word> target, bool isMarked,
                                                     std::vector<DxuiListView::Cell> & cells, uint32_t & fill, std::wstring & rowTip);
    bool                             SelectScrolledEdge (int view, const std::vector<DisassemblyOptions::Row> & built);
    void                             SelectAsked        (int view, const DebuggerViewSnapshot & snapshot, const std::vector<DebuggerViewSnapshot::CodeLine> & lines,
                                                         const std::vector<DisassemblyOptions::Row> & built);
    bool  GetOneBranchArrow (int view, BranchArrow::Input & input, Word & goesTo, bool & isTaken, bool isBranchedFrom) const;
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
    bool                                                  m_isArrowAtTarget = false;
    std::wstring                                          m_sourceKey;                // the debug file's path and program key the text was read for
    std::map<int, std::vector<std::wstring>>              m_sourceText;
    std::set<int>                                         m_sourceMisses;             // files not found for m_missesFor
    const DebuggerViewSnapshot                          * m_missesFor = nullptr;
    ByteChanges                                           m_changes;
    std::string                                           m_machine;

    //  The serial of the request each view last applied, so a rebuild of the
    //  view doesn't select the row again over the user's selection.
    std::array<uint32_t, DebuggerViewState::kMaxCodeViews>  m_askedSerial = {};
};
