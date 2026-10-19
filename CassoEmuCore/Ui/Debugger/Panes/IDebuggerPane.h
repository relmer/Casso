#pragma once

#include "Seams/IHostDialogs.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/IDebuggerWindowHost.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/StopChanges.h"
#include "Ui/Debugger/Panes/HeatMapView.h"
#include "Ui/Debugger/SourceSyntax.h"
#include "Ui/Debugger/TextDialog.h"

class DebuggerPaneFrame;





//  A context menu's rows: each its label and what choosing it does.
using PaneMenuItems = std::vector<std::pair<std::wstring, std::function<void()>>>;





////////////////////////////////////////////////////////////////////////////////
//
//  PaneTip
//
//  The tip a pane has under a point: a control's tip over its anchor, one
//  that follows the pointer at once, or none yet, with a tip already up left
//  as it is, while what it lists is being read.
//
////////////////////////////////////////////////////////////////////////////////

struct PaneTip
{
    enum class Kind
    {
        Control,
        Pointer,
        Pending,
    };

    Kind                   kind        = Kind::Pointer;
    RECT                   anchor      = {};
    std::wstring           text;
    bool                   isMonospace = false;

    //  A color for each line, 0 for the tip's own; none for a tip in one
    //  color.
    std::vector<uint32_t>  lineColors;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PaneColorKeySpec
//
//  A pane's info button: the slot at the top of the pane it sits at the end
//  of, the key it opens, and, for a pane whose colors depend on its
//  contents, the key for now, none hiding the button.
//
////////////////////////////////////////////////////////////////////////////////

struct PaneColorKeySpec
{
    DebuggerPaneFrame                                    * slot   = nullptr;
    ColorLegend::Pane                                      legend = ColorLegend::Pane::Disassembly;
    std::function<std::optional<ColorLegend::Pane> ()>     dynamic;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IDebuggerPaneHost
//
//  The window's calls a debug pane makes. A pane calls the window through
//  this and nothing else, so a test hosts a pane with a host of its own.
//
////////////////////////////////////////////////////////////////////////////////

class IDebuggerPaneHost
{
public:
    virtual ~IDebuggerPaneHost() = default;

    //  The panel a pane creates its controls in, in the order they paint.
    virtual DxuiPanel            & GetControlParent () = 0;

    //  Whoever owns the debugger, for the requests a pane sends the machine
    //  and the settings saved for it; none for a window with no host.
    virtual IDebuggerWindowHost  * GetWindowHost    () = 0;

    //  Runs a control's action, in the console's mode.
    virtual void                   RunAction        (const DebuggerAction & action) = 0;
    virtual CommandMode            GetMode          () const = 0;

    //  Moves the disassembly to an address, or to the PC; a memory window in
    //  view to one, or the last one used, bringing it forward; and the last
    //  one used to one.
    virtual void                   GoToCode         (Word address) = 0;
    virtual void                   ShowPcCode       () = 0;
    virtual void                   ShowMemoryAddress (Word address) = 0;
    virtual void                   GoToMemory       (Word address) = 0;

    //  The rows a menu gets to show an address or a Go to text in each open
    //  memory window.
    virtual void                   AddShowInMemoryItems (const std::wstring & what, const std::string & goTo, PaneMenuItems & items) = 0;

    //  Goes back to the machine's present from a point in its history.
    virtual void                   GoLive           () = 0;

    //  What changed since the last stop, a register's, a stack byte's or a
    //  watch's value, by its key.
    virtual const StopChanges    & GetStopChanges   () const = 0;

    //  Where the last press was, in the window's pixels, for a list's
    //  double-click to find which of a row's cells it landed in.
    virtual POINT                  GetLastPressPx   () const = 0;

    //  The panes' row height, in DIPs, at the usual text size.
    virtual int                    GetPaneRowDip    () const = 0;

    //  The file pickers, none for a window with no host.
    virtual IHostDialogs         * GetHostDialogs   () = 0;
    virtual HWND                   GetOwnerHwnd     () const = 0;

    //  The window's text renderer and popup host, for a bar configured in it.
    virtual IDxuiTextRenderer    * GetPaneTextRenderer () = 0;
    virtual DxuiHwndSource       * GetPanePopupHost    () = 0;

    //  The window a pane is in now: this one, or the floating one it was
    //  moved to. And the scaler panes are laid out with.
    virtual DxuiWindow           * GetPaneWindow    (const std::wstring & pane) = 0;
    virtual const DxuiDpiScaler  & GetPaneScaler    () const = 0;

    //  The theme in force, the text colors and the heat map's colors in it.
    virtual const DxuiTheme      * GetTheme         () const = 0;
    virtual DebuggerTextColors::Set  GetTextColors  () const = 0;
    virtual HeatMapView::Palette   GetHeatPalette   () const = 0;

    //  The rows a menu on a byte gets for its last write and read in a bank:
    //  show each in the disassembly, or go back to it.
    virtual void                   AddHeatAccessItems (Word address, HeatMapOptions::Bank bank, PaneMenuItems & items) = 0;

    //  The breakpoints' color in the theme, and the code views' gutter mark
    //  for one on or off.
    virtual uint32_t               GetBreakpointArgb () const = 0;
    virtual std::shared_ptr<const DxuiIconImage>  GetBreakpointIcon (bool enabled) = 0;

    //  Opens a source file's document at a line and brings it forward; and
    //  moves the BASIC program pane to a line.
    virtual void                   OpenSource       (int fileId, int line) = 0;
    virtual void                   ShowBasicLine    (Word line) = 0;

    //  Saves the open views, with the choices the panes save among them; and
    //  whether the views the window reopens at startup have yet to settle.
    virtual void                   SaveOpenViews    () = 0;
    virtual bool                   IsRestoringViews () const = 0;

    //  The PC's arrow, over a gutter mark when the line has one, and its
    //  row's color in the theme; and the source views' syntax colors.
    virtual std::shared_ptr<const DxuiIconImage>  GetPcMarker (const std::shared_ptr<const DxuiIconImage> & dot) = 0;
    virtual uint32_t               GetPcRowArgb     () const = 0;
    virtual SourceSyntax::Colors   GetSyntaxColors  () const = 0;

    //  Whether the disassembly views' heat column is on, which the other
    //  listings match.
    virtual bool                   IsCodeHeatOn     () const = 0;

    //  The PC marker's color and an instruction result's in the theme; the
    //  gray breakpoint where a click would set one; and the fills of the row
    //  another pane brought into view and of a branch's destination.
    virtual uint32_t               GetPcMarkerArgb  () const = 0;
    virtual uint32_t               GetResultArgb    () const = 0;
    virtual std::shared_ptr<const DxuiIconImage>  GetHoverBreakpointIcon () = 0;
    virtual uint32_t               GetNavigatedRowArgb () const = 0;
    virtual uint32_t               GetTargetRowArgb () const = 0;

    //  The column of a list at a point, counted from the list's pane lead;
    //  and a register's value now, none before the first snapshot.
    virtual int                    GetListColumnAt  (const DxuiListView * list, int xPx) const = 0;
    virtual std::optional<Byte>    GetRegisterByte  (const std::string & name) const = 0;

    //  New rows in a disassembly view, which take down the operand tip over
    //  them when its cell changes.
    virtual void                   CheckOperandTip  (int view) = 0;

    //  A pane's tab: its title, and the mark ahead of the title with the
    //  tab's tip.
    virtual void                   SetPaneTitle     (const std::wstring & pane, const std::wstring & title) = 0;
    virtual void                   SetPaneTabMark   (const std::wstring & pane, const DxuiTabGroup::LeadingMark & mark, const std::wstring & tip) = 0;

    //  SYM LOAD of a file the user picks, a source given then matched against
    //  it once it loads.
    virtual void                   OpenSymbolFile   (const std::wstring & thenShow) = 0;

    //  Sets the steps to follow a view, by source line for a source document
    //  and by instruction for the disassembly.
    virtual void                   NoteViewFocus    (bool isSource) = 0;

    //  Builds the window's menus again, after what they list changes; a
    //  menu of rows at a point in the window; and a pane's tab marked for
    //  new output while the pane is out of sight.
    virtual void                   UpdateWindowMenus () = 0;
    virtual void                   ShowPopupMenu    (const PaneMenuItems & items, POINT clientPx) = 0;
    virtual void                   MarkPaneTab      (const std::wstring & pane) = 0;

    //  The Find entry for a pane's toolbar, which opens the pane's find
    //  widget.
    virtual DxuiToolbar::Entry     GetFindEntry     (const std::wstring & pane) = 0;

    //  Runs a key scheme's action, as its key does, true when it acted; and a
    //  command bar entry, as a click on it does.
    virtual bool                   RunKeyAction     (DebuggerKeySchemes::Action action) = 0;
    virtual void                   RunCommandBarEntry (int id) = 0;

    //  Opens a pane and brings it forward, as its row on the View menu does.
    virtual void                   OpenPane         (const std::wstring & pane) = 0;

    //  A question put to the user in a message box centered on owner, with
    //  the user's answer; a test supplies the answer instead.
    virtual int                    ShowMessageBox   (HWND owner, const std::wstring & text, const wchar_t * caption, UINT type) = 0;

    //  The same with buttons of the pane's own, and the id of the one
    //  pressed; IDCANCEL for Escape.
    virtual int                    ShowMessageBox   (HWND owner, const std::wstring & text, const wchar_t * caption, UINT type,
                                                     const std::vector<DxuiMessageBoxButton> & buttons, int defaultId) = 0;

    //  Whether the event being routed goes to a control, and whether it
    //  came from the window a pane's bar is in.
    virtual bool                   IsRoutable       (const IDxuiControl * control) const = 0;
    virtual bool                   IsBarRouted      (const std::wstring & pane) const = 0;

    //  The part of a bar's slot its toolbar is laid out in, short of the
    //  pane's info button.
    virtual RECT                   GetBarStrip      (const std::wstring & pane, const RECT & slot) = 0;

    //  The snapshot the window last applied, none before the first.
    virtual const DebuggerViewSnapshot * GetLatestSnapshot () const = 0;

    //  Opens the dialog for a zero-page pointer to watch, as Add zero-page
    //  pointer... does.
    virtual void                   AddZeroPagePointer () = 0;

    //  Moves the keyboard focus to a control, as a press on it does.
    virtual void                   FocusControl     (IDxuiControl * control) = 0;

    //  Updates the tip under the pointer after a move over a bar, or after
    //  what a tip lists comes in; and whether a tip is up.
    virtual void                   UpdateTooltip    (POINT clientPx) = 0;
    virtual bool                   IsTooltipVisible () const = 0;

    //  Whether a pane is open and not closed from its close button, as the
    //  View menu's check has it.
    virtual bool                   IsPaneOpen       (const std::wstring & pane) const = 0;

    //  The control with the keyboard focus, and the step that moves it to the
    //  command line, for a pane whose focused control is hidden.
    virtual IDxuiControl         * GetFocusedControl () const = 0;
    virtual void                   FocusDefault     () = 0;

    //  Writes lines to the console; and the line the user types in a dialog,
    //  none when canceled, which a test supplies instead.
    virtual void                   AppendConsole    (const std::vector<std::string> & lines) = 0;
    virtual std::optional<Byte>    RunByteDialog    (const std::wstring & title, const std::wstring & label) = 0;
    virtual std::optional<std::wstring>  RunTextDialog (const std::wstring & title, const std::wstring & label, const std::wstring & placeholder,
                                                        TextDialog::CheckFn check) = 0;

    //  Hides the tip under the pointer at once, as a press or a key does.
    virtual void                   HideTooltip      () = 0;

    //  Brings a pane's tab forward in its group, and lays the dock out again;
    //  and lays the whole window out again.
    virtual void                   ActivatePaneTab  (const std::wstring & pane) = 0;
    virtual void                   RelayoutDock     () = 0;
    virtual void                   RelayoutWindow   () = 0;

    //  Sends the machine again whether to record the heat map's accesses.
    virtual void                   SyncHeatRecording () = 0;

    //  Moves controls that several panes share into another window, the one
    //  the pane they now act on is in.
    virtual void                   MoveSharedControls (const std::vector<IDxuiControl *> & controls, DxuiWindow * from, DxuiWindow * to) = 0;

    //  Paints the window again at its next frame, or once the messages
    //  waiting are handled, so a burst of input is taken before the paint.
    virtual void                   Repaint          () = 0;
    virtual void                   RepaintLater     () = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IDebuggerPane
//
//  One debug pane: its controls, the frame the dock places, and what it does
//  with a snapshot, the mouse, a tip and a context menu. The window has a list
//  of them and calls each in turn, so adding a pane is its class and one line
//  in that list.
//
//  A PANE CREATES ITS CONTROLS IN THE HOST'S PANEL, which paints and routes
//  them; a pane floated into a window of its own takes its controls with it.
//
////////////////////////////////////////////////////////////////////////////////

class IDebuggerPane
{
public:
    explicit IDebuggerPane (IDebuggerPaneHost & host) : m_host (host) {}
    virtual ~IDebuggerPane() = default;

    IDebuggerPane (const IDebuggerPane &)             = delete;
    IDebuggerPane & operator= (const IDebuggerPane &) = delete;

    //  The pane's layout id, as DebuggerLayout has it, and its tab's title.
    virtual std::wstring                 GetId          () const = 0;
    virtual std::wstring                 GetTitle       () const = 0;

    //  Where the pane's row is on the View menu, none while it has no row;
    //  and the cascade the row goes in, empty for a row of View itself.
    virtual std::optional<int>           GetViewMenuRank  (const DebuggerViewSnapshot * snapshot) const { (void) snapshot; return DebuggerLayout::GetViewMenuRank (GetId()); }
    virtual std::wstring                 GetViewMenuGroup () const { return DebuggerLayout::GetViewMenuGroup (GetId()); }

    //  The boxes the pane lays over its cells, which stay the window's last
    //  children so they paint over the dock.
    virtual std::vector<IDxuiControl *>  GetOverlays    () const { return {}; }

    //  Creates the pane's controls, ahead of the dock; then any box laid over
    //  a cell in place, after the dock, so it paints over the pane; then sets
    //  them up once all exist.
    virtual void                         CreateControls () = 0;
    virtual void                         CreateOverlays () {}
    virtual void                         Configure      () {}

    //  The pane's own settings for its lists, applied after the window's, so
    //  they take precedence.
    virtual void                         ConfigureLists () {}

    //  For a pane closed until the user first opens it, or one the machine
    //  opens, whether it is open, and the step that saves it opened; false for
    //  any other pane.
    virtual bool                         TryGetOpened   (bool & isOpened) const { (void) isOpened; return false; }
    virtual void                         Open           () {}

    //  For a pane the machine opens and closes, the request for either, as
    //  its row on the View menu or its close button makes it; false for any
    //  other pane. And whether the window always has the pane, closing it
    //  only hiding it.
    virtual bool                         RequestOpen    () { return false; }
    virtual bool                         RequestClose   () { return false; }
    virtual bool                         IsFixed        () const { return true; }

    //  Builds the frame and adds it to the dock under the pane's id; then,
    //  once every pane is in the dock and has its info button, sends the
    //  machine the pane's first requests.
    virtual void                         AddToDock      (DxuiDockSite & dock) = 0;
    virtual void                         OnDocked       () {}

    //  The control the dock places, and every control of the pane, which
    //  move together into a floating window.
    virtual IDxuiControl               * GetContent     () const = 0;
    virtual std::vector<IDxuiControl *>  GetControls    () const = 0;

    //  The control the keys go to when the pane gets the focus, null to use
    //  the first of its lists and views; whether the pane is the command
    //  input, which a stop moves the focus away from; and the pane's rank as
    //  the focus at a stop, highest first, 0 for none.
    virtual IDxuiControl               * GetMainControl   () const { return nullptr; }
    virtual bool                         IsCommandInput   () const { return false; }
    virtual int                          GetStopFocusRank () const { return 0; }

    //  The title on the pane's tab, where it differs from its row on the
    //  View menu.
    virtual std::wstring                 GetTabTitle    () const { return GetTitle(); }

    //  The text a find widget searches in the pane, and the frame the widget
    //  is drawn in; null for a pane with no find widget.
    virtual DxuiTextView               * GetFindView    () const { return nullptr; }
    virtual DebuggerPaneFrame          * GetFindFrame   () const { return nullptr; }

    //  The pane's lists, which the window makes dense, sets in the text face
    //  and routes the mouse to; and its toolbars, whose menus it ticks.
    virtual std::vector<DxuiListView *>  GetLists       () const { return {}; }
    virtual std::vector<DxuiToolbar *>   GetBars        () const { return {}; }

    //  The pane's controls a press is offered to ahead of the lists, and its
    //  text boxes, which follow the pointer and type the characters typed.
    virtual std::vector<IDxuiControl *>  GetPressTargets () const { return {}; }
    virtual std::vector<DxuiTextInput *> GetTextBoxes   () const { return {}; }

    //  The pane a control several panes share is in now, such as the memory
    //  windows' bar; empty for a control not the pane's.
    virtual std::wstring                 GetPaneOfSharedControl (const IDxuiControl * control) const { (void) control; return {}; }

    //  Whether a control of the pane takes typed characters, as a box does.
    virtual bool                         TakesTyping    (const IDxuiControl * control) const { (void) control; return false; }

    //  Whether the pane saves its lists' column widths in its own settings,
    //  in place of the debug project's; and whether its lists are in the
    //  proportional face, as every list but a disassembly's is.
    virtual bool                         HasOwnColumnWidths () const { return false; }
    virtual bool                         HasTextLists   () const { return true; }

    //  The pane's info button, none for a pane without one.
    virtual std::optional<PaneColorKeySpec>  GetColorKey () const { return std::nullopt; }

    virtual void                         Apply          (const DebuggerViewSnapshot & snapshot) { (void) snapshot; }

    //  The panes' text size, as a fraction of the usual.
    virtual void                         SetTextZoom    (float zoom) { (void) zoom; }

    //  Moves what moves between snapshots; true while something does.
    virtual bool                         Tick           (int64_t nowMs) { (void) nowMs; return false; }

    //  A floating window closed, its panes docked again.
    virtual void                         OnFloatClosed  (DxuiWindow * window) { (void) window; }

    //  Reads the pane's settings again after another debug project opens,
    //  and saves what it has to as the window closes.
    virtual void                         OnProjectSwitched () {}
    virtual void                         OnWindowClose  () {}

    //  The machine's log events sent since the last frame, each frame.
    virtual void                         ApplyLogDeltas (const DebugLogDeltas & deltas, bool isWindowVisible) { (void) deltas; (void) isWindowVisible; }

    //  Lays the pane's bar out in its slot, in the window the pane is in.
    virtual void                         PlaceBar       () {}

    //  The mouse, at four points of the window's routing: a box being
    //  edited in place; ahead of every other control, as a Ctrl+wheel over a
    //  map that zooms it; at the pane's bar, ahead of the panes' contents; and
    //  at what the pane draws, after the dock's strips and sashes.
    virtual bool                         RouteEditMouse    (const DxuiMouseEvent & ev) { (void) ev; return false; }
    virtual bool                         RouteEarlyMouse   (const DxuiMouseEvent & ev) { (void) ev; return false; }
    virtual bool                         RouteBarMouse     (const DxuiMouseEvent & ev) { (void) ev; return false; }
    virtual bool                         RouteContentMouse (const DxuiMouseEvent & ev) { (void) ev; return false; }

    //  A key for a box being edited in place, or for the pane's own control
    //  with the focus, ahead of the window's keys.
    virtual bool                         RouteEditKey      (const DxuiKeyEvent & ev, IDxuiControl * focused) { (void) ev; (void) focused; return false; }

    //  A key scheme's action, such as Find, while a control of the pane has
    //  the focus; and Enter in one of the pane's boxes.
    virtual bool                         OnMappedAction    (DebuggerKeySchemes::Action action, IDxuiControl * focused) { (void) action; (void) focused; return false; }
    virtual bool                         SubmitTextBox     (DxuiTextInput * box) { (void) box; return false; }

    //  The tip of the bar entry under a point, or null; and the tip of what
    //  the pane draws under a point.
    virtual const wchar_t              * GetBarTip      (POINT clientPx, RECT & anchor) const { (void) clientPx; (void) anchor; return nullptr; }
    virtual bool                         TryGetTip      (POINT clientPx, PaneTip & tip) { (void) clientPx; (void) tip; return false; }

    //  Whether a cell's tip at a point lines up in columns.
    virtual bool                         IsMonospaceTipAt (POINT clientPx) const { (void) clientPx; return false; }

    //  A press on one of the pane's lists ended at a point, the list having
    //  had it.
    virtual void                         OnListReleased (DxuiListView * list, POINT clientPx) { (void) list; (void) clientPx; }

    //  A list row's context menu, Copy among its rows where the list copies,
    //  false for a list not the pane's; and the menu on what the pane draws
    //  under a point.
    virtual bool                         AddMenuItems        (DxuiListView * list, int row, PaneMenuItems & items) { (void) list; (void) row; (void) items; return false; }
    virtual void                         AddContentMenuItems (POINT clientPx, PaneMenuItems & items) { (void) clientPx; (void) items; }

    //  A list's menu on its headings, false for a list not the pane's.
    virtual bool                         AddColumnMenuItems  (DxuiListView * list, std::vector<std::shared_ptr<DxuiCommand>> & commands) { (void) list; (void) commands; return false; }

    //  The rows of a column's Filter cascade on the headings' menu, none for
    //  a column with no filter; each row's command is added to `commands`.
    virtual std::vector<DxuiPopupMenuItem>  GetColumnFilterItems (DxuiListView * list, size_t column, std::vector<std::shared_ptr<DxuiCommand>> & commands)
                                         { (void) list; (void) column; (void) commands; return {}; }

protected:
    IDebuggerPaneHost  & m_host;
};
