#pragma once

#include "Seams/IHostDialogs.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerPaneLook.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/IDebuggerWindowHost.h"
#include "Ui/Debugger/LatestSnapshot.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/StopChanges.h"
#include "Ui/Debugger/Panes/DebuggerPaneParts.h"
#include "Ui/Debugger/Panes/HeatMapView.h"
#include "Ui/Debugger/SourceSyntax.h"
#include "Ui/Debugger/TextDialog.h"

class DebuggerPaneFrame;





//  A menu row drawn with a check: its label starts with kChecked, which the
//  menu takes off. A row in a submenu has the submenu's label and kSubmenu
//  ahead of the row's label; rows of one submenu are next to each other. A
//  row with no label and no action is a separator.
struct PaneMenu
{
    static constexpr wchar_t  kChecked = L'\x1F';
    static constexpr wchar_t  kSubmenu = L'\x1E';

    static std::wstring  MakeChecked   (const std::wstring & label) { return std::wstring (1, kChecked) + label; }
    static bool          IsChecked     (const std::wstring & label) { return !label.empty() && label[0] == kChecked; }
    static std::wstring  GetText       (const std::wstring & label) { return IsChecked (label) ? label.substr (1) : label; }
    static std::wstring  MakeInSubmenu (const std::wstring & submenu, const std::wstring & label) { return submenu + kSubmenu + label; }

    static std::pair<std::wstring, std::function<void()>>  MakeSeparator () { return { std::wstring(), nullptr }; }

    //  A code line's breakpoint rows, as both code views list them: Set
    //  breakpoint on a line without one; otherwise Enable breakpoint or
    //  Disable breakpoint for its state, then Remove breakpoint. A row with
    //  no action is drawn disabled.
    static void  AddBreakpointRows (PaneMenuItems & items, std::optional<bool> isEnabled,
                                    std::function<void()> set, std::function<void (bool enable)> enable, std::function<void()> remove)
    {
        if (!isEnabled.has_value())
        {
            items.push_back ({ L"Set breakpoint", std::move (set) });
            return;
        }

        items.push_back ({ *isEnabled ? L"Disable breakpoint" : L"Enable breakpoint",
                           enable ? std::function<void()> ([enable, on = !*isEnabled] { enable (on); }) : std::function<void()>() });
        items.push_back ({ L"Remove breakpoint", std::move (remove) });
    }
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
//  IDebuggerPane
//
//  One debug pane: its controls, the frame the dock places, and what it does
//  with a snapshot, the mouse, a tip and a context menu. The window has a list
//  of them and calls each in turn, so adding a pane is its class and one line
//  in that list.
//
//  A PANE CREATES ITS CONTROLS IN THE DEBUGGER WINDOW, which paints and routes
//  them; a pane floated into a window of its own takes its controls with it.
//
////////////////////////////////////////////////////////////////////////////////

class IDebuggerPane
{
public:
              IDebuggerPane  () = default;
    virtual   ~IDebuggerPane () = default;

    IDebuggerPane (const IDebuggerPane &)             = delete;
    IDebuggerPane & operator= (const IDebuggerPane &) = delete;

    //  The pane's layout id, as DebuggerLayout has it, and its tab's title.
    virtual const std::wstring         & GetId          () const = 0;
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
    virtual IDxuiFindTarget            * GetFindView    () const { return nullptr; }
    virtual DebuggerPaneFrame          * GetFindFrame   () const { return nullptr; }

    //  The pane's lists, which the window makes dense, sets in the text face
    //  and routes the mouse to; and its toolbars, whose menus it ticks.
    virtual std::vector<DxuiListView *>  GetLists       () const { return {}; }
    virtual std::vector<DxuiToolbar *>   GetBars        () const { return {}; }

    //  The pane's controls a press is offered to ahead of the lists, and its
    //  text boxes, which follow the pointer and type the characters typed.
    virtual std::vector<IDxuiControl *>   GetPressTargets () const { return {}; }
    virtual std::vector<DxuiTextInput *>  GetTextBoxes    () const { return {}; }

    //  The pane a control several panes share is in now, such as the memory
    //  windows' bar; empty for a control not the pane's.
    virtual std::wstring                 GetPaneOfSharedControl (const IDxuiControl * control) const { (void) control; return {}; }

    //  Whether a control of the pane takes typed characters, as a box does.
    virtual bool                         TakesTyping    (const IDxuiControl * control) const { (void) control; return false; }

    //  Whether the pane saves its lists' column widths in its own settings,
    //  in place of the debug project's; and whether its lists are in the
    //  proportional face, as every list but a disassembly's is.
    virtual bool  HasOwnColumnWidths () const { return false; }
    virtual bool  HasTextLists       () const { return true; }

    //  The pane's info button, none for a pane without one.
    virtual std::optional<PaneColorKeySpec>  GetColorKey () const { return std::nullopt; }

    virtual void                         Apply          (const DebuggerViewSnapshot & snapshot) { (void) snapshot; }

    //  The pane's text size, as a fraction of the usual.
    virtual void                         SetTextZoom    (float zoom) { (void) zoom; }

    //  Moves what moves between snapshots; true while something does.
    virtual bool                         Tick           (int64_t nowMs) { (void) nowMs; return false; }

    //  A floating window closed, its panes docked again.
    virtual void                         OnFloatClosed  (DxuiWindow * window) { (void) window; }

    //  Reads the pane's settings again after another debug project opens,
    //  and saves what it has to as the window closes.
    virtual void  OnProjectSwitched () {}
    virtual void  OnWindowClose     () {}

    //  The machine's log events sent since the last frame, each frame.
    virtual void                         ApplyLogDeltas (const DebugLogDeltas & deltas, bool isWindowVisible) { (void) deltas; (void) isWindowVisible; }

    //  Lays the pane's bar out in its slot, in the window the pane is in.
    virtual void                         PlaceBar       () {}

    //  The mouse, at four points of the window's routing: a box being
    //  edited in place; ahead of every other control, as the wheel over the
    //  profiler's timeline, which zooms it; at the pane's bar, ahead of the
    //  panes' contents; and at what the pane draws, after the dock's strips
    //  and sashes.
    virtual bool                         RouteEditMouse    (const DxuiMouseEvent & ev) { (void) ev; return false; }
    virtual bool                         RouteEarlyMouse   (const DxuiMouseEvent & ev) { (void) ev; return false; }
    virtual bool                         RouteBarMouse     (const DxuiMouseEvent & ev) { (void) ev; return false; }
    virtual bool                         RouteContentMouse (const DxuiMouseEvent & ev) { (void) ev; return false; }

    //  The zoom of a pane whose content zooms apart from the panes' text,
    //  such as the heat map: a step of notches about a point in the window,
    //  or about the content's middle with none; and back to the zoom the
    //  content starts at. Ctrl+wheel anywhere over the pane, and Ctrl+Plus,
    //  Ctrl+Minus and Ctrl+0 while it has the focus, are routed here first.
    //  False for a pane with no zoom apart from the text, whose Ctrl+wheel
    //  and keys then size that pane's text alone.
    virtual bool                         ZoomContent       (float notches, std::optional<POINT> clientPx) { (void) notches; (void) clientPx; return false; }
    virtual bool                         ResetContentZoom  () { return false; }

    //  A key for a box being edited in place, or for the pane's own control
    //  with the focus, ahead of the window's keys.
    virtual bool                         RouteEditKey      (const DxuiKeyEvent & ev, IDxuiControl * focused) { (void) ev; (void) focused; return false; }

    //  A key scheme's action, such as Find, while a control of the pane has
    //  the focus; and Enter in one of the pane's boxes.
    virtual bool                         OnMappedAction    (DebuggerKeySchemes::Action action, IDxuiControl * focused) { (void) action; (void) focused; return false; }

    //  Ctrl+G with the focus in the pane: true when the pane has a go-to and
    //  started it.
    virtual bool                         OnGoToKey         () { return false; }
    virtual bool                         SubmitTextBox     (DxuiTextInput * box) { (void) box; return false; }

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

};
