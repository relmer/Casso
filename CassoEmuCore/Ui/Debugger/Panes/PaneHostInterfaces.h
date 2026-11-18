#pragma once

#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/TextDialog.h"
#include "Seams/IHostDialogs.h"

//  A context menu's rows: each its label and what choosing it does.
using PaneMenuItems = std::vector<std::pair<std::wstring, std::function<void()>>>;





////////////////////////////////////////////////////////////////////////////////
//
//  The debugger window's parts that a pane calls back into
//
//  Each interface covers one part of the window's state, and a pane has
//  references only to the ones it calls. DebuggerWindow implements all of
//  them; a test can fake one for the class under test.
//
////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////
//
//  IPaneCommands
//
//  The actions a pane runs, as the console or a key would.
//
////////////////////////////////////////////////////////////////////////////////

class IPaneCommands
{
public:
    virtual ~IPaneCommands() = default;

    //  Runs a control's action, its reply in the pane with the focus.
    virtual void  RunAction (const DebuggerAction & action) = 0;

    //  Runs a key scheme's action, as its key does, true when it acted; and a
    //  command bar entry, as a click on it does.
    virtual bool  RunKeyAction       (DebuggerKeySchemes::Action action) = 0;
    virtual void  RunCommandBarEntry (int id) = 0;

    //  Opens files as files dropped on the window are opened.
    virtual void  OpenFiles (const std::vector<std::wstring> & paths) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IPaneNavigator
//
//  Moves a view to a place: the disassembly to an address, a memory window
//  to one, a source document or the BASIC program to a line, or the machine
//  back to its present.
//
////////////////////////////////////////////////////////////////////////////////

class IPaneNavigator
{
public:
    virtual ~IPaneNavigator() = default;

    //  The disassembly to an address, or to the PC with none.
    virtual void  GoToCode (std::optional<Word> address) = 0;

    //  The memory window in front, or the first one in view, to an address,
    //  bringing that window forward.
    virtual void  ShowMemoryAddress (Word address) = 0;

    //  A source file's document at a line, brought forward; and the BASIC
    //  program pane at a line.
    virtual void  OpenSource    (int fileId, int line) = 0;
    virtual void  ShowBasicLine (Word line) = 0;

    //  Goes back to the machine's present from a point in its history.
    virtual void  GoLive () = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IPanePlaces
//
//  Which panes are open and where.
//
////////////////////////////////////////////////////////////////////////////////

class IPanePlaces
{
public:
    virtual ~IPanePlaces() = default;

    //  Opens a pane and brings it forward, as its row on the View menu does,
    //  and moves the focus to it when isFocused.
    virtual void  OpenPane (const std::wstring & pane, bool isFocused) = 0;

    //  Whether a pane is open and not closed from its close button, as the
    //  View menu's check has it; and whether two panes are tabs of one group,
    //  so bringing one forward covers the other.
    virtual bool  IsPaneOpen     (const std::wstring & pane) const = 0;
    virtual bool  IsSameTabGroup (const std::wstring & a, const std::wstring & b) const = 0;

    //  Moves controls that several panes share into another window, the one
    //  the pane they now act on is in.
    virtual void  MoveSharedControls (const std::vector<IDxuiControl *> & controls, DxuiWindow * from, DxuiWindow * to) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IPaneTabs
//
//  A pane's tab in its group.
//
////////////////////////////////////////////////////////////////////////////////

class IPaneTabs
{
public:
    virtual ~IPaneTabs() = default;

    //  The tab's title, and the mark ahead of the title with the mark's tip.
    virtual void  SetPaneTitle   (const std::wstring & pane, const std::wstring & title) = 0;
    virtual void  SetPaneTabMark (const std::wstring & pane, const DxuiTabGroup::LeadingMark & mark, const std::wstring & tip) = 0;

    //  Marks the tab for new output while the pane is out of sight.
    virtual void  MarkPaneTab (const std::wstring & pane) = 0;

    //  Brings the tab forward in its group, and lays the dock out again.
    virtual void  ActivatePaneTab (const std::wstring & pane) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IPaneFocus
//
//  The keyboard focus.
//
////////////////////////////////////////////////////////////////////////////////

class IPaneFocus
{
public:
    virtual ~IPaneFocus() = default;

    //  Moves the focus to a control, as a press on it does.
    virtual void  FocusControl (IDxuiControl * control) = 0;

    //  The control with the focus, the floating window's focused control
    //  while an event from it is routed; and the step that moves the focus to
    //  the command line, for a pane whose focused control is hidden.
    virtual IDxuiControl  * GetFocusedControl () const = 0;
    virtual void            FocusDefault      () = 0;

    //  The pane with the focus, empty for none.
    virtual std::wstring  GetFocusPane () const = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IPaneRouting
//
//  The input event being routed.
//
////////////////////////////////////////////////////////////////////////////////

class IPaneRouting
{
public:
    virtual ~IPaneRouting() = default;

    //  Whether the event goes to a control, and whether it came from the
    //  window a pane's bar is in.
    virtual bool  IsRoutable  (const IDxuiControl * control) const = 0;
    virtual bool  IsBarRouted (const std::wstring & pane) const = 0;

    //  Where the last press was, in the window's pixels, for a list's
    //  double-click to find which of a row's cells it landed in.
    virtual POINT  GetLastPressPx () const = 0;

    //  The window the event came from, the owner of a dialog or a clipboard
    //  write it leads to.
    virtual HWND  GetOwnerHwnd () const = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IPaneBars
//
//  Where a pane's bar is laid out.
//
////////////////////////////////////////////////////////////////////////////////

class IPaneBars
{
public:
    virtual ~IPaneBars() = default;

    //  The window a pane is in now: the debugger window, or the floating one
    //  it was moved to.
    virtual DxuiWindow  * GetPaneWindow (const std::wstring & pane) = 0;

    //  The part of a bar's slot its toolbar is laid out in, short of the
    //  pane's info button.
    virtual RECT  GetBarStrip (const std::wstring & pane, const RECT & slot) = 0;

    //  The Find entry for a pane's toolbar, which opens the pane's find
    //  widget.
    virtual DxuiToolbar::Entry  GetFindEntry (const std::wstring & pane) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IPaneTips
//
//  The tip under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

class IPaneTips
{
public:
    virtual ~IPaneTips() = default;

    //  Updates the tip after a move over a bar, or after what a tip lists
    //  comes in.
    virtual void  UpdateTooltip (POINT clientPx) = 0;

    //  The same for a pane's tip outside a mouse event, in the window the
    //  pane is in now: the debugger window or the floating one; and whether
    //  that window's tip is up.
    virtual void  UpdatePaneTooltip    (const std::wstring & pane, POINT clientPx) = 0;
    virtual bool  IsPaneTooltipVisible (const std::wstring & pane) const = 0;

    //  Hides the tip at once, as a press or a key does.
    virtual void  HideTooltip () = 0;

    //  New rows in a disassembly view, which take down the operand tip over
    //  them when its cell changes.
    virtual void  CheckOperandTip (int view) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IPaneLayout
//
//  Requests to lay out, build or paint part of the window again.
//
////////////////////////////////////////////////////////////////////////////////

class IPaneLayout
{
public:
    virtual ~IPaneLayout() = default;

    //  Lays the dock out again, or the whole window.
    virtual void  RelayoutDock   () = 0;
    virtual void  RelayoutWindow () = 0;

    //  Builds the window's menus again, after what they list changes.
    virtual void  UpdateWindowMenus () = 0;

    //  Paints the window again at its next frame, or once the messages
    //  waiting are handled, so a burst of input is taken before the paint.
    virtual void  Repaint      () = 0;
    virtual void  RepaintLater () = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IPaneViews
//
//  The window's settings for its views.
//
////////////////////////////////////////////////////////////////////////////////

class IPaneViews
{
public:
    virtual ~IPaneViews() = default;

    //  Saves the open views, with the choices the panes save among them; and
    //  whether the views the window reopens at startup have yet to settle.
    virtual void  SaveOpenViews    () = 0;
    virtual bool  IsRestoringViews () const = 0;

    //  Sends the machine again whether to record the heat map's accesses;
    //  and whether the disassembly views' heat column is on, which the other
    //  listings match.
    virtual void  SyncHeatRecording () = 0;
    virtual bool  IsCodeHeatOn      () const = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IPaneDialogs
//
//  The modal dialogs a pane opens. A test overrides them to return fixed
//  results.
//
////////////////////////////////////////////////////////////////////////////////

class IPaneDialogs
{
public:
    virtual ~IPaneDialogs() = default;

    //  A question put to the user in a message box centered on owner, and
    //  the answer; the same with buttons of the pane's choosing, and the id
    //  of the one pressed, IDCANCEL for Escape.
    virtual int  ShowMessageBox (HWND owner, const std::wstring & text, const wchar_t * caption, UINT type) = 0;
    virtual int  ShowMessageBox (HWND owner, const std::wstring & text, const wchar_t * caption, UINT type,
                                 const std::vector<DxuiMessageBoxButton> & buttons, int defaultId) = 0;

    //  The value the user types in a dialog, none when canceled.
    virtual std::optional<Byte>          RunByteDialog (const std::wstring & title, const std::wstring & label, const std::wstring & description) = 0;
    virtual std::optional<std::wstring>  RunTextDialog (const std::wstring & title, const std::wstring & label, const std::wstring & placeholder,
                                                        TextDialog::CheckFn check) = 0;

    //  The file pickers, none for a window with no host.
    virtual IHostDialogs  * GetHostDialogs () = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IPaneReplies
//
//  What the window puts up in answer to something done in a pane.
//
////////////////////////////////////////////////////////////////////////////////

class IPaneReplies
{
public:
    virtual ~IPaneReplies() = default;

    //  A reply in an infobar over the pane, or in a message box with no pane
    //  or when the pane isn't on screen.
    virtual void  ShowPaneReply (const std::wstring & pane, const std::vector<std::string> & lines, bool isError) = 0;

    //  A list of rows over a control, at exactly its width, rising from its
    //  top edge in the look given.
    virtual void  ShowListAbove (const PaneMenuItems & items, const RECT & anchorPx, const DxuiContextMenu::ListLook & look) = 0;

    //  A routine's Never step into this routine row: checked while the step
    //  filter has it, a click then taking it out; otherwise a click adds it
    //  and opens a notice of how to take it out again.
    virtual void  AddSkipItem (const std::string & name, Word address, PaneMenuItems & items) = 0;
};
