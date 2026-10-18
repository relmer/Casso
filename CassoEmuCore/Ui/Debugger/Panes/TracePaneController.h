#pragma once

#include "Ui/Debugger/Panes/IDebuggerPane.h"

class DebuggerPaneFrame;
class KeyHintLine;
class TracePane;





////////////////////////////////////////////////////////////////////////////////
//
//  TracePaneController
//
//  The trace pane: its keys listed over its rows, with the rows from the
//  trace's model. The pane steps and runs from its own keys, without the
//  code pane or the toolbar; a click on a key's hint acts as the key does.
//
////////////////////////////////////////////////////////////////////////////////

class TracePaneController : public IDebuggerPane
{
public:
    explicit TracePaneController (IDebuggerPaneHost & host);
    ~TracePaneController() override;

    DxuiListView                   * GetList           () const { return m_list; }

    //  HISTORY SAVE to a file the user picks.
    void                             Save              ();

    std::wstring                     GetId             () const override;
    std::wstring                     GetTitle          () const override { return L"Trace"; }
    void                             CreateControls    () override;
    void                             Configure         () override;
    void                             AddToDock         (DxuiDockSite & dock) override;
    IDxuiControl                   * GetContent        () const override;
    std::vector<IDxuiControl *>      GetControls       () const override;
    std::vector<DxuiListView *>      GetLists          () const override { return { m_list }; }
    void                             Apply             (const DebuggerViewSnapshot & snapshot) override;
    bool                             Tick              (int64_t nowMs) override;
    bool                             RouteContentMouse (const DxuiMouseEvent & ev) override;
    bool                             RouteEditKey      (const DxuiKeyEvent & ev, IDxuiControl * focused) override;
    bool                             AddMenuItems      (DxuiListView * list, int row, PaneMenuItems & items) override;

private:
    static constexpr int  kHintDip = 20;

    //  A trace key's action, from the key or a click on its hint.
    bool                             RunTraceAction    (int action);

    DxuiListView                        * m_list = nullptr;
    KeyHintLine                         * m_hint = nullptr;
    std::unique_ptr<TracePane>            m_model;
    std::unique_ptr<DebuggerPaneFrame>    m_frame;
};
