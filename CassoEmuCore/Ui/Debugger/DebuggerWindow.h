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
#include "Ui/Debugger/Panes/ConsolePanes.h"
#include "Ui/Debugger/Panes/FindWidgets.h"
#include "Ui/Debugger/Panes/HeatMapView.h"
#include "Ui/Debugger/Panes/ICodeViews.h"
#include "Ui/Debugger/Panes/OperandTipController.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"
#include "Ui/Debugger/Panes/SourceDocuments.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/FloatingPanes.h"
#include "Ui/Debugger/PaneColorKeys.h"
#include "Ui/Debugger/TooltipCoordinator.h"
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
class CodeEditCommands;
class DisassemblyGutter;
class DisassemblyGroup;
class SourceDocumentGroup;
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

class DebuggerWindow : public DxuiWindow, private IDebuggerPaneHost, private ICodeViews, private IOperandTipHost, private IFindWidgetHost, private ITooltipSources,
                       private IFloatingPanesHost, private IColorKeysHost
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

    //  Protected so a test can apply one as the keys do.
    void     ApplyTextZoom   (float zoom);
    void     StepTextZoom    (int steps);
    float    GetTextZoom     () const { return m_textZoom; }

    //  Protected so a test can focus a control as a click does.
    void             FocusControl    (IDxuiControl * control) override { SetFocusedControl (control); }

    //  Protected so a test can write to the console as the host's lines do.
    void             AppendConsole   (const std::vector<std::string> & lines) override;

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

    static bool      IsSymbolFile      (const std::wstring & path);

    //  Protected so a test can see which pane a control is in.
    std::wstring     GetPaneOfControl (const IDxuiControl * control) const;

    //  Protected so a test can apply a snapshot's disassembly as a frame does.
    void            ApplyCodeSnapshot (std::shared_ptr<const DebuggerViewSnapshot> snapshot, int view) { m_snapshot = std::move (snapshot); ApplyCodeView (view); }
    DxuiListView  * GetCodeList       (int view) const override;

    //  Protected so a test can read the rows a view shows.
    int                          GetCodeLineOfRow    (int view, int row) const override;

    //  Protected so a test can see where the keys go.
    IDxuiControl  * GetFocused     () const;

    //  Protected so a test can see which pane has the focus border.
    std::wstring    GetPaneOfFocus () const;

    //  Protected so a test can press Shift+Esc's close.
    bool            ClosePaneOfFocus ();

    //  Protected so a test can close a floating pane as its close button
    //  does, and see the layout it leaves.
    void                    CloseFloatingPane (const std::wstring & pane);
    const DxuiPaneLayout &  GetPaneLayout     () const;
    DxuiPaneLayout &        EditPaneLayout    ();

    //  Protected so a test can work the View menu as a click does: the
    //  panes it lists, and showing or closing one.
    std::vector<std::wstring>  GetViewMenuPanes () const;
    void                       ShowPane         (const std::wstring & pane);
    void                       ClosePane        (const std::wstring & pane) override;
    bool                       IsPaneShown      (const std::wstring & pane) const override;

    //  Whether the host was last told to record the heat map, and the step
    //  that brings the recording up to date.
    bool                       IsHeatMapRecording   () const { return m_isHeatMapRecording; }
    void                       SyncHeatMapRecording ();

    //  The heat map's colors in the theme in force, for the map and the heat
    //  columns; and whether any heat column is on.
    HeatMapView::Palette       GetHeatPalette       () const override;
    bool                       IsAnyHeatColumnOn    () const;

    void                       ApplyHeatMap         ();

    //  Protected so a test can add a pane of its own ahead of OnCreate, as
    //  CreatePanes adds the window's, over the window's pane host.
    void                 AddPane            (std::unique_ptr<IDebuggerPane> pane) { m_panes.push_back (std::move (pane)); }
    IDebuggerPaneHost  & GetPaneHostForTest () { return *this; }

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

    //  Protected so a test can apply the breakpoints and the open views as a
    //  snapshot does, set a snapshot without applying it, and read a pane's
    //  content.
    void                                          ApplyBreakpoints       ();
    void                                          KeepOpenViews          ();
    void                                          SetSnapshotForTest     (std::shared_ptr<const DebuggerViewSnapshot> snapshot) { m_snapshot = std::move (snapshot); }
    IDxuiControl *                                GetPaneContentForTest  (const std::wstring & pane) const { return GetPaneContent (pane); }

    //  Protected so a test can open the zero-page pointer dialog as Add
    //  zero-page pointer... does.
    void                                               AddZeroPagePointer     () override;

    //  Protected so a test can work the registers, stack and watch panes'
    //  Undo and Redo bars as a click does.
    DxuiToolbar *  GetUndoBar (const std::wstring & pane) const;

    //  Protected so a test can read the menu bar's menus as a click opens
    //  them, and the command bar's and the console bar's entries.
    static constexpr int  kDialectEntry = 1;
    static constexpr int  kFindEntry    = 2;

    void                                  SetWindowMenus     ();
    void                                  Detach             ();
    const std::vector<DxuiMenuBarItem> &  GetMenuBarItems    () const { return m_menuBarItems; }
    DxuiToolbar *                         GetCommandBar      () const { return m_commandBar; }
    static std::wstring                   GetModeLabel       (const wchar_t * mode);
    DxuiMenuBar *                         GetMenuBar         () const { return m_menuBar; }

    //  The console's modes in its drop-down's order: alphabetical by label,
    //  without regard to case.
    using ModeChoice = ConsoleModeBar::ModeChoice;

    static const std::vector<ModeChoice> &  GetModeChoices ();

    //  Protected so a test can see whether a tip shows over the command bar,
    //  and that none shows while a menu is open.
    const DxuiTooltip &  GetTooltip    () const { return m_tooltip; }

    //  Protected so a test can see whether a menu is open.
    bool                 IsAnyMenuOpen () const override;

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
    //  the pointer rests: over the PC's branch arrow, the disk head or the
    //  history meter. And the colors the legend shows.
    bool                  TryGetBranchTip  (POINT clientPx, RECT & anchor, std::wstring & text) const override;
    bool                  TryGetGraphicTip (POINT clientPx, RECT & anchor, std::wstring & text) const override;
    ColorLegend::Palette  GetColorPalette  () const override;

    //  The screen and cell of the tip over a disassembly row's operand and
    //  result, as a test passes them to the OperandTipController.
    using OperandTipCell   = ::OperandTipCell;
    using OperandTipScreen = ::OperandTipScreen;

    void  CheckOperandTip        (int view) override { m_operandTips->Check (view); }

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

    //  Protected so a test can work each pane and group through its own
    //  class, as the window does.
    BreakpointsPane        & GetBreakpointsPane () const { return *m_breakpointsPane; }
    CodeEditCommands       & GetCodeEdits       () const { return *m_codeEdits; }
    DisassemblyGroup       & GetCodeGroup       () const { return *m_code; }
    DisassemblyGutter      & GetCodeGutter      () const { return *m_gutter; }
    PaneColorKeys          & GetColorKeys       () const { return *m_colorKeys; }
    ConsolePane            & GetConsolePane     () const { return *m_consolePane; }
    FindWidgetSet          & GetFindWidgets     () const { return *m_finds; }
    HeatMapPane            & GetHeatMapPane     () const { return *m_heatMapPane; }
    HeatRangesPane         & GetHeatRangesPane  () const { return *m_heatRangesPane; }
    MemoryWindowGroup      & GetMemoryGroup     () const { return *m_memoryGroup; }
    ProfilerPane           & GetProfilerPane    () const { return *m_profilerPane; }
    RegistersPane          & GetRegistersPane   () const { return *m_registersPane; }
    SourceDocumentGroup    & GetSourceGroup     () const { return *m_sources; }
    StackPane              & GetStackPane       () const { return *m_stackPane; }
    TooltipCoordinator     & GetTipCoordinator  () const { return *m_tips; }
    WatchesPane            & GetWatchesPane     () const { return *m_watchesPane; }
    OperandTipController   & GetOperandTips     () override { return *m_operandTips; }

private:
    static constexpr int    kPreferredWidthDip  = 1100;
    static constexpr int    kPreferredHeightDip = 840;
    static constexpr int    kMinWidthDip        = 760;
    static constexpr int    kMinHeightDip       = 520;

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


    void     ConfigureWidgets ();
    void     LayoutWidgets    ();
    void     CreateStatusBar  ();
    int      PlaceStatusBar   (int width, int height);
    void     ToggleBeamMark   ();
    void     OpenZoomPopup    ();
    void     CloseZoomPopup   ();
    void     PaintZoomPopup   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void     ApplySnapshot    ();
    void     UpdateChanges    ();
    void     RunToCursor      (Word address);
    void     RunAction        (const DebuggerAction & action) override;
    CommandMode  GetMode      () const override;
    DebuggerKeyScheme  GetSavedKeyScheme () const;
    bool     RouteBoxKey      (const DxuiKeyEvent & ev, bool & handled);
    void     UpdateTopLayer   ();
    void     SearchAsTyped    ();
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
    void     OpenSymbolFile       (const std::wstring & thenShow = std::wstring()) override;
    void     LoadFileIntoMemory   ();

    //  The window's state a debug project saves with no setting of its own:
    //  the console history, each pane's find history and options, and the
    //  widths of the columns the user sized. A project's switch brings back
    //  the rest of its settings too.
    void     RestoreWindowState   (bool isProjectSwitch);
    void     SaveColumnWidths     ();
    void     ApplyColumnWidths    ();
    void     SaveUndoHistories    ();
    std::string  GetUndoHistoriesText () const;
    void     OpenSpeedMenu        (const RECT & field);
    void     SaveMemoryToFile     ();
    DxuiToolbar::Entry  MakeFindEntry (const std::wstring & pane);
    bool     RouteSourceBarMouse  (DxuiToolbar * bar, const DxuiMouseEvent & ev);
    bool     IsDocumentPane     (const std::wstring & pane) const;
    bool     CanClosePane       (const std::wstring & pane) const override;
    bool     IsFixedPane        (const std::wstring & pane) const;
    void     ShowPendingPane    ();
    void     ShowDockToMenu     (const std::wstring & pane, POINT clientPx) override;

    //  The floating panes' host calls (IFloatingPanesHost).
    HWND                           GetFloatOwner       () const override { return GetHwnd(); }
    HINSTANCE                      GetFloatInstance    () const override { return m_hInstance; }
    DxuiDockSite *                 GetFloatDockSite    () const override { return m_dockSite; }
    const DxuiKeyMap *             GetFloatKeyMap      () const override { return &DebuggerKeySchemes::GetMap (m_keyScheme); }
    const DxuiDpiScaler &          GetFloatScaler      () const override { return m_scaler; }
    std::unique_ptr<IDxuiControl>  DetachFloatControl  (IDxuiControl * control) override { return DetachChild (control); }
    void                           AttachFloatControl  (std::unique_ptr<IDxuiControl> control) override { (void) AttachChild (std::move (control)); }
    bool                           RouteFloatedMouse   (const DxuiMouseEvent & ev) override { return OnMouse (ev); }
    bool                           RouteFloatedKey     (const DxuiKeyEvent & ev) override { return OnKey (ev); }
    bool                           RunFloatedCommand   (int commandId) override { return OnMappedCommand (commandId); }
    bool                           DropFloatedFiles    (const std::vector<std::wstring> & paths) override { return OnFilesDropped (paths); }
    void                           OnFloatFocusChanged (const std::wstring & pane, bool focused) override;
    void                           OnFloatDocked       (const std::wstring & pane, DxuiDockedWindow * window) override;
    void                           OnFloatsSynced      () override;
    void                           OnFloatTipErased    (const DxuiTooltip * tip) override { m_operandTips->ForgetColorTip (tip); }
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
    std::vector<IDxuiControl *>  GetPaneControls   (const std::wstring & pane) const override;
    std::vector<IDxuiControl *>  GetOwnControls    (const std::wstring & pane) const;
    IDxuiControl *               GetPaneContent    (const std::wstring & pane) const override;
    std::wstring                 GetPaneTitle      (const std::wstring & pane) const override;
    bool                         IsRoutable        (const IDxuiControl * control) const override;
    void                         SetFocusedControl (IDxuiControl * control);
    HWND                         GetRoutingHwnd    () const;
    void                         PlaceOpeningFocus ();
    void                         PlaceLateFocus    ();
    std::wstring                 GetSavedFocusPane () const;
    bool                         IsFocusPaneReady  (const std::wstring & pane) const;
    void                         FocusPane         (const std::wstring & pane);
    IDxuiControl *               GetMainControl    (const std::wstring & pane) const;
    bool                         IsCommandPane     (const std::wstring & pane) const;
    std::wstring                 GetCommandPane    () const;
    std::wstring                 GetStopFocusPane  () const;
    void                         SaveFocusedPane   ();
    void                         SaveLayout        () override;
    std::wstring                 ReadSavedLayout   ();
    SIZE                         GetPaneMinimum    (const std::wstring & pane) const;
    void                         ShowFocusAccent   ();

    static std::vector<DxuiPaneLayout::Monitor>  GetMonitors   ();

    void     ApplyUndoHistories ();
    DxuiWindow * GetPaneHost    (const std::wstring & pane);
    std::wstring GetBarRoutingPane (const std::wstring & pane) const;
    DxuiHwndSource * GetMenuHost () const;
    DxuiTooltip &    GetRoutedTooltip () override;
    void     ClipPaneControls   ();
    void     NoteViewFocus      (bool isSource) override;
    void     NoteTabFocus       (POINT pointDip);
    static IDxuiControl * FindFirstFocusable (IDxuiControl * node);
    int      GetSourceSlotOf    (const std::wstring & pane) const;
    bool     IsRestoringViews   () const override;
    bool     ForwardToList    (DxuiListView * list, const DxuiMouseEvent & ev);
    void     ShowCode         (std::optional<Word> address);
    const std::vector<DebuggerViewSnapshot::CodeLine> &  GetCodeLines (int view) const override;
    void     ApplyCodeView    (int view);
    void     UpdateTooltip    (POINT clientPx) override;

    //  The operand tip's window facts (IOperandTipHost): the view and row under
    //  a point, and what may lie over the tip.
    DxuiHwndSource       * GetTipPopupHost      () override { return GetRoutedTooltip().GetPopupHost(); }
    IDxuiTextRenderer    * GetTipText           () override { return GetTextRenderer(); }
    const DxuiDpiScaler  & GetTipScaler         () const override { return m_scaler; }
    const DxuiTheme      * GetTipTheme          () const override { return m_theme; }
    HWND                   GetTipOwner          () const override { return GetHwnd(); }
    bool                   IsTipBlocked         () const override;
    bool                   TryFindTipRow        (POINT clientPx, int & view, int & row) const override { return TryFindOperandTipRow (clientPx, view, row); }
    bool                   IsTipCovered         (int view, POINT clientPx, const RECT & tipRect, const OperandTipScreen & screen) const override
                                                { return IsOperandTipCovered (view, clientPx, tipRect, screen); }
    int                    FindOperandTipView   (POINT clientPx) const;
    bool                   TryFindOperandTipRow (POINT clientPx, int & view, int & row) const;
    bool                   IsOperandTipCovered  (int view, POINT clientPx, const RECT & tipRect, const OperandTipScreen & screen) const;
    static bool            Meets                (const RECT & over, POINT clientPx, const RECT & tipRect);

    //  The tip sources (ITooltipSources).
    const std::vector<std::unique_ptr<IDebuggerPane>> &  GetTipPanes () const override { return m_panes; }
    const wchar_t *              GetCommandBarTip (POINT clientPx, RECT & anchor) const override;
    const wchar_t *              GetFindTip       (POINT clientPx, RECT & anchor) const override { return m_finds->GetTip (clientPx, anchor); }
    std::wstring                 GetChromeTipAt   (POINT clientPx, RECT & anchor) const override;
    std::vector<DxuiListView *>  GetTipLists      () const override;
    bool                         TryGetSymbolTip  (POINT clientPx, RECT & anchor, std::wstring & text) const override;

    //  The anchor of a tip that follows the pointer: the pointer's own pixel.
    static RECT  GetPointerAnchor (POINT clientPx) { return RECT { clientPx.x, clientPx.y, clientPx.x + 1, clientPx.y + 1 }; }
    std::optional<Byte>  GetRegisterByte (const std::string & name) const override;

    //  The debugger's colors, from the active theme: a breakpoint's red, the
    //  PC's arrow and row, the row another pane brought into view, a branch's
    //  destination, the annotations' comment color, and the syntax colors.
    bool      IsDarkTheme          () const;
    uint32_t  GetBreakpointArgb    () const override;
    std::shared_ptr<const DxuiIconImage>  GetBreakpointIcon (bool enabled) override;
    std::shared_ptr<const DxuiIconImage>  GetHoverBreakpointIcon () override;

    //  The PC's arrow for the glyph margin, over `dot` when the line has one.
    std::shared_ptr<const DxuiIconImage>  GetPcIcon (const std::shared_ptr<const DxuiIconImage> & dot);
    uint32_t  GetPcMarkerArgb      () const override;
    uint32_t  GetPcRowArgb         () const override;
    uint32_t  GetNavigatedRowArgb  () const override;
    uint32_t  GetTargetRowArgb     () const override;
    uint32_t  GetAnnotationArgb    () const;
    uint32_t  GetChangedArgb       () const;

    //  The disassembly views' bytes, for the changed color in their bytes
    //  column.
    uint32_t  GetResultArgb        () const override;
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


    std::map<std::wstring, std::vector<int>>  m_columnWidths;
    uint64_t                                  m_undoSavedMs = 0;

    //  Set when a restore replaced the undo histories, so the machine change
    //  that follows clears none of them.
    bool                                      m_isHistoryRestored = false;

    RECT                                    m_openedRect         = {};
    bool                                    m_placed             = false;
    std::string                             m_menuState;
    uint32_t                                m_goToSerial         = 0;
    StopChanges                             m_stopChanges;

    POINT                                   m_lastPressPx        = {};

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
    //  whole over their cell.
    std::unique_ptr<OperandTipController>   m_operandTips;

    //  The tip at the pointer.
    std::unique_ptr<TooltipCoordinator>     m_tips;

    //  The disassembly views' breakpoint column and the PC's arrow in it.
    std::unique_ptr<DisassemblyGutter>      m_gutter;

    //  The disassembly views and what they share.
    std::unique_ptr<DisassemblyGroup>       m_code;

    //  The searchable panes' find widgets.
    std::unique_ptr<FindWidgetSet>          m_finds;

    //  The console, on the pane list.
    ConsolePane                           * m_consolePane = nullptr;

    //  The window's parts the find widgets reach (IFindWidgetHost).
    DxuiTextView         * GetFindView      (const std::wstring & pane) const override;
    DebuggerPaneFrame    * GetFindFrame     (const std::wstring & pane) const override;
    DxuiWindow           * GetFindWindow    (const std::wstring & pane) override { return GetPaneHost (pane); }
    DxuiWindow           & GetMainWindow    () override { return *this; }
    void                   ReleaseFocusOf   (IDxuiControl * control) override;
    void                   KeepDockLast     () override;
    void                   KeepOverlaysLast () override;
    void                   FocusInPane      (const std::wstring & pane, IDxuiControl * control) override;
    IDxuiControl         * GetFindFocus     () const override { return GetFocused(); }
    std::wstring           GetFocusPane     () const override { return GetPaneOfFocus(); }
    IDxuiControl         * GetCommandLine   () const override { return m_consolePane->GetCommandLine().GetBox(); }
    void                   BringPaneForward (const std::wstring & pane) override;
    void                   UpdateFindLayer  () override { UpdateTopLayer(); }

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
    bool                                  m_isHistoryPressed    = false;
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

    DxuiToolbar                                * m_commandBar             = nullptr;
    DxuiMenuBar                                * m_menuBar                = nullptr;
    std::vector<DxuiMenuBarItem>                 m_menuBarItems;
    std::unique_ptr<DebuggerCommands>            m_commands;
    std::vector<std::shared_ptr<DxuiCommand>>    m_menuCommands;
    DxuiDockSite                               * m_dockSite               = nullptr;
    HINSTANCE                                    m_hInstance              = nullptr;
    std::unique_ptr<FloatingPanes>               m_floating;
    FocusAccentOwner                             m_accentOwner;
    bool                                         m_isOpeningFocusDue      = true;
    OpeningFocusDeferral                         m_openingFocus;
    StopFocus                                    m_stopFocus;
    uint32_t                                     m_shownPaneSerial        = 0;
    bool                                         m_isHeatMapRecording     = false;
    bool                                         m_isHeatColumnsRecording = false;
    int64_t                                      m_framePaintedAt         = 0;

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
    void                 FocusDefault           () override { m_focusMgr.SetFocused (m_consolePane->GetCommandLine().GetBox()); }
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
    bool                 IsBarRouted            (const std::wstring & pane) const override { return m_floating->GetRoutingPane() == GetBarRoutingPane (pane); }
    void                 Repaint                () override { Invalidate(); }
    int                  GetListColumnAt        (const DxuiListView * list, int xPx) const override { return GetColumnAt (list, xPx); }
    void                 OpenSource             (int fileId, int line) override;
    void                 SetPaneTitle           (const std::wstring & pane, const std::wstring & title) override { m_dockSite->SetTitle (pane, title); }
    void                 SetPaneTabMark         (const std::wstring & pane, const DxuiTabGroup::LeadingMark & mark, const std::wstring & tip) override;
    void                 UpdateWindowMenus      () override { SetWindowMenus(); }
    void                 ShowPopupMenu          (const PaneMenuItems & items, POINT clientPx) override;
    void                 MarkPaneTab            (const std::wstring & pane) override;
    DxuiToolbar::Entry   GetFindEntry           (const std::wstring & pane) override { return MakeFindEntry (pane); }
    void                 SaveOpenViews          () override { KeepOpenViews(); }
    std::shared_ptr<const DxuiIconImage>  GetPcMarker (const std::shared_ptr<const DxuiIconImage> & dot) override { return GetPcIcon (dot); }
    bool                 IsCodeHeatOn           () const override;
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

    //  The panes' info buttons and the color key they open, and the window's
    //  calls for them (IColorKeysHost).
    std::unique_ptr<PaneColorKeys>  m_colorKeys;

    ColorKeyButton  * CreateColorKeyButton (const std::wstring & pane, ColorLegend::Pane legend) override { return CreateChild<ColorKeyButton> (pane, legend); }
    DxuiDockSite    * FindDockSiteOf       (const std::wstring & pane) const override { return m_floating->Contains (pane) ? m_floating->FindSite (pane) : m_dockSite; }
    void              HideRoutedTip        () override { GetRoutedTooltip().HideImmediate(); }
    RECT              GetBarStrip          (const std::wstring & pane, const RECT & slot) override { return m_colorKeys->GetBarStrip (pane, slot); }

    std::string                                                   m_machine;
    std::wstring                                                  m_slidPane;
    std::unique_ptr<SourceDocumentGroup>                          m_sources;
    MemoryFileDialog::Fields                                      m_loadFileFields;
    MemoryFileDialog::Fields                                      m_saveFileFields;
    bool                                                          m_showSourceCode      = true;

    //  The fixed panes closed from their close buttons, which the View menu
    //  shows again where the layout still keeps them, and a pane the View
    //  menu opened that comes forward once the next snapshot shows it.
    std::set<std::wstring>                                                           m_closedPanes;
    std::wstring                                                                     m_pendingShowPane;

    //  The disassembly views' menu entries that change a line.
    std::unique_ptr<CodeEditCommands>                                                m_codeEdits;

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
