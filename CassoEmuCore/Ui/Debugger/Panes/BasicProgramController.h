#pragma once

#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"

class BasicProgramPane;
class DebuggerPaneFrame;





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramController
//
//  The Applesoft program pane (FR-173): a bar that loads a listing as the
//  program and saves the program as one, over the list. The program's
//  breakpoints are set by activating a row, and with the keys in
//  the list a step is the interpreter's. Its variables are in the Watches
//  pane and its frames in the Call stack pane. While the machine runs, its
//  thread reads the program for the snapshots only while the pane is open
//  and on screen.
//
////////////////////////////////////////////////////////////////////////////////

class BasicProgramController : public IDebuggerPane
{
public:
    explicit BasicProgramController (IDebuggerPaneHost & host);
    ~BasicProgramController() override;

    DxuiListView                   * GetList        () const { return m_list; }

    //  Whether the last snapshot stopped in the interpreter while a program
    //  runs, which brings the pane forward with the focus.
    bool                             IsStop         () const { return m_isStop; }

    //  The line, brought forward in the pane, and scrolled to again once the
    //  pane has the program.
    void                             ShowLine       (Word line, bool isFocused);

    const std::wstring            & GetId          () const override;
    std::wstring                     GetTitle       () const override { return L"BASIC program"; }
    void                           CreateControls   () override;
    void                           Configure        () override;
    void                           AddToDock        (DxuiDockSite & dock) override;
    IDxuiControl                 * GetContent       () const override;
    std::vector<IDxuiControl *>    GetControls      () const override { return { m_list }; }
    int                            GetStopFocusRank () const override { return m_isStop ? 2 : 0; }
    std::vector<DxuiListView *>    GetLists         () const override { return { m_list }; }
    IDxuiFindTarget              * GetFindView      () const override { return m_list; }
    DebuggerPaneFrame            * GetFindFrame     () const override { return m_frame.get(); }
    bool                           HasTextLists     () const override { return false; }   // monospace, as LIST prints
    std::vector<DxuiToolbar *>     GetBars          () const override { return { m_bar.GetBar() }; }
    void                           PlaceBar         () override { m_bar.Place(); }
    bool                           RouteBarMouse    (const DxuiMouseEvent & ev) override { return m_bar.RouteMouse (ev); }
    void                           Apply            (const DebuggerViewSnapshot & snapshot) override;
    bool                           Tick             (int64_t nowMs) override;
    bool                           OnMappedAction   (DebuggerKeySchemes::Action action, IDxuiControl * focused) override;
    bool                           AddMenuItems     (DxuiListView * list, int row, PaneMenuItems & items) override;
    bool                           RouteEarlyMouse  (const DxuiMouseEvent & ev) override;

private:
    void                Follow           (const DebuggerViewSnapshot & snapshot);
    static bool         IsMachineStop    (const DebuggerViewSnapshot & snapshot);
    void                ApplyRows        (const DebuggerViewSnapshot & snapshot);
    bool                UpdateWrap       ();
    void                ToggleBreakpoint (Word line);
    std::optional<int>  FindBreakpoint   (Word line) const;

    std::vector<DxuiToolbar::Entry>      MakeBarEntries () const;

    PaneBarSlot                           m_bar;
    DxuiListView                        * m_list      = nullptr;
    std::unique_ptr<BasicProgramPane>     m_model;
    std::unique_ptr<DebuggerPaneFrame>    m_frame;
    std::optional<Word>                   m_shownLine;
    std::optional<Word>                   m_pendingLine;
    std::optional<std::set<int>>          m_breakpointIds;
    bool                                  m_isReading = false;
    bool                                  m_wasPaused = false;
    bool                                  m_isStop    = false;
};
