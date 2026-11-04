#pragma once

#include "Ui/Debugger/Panes/IDebuggerPane.h"
#include "Ui/Debugger/Panes/DiskHeadView.h"

class DiagnosticsPane;
class PaneBarSlot;





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
    bool                             TryGetTipAt    (POINT clientPx, DxuiTip & tip);

    //  The Video panel's menu opens the video log, the Mockingboard panel's
    //  the sound log.
    void                             AddContentMenuItems (POINT clientPx, PaneMenuItems & items) override;
    bool                             RouteContentMouse (const DxuiMouseEvent & ev) override;
    std::vector<DxuiToolbar *>       GetBars        () const override;
    void                             PlaceBar       () override;
    bool                             RouteBarMouse  (const DxuiMouseEvent & ev) override;

    //  The Disk II panel's choice of drives as it is saved, "1", "2" or
    //  "both", and back; anything else reads as both.
    static std::string                   ToText   (DiskHeadView::DriveChoice choice);
    static DiskHeadView::DriveChoice     FromText (const std::string & text);

private:
    void  SetDriveBarEntries ();
    void  SetDrives          (DiskHeadView::DriveChoice choice);

    std::string                                m_panelId;
    std::wstring                               m_title;
    std::unique_ptr<DiagnosticsPane>           m_model;
    std::unique_ptr<PaneBarSlot>               m_bar;
    std::vector<std::shared_ptr<DxuiCommand>>  m_commands;
    bool                                       m_isOpen         = false;
    std::wstring                               m_boardTipText;
    RECT                                       m_boardTipAnchor = {};
};
