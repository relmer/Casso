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

    //  The heat map's colors in the theme in force.
    virtual HeatMapView::Palette   GetHeatPalette   () const = 0;

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

    //  Creates the pane's controls, then sets them up once all exist.
    virtual void                         CreateControls () = 0;
    virtual void                         Configure      () {}

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

    //  The pane's info button, none for a pane without one.
    virtual std::optional<PaneColorKeySpec>  GetColorKey () const { return std::nullopt; }

    virtual void                         Apply          (const DebuggerViewSnapshot & snapshot) { (void) snapshot; }

    //  Moves what moves between snapshots; true while something does.
    virtual bool                         Tick           (int64_t nowMs) { (void) nowMs; return false; }

    //  Reads the pane's settings again after another debug project opens.
    virtual void                         OnProjectSwitched () {}

    //  Lays the pane's bar out in its slot, in the window the pane is in.
    virtual void                         PlaceBar       () {}

    //  The mouse, at three points of the window's routing: ahead of every
    //  other control, as a Ctrl+wheel over a map that zooms it; at the
    //  pane's bar, ahead of the panes' contents; and at what the pane draws,
    //  after the dock's strips and sashes.
    virtual bool                         RouteEarlyMouse   (const DxuiMouseEvent & ev) { (void) ev; return false; }
    virtual bool                         RouteBarMouse     (const DxuiMouseEvent & ev) { (void) ev; return false; }
    virtual bool                         RouteContentMouse (const DxuiMouseEvent & ev) { (void) ev; return false; }

    //  The tip of the bar entry under a point, or null; and the tip of what
    //  the pane draws under a point.
    virtual const wchar_t              * GetBarTip      (POINT clientPx, RECT & anchor) const { (void) clientPx; (void) anchor; return nullptr; }
    virtual bool                         TryGetTip      (POINT clientPx, PaneTip & tip) { (void) clientPx; (void) tip; return false; }

    //  A list row's context menu, false for a list not the pane's; and the
    //  menu on what the pane draws under a point.
    virtual bool                         AddMenuItems        (DxuiListView * list, int row, PaneMenuItems & items) { (void) list; (void) row; (void) items; return false; }
    virtual void                         AddContentMenuItems (POINT clientPx, PaneMenuItems & items) { (void) clientPx; (void) items; }

protected:
    IDebuggerPaneHost  & m_host;
};
