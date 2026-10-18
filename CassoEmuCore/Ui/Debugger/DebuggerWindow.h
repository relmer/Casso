#pragma once

#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/IDebuggerWindowHost.h"
#include "Debugger/Events/DebugLogDelta.h"
#include "Debugger/HeatMapRangeSets.h"
#include "Debugger/HeatMapSymbols.h"
#include "Debugger/Project/DebugProject.h"
#include "Seams/IHostDialogs.h"
#include "Debugger/Reverse/ReplayControl.h"
#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/CommandCompletion.h"
#include "Ui/Debugger/ByteChanges.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/ColorKeyPopup.h"
#include "Ui/Debugger/BreakpointColumns.h"
#include "Ui/Debugger/ConsoleHistory.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/DebuggerThemes.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/DisassemblyOptions.h"
#include "Ui/Debugger/RegisterHistory.h"
#include "Ui/Debugger/StackHistory.h"
#include "Ui/Debugger/StopChanges.h"
#include "Ui/Debugger/KeyHintLine.h"
#include "Ui/Debugger/OpeningFocusDeferral.h"
#include "Ui/Debugger/StopFocus.h"
#include "Ui/Debugger/FocusAccentOwner.h"
#include "Ui/Debugger/OperandResultTip.h"
#include "Debugger/Reverse/HistoryThumbnails.h"
#include "Debugger/Reverse/HistoryTimelineScrub.h"
#include "Ui/Debugger/DebuggerOptionsSheet.h"
#include "Ui/Debugger/MemoryFileDialog.h"
#include "Ui/Debugger/BreakpointDialog.h"
#include "Ui/Debugger/ByteDialog.h"
#include "Ui/Debugger/TextDialog.h"
#include "Ui/UiCommandTypes.h"
#include "Ui/Debugger/WholeWordButton.h"
#include "Ui/Debugger/WatchHistory.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/DiskLogPane.h"
#include "Ui/Debugger/Panes/InputLogPane.h"
#include "Ui/Debugger/ToolbarBoxEntry.h"
#include "Ui/Debugger/Panes/FindWidgetPlate.h"
#include "Ui/Debugger/Panes/HeatMapView.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"
#include "Ui/Debugger/Panes/SourceDocuments.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Chrome/CassoTheme.h"

class DebuggerCommands;
class MemoryAddressEntry;
class UndoBarCommands;
class MemoryBarCommands;
class HeatMapBarCommands;
class HeatMapRangeBarCommands;
class ColorKeyButton;
class ToolbarCheckEntry;
class ToolbarLabelEntry;
class HistoryBand;
class BasicProgramController;
class BookmarksPane;
class DataBlocksPane;
class ProfilerPane;
class ProfileTimeline;
class FlameGraphView;
class HeatMapPane;
class HeatRangesPane;
class MemoryWindowGroup;
class RegistersPane;
class StackPane;
class WatchesPane;
class SymbolsPane;
class BreakpointsPane;
class MemoryPane;
class TracePaneController;





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

class DebuggerWindow : public DxuiWindow, private IDebuggerPaneHost
{
public:
    DebuggerWindow();
    ~DebuggerWindow() override;

    //  The dense look every pane's list has, public so the Options sheet's
    //  preview matches the disassembly views.
    static void  MakeDense (DxuiListView * list);

    //  An Undo or Redo item and tip, quoting the edit: Undo "changed 2 bytes
    //  at $0300", or the bare verb when there is nothing to say.
    static std::wstring        GetUndoLabel            (bool redo, const std::wstring & text);

    HRESULT  Create      (HINSTANCE hInstance, HWND hwndOwner, const CassoTheme * theme, IDebuggerWindowHost * host, bool activate);
    void     RenderFrame ();

    //  Detaches the host, for a shell tearing down before the window.
    void     DetachHost  () { m_host = nullptr; }

    //  Writes the placement if the user put the window somewhere else, for a
    //  shell shutting down while the window is still open.
    void     SavePlacementIfMoved ();

    //  Puts an event log pane in front, opening it for good.
    void     OpenEventLog         (DebugLogKind kind);

protected:
    void     OnCreate        () override;
    void     OnWindowPlaced  () override;
    void     OnWindowClose   () override;
    void     Layout          (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool     OnMouse         (const DxuiMouseEvent & ev) override;
    bool     OnKey           (const DxuiKeyEvent   & ev) override;
    bool     OnMappedCommand (int commandId) override;
    bool     IsCommandBarEntryEnabled (int id) const;
    bool     TryStopReplay   ();
    bool     OnFilesDropped  (const std::vector<std::wstring> & paths) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;
    void     PaintTopLayer   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool     HasTopLayer     () const override;
    void     PaintDragLayer  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool     HasDragLayer    () const override;
    void     OnWindowFocusChanged (bool focused) override;
    void     PaintBranchArrow (IDxuiPainter & painter, int view);
    bool     GetBranchArrow   (int view, BranchArrow::Input & input, Word & goesTo, bool & isTaken, bool isBranchedFrom = false) const;
    bool     ClickBranchArrow (POINT pointPx);

    //  Protected so a test can apply one as the keys do.
    void     ApplyTextZoom   (float zoom);
    void     StepTextZoom    (int steps);
    float    GetTextZoom     () const { return m_textZoom; }

    //  Protected so a test can submit the Address box as Enter does.
    void             SubmitMemoryBox ();
    DxuiTextInput  * GetMemoryBox    () const;

    //  Protected so a test can focus a control as a click does.
    DxuiTextInput  * GetCommandBox   () const { return m_commandBox; }
    void             FocusControl    (IDxuiControl * control) override { SetFocusedControl (control); }

    //  Protected so a test can write to the console as the host's lines do,
    //  and search it as the find bar does.
    void             AppendConsole   (const std::vector<std::string> & lines) override;
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

    //  The colors the system themes take, given by a test in place of the
    //  ones Windows holds.
    void             SetSystemColorsForTest (const DxuiWindowsThemeColors::SystemColors & colors) { m_systemColorsForTest = colors; }

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
    DxuiTextInput  * GetWatchEditor   () const;

    //  Protected so a test can apply a snapshot's disassembly as a frame does.
    void            ApplyCodeSnapshot (std::shared_ptr<const DebuggerViewSnapshot> snapshot, int view) { m_snapshot = std::move (snapshot); ApplyCodeView (view); }
    DxuiListView  * GetCodeList       (int view) const { return m_codeLists[(size_t) view]; }

    //  Protected so a test can read and run a code row's menu entries.
    std::optional<std::pair<Word, Word>>  GetCodeSelection (int view, int row) const;
    void                   AddCodeEditItems (int view, const DebuggerViewSnapshot::CodeLine & line, int row,
                                             std::vector<std::pair<std::wstring, std::function<void()>>> & items);
    void                   EditInstruction  (int view, const DebuggerViewSnapshot::CodeLine & line);
    void                   ReplaceWithNops  (int view, std::pair<Word, Word> range);
    void                   UndoCodeEdit     (bool redo);
    bool                   TryGetCodeBytes  (int view, Word first, Word last, std::vector<Byte> & bytes) const;

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

    //  Renaming a breakpoint in place (FR-172), as F2, a double-click on its
    //  Name or Rename does; protected so a test can do the same.
    void            BeginNameEdit  (int row);
    void            EndNameEdit    (bool commit);
    IDxuiControl  * GetFocused     () const;
    DxuiListView  * GetWatchList   () const;

    //  Protected so a test can edit a stack byte or a register in place as a
    //  click, F2 and Enter do.
    DxuiTextInput * GetStackEditor    () const;
    DxuiTextInput * GetRegisterEditor () const;
    DxuiListView  * GetStackList      () const;
    DxuiListView  * GetRegisterList   () const;

    //  Protected so a test can see which pane has the focus border.
    std::wstring    GetPaneOfFocus () const;

    //  Protected so a test can press Shift+Esc's close.
    bool            ClosePaneOfFocus ();

    //  Protected so a test can work the memory bar as a click does, and see
    //  which of its entries a narrow pane moves into its overflow menu.
    DxuiToolbar *                      GetMemoryBar         () const;
    const std::vector<std::wstring> &  GetMemoryHistory     () const;
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

    //  The heat map pane, whether the host was last told to record for it, and
    //  the step that keeps the recording current. The rest forward to the
    //  pane, protected so a test can work its bar.
    HeatMapView              * GetHeatMapView       () const;
    DxuiToolbar              * GetHeatMapBar        () const;
    DxuiToolbar              * GetHeatViewBar       () const;
    bool                       IsHeatMapRecording   () const { return m_isHeatMapRecording; }
    void                       SyncHeatMapRecording ();

    //  The heat map's colors in the theme in force, for the map and the heat
    //  columns; and whether any heat column is on.
    HeatMapView::Palette       GetHeatPalette       () const override;
    bool                       IsAnyHeatColumnOn    () const;

    void                       ApplyHeatMap         ();
    void                       SetHeatMapBarMenus   ();
    void                       SetHeatMapOptions    (const HeatMapOptions & options);
    bool                       IsHeatMapBarEnabled  (int id) const;
    bool                       IsHeatMapBarChecked  (int id) const;
    std::wstring               GetHeatMapBarLabel   (int id) const;
    void                       AddHeatMapAccessItems (Word address, HeatMapOptions::Bank bank, PaneMenuItems & items);

    const std::vector<std::shared_ptr<DxuiCommand>> &  GetHeatMapFadeCommands () const;
    const std::vector<std::shared_ptr<DxuiCommand>> &  GetHeatViewCommands    () const;
    const std::vector<std::shared_ptr<DxuiCommand>> &  GetHeatMapBankCommands () const;

    //  The heat map's sets of ranges and the pane that edits them, protected
    //  so a test can work them as the bars, the list and the edit box do:
    //  the set the map shows and the set the pane edits, the pane's entries,
    //  an edit in place over a cell or the set's name, and a range's check.
    const HeatMapRangeSets &   GetHeatRanges          () const;
    const std::string &        GetEditedRangeSet      () const;
    DxuiEditableGrid *         GetHeatRangeList       () const;
    DxuiTextInput *            GetHeatRangeEditor     () const;
    std::wstring               GetHeatRangeError      () const;
    bool                       IsEditingHeatRange     () const;
    void                       ShowRangeSet           (const std::string & name);
    void                       EditRangeSet           (const std::string & name);
    void                       RunHeatRangeBarEntry   (int id);

    //  Protected so a test can add a pane of its own ahead of OnCreate, as
    //  CreatePanes adds the window's, over the window's pane host.
    void                 AddPane            (std::unique_ptr<IDebuggerPane> pane) { m_panes.push_back (std::move (pane)); }
    IDebuggerPaneHost  & GetPaneHostForTest () { return *this; }

    //  The profiler bar's commands, protected so a test can work the bar.
    const std::vector<std::shared_ptr<DxuiCommand>> &  GetProfilerCommands () const;
    bool                       IsHeatRangeBarEnabled  (int id) const;
    void                       BeginHeatRangeEdit     (int row, HeatMapRangeSets::Field field);
    void                       EndHeatRangeEdit       (bool commit);
    void                       ToggleHeatRange        (int row, bool included);
    const std::vector<std::shared_ptr<DxuiCommand>> &  GetRangeSetCommands () const;

    //  A question put to the user in a message box centered on owner, as
    //  DxuiMessageBox puts it, with its answer; a test answers it instead.
    int                        ShowMessageBox         (HWND owner, const std::wstring & text, const wchar_t * caption, UINT type) override;

    //  The format, address and length of a file loaded into memory, or the
    //  address and length of memory saved to one; none when canceled. A test
    //  overrides it.
    //  A byte for Push byte..., none when canceled; a test overrides it.
    std::optional<Byte>  RunByteDialog (const std::wstring & title, const std::wstring & label) override;

    //  A line typed for a Memory pane's Find, Fill, Copy or Compare, none when
    //  canceled; a test overrides it.
    std::optional<std::wstring>  RunTextDialog (const std::wstring & title, const std::wstring & label, const std::wstring & placeholder,
                                                TextDialog::CheckFn check) override;

    virtual std::optional<MemoryFileDialog::Fields>  RunMemoryFileDialog (const std::wstring & path, bool isSave, const MemoryFileDialog::Fields & initial);

    //  The step filters after Debug > Step filters..., none when canceled;
    //  a test overrides it.
    virtual std::optional<std::vector<std::wstring>>  RunStepFiltersDialog (const std::vector<std::wstring> & filters,
                                                                            std::function<bool (const std::wstring &, std::wstring &)> check);
    void                       EditStepFilters        ();

    //  Whether a frame repaints the window: when it took an update, while
    //  something moves, and otherwise every kIdlePaintMs, for a caret's blink.
    static constexpr int64_t   kIdlePaintMs           = 100;
    static bool                IsFramePaintDue        (bool hasUpdate, bool isAnimating, int64_t nowMs, int64_t paintedAtMs);
    void                       RequestPaint           ();

    //  Protected so a test can read the breakpoints pane's columns (FR-117)
    //  and the breakpoint each row shows once sorted.
    void                                          ApplyBreakpoints       ();
    const DebuggerViewSnapshot::BreakpointLine *  GetBreakpointOfRow     (int row) const;
    void                                          ToggleBreakpointColumn (BreakpointColumns::Column column);
    void                                          SortBreakpoints        (BreakpointColumns::Column column);
    void                                          KeepOpenViews          ();
    DxuiListView *                                GetBreakpointList      () const;
    void                                          SetSnapshotForTest     (std::shared_ptr<const DebuggerViewSnapshot> snapshot) { m_snapshot = std::move (snapshot); }
    IDxuiControl *                                GetPaneContentForTest  (const std::wstring & pane) const { return GetPaneContent (pane); }

    //  Protected so a test can work the breakpoints pane's toolbar (FR-119)
    //  as a click does, and see which of its buttons can act.
    DxuiToolbar *                                      GetBreakpointBar       () const;
    bool                                               IsBreakpointBarEnabled (int id) const;
    void                                               RunBreakpointBarEntry  (int id);
    void                                               AddZeroPagePointer     () override;

    //  Protected so a test can work the registers, stack and watch panes'
    //  Undo and Redo bars as a click does.
    DxuiToolbar *  GetUndoBar (const std::wstring & pane) const;

    //  Protected so a test can read whether the memory bar's entries,
    //  Undo and Redo among them, are enabled.
    bool  IsMemoryBarEnabled (int id) const;

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

    //  The console's modes in its drop-down's order: alphabetical by label,
    //  without regard to case.
    using ModeChoice = std::pair<CommandMode, const wchar_t *>;

    static const std::vector<ModeChoice> &  GetModeChoices ();

    //  Protected so a test can see whether a tip shows over the command bar,
    //  and that none shows while a menu is open.
    const DxuiTooltip &  GetTooltip    () const { return m_tooltip; }

    //  A pane's info button, and the color key it opens.
    ColorKeyButton *       GetColorKey      (const std::wstring & pane) const;
    const ColorKeyPopup &  GetColorKeyPopup () const { return m_colorKeyPopup; }
    bool                 IsAnyMenuOpen () const;

    //  Protected so a test can read the status bar as a frame leaves it, and
    //  work its zoom popup as a click does.
    static constexpr size_t  kStatusReplay  = 0;
    static constexpr size_t  kStatusBeam    = 1;
    static constexpr size_t  kStatusHistory = 2;
    static constexpr size_t  kStatusSpeed   = 3;
    static constexpr size_t  kStatusZoom    = 4;

    DxuiStatusBar *  GetStatusBar        () const { return m_statusBar; }
    DxuiSlider &     GetZoomSlider       ()       { return m_zoomSlider; }
    bool             IsZoomPopupOpen     () const { return m_zoomOpen; }
    RECT             GetZoomPopupRect    () const { return m_zoomRect; }
    void             UpdateStatusBar     ();
    bool             RouteStatusBarMouse (const DxuiMouseEvent & ev);
    void             FitHistoryField     (IDxuiTextRenderer & text);

    //  Protected so a test can read the tip that says what a color means where
    //  the pointer rests: over a list's cell, the PC's branch arrow, the disk
    //  head or the history meter. And the colors the legend shows.
    bool                  TryGetCellTip    (POINT clientPx, RECT & anchor, std::wstring & text) const;
    bool                  TryGetBranchTip  (POINT clientPx, RECT & anchor, std::wstring & text) const;
    bool                  TryGetGraphicTip (POINT clientPx, RECT & anchor, std::wstring & text) const;
    ColorLegend::Palette  GetColorPalette  () const;

    //  Protected so a test can lay out the tip over a disassembly row's
    //  operand and result where the pane cuts them off, measured by a
    //  renderer of its own on a screen of its own, and see the tip the
    //  pointer's moves, presses and keys, new rows and new layouts leave up:
    //  the window keeps it whether or not a popup draws it. The cell is the
    //  view and row the tip covers, and the part of the cell, in the window's
    //  pixels, the pointer keeps it up over.
    struct OperandTipCell
    {
        int   view = -1;
        int   row  = -1;
        RECT  area = {};
    };

    //  The screen around the window the tip is in, in that window's client
    //  pixels: each monitor's work area, and the windows lying over this
    //  one, such as a floating pane, command bar or timeline.
    struct OperandTipScreen
    {
        std::vector<RECT>  workAreas;
        std::vector<RECT>  overWindows;
    };

    bool  TryGetOperandTip       (POINT                       clientPx,
                                  const DxuiDpiScaler       & scaler,
                                  IDxuiTextRenderer         & text,
                                  const OperandTipScreen    & screen,
                                  OperandResultTip::Layout  & out,
                                  OperandTipCell            & outCell) const;
    void  PlaceOperandTip        (const DxuiMouseEvent      & ev,
                                  bool                        isPressed,
                                  const DxuiDpiScaler       & scaler,
                                  IDxuiTextRenderer         & text,
                                  const OperandTipScreen    & screen);
    void  CheckOperandTip        (int view);
    void  CheckOperandTipPointer (POINT clientPx);
    RECT  GetColorTipAnchor      (const RECT & cell) const;
    bool  HasOperandTip          () const { return m_hasOperandTip; }

    const OperandResultTip::Layout &  GetOperandTipLayout () const { return m_operandTipLayout; }
    const OperandTipCell &            GetOperandTipCell   () const { return m_operandTipCell; }

    //  The renderer and screen the tip is measured with in a window that has
    //  no HWND, as a test builds it.
    void  SetOperandTipDeviceForTest (IDxuiTextRenderer * text, const OperandTipScreen & screen) { m_operandTipTestText = text; m_operandTipTestScreen = screen; }

    //  Protected so a test can slide out an auto-hidden pane, as a press on
    //  its tab does.
    DxuiDockSite *  GetDockSite () const { return m_dockSite; }

    //  Protected so a test can make a seek or a go live asked for while the
    //  machine ran once it has stopped, as a frame does.
    void  MakePendingSeek ();

    //  Set by Create; protected so a test can build the controls without a
    //  window, as OnCreate does, over a theme and host of its own.
    //  m_theme is the one in force, m_emulatorTheme the emulator's, which a
    //  window on its own theme still returns to.
    const DxuiTheme      * m_theme         = nullptr;
    const CassoTheme     * m_emulatorTheme = nullptr;
    IDebuggerWindowHost  * m_host          = nullptr;

    //  The text colors for m_theme, which GetTextColors reads every frame.
    mutable DebuggerTextColors::Cache  m_textColors;

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
    static constexpr int    kPanePadDip            = 4;
    static constexpr int    kPaneRows              = 8;
    static constexpr int    kRegisterRows          = 6;
    //  The breakpoint icon's size in a list cell, which the source view's
    //  gutter draws it at too.
    static constexpr int    kBreakpointIconDip     = 16;
    //  The glyph margin a code view keeps for its breakpoints and the PC's
    //  arrow, as Visual Studio's is: each glyph centered 8.4 DIP in from the
    //  pane's body, 10.5 pixels at 125%, so its left edge is 2.8 DIP inside
    //  it, in a column twice that wide; the address follows the column.
    static constexpr float  kGlyphCenterDip        = DisassemblyCells::kGlyphCenterDip;
    static constexpr int    kGutterColumnDip       = DisassemblyCells::kGutterColumnDip;
    static constexpr size_t kCodeGutterColumn      = DisassemblyCells::kGutterColumn;
static constexpr size_t kCodeHeatColumn        = DisassemblyCells::kHeatColumn;
    static constexpr size_t kCodeAddressColumn     = DisassemblyCells::kAddressColumn;
    static constexpr size_t kCodeBytesColumn       = DisassemblyCells::kBytesColumn;
    static constexpr size_t kCodeLabelColumn       = DisassemblyCells::kLabelColumn;
    static constexpr size_t kCodeInstructionColumn = DisassemblyCells::kInstructionColumn;
    static constexpr size_t kCodeOperandColumn     = DisassemblyCells::kOperandColumn;
    static constexpr size_t kCodeFirstTextColumn   = kCodeAddressColumn;
    static constexpr size_t kCodeColumnCount       = DisassemblyCells::kColumnCount;

    //  The panes' text size runs from half to three times the usual, in
    //  steps of ten percentage points.
    static constexpr float  kMinTextZoom           = 0.5f;
    static constexpr float  kMaxTextZoom           = 3.0f;
    static constexpr float  kTextZoomStep          = 0.1f;

    static std::wstring  GetPromptText  (CommandMode mode);
    static std::wstring  GetHelpCommand (CommandMode mode);

    void     ConfigureWidgets ();
    void     LayoutWidgets    ();
    void     CreateStatusBar  ();
    int      PlaceStatusBar   (int width, int height);
    void     ToggleBeamMark   ();
    void     OpenZoomPopup    ();
    void     CloseZoomPopup   ();
    void     PaintZoomPopup   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void     ApplySnapshot    ();
    void     ApplyHistory     ();
    void     UpdateChanges    ();
    void     UpdateCodeLines  ();
    void     SubmitCommandBox ();
    void     RunToCursor      (Word address);
    void     RunAction        (const DebuggerAction & action) override;
    CommandMode  GetMode      () const override;
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
    void     RunCommandBarEntry  (int id) override;
    bool     RouteCommandBarMouse (const DxuiMouseEvent & ev);
    void     ConfigureCommandBarHost ();
    void     SyncCommandBarFloatTip  ();
    bool     RouteFloatingBarMouse (const DxuiMouseEvent & ev);

    //  The history timeline: a toolbar of history thumbnails docked and
    //  floated as the command bar is, and the click that seeks there. A
    //  history band's Go live link goes live through it while the machine
    //  runs.
    void     ConfigureTimeline     ();
    bool     RouteTimelineMouse    (const DxuiMouseEvent & ev);
    void     SyncTimeline          ();
    void     OnTimelineSeek        (const HistoryThumbnailCell & cell);
    void     OnTimelineScrub       (uint64_t cycle, bool isFinal);
    void     SyncTimelineScrub     ();
    void     ApplyTimelineScrub    (const HistoryTimelineScrubStep & step);
    void     GoLiveFromBand        ();
    void     ConfigureMenuBar     ();
    bool     RouteMenuBarMouse    (const DxuiMouseEvent & ev);
    bool     RouteMenuBarKey      (const DxuiKeyEvent & ev, bool & handled);
    DxuiMessageResult  OnKeyUp    (WPARAM vk, LPARAM lParam) override;
    std::shared_ptr<DxuiCommand>  MakeKeyedMenuCommand (int id, const std::wstring & label);
    std::shared_ptr<DxuiCommand>  MakeEditMenuCommand  (DxuiStandardCommand command, const std::wstring & label, const std::wstring & accelerator);
    void     ResetPaneLayout      ();
    void     OpenSourceFile       ();
    void     OpenOptions          ();
    void     OpenSymbolFile       (const std::wstring & thenShow = std::wstring());
    void     LoadFileIntoMemory   ();

    //  The window's state a debug project saves with no setting of its own:
    //  the console history, each pane's find history and options, and the
    //  widths of the columns the user sized. A project's switch brings back
    //  the rest of its settings too.
    void     RestoreWindowState   (bool isProjectSwitch);
    void     SaveConsoleHistory   ();
    void     SaveFindStates       ();
    void     SaveColumnWidths     ();
    void     ApplyColumnWidths    ();
    void     SaveUndoHistories    ();
    std::string  GetUndoHistoriesText () const;
    void     ApplySavedFind       (const std::wstring & pane, FindState & state) const;
    void     OpenSpeedMenu        (const RECT & field);
    void     SaveMemoryToFile     ();
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
    void     ApplySavedPlacement ();

    //  Debug projects (FR-170, FR-171): File's Save project as..., Open
    //  project... and Project settings..., and the layout, options and views
    //  read again when another project opens.
    void     SaveProjectAs        ();
    void     OpenProject          ();
    void     EditProjectSettings  ();
    void     ReloadProjectState   ();

    //  Floating panes (FR-040): each floats in a DxuiDockedWindow of its own,
    //  its controls moved there whole. The window's routing serves every
    //  window, filtered to the controls of the one the event came from.
    std::vector<IDxuiControl *>  GetPaneControls   (const std::wstring & pane) const;
    std::vector<IDxuiControl *>  GetOwnControls    (const std::wstring & pane) const;
    IDxuiControl *               GetPaneContent    (const std::wstring & pane) const;
    std::wstring                 GetPaneTitle      (const std::wstring & pane) const;
    bool                         IsRoutable        (const IDxuiControl * control) const override;
    void                         SetFocusedControl (IDxuiControl * control);
    HWND                         GetRoutingHwnd    () const;
    void                         RequestFloat      (const std::wstring & pane, POINT clientPx);
    void                         TearOffPane       (const std::wstring & pane, POINT clientPx);
    void                         CarryTornOffPane  ();
    void                         PlaceUnderGrab    (const std::wstring & pane);
    void                         DropCarriedTab    (const std::wstring & pane);
    void                         SetFloatFade      (const std::wstring & pane, bool on);
    void                         SyncFloats        ();
    void                         PlaceOpeningFocus ();
    void                         PlaceLateFocus    ();
    std::wstring                 GetSavedFocusPane () const;
    bool                         IsFocusPaneReady  (const std::wstring & pane) const;
    void                         FocusPane         (const std::wstring & pane);
    IDxuiControl *               GetMainControl    (const std::wstring & pane) const;
    void                         SaveFocusedPane   ();
    void                         FloatControls     (const std::wstring & pane);
    void                         DockControls      (const std::wstring & pane);
    void                         SaveLayout        ();
    std::wstring                 ReadSavedLayout   ();
    SIZE                         GetPaneMinimum    (const std::wstring & pane) const;
    bool                         RouteFloatMouse   (const std::wstring & pane, const DxuiMouseEvent & ev);
    bool                         RouteFloatKey     (const std::wstring & pane, const DxuiKeyEvent & ev);
    void                         OnFloatDrag       (const std::wstring & pane, POINT screenPx, bool ended);
    void                         ShowDragMarks     ();
    void                         HideDragMarks     ();
    void                         ShowFocusAccent   ();
    void                         DockFloatingPane  (const std::wstring & pane);

    static std::wstring                          GetMonitorKey (const RECT & rectPx);
    static std::vector<DxuiPaneLayout::Monitor>  GetMonitors   ();

    void     ApplyUndoHistories ();
    DxuiWindow * GetPaneHost    (const std::wstring & pane);
    std::wstring GetBarRoutingPane (const std::wstring & pane) const;
    DxuiHwndSource * GetMenuHost () const;
    DxuiTooltip &    GetRoutedTooltip ();
    void     TickFloats         (int64_t now);
    void     ClipPaneControls   ();
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
    bool     IsRestoringViews   () const override;
    bool     ForwardToList    (DxuiListView * list, const DxuiMouseEvent & ev);
    void     ShowCode         (std::optional<Word> address);
    void     ConfigureCodeList (int view);
    int      GetCodeViewOf    (const IDxuiControl * control) const;
    int      GetOpenCodeViewCount () const;
    const std::vector<DebuggerViewSnapshot::CodeLine> &  GetCodeLines (int view) const;
    void                                                 AddSymbolAt  (Word address);
    void     ApplyCodeView    (int view);
    void     UpdateTooltip    (POINT clientPx) override;
    void     UpdateOperandTip (const DxuiMouseEvent & ev, bool isPressed);
    void     ShowOperandTip   ();
    void     HideOperandTip   ();

    //  The operand tip's parts: the pointer it follows between moves, the
    //  view and row under a point, its layout over a row, what may lie over
    //  it, the fill under the row, the screen it stays on, and the color tip
    //  put below it.
    struct OperandTipWindowSearch;

    void                   FollowOperandTipPointer ();
    int                    FindOperandTipView      (POINT clientPx) const;
    bool                   TryFindOperandTipRow    (POINT clientPx, int & view, int & row) const;
    bool                   TryMakeOperandTip       (int                         view,
                                                    int                         row,
                                                    const DxuiDpiScaler       & scaler,
                                                    IDxuiTextRenderer         & text,
                                                    const OperandTipScreen    & screen,
                                                    OperandResultTip::Layout  & out,
                                                    OperandTipCell            & outCell) const;
    bool                   IsOperandTipCovered     (int                         view,
                                                    POINT                       clientPx,
                                                    const RECT                & tipRect,
                                                    const OperandTipScreen    & screen) const;
    uint32_t               GetCodeRowFill          (const DxuiListView * list, int row) const;
    OperandTipScreen       GetOperandTipScreen     (const DxuiHwndSource * host) const;
    void                   EraseFloatTip           (const std::wstring & key);
    static bool            IsTextSelectedUnderTip  (const DxuiListView * list, int row, const OperandResultTip::Layout & layout);
    static bool            Meets                   (const RECT & over, POINT clientPx, const RECT & tipRect);
    static BOOL CALLBACK   CollectWindowOver       (HWND hwnd, LPARAM param);

    std::wstring  GetTitleButtonTipAt (POINT clientPx, RECT & button) const;
    bool     TryGetSymbolTip  (POINT clientPx, RECT & anchor, std::wstring & text) const;

    //  The anchor of a tip that follows the pointer: the pointer's own pixel.
    static RECT  GetPointerAnchor (POINT clientPx) { return RECT { clientPx.x, clientPx.y, clientPx.x + 1, clientPx.y + 1 }; }
    std::optional<Byte>  GetRegisterByte (const std::string & name) const;

    //  The debugger's colors, from the active theme: a breakpoint's red, the
    //  PC's arrow and row, the row another pane brought into view, a branch's
    //  destination, the annotations' comment color, and the syntax colors.
    bool      IsDarkTheme          () const;
    uint32_t  GetBreakpointArgb    () const override;
    std::shared_ptr<const DxuiIconImage>  GetBreakpointIcon (bool enabled) override;
    std::shared_ptr<const DxuiIconImage>  GetHoverBreakpointIcon ();

    //  The PC's arrow for the glyph margin, over `dot` when the line has one.
    std::shared_ptr<const DxuiIconImage>  GetPcIcon (const std::shared_ptr<const DxuiIconImage> & dot);
    bool      TryGetSourceText     (int fileId, int line, std::wstring & text);
    uint32_t  GetPcMarkerArgb      () const;
    uint32_t  GetPcRowArgb         () const override;
    uint32_t  GetNavigatedRowArgb  () const;
    uint32_t  GetTargetRowArgb     () const;
    uint32_t  GetAnnotationArgb    () const;
    uint32_t  GetChangedArgb       () const;

    //  The disassembly views' bytes, for the changed color in their bytes
    //  column.
    void      UpdateCodeChanges    ();
    uint32_t  GetResultArgb        () const;
    SourceSyntax::Colors  GetSyntaxColors () const override;
    DebuggerTextColors::Set  GetTextColors () const override;
    void     OfferPress       (IDxuiControl * control, const DxuiMouseEvent & ev, bool & handled);

    std::vector<DxuiListView *>  GetLists          () const;
    std::vector<DxuiListView *>  GetTextLists      () const;
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

    //  Set by a test, so choosing a system theme reads nothing from Windows.
    std::optional<DxuiWindowsThemeColors::SystemColors>  m_systemColorsForTest;

    bool                                    m_swallowSpace       = false;
    bool                                    m_swallowChordChar   = false;


    //  Find histories and options a project brought back for panes whose find
    //  widgets are not built yet, and the column widths in dips by pane.
    struct SavedFind
    {
        bool                       matchCase = false;
        bool                       wholeWord = false;
        bool                       isRegex   = false;
        std::vector<std::wstring>  history;
    };

    std::map<std::wstring, SavedFind>         m_savedFinds;
    std::map<std::wstring, std::vector<int>>  m_columnWidths;
    uint64_t                                  m_undoSavedMs = 0;

    //  Set when a restore replaced the undo histories, so the machine change
    //  that follows clears none of them.
    bool                                      m_isHistoryRestored = false;

    RECT                                    m_openedRect         = {};
    bool                                    m_placed             = false;
    std::optional<Word>                     m_navigatedTo;
    int                                     m_navigatedView      = 0;
    std::string                             m_menuState;
    uint32_t                                m_goToSerial         = 0;
    StopChanges                             m_stopChanges;
    ByteChanges                             m_codeChanges;

    POINT                                   m_lastPressPx        = {};
    bool                                    m_arrowShowsTarget   = false;

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

    //  A disassembly row's operand and result that the pane cuts off, shown
    //  whole over their cell: the popup, the window it shows in, whether the
    //  tip is up, what it shows, the cell it covers, and the renderer, scale
    //  and screen it was laid out with, which new rows and layouts are
    //  checked against. A window with no HWND, as a test builds it, measures
    //  with the renderer and screen the test gives it. The color tip is the
    //  tooltip last put up below the operand tip; it goes when that tip does.
    DxuiInPlaceTip                        m_operandTip;
    DxuiHwndSource                      * m_operandTipHost      = nullptr;
    bool                                  m_hasOperandTip       = false;
    OperandResultTip::Layout              m_operandTipLayout;
    OperandTipCell                        m_operandTipCell;
    IDxuiTextRenderer                   * m_operandTipText      = nullptr;
    DxuiDpiScaler                         m_operandTipScaler;
    OperandTipScreen                      m_operandTipScreen;
    IDxuiTextRenderer                   * m_operandTipTestText  = nullptr;
    OperandTipScreen                      m_operandTipTestScreen;
    DxuiTooltip                         * m_operandColorTip     = nullptr;

    //  The status bar along the bottom, and the zoom popup its zoom field
    //  opens above it, drawn on the top layer.
    static constexpr int                  kZoomPopupWidthDip    = 240;
    static constexpr int                  kZoomPopupHeightDip   = 52;
    static constexpr int                  kStatusZoomDip        = 64;
    static constexpr int                  kStatusSpeedDip       = 90;
    static constexpr int                  kStatusHistoryDip     = 480;
    static constexpr int                  kStatusBeamDip        = 220;
    DxuiStatusBar                       * m_statusBar           = nullptr;
    DxuiSlider                            m_zoomSlider;
    bool                                  m_zoomOpen            = false;
    bool                                  m_isSpeedPressed      = false;
    RECT                                  m_zoomRect            = {};
    bool                                  m_isHistoryFitted     = false;

    std::shared_ptr<const DxuiIconImage>  m_breakpointIcons[2];
    uint32_t                              m_breakpointIconArgb  = 0;
    std::shared_ptr<const DxuiIconImage>  m_hoverBreakpointIcon;
    uint32_t                              m_hoverBreakpointArgb = 0;

    //  The PC's arrow, alone and over each dot it has been drawn over. Each is
    //  kept with its dot, so a dot made later cannot take an old one's place
    //  at the same address; all are made again when the arrow's color
    //  changes.
    struct PcIcon
    {
        std::shared_ptr<const DxuiIconImage>  dot;
        std::shared_ptr<const DxuiIconImage>  icon;
    };

    std::vector<PcIcon>                   m_pcIcons;
    uint32_t                              m_pcIconArgb          = 0;

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
    FocusAccentOwner                                                                 m_accentOwner;
    DxuiDragOverlay                                                                  m_dragOverlay;
    std::wstring                                                                     m_routingPane;
    bool                                                                             m_syncFloats         = false;
    bool                                                                             m_isOpeningFocusDue  = true;
    OpeningFocusDeferral                                                             m_openingFocus;
    StopFocus                                                                        m_stopFocus;
    std::wstring                                                                     m_tornOffPane;
    std::unique_ptr<DebuggerPaneFrame>                                               m_consoleFrame;
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
    HistoryBand                                                                    * m_codeHistoryBand    = nullptr;
    bool                                                                             m_isHeatMapRecording = false;
    bool                                                                             m_isHeatColumnsRecording = false;
    int64_t                                                                          m_framePaintedAt     = 0;

    //  The BASIC program pane and the tool panes, each closed until the user
    //  first opens it, that were opened.
    std::set<std::wstring>                 m_basicOpened;

    //  The panes on the pane interface, in the order the window creates and
    //  calls them, and the profiler among them, whose bar's commands a test
    //  reads.
    std::vector<std::unique_ptr<IDebuggerPane>>    m_panes;
    ProfilerPane                                 * m_profilerPane    = nullptr;
    HeatMapPane                                  * m_heatMapPane     = nullptr;
    HeatRangesPane                               * m_heatRangesPane  = nullptr;
    MemoryWindowGroup                            * m_memoryGroup     = nullptr;
    RegistersPane                                * m_registersPane   = nullptr;
    StackPane                                    * m_stackPane       = nullptr;
    WatchesPane                                  * m_watchesPane     = nullptr;
    BreakpointsPane                              * m_breakpointsPane = nullptr;
    TracePaneController                          * m_tracePane       = nullptr;
    BasicProgramController                       * m_basicProgram    = nullptr;

    void                 CreatePanes            ();
    IDebuggerPane      * FindPane               (const std::wstring & pane) const;

    //  The window's calls the panes make (IDebuggerPaneHost).
    DxuiPanel          & GetControlParent       () override { return *this; }
    void                 GoToCode               (Word address) override { ShowCode (address); }
    IDebuggerWindowHost * GetWindowHost         () override { return m_host; }
    bool                 IsPaneOpen             (const std::wstring & pane) const override { return IsPaneShown (pane); }
    IDxuiControl       * GetFocusedControl      () const override { return m_focusMgr.GetFocusedControl(); }
    void                 FocusDefault           () override { m_focusMgr.SetFocused (m_commandBox); }
    void                 HideTooltip            () override { GetRoutedTooltip().HideImmediate(); }
    void                 ShowPcCode             () override { ShowCode (std::nullopt); }
    void                 GoToMemory             (Word address) override;
    void                 AddShowInMemoryItems   (const std::wstring & what, const std::string & goTo, PaneMenuItems & items) override { AddShowInMemory (what, goTo, items); }
    void                 GoLive                 () override { GoLiveFromBand(); }
    const StopChanges  & GetStopChanges         () const override { return m_stopChanges; }
    POINT                GetLastPressPx         () const override { return m_lastPressPx; }
    int                  GetPaneRowDip          () const override { return kPaneRowDip; }
    const DebuggerViewSnapshot * GetLatestSnapshot () const override { return m_snapshot.get(); }
    void                 ActivatePaneTab        (const std::wstring & pane) override;
    void                 RelayoutDock           () override;
    void                 RelayoutWindow         () override { LayoutWidgets(); }
    void                 SyncHeatRecording      () override { SyncHeatMapRecording(); }
    void                 MoveSharedControls     (const std::vector<IDxuiControl *> & controls, DxuiWindow * from, DxuiWindow * to) override;
    void                 AddHeatAccessItems     (Word address, HeatMapOptions::Bank bank, PaneMenuItems & items) override;
    const DxuiTheme    * GetTheme               () const override { return m_theme; }
    void                 OpenPane               (const std::wstring & pane) override { ShowPane (pane); }
    void                 ShowMemoryAddress      (Word address) override;
    IHostDialogs       * GetHostDialogs         () override { return (m_host != nullptr) ? &m_host->GetHostDialogs() : nullptr; }
    bool                 IsTooltipVisible       () const override { return m_tooltip.IsVisible(); }
    void                 RepaintLater           () override { RequestPaint(); }
    HWND                 GetOwnerHwnd           () const override { return GetHwnd(); }
    IDxuiTextRenderer  * GetPaneTextRenderer    () override { return GetTextRenderer(); }
    DxuiHwndSource     * GetPanePopupHost       () override { return GetPopupHost(); }
    DxuiWindow         * GetPaneWindow          (const std::wstring & pane) override { return GetPaneHost (pane); }
    const DxuiDpiScaler & GetPaneScaler         () const override { return m_scaler; }
    bool                 IsBarRouted            (const std::wstring & pane) const override { return m_routingPane == GetBarRoutingPane (pane); }
    void                 Repaint                () override { Invalidate(); }
    void                 OpenSource             (int fileId, int line) override { OpenSourceDocument (fileId, line, true); }
    void                 SaveOpenViews          () override { KeepOpenViews(); }
    std::shared_ptr<const DxuiIconImage>  GetPcMarker (const std::shared_ptr<const DxuiIconImage> & dot) override { return GetPcIcon (dot); }
    bool                 IsCodeHeatOn           () const override { return m_codeOptions.IsOn (DisassemblyOptions::Option::Heat); }
    bool                 RunKeyAction           (DebuggerKeySchemes::Action action) override { return OnMappedCommand ((int) action); }

    Word                 GetCursorAddress       () const;
    void                 ToggleBookmarkAtCursor ();
    void                 GoToBookmark           (bool isBackward);

    void                 ShowBasicLine          (Word line) override;

    bool                     ShowColumnMenu           (DxuiListView * list, POINT clientPx);
    bool                     HasOwnColumnWidths       (const DxuiListView * list) const;
    bool                     IsTextBox                (const IDxuiControl * control) const;
    void                     NoteListReleased         (DxuiListView * list, POINT clientPx);
    std::vector<IDxuiControl *>  GetLastChildren      () const;
    void                     ApplyLogDeltas           ();

    //  Each pane whose colors mean something has an info button at the end of
    //  its toolbar, in a band of its own where the pane has no toolbar (the
    //  call stack, a device panel with a memory map or a disk head), and one
    //  popup shows the key of the button the pointer rests on or that was
    //  pressed. A legend function gives the key of a pane whose colors depend
    //  on what it shows, or none to hide the button.
    struct PaneColorKey
    {
        ColorKeyButton                                     * button = nullptr;
        DebuggerPaneFrame                                  * slot   = nullptr;
        std::function<std::optional<ColorLegend::Pane> ()>   legend;
    };

    std::vector<PaneColorKey>              m_colorKeys;
    ColorKeyPopup                          m_colorKeyPopup;
    ColorKeyButton                       * m_colorKeyOwner = nullptr;
    ColorKeyButton                       * m_colorKeyHover = nullptr;

    void  CreateColorKeys     ();
    void  AddColorKey         (const std::wstring & pane, DebuggerPaneFrame * slot, ColorLegend::Pane legend,
                               std::function<std::optional<ColorLegend::Pane> ()> dynamic = nullptr);
    void  PlaceColorKeys      ();
    bool  TryGetColorKeyTitleRect (const std::wstring & pane, bool keep, RECT * rect = nullptr);
    RECT  GetBarStrip         (const std::wstring & pane, const RECT & slot) override;
    bool  RouteColorKeyMouse  (const DxuiMouseEvent & ev);
    void  ShowColorKey        (ColorKeyButton * button, bool hold);
    void  ToggleColorKey      (ColorKeyButton * button);
    void  HideColorKey        ();
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
    MemoryFileDialog::Fields                                      m_loadFileFields;
    MemoryFileDialog::Fields                                      m_saveFileFields;
    bool                                                          m_showSourceCode      = true;
    uint64_t                                                      m_sourceClickMs       = 0;
    POINT                                                         m_sourceClickAt       = {};

    //  The fixed panes closed from their close buttons, which the View menu
    //  shows again where the layout still keeps them, and a pane the View
    //  menu opened that comes forward once the next snapshot shows it.
    std::set<std::wstring>                                                           m_closedPanes;
    std::wstring                                                                     m_pendingShowPane;

    //  Where Start stopwatch here was chosen, until Stop stopwatch here.
    std::optional<Word>                                                              m_stopwatchStart;

    //  The code pane's edits of instructions, each the bytes before and
    //  after, to undo and redo (FR-185, FR-186).
    struct CodeEdit
    {
        Word               address = 0;
        std::vector<Byte>  before;
        std::vector<Byte>  after;
        std::wstring       text;
    };

    std::vector<CodeEdit>                                                            m_codeUndo;
    std::vector<CodeEdit>                                                            m_codeRedo;

    //  The command bar and the history timeline, laid out together in bands
    //  along the window's edges. Declared ahead of their hosts, which leave
    //  it as they go.
    DxuiToolbarDockGroup                                                             m_toolbarDocks;

    //  The command bar's place, docked or floating, and the drags that move
    //  it. Events from its floating window route under kBarFloatKey, which
    //  no pane has, with that window's own tooltip, which shows only over it.
    DxuiToolbarHost                                                                  m_barHost;
    static constexpr wchar_t                                                         kBarFloatKey[] = L"~commandBar";

    //  The history timeline, its entries the Live or Replay state and the strip
    //  of thumbnails, its own dock, a seek asked for while the machine ran,
    //  made once it stops, and the drag of its playhead line.
    DxuiToolbar                          * m_timelineBar           = nullptr;
    DxuiImageStrip                         m_timelineStrip;
    DxuiToolbarHost                        m_timelineHost;
    std::shared_ptr<DxuiCommand>           m_timelineCommand;
    std::optional<HistoryThumbnailCell>    m_pendingSeek;
    HistoryTimelineScrub                   m_timelineScrub;
};
