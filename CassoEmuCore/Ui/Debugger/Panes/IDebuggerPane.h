#pragma once

#include "Seams/IHostDialogs.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerViewState.h"





//  A context menu's rows: each its label and what choosing it does.
using PaneMenuItems = std::vector<std::pair<std::wstring, std::function<void()>>>;





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

    //  Runs a control's action, in the console's mode.
    virtual void                   RunAction        (const DebuggerAction & action) = 0;
    virtual CommandMode            GetMode          () const = 0;

    //  Moves the disassembly to an address.
    virtual void                   GoToCode         (Word address) = 0;

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

    //  Whether the event being routed goes to a control, and whether it
    //  came from the window a pane's bar is in.
    virtual bool                   IsRoutable       (const IDxuiControl * control) const = 0;
    virtual bool                   IsBarRouted      (const std::wstring & pane) const = 0;

    //  The part of a bar's slot its toolbar is laid out in, short of the
    //  pane's info button.
    virtual RECT                   GetBarStrip      (const std::wstring & pane, const RECT & slot) = 0;

    //  Updates the tip under the pointer after a move over a bar.
    virtual void                   UpdateTooltip    (POINT clientPx) = 0;

    //  Paints the window again at its next frame.
    virtual void                   Repaint          () = 0;
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

    //  Builds the frame and adds it to the dock under the pane's id.
    virtual void                         AddToDock      (DxuiDockSite & dock) = 0;

    //  The control the dock places, and every control of the pane, which
    //  move together into a floating window.
    virtual IDxuiControl               * GetContent     () const = 0;
    virtual std::vector<IDxuiControl *>  GetControls    () const = 0;

    //  The pane's lists, which the window makes dense, sets in the text face
    //  and routes the mouse to; and its toolbars, whose menus it ticks.
    virtual std::vector<DxuiListView *>  GetLists       () const { return {}; }
    virtual std::vector<DxuiToolbar *>   GetBars        () const { return {}; }

    virtual void                         Apply          (const DebuggerViewSnapshot & snapshot) { (void) snapshot; }

    //  Lays the pane's bar out in its slot, in the window the pane is in.
    virtual void                         PlaceBar       () {}

    //  The pane's bar takes the mouse ahead of the panes' contents; a graphic
    //  of the pane takes it ahead of the lists.
    virtual bool                         RouteBarMouse     (const DxuiMouseEvent & ev) { (void) ev; return false; }
    virtual bool                         RouteContentMouse (const DxuiMouseEvent & ev) { (void) ev; return false; }

    //  The tip of the bar entry under a point, or null; and the tip of what
    //  the pane draws under a point, which follows the pointer.
    virtual const wchar_t              * GetBarTip      (POINT clientPx, RECT & anchor) const { (void) clientPx; (void) anchor; return nullptr; }
    virtual bool                         TryGetContentTip (POINT clientPx, std::wstring & text) const { (void) clientPx; (void) text; return false; }

    //  A list row's context menu; false for a list not the pane's.
    virtual bool                         AddMenuItems   (DxuiListView * list, int row, PaneMenuItems & items) { (void) list; (void) row; (void) items; return false; }

protected:
    IDebuggerPaneHost  & m_host;
};
