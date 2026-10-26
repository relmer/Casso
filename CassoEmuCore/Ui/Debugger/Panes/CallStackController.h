#pragma once

#include "Ui/Debugger/Panes/IDebuggerPane.h"

class CallStackPane;
class DebuggerPaneFrame;





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackController
//
//  The call stack pane: its list, with the rows from the call stack's model,
//  the BASIC frames ahead of the machine's while a program is stopped. The
//  pane is always in hybrid mode; CALLS MODE picks another (FR-068), so the
//  button that cycled them is hidden.
//
////////////////////////////////////////////////////////////////////////////////

class CallStackController : public IDebuggerPane
{
public:
    explicit CallStackController (IDebuggerPaneHost & host);
    ~CallStackController() override;

    DxuiListView                   * GetList         () const { return m_list; }

    std::wstring                     GetId           () const override;
    std::wstring                     GetTitle        () const override { return L"Call stack"; }
    void                             CreateControls  () override;
    void                             Configure       () override;
    void                             AddToDock       (DxuiDockSite & dock) override;
    IDxuiControl                   * GetContent      () const override;
    std::vector<IDxuiControl *>      GetControls     () const override { return { m_button, m_list }; }
    std::vector<DxuiListView *>      GetLists        () const override { return { m_list }; }
    std::vector<IDxuiControl *>      GetPressTargets () const override { return { m_button }; }
    void                             Apply           (const DebuggerViewSnapshot & snapshot) override;
    bool                             AddMenuItems    (DxuiListView * list, int row, PaneMenuItems & items) override;

private:
    DxuiListView                        * m_list   = nullptr;
    DxuiButton                          * m_button = nullptr;
    std::unique_ptr<CallStackPane>        m_model;
    std::unique_ptr<DebuggerPaneFrame>    m_frame;
};
