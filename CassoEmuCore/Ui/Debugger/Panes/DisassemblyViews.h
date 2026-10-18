#pragma once

#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/ByteChanges.h"
#include "Ui/Debugger/DisassemblyOptions.h"
#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/ICodeViews.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"

class CodeEditCommands;
class DebuggerPaneFrame;
class DisassemblyGroup;
class DisassemblyGutter;
class HistoryBand;
class SourceDocumentGroup;
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
    const std::vector<DisassemblyOptions::Row> &  GetRows () const { return m_rows; }
    void                             SetRows        (std::vector<DisassemblyOptions::Row> rows) { m_rows = std::move (rows); }

    std::wstring                     GetId          () const override;
    std::wstring                     GetTitle       () const override;
    void                             CreateControls () override;
    void                             Configure      () override;
    bool                             TryGetOpened   (bool & isOpened) const override;
    bool                             RequestOpen    () override;
    bool                             RequestClose   () override;
    bool                             IsFixed        () const override { return m_view == 0; }
    void                             AddToDock      (DxuiDockSite & dock) override;
    IDxuiControl                   * GetContent     () const override;
    std::vector<IDxuiControl *>      GetControls    () const override;
    std::vector<DxuiListView *>      GetLists       () const override { return { m_list }; }
    std::vector<DxuiToolbar *>       GetBars        () const override { return { m_bar.GetBar() }; }
    bool                             HasTextLists   () const override { return false; }
    std::optional<PaneColorKeySpec>  GetColorKey    () const override;
    void                             PlaceBar       () override { m_bar.Place(); }
    bool                             RouteBarMouse  (const DxuiMouseEvent & ev) override;
    const wchar_t                  * GetBarTip      (POINT clientPx, RECT & anchor) const override { return m_bar.GetTip (clientPx, anchor); }
    bool                             AddMenuItems   (DxuiListView * list, int row, PaneMenuItems & items) override;

private:
    DisassemblyGroup                      & m_group;
    int                                     m_view        = 0;
    PaneBarSlot                             m_bar;
    DxuiListView                          * m_list        = nullptr;
    HistoryBand                           * m_historyBand = nullptr;
    std::unique_ptr<DebuggerPaneFrame>      m_frame;
    std::vector<DisassemblyOptions::Row>    m_rows;
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
    const std::vector<DebuggerViewSnapshot::CodeLine> &  GetCodeLines (int view) const override;
    DxuiListView *                                       GetCodeList  (int view) const override;
    int                                                  GetCodeLineOfRow (int view, int row) const override;

    //  The view a list is, or -1; and how many views are open.
    int                              GetViewOf      (const IDxuiControl * control) const;
    int                              GetOpenCount   () const;
    bool                             IsOpen         (int view) const { return m_isOpen[(size_t) view]; }

    //  The view last used, which the keys and the command bar act on.
    int                              GetActive      () const { return m_active; }
    void                             SetActive      (int view) { m_active = view; }

    //  The viewing options, and all of them replaced, as the options sheet
    //  does; each check box's entries for a view's bar, a check box switched,
    //  every view drawn again with it and the choice saved; and whether an
    //  option can be switched now.
    const DisassemblyOptions       & GetOptions     () const { return m_options; }
    void                             SetOptions     (const DisassemblyOptions & options) { m_options = options; }
    std::vector<DxuiToolbar::Entry>  BuildBarEntries ();
    void                             ToggleOption   (DisassemblyOptions::Option option);
    bool                             IsOptionEnabled (DisassemblyOptions::Option option) const;

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

    //  The PC's branch arrow in a view, a view past the code views being a
    //  source document's; drawn, clicked to go where it points and back, and
    //  its tip.
    bool                             GetBranchArrow (int view, BranchArrow::Input & input, Word & goesTo, bool & isTaken, bool isBranchedFrom = false) const;
    void                             PaintBranchArrow (IDxuiPainter & painter, int view);
    bool                             ClickBranchArrow (POINT pointPx);
    bool                             TryGetBranchTip  (POINT clientPx, RECT & anchor, std::wstring & text) const;

    //  Over a name in a view -- a line's label, or the symbol an operand is
    //  written with -- its address and what it is.
    bool                             TryGetSymbolTip  (POINT clientPx, RECT & anchor, std::wstring & text) const;

private:
    void                             UpdateChanges  (const DebuggerViewSnapshot & snapshot);
    bool                             TryGetSourceText (int fileId, int line, std::wstring & text);

    IDebuggerPaneHost                                   & m_host;
    DisassemblyGutter                                   & m_gutter;
    CodeEditCommands                                    & m_edits;
    SourceDocumentGroup                                 & m_sources;
    std::array<DisassemblyViewPane *, DebuggerViewState::kMaxCodeViews>   m_views        = {};
    DisassemblyOptions                                    m_options;
    std::vector<std::unique_ptr<ToolbarCheckEntry>>       m_optionEntries;
    int                                                   m_active       = 0;
    std::array<bool, DebuggerViewState::kMaxCodeViews>    m_isOpen       = { true };
    std::array<int, DebuggerViewState::kMaxCodeViews>     m_linesSentTo  = {};
    std::array<std::optional<Word>, DebuggerViewState::kMaxCodeViews>     m_selected;
    std::optional<Word>                                   m_navigatedTo;
    int                                                   m_navigatedView = 0;
    bool                                                  m_isArrowAtTarget = false;
    std::wstring                                          m_sourceKey;
    std::map<int, std::vector<std::wstring>>              m_sourceText;
    ByteChanges                                           m_changes;
    std::string                                           m_machine;
};
