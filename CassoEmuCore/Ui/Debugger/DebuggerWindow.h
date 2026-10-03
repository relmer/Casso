#pragma once

#include "Ui/Debugger/DebuggerCommands.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/MemoryAddressEntry.h"
#include "Ui/Debugger/MemoryBarCommands.h"
#include "Seams/IHostDialogs.h"
#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/CommandCompletion.h"
#include "Ui/Debugger/ByteChanges.h"
#include "Ui/Debugger/CommandBarDock.h"
#include "Ui/Debugger/BreakpointBarCommands.h"
#include "Ui/Debugger/BreakpointColumns.h"
#include "Ui/Debugger/ConsoleHistory.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/DebuggerThemes.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/DisassemblyOptions.h"
#include "Ui/Debugger/RegisterHistory.h"
#include "Ui/Debugger/StackHistory.h"
#include "Ui/Debugger/StopChanges.h"
#include "Ui/Debugger/UndoBarCommands.h"
#include "Ui/Debugger/ToolbarCheckEntry.h"
#include "Ui/Debugger/ToolbarLabelEntry.h"
#include "Ui/Debugger/KeyHintLine.h"
#include "Ui/Debugger/HistoryBand.h"
#include "Ui/Debugger/ReverseOptionsDialog.h"
#include "Ui/Debugger/WholeWordButton.h"
#include "Ui/Debugger/WatchHistory.h"
#include "Ui/Debugger/Panes/CallStackPane.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/DiagnosticsPane.h"
#include "Ui/Debugger/Panes/FindWidgetPlate.h"
#include "Ui/Debugger/Panes/HeatMapView.h"
#include "Ui/Debugger/Panes/MemoryPane.h"
#include "Ui/Debugger/Panes/SourceDocuments.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/Panes/TracePane.h"

struct CassoTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  IDebuggerWindowHost
//
//  What the window asks of whoever owns the debugger. Called on the UI thread;
//  the host carries each request to the CPU thread, where the session lives.
//
////////////////////////////////////////////////////////////////////////////////

class IDebuggerWindowHost
{
public:
    virtual ~IDebuggerWindowHost() = default;

    virtual void  RunDebuggerCommand      (const std::string & line)       = 0;
    virtual void  PauseDebugger           ()                               = 0;
    //  A line read in `mode` rather than in the session's own mode.
    virtual void  RunDebuggerCommandInMode (const std::string & line, CommandMode mode) = 0;
    //  A control's action, run directly with its echo shown (FR-135). A host
    //  with no session to run it on, as a test's is, has nothing to do.
    virtual void  RunDebuggerAction       (const DebuggerAction &)          {}
    //  One of the emulator's own menu commands (a resource IDM_ id), such as
    //  Reset or Restart under debugger, which the debugger's menu bar offers
    //  too. A host with no emulator, as a test's is, has nothing to do.
    virtual void  RunEmulatorCommand      (int commandId)                   { (void) commandId; }
    //  How many lines the code pane has room for, measured by the window.
    virtual void  SetDebuggerCodeLines    (int lines, int view)             = 0;
    //  A code view: 0 is the first, 1 to 3 the others. An address given for
    //  a closed view opens it there.
    virtual void  SetDebuggerCodeAddress  (std::optional<Word> address, int view) = 0;
    //  Opens a code view with `top` on its first line rather than centered.
    virtual void  SetDebuggerCodeTop      (Word top, int view)             = 0;
    virtual void  SetDebuggerFollowView   (int view)                       = 0;
    virtual void  CloseDebuggerCodeView   (int view)                       = 0;
    virtual void  SetDebuggerMemoryWindow (int id, std::optional<Word> address) = 0;

    //  Where the trace pane reads from: an entry, or the newest when empty.
    virtual void  SetDebuggerTraceTop     (std::optional<uint64_t> first) = 0;

    //  Whether the heat map pane is shown; the machine records its accesses
    //  only while it is. A host with no machine, as a test's is, records none.
    virtual void  SetDebuggerHeatMapShown (bool shown)                     { (void) shown; }

    //  A memory window's Go to text, resolved on the CPU thread; the window
    //  acts on it when a snapshot carries the answer.
    virtual void  GoToDebuggerMemory      (int window, const std::string & text) = 0;

    //  Scrolls the code pane by instructions, through the whole address space.
    virtual void  ScrollDebuggerCode      (int lines, int view) = 0;

    //  The newest snapshot, if one arrived since the last call, and every
    //  console line written since then.
    virtual bool  TakeDebuggerUpdate      (std::shared_ptr<const DebuggerViewSnapshot> & snapshot,
                                           std::vector<std::string>                     & consoleLines) = 0;

    virtual void  OnDebuggerWindowClosed  ()                               = 0;

    //  Detach: the close that follows leaves the machine running with the
    //  debugger's CPU hook removed. A host with no machine, as a test's is,
    //  has nothing to do.
    virtual void  DetachDebugger          ()                               {}

    //  The file pickers the R and W prompt opens.
    virtual IHostDialogs &  GetHostDialogs () noexcept                     = 0;

    //  The keyboard scheme preference, by name. Saving it is the host's, so
    //  the choice survives the window.
    virtual std::string  GetDebuggerKeyScheme ()                           = 0;
    virtual void         SetDebuggerKeyScheme (const std::string & name)   = 0;

    //  The window's own theme, by DebuggerThemes name, kept the same way. A
    //  host that keeps no preferences, as a test's is, follows the emulator.
    virtual std::string  GetDebuggerTheme     ()                           { return {}; }

    //  Whether the emulator's screen marks where the video beam is while the
    //  machine is stopped. A host with no screen, as a test's is, has none.
    virtual bool         IsBeamOverlayOn      ()                           { return false; }
    virtual void         SetBeamOverlayOn     (bool on)                    { (void) on; }
    virtual void         SetDebuggerTheme     (const std::string &)        {}

    //  Reverse execution's settings, kept the same way. A host that keeps no
    //  preferences, as a test's is, has the defaults and keeps nothing.
    virtual ReverseOptions  GetReverseOptions ()                        { return {}; }
    virtual void            SetReverseOptions (const ReverseOptions &)  {}

    //  The pane arrangement as DxuiPaneLayout text, kept the same way.
    virtual std::string  GetDebuggerLayout    ()                           = 0;
    virtual void         SetDebuggerLayout    (const std::string & text)   = 0;

    //  The fixed panes the user closed, in DebuggerLayout's text for them,
    //  kept in a setting of their own. A host that keeps no preferences, as a
    //  test's is, has none closed.
    virtual std::string  GetDebuggerClosedPanes ()                         { return {}; }
    virtual void         SetDebuggerClosedPanes (const std::string &)      {}

    //  Where the command bar is docked, in CommandBarDock's text, kept the
    //  same way. A host with no preferences keeps it across the top.
    virtual std::string  GetDebuggerCommandBarDock ()                      { return {}; }
    virtual void         SetDebuggerCommandBarDock (const std::string &)   {}
    //  The disassembly views' viewing options, in DisassemblyOptions' text for
    //  them, kept the same way; none kept gives the defaults.
    virtual std::string  GetDebuggerDisassemblyOptions ()                  { return {}; }
    virtual void         SetDebuggerDisassemblyOptions (const std::string &) {}

    //  Which optional views were open, in DebuggerViewState's text for them,
    //  kept the same way.
    virtual std::string  GetDebuggerOpenViews ()                           = 0;
    virtual void         SetDebuggerOpenViews (const std::string & text)   = 0;

    //  Where the user last put the debugger window, for this monitor
    //  arrangement. False when there is nothing to restore.
    virtual bool  TryGetDebuggerPlacement (RECT & rectPx)                  = 0;
    virtual void  SetDebuggerPlacement    (const RECT & rectPx)            = 0;

    //  A debug file's source file, found by the rules of FR-058, and a file
    //  the user dropped, matched against the debug file's records. Where a
    //  file is found goes into the preferences, which the host keeps.
    virtual SourceLookup  FindDebuggerSource         (const DebugSourceFile & record, const std::wstring & debugFilePath,
                                                      const std::string & programKey) = 0;
    virtual SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> & files, const std::wstring & path,
                                                      const std::string & programKey, int & recordIndex) = 0;

    //  Whether a file is on disk, for the debug file beside a source. A host
    //  with no file system, as a test's is, has none.
    virtual bool  DoesDebuggerFileExist (const std::wstring & path)        { (void) path; return false; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindow
//
//  The debugger beside the emulator: code, registers, memory, stack, watches
//  and breakpoints, a command box, and step and run controls.
//
//  IT DRAWS SNAPSHOTS AND SENDS COMMANDS. It holds no session and reads no
//  machine state; the CPU thread builds a DebuggerViewSnapshot and the window
//  paints the latest one it has. Every control produces a command line through
//  DebuggerViewState, so what a click does is exactly what typing the command
//  would do.
//
//  THE CURRENT LINE AND BREAKPOINTS ARE MARKED IN A GUTTER. The code pane's
//  first column carries the PC's arrow and a breakpoint's dot, and a click
//  there sets or clears the breakpoint.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerWindow : public DxuiWindow
{
public:
    DebuggerWindow() = default;
    ~DebuggerWindow() override;

    static DxuiListView::Cell  GetOperandAndResultCell (const std::string & annotation, const std::string & effect, uint32_t resultArgb);

    //  An Undo or Redo item and tip, quoting the edit: Undo "changed 2 bytes
    //  at $0300", or the bare verb when there is nothing to say.
    static std::wstring        GetUndoLabel            (bool redo, const std::wstring & text);

    HRESULT  Create      (HINSTANCE hInstance, HWND hwndOwner, const CassoTheme * theme, IDebuggerWindowHost * host);
    void     RenderFrame ();

    //  Detaches the host, for a shell tearing down before the window.
    void     DetachHost  () { m_host = nullptr; }

    //  Writes the placement if the user put the window somewhere else, for a
    //  shell shutting down while the window is still open.
    void     SavePlacementIfMoved ();

protected:
    void     OnCreate        () override;
    void     OnWindowPlaced  () override;
    void     OnWindowClose   () override;
    void     Layout          (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool     OnMouse         (const DxuiMouseEvent & ev) override;
    bool     OnKey           (const DxuiKeyEvent   & ev) override;
    bool     OnMappedCommand (int commandId) override;
    bool     OnFilesDropped  (const std::vector<std::wstring> & paths) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;
    void     PaintTopLayer   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool     HasTopLayer     () const override;
    void     PaintBranchArrow (IDxuiPainter & painter, int view);
    bool     GetBranchArrow   (int view, BranchArrow::Input & input, Word & goesTo, bool & isTaken) const;
    bool     ClickBranchArrow (POINT pointPx);

    //  Protected so a test can apply one as the keys do.
    void     ApplyTextZoom   (float zoom);
    void     StepTextZoom    (int steps);
    float    GetTextZoom     () const { return m_textZoom; }

    //  Protected so a test can submit the Address box as Enter does.
    void             SubmitMemoryBox ();
    DxuiTextInput  * GetMemoryBox    () const { return m_memoryBox; }

    //  Protected so a test can focus a control as a click does.
    DxuiTextInput  * GetCommandBox   () const { return m_commandBox; }
    void             FocusControl    (IDxuiControl * control) { SetFocusedControl (control); }

    //  Protected so a test can write to the console as the host's lines do,
    //  and search it as the find bar does.
    void             AppendConsole   (const std::vector<std::string> & lines);
    void             OpenFind        ();
    void             OpenFindIn      (const std::wstring & pane);
    void             CloseFind       ();
    void             FindInPane      (bool forward);
    std::wstring     GetFindPane     () const { return m_findPane; }
    DxuiTextView   * GetSourceView   (int slot) const { return m_sourceDocs[(size_t) slot].view; }
    bool             IsFindOpen      () const { return m_findOpen; }
    bool             IsFindOpenIn    (const std::wstring & pane) const;
    DxuiTextInput  * GetFindBoxOf    (const std::wstring & pane) const;
    DxuiTextView   * GetConsoleView  () const { return m_consoleView; }
    DxuiTextInput  * GetFindBox      () const { return m_findBox; }
    DxuiButton     * GetFindWordButton () const { return m_findWordButton; }
    void             SetFindOptions  (bool matchCase, bool wholeWord, bool isRegex);
    void             RefocusFindBox  ();
    std::wstring     GetFindStatus   () const { return m_findStatusText; }
    void             SetFindInSelection (bool on);
    bool             IsFindInSelection  () const { return m_findInSelection; }
    std::vector<std::wstring>  GetFindHistory (const std::wstring & pane) const;
    bool             StepFindHistory (int step);
    RECT             GetFindWidgetBounds () const { return m_findPlate->GetBounds(); }

    //  Protected so a test can choose a scheme as the Keys menu does, and
    //  read the drop-down rows it leaves.
    void             ApplyKeyScheme  (DebuggerKeyScheme scheme);
    const std::vector<std::shared_ptr<DxuiCommand>> &  GetMenuCommands () const { return m_menuCommands; }

    //  Protected so a test can choose a theme as the Theme menu does.
    void             ApplyTheme      (const std::string & name);
    const std::string &  GetThemeName () const { return m_themeName; }

    //  A Theme row under the highlight shows its theme at once; the theme in
    //  force when the menu opened comes back if it closes without a choice.
    void                 PreviewTheme    (const std::string & name);
    void                 EndThemePreview ();

    //  Protected so a test can hand the window a snapshot as a frame does.
    void             TakeSnapshot    (std::shared_ptr<const DebuggerViewSnapshot> snapshot);

    void             ShowDroppedSource (const std::wstring & path);
    static bool      IsSymbolFile      (const std::wstring & path);

    //  Protected so a test can open a source file as File > Open source file
    //  does and press a document's Load symbols button.
    void                OpenSourcePath      (const std::wstring & path);
    void                LoadSymbolsFor      (int slot);
    bool                IsLooseSource       (int slot) const;
    const SourcePane &  GetSourcePane       (int slot) const { return *m_sourceDocs[(size_t) slot].pane; }

    //  Protected so a test can see which pane the watch editor goes with.
    std::wstring     GetPaneOfControl (const IDxuiControl * control) const;
    DxuiTextInput  * GetWatchEditor   () const { return m_watchEditor; }

    //  Protected so a test can apply a snapshot's disassembly as a frame does.
    void            ApplyCodeSnapshot (std::shared_ptr<const DebuggerViewSnapshot> snapshot, int view) { m_snapshot = std::move (snapshot); ApplyCodeView (view); }
    DxuiListView  * GetCodeList       (int view) const { return m_codeLists[(size_t) view]; }

    //  Protected so a test can switch a disassembly viewing option as its
    //  check box does, read the rows a view shows, and hover its gutter.
    void                         ToggleCodeOption    (DisassemblyOptions::Option option);
    bool                         IsCodeOptionEnabled (DisassemblyOptions::Option option) const;
    const DisassemblyOptions  &  GetCodeOptions      () const { return m_codeOptions; }
    DxuiToolbar               *  GetCodeBar          (int view) const { return m_codeBars[(size_t) view]; }
    int                          GetCodeLineOfRow    (int view, int row) const;
    void                         HoverGutter         (POINT atDip);

    //  Protected so a test can press the gutter and drag the PC's arrow to
    //  another line, which sets the next statement there.
    bool                         ClickGutter         (const DxuiMouseEvent & ev);
    void                         DragPcMarker        (POINT atDip);
    bool                         DropPcMarker        (const DxuiMouseEvent & ev);
    void                         SetNextStatement    (Word address);

    //  Protected so a test can edit a watch as F2 and Enter do, and see where
    //  the keys go.
    void            BeginWatchEdit (int row, int column);
    void            EndWatchEdit   (bool commit);
    IDxuiControl  * GetFocused     () const;
    DxuiListView  * GetWatchList   () const { return m_watchList; }

    //  Protected so a test can edit a stack byte or a register in place as a
    //  click, F2 and Enter do.
    void            BeginValueEdit    (DxuiListView * list, int row);
    void            EndValueEdit      (bool commit);
    DxuiTextInput * GetStackEditor    () const { return m_stackEditor; }
    DxuiTextInput * GetRegisterEditor () const { return m_registerEditor; }
    DxuiListView  * GetStackList      () const { return m_stackList; }
    DxuiListView  * GetRegisterList   () const { return m_registerList; }

    //  Protected so a test can see which pane has the focus border.
    std::wstring    GetPaneOfFocus () const;

    //  Protected so a test can press Shift+Esc's close.
    bool            ClosePaneOfFocus ();

    //  Protected so a test can work the memory bar as a click does, and see
    //  which of its entries a narrow pane moves into its overflow menu.
    DxuiToolbar *                      GetMemoryBar         () const { return m_memoryBar; }
    const std::vector<std::wstring> &  GetMemoryHistory     () const { return m_memoryHistory; }
    void                               RunMemoryBarEntry    (int id);
    void                               ChooseMemoryColumns  (int columns);
    void                               ChooseMemoryGrouping (int grouping);

    //  Protected so a test can see that every find bar control has a tip.
    const wchar_t *              GetFindBarTip   (POINT clientPx, RECT & anchor) const;
    std::vector<IDxuiControl *>  GetFindControls () const;

    //  Protected so a test can close a floating pane as its close button
    //  does, and see the layout it leaves.
    void                    CloseFloatingPane (const std::wstring & pane);
    const DxuiPaneLayout &  GetPaneLayout     () const;
    DxuiPaneLayout &        EditPaneLayout    ();

    //  Protected so a test can work the View menu as a click does: the
    //  panes it lists, and showing or closing one.
    std::vector<std::wstring>  GetViewMenuPanes () const;
    void                       ShowPane         (const std::wstring & pane);
    void                       ClosePane        (const std::wstring & pane);
    bool                       IsPaneShown      (const std::wstring & pane) const;

    //  The heat map pane: its view, whether the host was last told to record
    //  for it, and the two steps that keep the view and the recording current.
    HeatMapView              * GetHeatMapView       () const { return m_heatMapView; }
    bool                       IsHeatMapRecording   () const { return m_isHeatMapRecording; }
    void                       SyncHeatMapRecording ();
    void                       ApplyHeatMap         ();
    bool                       RouteHeatMapMouse    (const DxuiMouseEvent & ev);

    //  Protected so a test can read the breakpoints pane's columns (FR-117)
    //  and the breakpoint each row shows once sorted.
    void                                          ApplyBreakpoints       ();
    const DebuggerViewSnapshot::BreakpointLine *  GetBreakpointOfRow     (int row) const;
    void                                          ToggleBreakpointColumn (BreakpointColumns::Column column);
    void                                          SortBreakpoints        (BreakpointColumns::Column column);
    void                                          KeepOpenViews          ();
    DxuiListView *                                GetBreakpointList      () const { return m_breakpointList; }
    void                                          SetSnapshotForTest     (std::shared_ptr<const DebuggerViewSnapshot> snapshot) { m_snapshot = std::move (snapshot); }
    IDxuiControl *                                GetPaneContentForTest  (const std::wstring & pane) const { return GetPaneContent (pane); }

    //  Protected so a test can work the breakpoints pane's toolbar (FR-119)
    //  as a click does, and see which of its buttons can act.
    DxuiToolbar *                                      GetBreakpointBar       () const { return m_breakpointBar; }
    bool                                               IsBreakpointBarEnabled (int id) const;
    void                                               RunBreakpointBarEntry  (int id);
    void                                               NewBreakpoint          (BreakpointKind kind, WatchAccess access);
    void                                               RunBreakpointStep      (BreakpointStep step);
    std::vector<DebuggerViewSnapshot::BreakpointLine>  GetSelectedBreakpoints () const;

    //  Protected so a test can work the registers, stack and watch panes'
    //  Undo and Redo bars as a click does.
    DxuiToolbar *  GetUndoBar (const std::wstring & pane) const;

    //  Protected so a test can read the menu bar's menus as a click opens
    //  them, and the command bar's and the console bar's entries.
    static constexpr int  kDialectEntry    = 1;
    static constexpr int  kFindEntry       = 2;
    static constexpr int  kCodeOptionEntry = 100;

    void                                  SetWindowMenus     ();
    void                                  Detach             ();
    static std::wstring                   GetViewMenuGroup   (const std::wstring & pane);
    void                                  SetConsoleBarMenus ();
    const std::vector<DxuiMenuBarItem> &  GetMenuBarItems    () const { return m_menuBarItems; }
    DxuiToolbar *                         GetCommandBar      () const { return m_commandBar; }
    DxuiToolbar *                         GetConsoleBar      () const { return m_consoleBar; }
    std::wstring                          GetModeText        () const { return m_dialectCommand->label; }
    static std::wstring                   GetModeLabel       (const wchar_t * mode);
    DxuiToolbar *                         GetSourceBar       (int slot) const { return m_sourceDocs[(size_t) slot].bar; }
    DxuiMenuBar *                         GetMenuBar         () const { return m_menuBar; }

    //  Protected so a test can see whether a tip shows over the command bar,
    //  and that none shows while a menu is open.
    const DxuiTooltip &  GetTooltip    () const { return m_tooltip; }
    bool                 IsAnyMenuOpen () const;

    //  Set by Create; protected so a test can build the controls without a
    //  window, as OnCreate does, over a theme and host of its own.
    //  m_theme is the one in force, m_emulatorTheme the emulator's, which a
    //  window on its own theme still returns to.
    const DxuiTheme      * m_theme         = nullptr;
    const CassoTheme     * m_emulatorTheme = nullptr;
    IDebuggerWindowHost  * m_host          = nullptr;

private:
    //  The ids of files opened with no debug file record, above any a debug
    //  file gives.
    static constexpr int  kFirstLooseFileId = 1 << 20;

    //  A source document (FR-054): its text and banner, the pane over them,
    //  the frame the dock shows, and whether each is shown now.
    //  Each searchable pane's own find widget: its controls, whether it is
    //  open, its options, its count and its history. The m_find members
    //  hold a copy of the active one's.
    struct FindState
    {
        FindWidgetPlate          * plate       = nullptr;
        DxuiButton               * chevron     = nullptr;
        DxuiTextInput            * box         = nullptr;
        DxuiButton               * caseButton  = nullptr;
        DxuiButton               * wordButton  = nullptr;
        DxuiButton               * regexButton = nullptr;
        DxuiLabel                * status      = nullptr;
        DxuiButton               * prevButton  = nullptr;
        DxuiButton               * nextButton  = nullptr;
        DxuiButton               * selButton   = nullptr;
        DxuiButton               * closeButton = nullptr;
        DxuiWindow               * host        = nullptr;
        bool                       open        = false;
        bool                       matchCase   = false;
        bool                       wholeWord   = false;
        bool                       isRegex     = false;
        bool                       inSelection = false;
        int                        historyAt   = -1;
        std::wstring               statusText;
        std::vector<std::wstring>  history;
    };

    static constexpr size_t  kFindHistoryMax      = 20;
    static constexpr int     kFindWidgetWidthDip  = 460;
    static constexpr int     kFindWidgetHeightDip = 34;
    static constexpr int     kFindWidgetScrollDip = 18;
    static constexpr int     kFindCountDip        = 74;

    struct SourceDocument
    {
        DxuiTextView                        * view        = nullptr;
        DxuiActionBanner                    * banner      = nullptr;
        DxuiToolbar                         * bar         = nullptr;
        std::unique_ptr<SourcePane>           pane;
        std::unique_ptr<DebuggerPaneFrame>    frame;
        std::unique_ptr<DebuggerPaneFrame>    barSlot;
        bool                                  shown       = false;
        bool                                  bannerShown = false;
        std::wstring                          bannerKey;
        std::wstring                          title       = L"Source";
    };

    static constexpr int    kPreferredWidthDip  = 1100;
    static constexpr int    kPreferredHeightDip = 840;
    static constexpr int    kMinWidthDip        = 760;
    static constexpr int    kMinHeightDip       = 520;
    static constexpr int    kConsoleLineLimit   = 2000;
    static constexpr float  kConsoleLineSpacing = 1.2f;

    //  Pane metrics (FR-026a): the monospace face at a size whose line height
    //  a row barely exceeds, small cell padding, and eight rows owed to every
    //  pane at the default size.
    static constexpr float  kPaneFontDip           = 12.0f;
    static constexpr int    kPaneRowDip            = 16;
    static constexpr int    kPaneHeaderDip         = 22;
    static constexpr int    kPaneEdgeDip           = 6;
    static constexpr int    kPanePadDip            = 4;
    static constexpr int    kTraceHintDip          = 20;
    static constexpr int    kPaneRows              = 8;
    static constexpr int    kRegisterRows          = 6;
    static constexpr int    kMarkerColumnDip       = 20;
    //  The breakpoint icon's size in a list cell, which the source view's
    //  gutter draws it at too.
    static constexpr int    kBreakpointIconDip     = 16;
    static constexpr int    kGutterColumnDip       = 24;
    static constexpr int    kCodeInstructionColumn = 5;
    static constexpr size_t kCodeFirstTextColumn   = 2;
    static constexpr size_t kCodeColumnCount       = 7;

    //  The panes' text size runs from half to three times the usual, in
    //  steps of ten percentage points.
    static constexpr float  kMinTextZoom           = 0.5f;
    static constexpr float  kMaxTextZoom           = 3.0f;
    static constexpr float  kTextZoomStep          = 0.1f;

    static void  MakeDense (DxuiListView * list);
    static std::wstring  GetPromptText  (CommandMode mode);
    static std::wstring  GetHelpCommand (CommandMode mode);

    void     ConfigureWidgets ();
    void     LayoutWidgets    ();
    void     ApplySnapshot    ();
    void     ApplyHistory     ();
    void     UpdateChanges    ();
    std::vector<DxuiListView::Cell>  MakeWatchHeading (const std::wstring & title) const;
    void     RemoveSelectedWatch ();
    void     UndoWatchEdit    (bool redo);
    void     UndoRegisterEdit (bool redo);
    void     UndoStackEdit    (bool redo);
    void     UndoMemoryEdit   (MemoryPane * pane, bool redo);
    void     UpdateCodeLines  ();
    void     SubmitCommandBox ();
    void     RunToCursor      (Word address);
    void     RunAction        (const DebuggerAction & action);
    CommandMode  GetMode      () const;
    DebuggerKeyScheme  GetSavedKeyScheme () const;
    bool     RouteBoxKey      (const DxuiKeyEvent & ev, bool & handled);
    bool     RouteFindKey     (const DxuiKeyEvent & ev, bool & handled);
    bool     RouteCompletionKey (const DxuiKeyEvent & ev);
    void     RefreshCommandGhost ();
    void     ShowHistoryList  ();
    void     ConfigureFindBar ();
    void     PlaceFindBar     ();
    void     PlaceActiveFindBar ();
    void     SetFindBarVisible (bool shown);
    void     UpdateTopLayer   ();
    void     MoveFindBar      (DxuiWindow * to);
    std::wstring        GetFindTarget () const;
    DxuiTextView      * GetFindView   () const;
    void     SaveFindState    ();
    void     LoadFindState    ();
    void     ActivateFind     (const std::wstring & pane);
    void     CreateFindWidget (const std::wstring & pane);
    std::wstring  GetFindPaneOfControl (const IDxuiControl * control) const;
    void     RecordFindHistory ();
    void     SearchAsTyped    ();
    void     RefindAfterOptionChange ();
    DebuggerPaneFrame * GetFindFrame  () const;
    static std::wstring  GetFindStatusText (DxuiTextView::FindResult result, int index, int count);
    void     ConfigureDockSite  ();
    void     ConfigureCommandBar ();
    std::string  GetMenuState   () const;
    void     RunCommandBarEntry  (int id);
    bool     IsCommandBarEntryEnabled (int id) const;
    bool     RouteCommandBarMouse (const DxuiMouseEvent & ev);
    bool     RouteCommandBarDrag  (const DxuiMouseEvent & ev);
    void     TearOffCommandBar    (POINT clientPx);
    void     SyncCommandBarFloat  ();
    void     FloatCommandBar      ();
    void     DockCommandBarBack   ();
    void     OnCommandBarDragEnd  (POINT screenPx);
    void     OnCommandBarFloatDrag (POINT screenPx);
    bool     RouteFloatingBarMouse (const DxuiMouseEvent & ev);
    RECT     GetFloatingBarRect   (POINT topLeftPx);
    void     SaveCommandBarDock   ();
    void     FinishCommandBarSnap ();
    void     ConfigureMenuBar     ();
    bool     RouteMenuBarMouse    (const DxuiMouseEvent & ev);
    bool     RouteMenuBarKey      (const DxuiKeyEvent & ev, bool & handled);
    DxuiMessageResult  OnKeyUp    (WPARAM vk, LPARAM lParam) override;
    std::shared_ptr<DxuiCommand>  MakeKeyedMenuCommand (int id, const std::wstring & label);
    std::shared_ptr<DxuiCommand>  MakeEditMenuCommand  (DxuiStandardCommand command, const std::wstring & label, const std::wstring & accelerator);
    void     ResetPaneLayout      ();
    void     OpenSourceFile       ();
    void     OpenReverseOptions   ();
    void     OpenSymbolFile       (const std::wstring & thenShow = std::wstring());
    void     OpenLooseFile        (const std::wstring & path, const std::string & text, bool isSource);
    void     ConfigureCodeBars    ();
    void     PlaceCodeBars        ();
    void     ConfigureConsoleBar  ();
    void     PlaceConsoleBar      ();
    bool     RouteConsoleBarMouse (const DxuiMouseEvent & ev);
    DxuiToolbar::Entry  MakeFindEntry (const std::wstring & pane);
    void     ConfigureSourceBars  ();
    void     PlaceSourceBars      ();
    bool     RouteSourceBarMouse  (DxuiToolbar * bar, const DxuiMouseEvent & ev);
    void     ConfigureMemoryBar   ();
    void     SetMemoryBarMenus    ();
    void     AddMemoryHistory     (const std::wstring & text);
    bool     RouteMemoryBarMouse  (const DxuiMouseEvent & ev);
    void     ConfigureBreakpointBar  ();
    void     SetBreakpointBarMenus   ();
    void     PlaceBreakpointBar      ();
    bool     RouteBreakpointBarMouse (const DxuiMouseEvent & ev);
    void     ExportBreakpoints       ();
    void     ImportBreakpoints       ();
    std::wstring  GetMemoryBarLabel  (int id) const;
    std::wstring  GetMemoryBarTip    (int id) const;
    bool          IsMemoryBarEnabled (int id) const;
    static std::shared_ptr<DxuiCommand>  MakeMenuCommand (const std::wstring & label, bool checked, std::function<void()> chosen);
    bool     IsDocumentPane     (const std::wstring & pane) const;
    bool     CanClosePane       (const std::wstring & pane) const;
    bool     IsFixedPane        (const std::wstring & pane) const;
    void     ShowPendingPane    ();
    void     ShowDockToMenu     (const std::wstring & pane, POINT clientPx);
    bool     ShowContentMenu    (const std::wstring & pane, POINT clientPx);
    void     ShowEditMenu       (IDxuiControl * control, POINT clientPx, std::vector<std::pair<std::wstring, std::function<void()>>> extra);
    void     AddListMenuItems   (DxuiListView * list, int row, int column, std::vector<std::pair<std::wstring, std::function<void()>>> & items);
    void     AddShowInMemory    (const std::wstring & what, const std::string & goTo, std::vector<std::pair<std::wstring, std::function<void()>>> & items);
    static int  GetColumnAt     (const DxuiListView * list, int xPx);
    bool     RouteDockKey       (const DxuiKeyEvent & ev);
    bool     RouteTraceKey      (const DxuiKeyEvent & ev);
    void     SaveTrace          ();
    void     ApplySavedPlacement ();

    //  Floating panes (FR-040): each floats in a DxuiDockedWindow of its own,
    //  its controls moved there whole. The window's routing serves every
    //  window, filtered to the controls of the one the event came from.
    std::vector<IDxuiControl *>  GetPaneControls   (const std::wstring & pane) const;
    IDxuiControl *               GetPaneContent    (const std::wstring & pane) const;
    std::wstring                 GetPaneTitle      (const std::wstring & pane) const;
    bool                         IsRoutable        (const IDxuiControl * control) const;
    void                         SetFocusedControl (IDxuiControl * control);
    HWND                         GetRoutingHwnd    () const;
    void                         RequestFloat      (const std::wstring & pane, POINT clientPx);
    void                         TearOffPane       (const std::wstring & pane, POINT clientPx);
    void                         CarryTornOffPane  ();
    void                         PlaceUnderGrab    (const std::wstring & pane);
    void                         DropCarriedTab    (const std::wstring & pane);
    void                         SetFloatFade      (const std::wstring & pane, bool on);
    void                         SyncFloats        ();
    void                         FloatControls     (const std::wstring & pane);
    void                         DockControls      (const std::wstring & pane);
    void                         SaveLayout        ();
    std::wstring                 ReadSavedLayout   ();
    bool                         RouteFloatMouse   (const std::wstring & pane, const DxuiMouseEvent & ev);
    bool                         RouteFloatKey     (const std::wstring & pane, const DxuiKeyEvent & ev);
    void                         OnFloatDrag       (const std::wstring & pane, POINT screenPx, bool ended);
    void                         ShowDragMarks     ();
    void                         HideDragMarks     ();
    void                         DockFloatingPane  (const std::wstring & pane);
    void                         SetBreakpointColumns ();

    static std::wstring                          GetMonitorKey (const RECT & rectPx);
    static std::vector<DxuiPaneLayout::Monitor>  GetMonitors   ();

    void     ApplyMemoryWindows ();
    void     PlaceMemoryBar     ();
    void     MoveMemoryBar      (DxuiWindow * to);
    DxuiWindow * GetPaneHost    (const std::wstring & pane);
    std::wstring GetBarRoutingPane (const std::wstring & pane) const;
    DxuiHwndSource * GetMenuHost () const;
    DxuiTooltip &    GetRoutedTooltip ();
    void     TickFloats         (int64_t now);
    void     ClipPaneControls   ();
    bool     RouteMemoryMouse   (const DxuiMouseEvent & ev);
    bool     RouteSourceMouse   (const DxuiMouseEvent & ev);
    bool     RouteConsoleMouse  (const DxuiMouseEvent & ev);
    void     NoteViewFocus      (bool isSource);
    void     NoteTabFocus       (POINT pointDip);
    static IDxuiControl * FindFirstFocusable (IDxuiControl * node);
    void     ApplySource        ();
    void     FollowPcSource     ();
    void     OpenSourceDocument (int fileId, int line, bool activate);
    void     CloseSourceDocument (int slot);
    void     RestoreSourceDocuments ();
    void     ToggleMacroBody    ();
    void     ShowSourceLine     (int fileId, int line);
    int      GetSourceSlotOf    (const std::wstring & pane) const;
    int      GetSourceSlotAt    (POINT atDip) const;
    void     ApplyDiagnostics   ();
    bool     IsRestoringViews   () const;
    DiagnosticsPane *  GetDiagnosticsPane (const std::wstring & pane) const;
    bool     ForwardToList    (DxuiListView * list, const DxuiMouseEvent & ev);
    void     ShowCode         (std::optional<Word> address);
    void     ConfigureCodeList (int view);
    int      GetCodeViewOf    (const IDxuiControl * control) const;
    int      GetOpenCodeViewCount () const;
    const std::vector<DebuggerViewSnapshot::CodeLine> &  GetCodeLines (int view) const;
    void     ApplyCodeView    (int view);
    void     EditRegister     (int row, bool onFlags);
    void     ClickRegister    (POINT clientPx);
    void     CommitStackByte  (int row, Byte typed);
    void     CommitRegister   (const std::string & name, Byte typed);
    void     UpdateTooltip    (POINT clientPx);
    bool     TryGetSymbolTip  (POINT clientPx, RECT & anchor, std::wstring & text) const;
    std::optional<Byte>  GetRegisterByte (const std::string & name) const;

    //  The debugger's colors, from the active theme: a breakpoint's red, the
    //  PC's arrow and row, the row another pane brought into view, a branch's
    //  destination, the annotations' comment color, and the syntax colors.
    bool      IsDarkTheme          () const;
    uint32_t  GetBreakpointArgb    () const;
    std::shared_ptr<const DxuiIconImage>  GetBreakpointIcon (bool enabled);
    std::shared_ptr<const DxuiIconImage>  GetHoverBreakpointIcon ();
    static std::shared_ptr<DxuiIconImage>  MakeDotIcon (uint32_t argb, bool filled);
    bool      TryGetSourceText     (int fileId, int line, std::wstring & text);
    uint32_t  GetPcMarkerArgb      () const;
    uint32_t  GetPcRowArgb         () const;
    uint32_t  GetNavigatedRowArgb  () const;
    uint32_t  GetTargetRowArgb     () const;
    uint32_t  GetAnnotationArgb    () const;
    uint32_t  GetChangedArgb       () const;

    //  The disassembly views' bytes, for the changed color in their bytes
    //  column.
    void      UpdateCodeChanges    ();
    uint32_t  GetResultArgb        () const;
    SourceSyntax::Colors  GetSyntaxColors () const;
    DebuggerTextColors::Set  GetTextColors () const;
    void     OfferPress       (IDxuiControl * control, const DxuiMouseEvent & ev, bool & handled);

    std::vector<DxuiListView *>  GetLists          () const;
    std::vector<MemoryPane *>    GetOpenMemoryPanes () const;
    MemoryPane *                 GetActiveMemoryPane () const;
    MemoryPane *                 GetFocusedMemoryPane () const;
    std::vector<IDxuiControl *>  GetPressTargets   () const;
    DxuiTextInput *              GetFocusedBox     () const;

    DxuiDpiScaler                           m_scaler;
    int                                     m_widthDip           = 0;
    int                                     m_heightDip          = 0;
    DxuiFocusManager                        m_focusMgr;
    DebuggerKeyScheme                       m_keyScheme          = DebuggerKeySchemes::kDefault;
    std::string                             m_themeName;
    std::optional<std::string>              m_themeBeforePreview;
    CassoTheme                              m_ownTheme;
    DxuiLightTheme                          m_lightTheme;
    DxuiDarkTheme                           m_darkTheme;
    bool                                    m_swallowSpace       = false;
    RECT                                    m_openedRect         = {};
    bool                                    m_placed             = false;
    std::optional<Word>                     m_navigatedTo;
    int                                     m_navigatedView      = 0;
    std::string                             m_menuState;
    uint32_t                                m_goToSerial         = 0;
    StopChanges                             m_stopChanges;
    ByteChanges                             m_codeChanges;

    //  What each row of the watch pane is, since the list mixes headings,
    //  automatic watches and the user's own: an automatic row carries its
    //  index into the snapshot, a manual one its watch id.
    enum class WatchRowKind { Heading, Automatic, Manual, Add };

    struct WatchRow
    {
        WatchRowKind  kind  = WatchRowKind::Heading;
        int           index = 0;
    };

    std::vector<WatchRow>                   m_watchRows;

    //  Editing a watch in place (FR-096): a box laid over the cell, as Visual
    //  Studio's watch window opens one. Column 0 is the expression, 1 the
    //  value; an automatic watch's expression is not editable. An automatic
    //  watch is found again by its key, since a snapshot taken during the
    //  edit can list the automatic watches in another order.
    struct WatchEdit
    {
        int          row    = -1;
        int          column = 0;
        WatchRow     what;
        std::string  autoKey;
    };

    DxuiTextInput                         * m_watchEditor        = nullptr;
    WatchEdit                               m_watchEdit;

    //  Editing a stack byte or a register's value in place: a box laid over
    //  the value cell. Each pane has its own box, so the box goes with the
    //  pane into a floating window.
    struct ValueEdit
    {
        DxuiListView  * list = nullptr;
        int             row  = -1;
    };

    static constexpr int                    kValueEditMaxChars   = 4;
    DxuiTextInput                         * m_stackEditor        = nullptr;
    DxuiTextInput                         * m_registerEditor     = nullptr;
    ValueEdit                               m_valueEdit;
    POINT                                   m_lastPressPx        = {};
    bool                                    m_arrowShowsTarget   = false;

    //  The watch pane's own undo and redo (FR-097), apart from every memory
    //  window's: Ctrl+Z in the watch pane puts back its last edit only.
    WatchHistory                               m_watchHistory;

    //  The registers pane's own undo and redo, cleared when the machine
    //  moves on from the stop its edits were made at.
    RegisterHistory                            m_registerHistory;

    //  The stack pane's, cleared the same way.
    StackHistory                               m_stackHistory;

    //  The optional views open at the last save, and whether the ones saved
    //  before have been reopened yet. Reopening is asynchronous, so saving
    //  waits until a snapshot shows them -- or a few pass without -- rather
    //  than write back the empty set the window started with.
    static constexpr int                  kSettlingSnapshots    = 10;
    std::string                           m_openViewsSaved;
    bool                                  m_openViewsRestored   = false;
    int                                   m_openViewsSettling   = 0;
    float                                 m_textZoom            = 1.0f;
    DxuiTooltip                           m_tooltip;
    std::shared_ptr<const DxuiIconImage>  m_breakpointIcons[2];
    uint32_t                              m_breakpointIconArgb  = 0;
    std::shared_ptr<const DxuiIconImage>  m_hoverBreakpointIcon;
    uint32_t                              m_hoverBreakpointArgb = 0;

    std::shared_ptr<const DebuggerViewSnapshot>     m_snapshot;
    std::vector<std::string>                        m_console;
    ConsoleHistory                                  m_consoleHistory;
    CommandCompletion                               m_completion;
    uint32_t                                        m_offeredSuggestionSerial = 0;

    DxuiToolbar                                                                    * m_commandBar         = nullptr;
    DxuiMenuBar                                                                    * m_menuBar            = nullptr;
    std::vector<DxuiMenuBarItem>                                                     m_menuBarItems;
    std::unique_ptr<DebuggerPaneFrame>                                               m_consoleBarSlot;
    DxuiToolbar                                                                    * m_consoleBar         = nullptr;
    std::shared_ptr<DxuiCommand>                                                     m_dialectCommand;
    std::unique_ptr<ToolbarLabelEntry>                                               m_modeLabel;
    std::unique_ptr<DebuggerCommands>                                                m_commands;
    std::vector<std::shared_ptr<DxuiCommand>>                                        m_menuCommands;
    DxuiDockSite                                                                   * m_dockSite           = nullptr;
    HINSTANCE                                                                        m_hInstance          = nullptr;
    std::map<std::wstring, std::unique_ptr<DxuiDockedWindow>>                        m_floats;
    std::map<std::wstring, IDxuiControl *>                                           m_floatFocus;
    std::map<std::wstring, std::unique_ptr<DxuiTooltip>>                             m_floatTips;
    DxuiDragOverlay                                                                  m_dragOverlay;
    std::wstring                                                                     m_routingPane;
    bool                                                                             m_syncFloats         = false;
    std::wstring                                                                     m_tornOffPane;
    std::array<bool, BreakpointColumns::kCount>                                      m_breakpointShown    = BreakpointColumns::GetDefaultShown();
    std::vector<size_t>                                                              m_breakpointOrder;
    int                                                                              m_breakpointSort     = -1;
    bool                                                                             m_breakpointReverse  = false;
    std::unique_ptr<DebuggerPaneFrame>                                               m_consoleFrame;
    std::array<bool, DebuggerViewState::kMaxMemoryWindows>                           m_memoryOpen         = {};
    DxuiListView                                                                   * m_codeList           = nullptr;
    std::array<DxuiListView *, DebuggerViewState::kMaxCodeViews>                     m_codeLists          = {};
    std::array<std::unique_ptr<DebuggerPaneFrame>, DebuggerViewState::kMaxCodeViews> m_codeFrames;
    std::array<std::unique_ptr<DebuggerPaneFrame>, DebuggerViewState::kMaxCodeViews> m_codeBarSlots;
    std::array<DxuiToolbar *, DebuggerViewState::kMaxCodeViews>                      m_codeBars           = {};
    std::vector<std::unique_ptr<ToolbarCheckEntry>>                                  m_codeOptionEntries;
    DisassemblyOptions                                                               m_codeOptions;
    std::array<std::vector<DisassemblyOptions::Row>, DebuggerViewState::kMaxCodeViews> m_codeRows;
    std::wstring                                                                     m_codeSourceKey;
    std::map<int, std::vector<std::wstring>>                                         m_codeSourceText;
    int                                                                              m_gutterHoverView    = -1;
    int                                                                              m_gutterHoverRow     = -1;
    int                                                                              m_pcDragView         = -1;
    int                                                                              m_pcDragRow          = -1;
    int                                                                              m_pcDragOverRow      = -1;
    std::array<bool, DebuggerViewState::kMaxCodeViews>                               m_codeOpen           = { true };
    uint32_t                                                                         m_shownPaneSerial    = 0;
    std::array<int, DebuggerViewState::kMaxCodeViews>                                m_codeLinesSentTo    = {};
    int                                                                              m_activeCode         = 0;
    std::array<std::optional<Word>, DebuggerViewState::kMaxCodeViews>                m_codeSelected;
    DxuiListView                                                                   * m_registerList       = nullptr;
    DxuiListView                                                                   * m_breakpointList     = nullptr;
    DxuiListView                                                                   * m_watchList          = nullptr;
    DxuiListView                                                                   * m_stackList          = nullptr;
    DxuiListView                                                                   * m_callStackList      = nullptr;
    DxuiButton                                                                     * m_callStackButton    = nullptr;
    std::unique_ptr<CallStackPane>                                                   m_callStackPane;
    std::unique_ptr<DebuggerPaneFrame>                                               m_callStackFrame;
    DxuiListView                                                                   * m_traceList          = nullptr;
    std::unique_ptr<TracePane>                                                       m_tracePane;
    KeyHintLine                                                                    * m_traceHint          = nullptr;
    HistoryBand                                                                    * m_codeHistoryBand    = nullptr;
    HistoryBand                                                                    * m_regHistoryBand     = nullptr;
    std::unique_ptr<DebuggerPaneFrame>                                               m_traceFrame;
    HeatMapView                                                                    * m_heatMapView        = nullptr;
    std::unique_ptr<DebuggerPaneFrame>                                               m_heatMapFrame;
    bool                                                                             m_isHeatMapOpen      = false;
    bool                                                                             m_isHeatMapRecording = false;
    std::array<std::unique_ptr<MemoryPane>, DebuggerViewState::kMaxMemoryWindows>    m_memoryPanes;
    std::array<std::unique_ptr<DebuggerPaneFrame>, DebuggerViewState::kMaxMemoryWindows>  m_memoryFrames;
    std::array<std::unique_ptr<DebuggerPaneFrame>, DebuggerViewState::kMaxMemoryWindows>  m_memoryBars;
    DxuiToolbar                                                                    * m_memoryBar          = nullptr;
    DxuiWindow                                                                     * m_memoryBarHost      = nullptr;
    std::wstring                                                                     m_memoryBarPane;
    std::unique_ptr<MemoryBarCommands>                                               m_memoryCommands;
    std::unique_ptr<MemoryAddressEntry>                                              m_addressEntry;
    std::unique_ptr<DebuggerPaneFrame>                                               m_breakpointFrame;
    std::unique_ptr<DebuggerPaneFrame>                                               m_breakpointSlot;
    DxuiToolbar                                                                    * m_breakpointBar      = nullptr;
    std::unique_ptr<BreakpointBarCommands>                                           m_breakpointCommands;

    //  The registers, stack and watch panes are each a bar of Undo and Redo
    //  over the pane's list, as the breakpoints pane is its bar over its rows.
    struct PaneUndoBar
    {
        std::wstring                         pane;
        DxuiListView                       * list = nullptr;
        DxuiToolbar                        * bar  = nullptr;
        std::unique_ptr<DebuggerPaneFrame>   slot;
        std::unique_ptr<DebuggerPaneFrame>   frame;
        std::unique_ptr<UndoBarCommands>     commands;
    };

    static constexpr size_t  kRegisterUndoBar = 0;
    static constexpr size_t  kStackUndoBar    = 1;
    static constexpr size_t  kWatchUndoBar    = 2;
    static constexpr size_t  kUndoBarCount    = 3;

    std::array<PaneUndoBar, kUndoBarCount>  m_undoBars;

    void          ConfigureUndoBars  ();
    void          PlaceUndoBars      ();
    bool          RouteUndoBarMouse  (const DxuiMouseEvent & ev);
    void          RunPaneUndo        (size_t index, bool redo);
    bool          IsPaneUndoEnabled  (size_t index, bool redo) const;
    std::wstring  GetPaneUndoTip     (size_t index, bool redo) const;
    std::vector<std::wstring>                                     m_memoryHistory;
    MemoryPane                                                  * m_activePane          = nullptr;
    std::string                                                   m_machine;
    DxuiTextView                                                * m_consoleView         = nullptr;
    DxuiTextInput                                               * m_commandBox          = nullptr;
    bool                                                          m_findOpen            = false;
    std::wstring                                                  m_findPane;
    std::wstring                                                  m_slidPane;
    DxuiWindow                                                  * m_findBarHost         = nullptr;
    std::wstring                                                  m_findStatusText;
    DxuiTextInput                                               * m_findBox             = nullptr;
    DxuiButton                                                  * m_findCaseButton      = nullptr;
    DxuiButton                                                  * m_findWordButton      = nullptr;
    DxuiButton                                                  * m_findRegexButton     = nullptr;
    bool                                                          m_findMatchCase       = false;
    bool                                                          m_findWholeWord       = false;
    bool                                                          m_findRegex           = false;
    bool                                                          m_findKeyClick        = false;
    DxuiButton                                                  * m_findPrevButton      = nullptr;
    DxuiButton                                                  * m_findNextButton      = nullptr;
    DxuiButton                                                  * m_findCloseButton     = nullptr;
    DxuiLabel                                                   * m_findStatus          = nullptr;
    FindWidgetPlate                                             * m_findPlate           = nullptr;
    DxuiButton                                                  * m_findChevronButton   = nullptr;
    DxuiButton                                                  * m_findSelectionButton = nullptr;
    bool                                                          m_findInSelection     = false;
    int                                                           m_findHistoryAt       = -1;
    std::map<std::wstring, FindState>                             m_findStates;
    DxuiTextInput                                               * m_memoryBox           = nullptr;
    std::array<SourceDocument, SourceDocuments::kMaxDocuments>    m_sourceDocs;
    SourceDocuments                                               m_documents;
    int                                                           m_macroLevel          = 0;
    std::wstring                                                  m_sourceLoadedFor;
    std::pair<int, int>                                           m_pcPlace             = { -1, 0 };
    std::vector<SourceDocuments::Saved>                           m_pendingSourceDocs;
    int                                                           m_activeSource        = 0;
    std::map<int, std::wstring>                                   m_looseSources;
    int                                                           m_nextLooseId         = kFirstLooseFileId;
    std::wstring                                                  m_pendingLooseSource;
    bool                                                          m_showSourceCode      = true;
    uint64_t                                                      m_sourceClickMs       = 0;
    POINT                                                         m_sourceClickAt       = {};
    std::vector<std::unique_ptr<DiagnosticsPane>>                 m_diagPanes;
    std::set<std::string>                                         m_diagOpen;

    //  The fixed panes closed from their close buttons, which the View menu
    //  shows again where the layout still keeps them, and a pane the View
    //  menu opened that comes forward once the next snapshot shows it.
    std::set<std::wstring>                                                           m_closedPanes;
    std::wstring                                                                     m_pendingShowPane;

    //  The command bar's edge and place along it, and a drag of its grab
    //  handle in progress, with where in the bar the handle was taken and
    //  the region under the menu bar it docks around.
    CommandBarDock                                                                   m_barDock;
    RECT                                                                             m_barArea     = {};
    bool                                                                             m_barDragging = false;
    POINT                                                                            m_barGrab     = {};

    //  A floating bar dragged into a band snaps into it: the place it takes
    //  once the move loop has ended, and, until the frame docks it, whether
    //  the drag goes on in this window.
    CommandBarDock                                                                   m_barSnapDock;
    bool                                                                             m_barSnapping   = false;
    bool                                                                             m_barSnapDragOn = false;

    //  The window the command bar floats in while it is torn off, and its
    //  tooltip, which shows only over that window. Events from it route
    //  under kBarFloatKey, which no pane has.
    static constexpr wchar_t                                                         kBarFloatKey[] = L"~commandBar";
    std::unique_ptr<DxuiToolbarWindow>                                               m_barFloat;

    //  How near an edge, past the bar's own thickness, a drag of the bar has
    //  to stay to dock there rather than float.
    static constexpr int                                                             kBarDockReachDp = 24;

    //  How far past its band a drag of the docked bar has to pull before
    //  the bar tears off to float.
    static constexpr int                                                             kBarPullDp      = 32;
};
