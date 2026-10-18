#pragma once

#include "Seams/IHostDialogs.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/IDebuggerWindowHost.h"
#include "Ui/Debugger/Panes/HeatMapView.h"

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

    Kind          kind        = Kind::Pointer;
    RECT          anchor      = {};
    std::wstring  text;
    bool          isMonospace = false;
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

    //  Moves the disassembly to an address, and a memory window in view to
    //  one, or the last one used, bringing it forward.
    virtual void                   GoToCode         (Word address) = 0;
    virtual void                   ShowMemoryAddress (Word address) = 0;

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

    //  The theme in force, and the heat map's colors in it.
    virtual const DxuiTheme      * GetTheme         () const = 0;
    virtual HeatMapView::Palette   GetHeatPalette   () const = 0;

    //  Opens a pane and brings it forward, as its row on the View menu does.
    virtual void                   OpenPane         (const std::wstring & pane) = 0;

    //  A question put to the user in a message box centered on owner, with
    //  the user's answer; a test supplies the answer instead.
    virtual int                    ShowMessageBox   (HWND owner, const std::wstring & text, const wchar_t * caption, UINT type) = 0;

    //  Whether the event being routed goes to a control, and whether it
    //  came from the window a pane's bar is in.
    virtual bool                   IsRoutable       (const IDxuiControl * control) const = 0;
    virtual bool                   IsBarRouted      (const std::wstring & pane) const = 0;

    //  The part of a bar's slot its toolbar is laid out in, short of the
    //  pane's info button.
    virtual RECT                   GetBarStrip      (const std::wstring & pane, const RECT & slot) = 0;

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

    //  Creates the pane's controls, ahead of the dock; then any box laid over
    //  a cell in place, after the dock, so it paints over the pane; then sets
    //  them up once all exist.
    virtual void                         CreateControls () = 0;
    virtual void                         CreateOverlays () {}
    virtual void                         Configure      () {}

    //  For a pane closed until the user first opens it, whether that has
    //  happened, and the step that saves it; false for any other pane.
    virtual bool                         TryGetOpened   (bool & isOpened) const { (void) isOpened; return false; }
    virtual void                         Open           () {}

    //  Builds the frame and adds it to the dock under the pane's id; then,
    //  once every pane is in the dock and has its info button, sends the
    //  machine the pane's first requests.
    virtual void                         AddToDock      (DxuiDockSite & dock) = 0;
    virtual void                         OnDocked       () {}

    //  The control the dock places, and every control of the pane, which
    //  move together into a floating window.
    virtual IDxuiControl               * GetContent     () const = 0;
    virtual std::vector<IDxuiControl *>  GetControls    () const = 0;

    //  The pane's lists, which the window makes dense, sets in the text face
    //  and routes the mouse to; and its toolbars, whose menus it ticks.
    virtual std::vector<DxuiListView *>  GetLists       () const { return {}; }
    virtual std::vector<DxuiToolbar *>   GetBars        () const { return {}; }

    //  The pane's controls a press is offered to ahead of the lists, and its
    //  text boxes, which follow the pointer and type the characters typed.
    virtual std::vector<IDxuiControl *>  GetPressTargets () const { return {}; }
    virtual std::vector<DxuiTextInput *> GetTextBoxes   () const { return {}; }

    //  Whether the pane saves its lists' column widths in its own settings,
    //  in place of the debug project's.
    virtual bool                         HasOwnColumnWidths () const { return false; }

    //  The pane's info button, none for a pane without one.
    virtual std::optional<PaneColorKeySpec>  GetColorKey () const { return std::nullopt; }

    virtual void                         Apply          (const DebuggerViewSnapshot & snapshot) { (void) snapshot; }

    //  Moves what moves between snapshots; true while something does.
    virtual bool                         Tick           (int64_t nowMs) { (void) nowMs; return false; }

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

    //  The tip of the bar entry under a point, or null; and the tip of what
    //  the pane draws under a point.
    virtual const wchar_t              * GetBarTip      (POINT clientPx, RECT & anchor) const { (void) clientPx; (void) anchor; return nullptr; }
    virtual bool                         TryGetTip      (POINT clientPx, PaneTip & tip) { (void) clientPx; (void) tip; return false; }

    //  Whether a cell's tip at a point lines up in columns.
    virtual bool                         IsMonospaceTipAt (POINT clientPx) const { (void) clientPx; return false; }

    //  A list row's context menu, Copy among its rows where the list copies,
    //  false for a list not the pane's; and the menu on what the pane draws
    //  under a point.
    virtual bool                         AddMenuItems        (DxuiListView * list, int row, PaneMenuItems & items) { (void) list; (void) row; (void) items; return false; }
    virtual void                         AddContentMenuItems (POINT clientPx, PaneMenuItems & items) { (void) clientPx; (void) items; }

    //  A list's menu on its headings, false for a list not the pane's.
    virtual bool                         AddColumnMenuItems  (DxuiListView * list, std::vector<std::shared_ptr<DxuiCommand>> & commands) { (void) list; (void) commands; return false; }

protected:
    IDebuggerPaneHost  & m_host;
};
