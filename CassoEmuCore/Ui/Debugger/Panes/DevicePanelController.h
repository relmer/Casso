#pragma once

#include "Ui/Debugger/Panes/IDebuggerPane.h"

class DiagnosticsPane;





////////////////////////////////////////////////////////////////////////////////
//
//  DevicePanelController
//
//  A device's panel: its rows, with the memory map, disk head and meters the
//  device has, from the panel's model. The machine opens and closes the
//  panel; the snapshot has a panel for each device whose panel is open, so
//  a panel whose device left the machine hides. A panel that opens comes to
//  the front of its tab group.
//
////////////////////////////////////////////////////////////////////////////////

class DevicePanelController : public IDebuggerPane
{
public:
    DevicePanelController (IDebuggerPaneHost & host, std::string panelId, std::wstring title);
    ~DevicePanelController() override;

    std::wstring                     GetId          () const override;
    std::wstring                     GetTitle       () const override { return m_title; }
    void                             CreateControls () override;
    void                             Configure      () override;
    bool                             TryGetOpened   (bool & isOpened) const override { isOpened = m_isOpen; return true; }
    bool                             RequestOpen    () override;
    bool                             RequestClose   () override;
    bool                             IsFixed        () const override { return false; }
    std::optional<int>               GetViewMenuRank (const DebuggerViewSnapshot * snapshot) const override;
    void                             AddToDock      (DxuiDockSite & dock) override;
    IDxuiControl                   * GetContent     () const override;
    std::vector<IDxuiControl *>      GetControls    () const override;
    std::vector<DxuiListView *>      GetLists       () const override;
    std::optional<PaneColorKeySpec>  GetColorKey    () const override;
    void                             Apply          (const DebuggerViewSnapshot & snapshot) override;
    bool                             Tick           (int64_t nowMs) override;
    bool                             TryGetTip      (POINT clientPx, PaneTip & tip) override;

private:
    std::string                         m_panelId;
    std::wstring                        m_title;
    std::unique_ptr<DiagnosticsPane>    m_model;
    bool                                m_isOpen = false;
};
