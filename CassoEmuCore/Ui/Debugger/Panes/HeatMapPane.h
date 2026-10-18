#pragma once

#include "Debugger/HeatMapRangeSets.h"
#include "Debugger/HeatMapSymbols.h"
#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/HeatMapView.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"

class HeatMapBarCommands;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPane
//
//  The heat map: its bar of how it counts, over the map, with the set of
//  ranges, the bank and the view in a strip beside the map's tabs. The
//  options are kept and sent on to the machine as each is chosen; a cell
//  clicked is shown in memory, and with Ctrl its last access is looked up;
//  the machine is told which cell the mouse is over, for that cell's last
//  writer and reader.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapPane : public IDebuggerPane
{
public:
    //  What the pane reads of the sets of ranges, which the ranges pane
    //  edits: the sets, the symbols they read against, and the step that
    //  builds the range set drop-down on the strip.
    struct Ranges
    {
        std::function<const HeatMapRangeSets & ()>   sets;
        std::function<const HeatMapSymbols & ()>     symbols;
        std::function<void ()>                       buildMenu;
    };

    HeatMapPane (IDebuggerPaneHost & host, Ranges ranges);
    ~HeatMapPane() override;

    HeatMapView                        * GetView      () const { return m_view; }
    DxuiToolbar                        * GetBar       () const { return m_bar.GetBar(); }
    DxuiToolbar                        * GetViewBar   () const { return m_viewBar; }

    //  A choice made on the bar: shown at once, kept, and sent on to the
    //  machine with the spans of the set whose reads before written are left
    //  out.
    void                                 SetOptions   (const HeatMapOptions & options);
    void                                 SetBarMenus  ();
    void                                 SendIgnore   ();
    void                                 RunBarEntry  (int id);
    bool                                 IsBarEnabled (int id) const;
    bool                                 IsBarChecked (int id) const;
    std::wstring                         GetBarLabel  (int id) const;

    //  The rows a menu on a byte or a cell gets for its last write and read:
    //  show each in the disassembly, or go back to it.
    void                                 AddAccessItems (Word address, HeatMapOptions::Bank bank, PaneMenuItems & items);

    //  The banks the machine has, and the one shown.
    std::vector<HeatMapOptions::Bank>    GetBanks     () const;
    HeatMapOptions::Bank                 GetShownBank () const;

    //  The drop-downs' rows, as last built.
    const std::vector<std::shared_ptr<DxuiCommand>> &  GetFadeCommands   () const { return m_fadeCommands; }
    const std::vector<std::shared_ptr<DxuiCommand>> &  GetViewCommands   () const { return m_viewCommands; }
    const std::vector<std::shared_ptr<DxuiCommand>> &  GetBankCommands   () const { return m_bankCommands; }
    const std::vector<std::shared_ptr<DxuiCommand>> &  GetIgnoreCommands () const { return m_ignoreCommands; }

    std::wstring                     GetId               () const override;
    std::wstring                     GetTitle            () const override { return L"Heat map"; }
    void                             CreateControls      () override;
    void                             Configure           () override;
    void                             AddToDock           (DxuiDockSite & dock) override;
    void                             OnDocked            () override;
    IDxuiControl                   * GetContent          () const override;
    std::vector<IDxuiControl *>      GetControls         () const override;
    std::vector<DxuiToolbar *>       GetBars             () const override { return { m_bar.GetBar(), m_viewBar }; }
    std::optional<PaneColorKeySpec>  GetColorKey         () const override;
    void                             Apply               (const DebuggerViewSnapshot & snapshot) override;
    bool                             Tick                (int64_t nowMs) override;
    void                             OnProjectSwitched   () override;
    void                             PlaceBar            () override;
    bool                             RouteEarlyMouse     (const DxuiMouseEvent & ev) override;
    bool                             RouteBarMouse       (const DxuiMouseEvent & ev) override;
    bool                             RouteContentMouse   (const DxuiMouseEvent & ev) override;
    const wchar_t                  * GetBarTip           (POINT clientPx, RECT & anchor) const override;
    bool                             TryGetTip           (POINT clientPx, PaneTip & tip) override;
    void                             AddContentMenuItems (POINT clientPx, PaneMenuItems & items) override;

private:
    void  SetIgnoreMenu  ();
    void  RequestAccess  (Word address, HeatMapView::PickAction action, HeatMapOptions::Bank bank);
    void  SendHover      (std::optional<Word> address);
    void  SaveOptions    (const HeatMapOptions & options);

    Ranges                                       m_ranges;
    PaneBarSlot                                  m_bar;
    HeatMapView                                * m_view          = nullptr;
    DxuiToolbar                                * m_viewBar       = nullptr;
    std::unique_ptr<DebuggerPaneFrame>           m_frame;
    std::unique_ptr<HeatMapBarCommands>          m_commands;
    std::vector<std::shared_ptr<DxuiCommand>>    m_fadeCommands;
    std::vector<std::shared_ptr<DxuiCommand>>    m_viewCommands;
    std::vector<std::shared_ptr<DxuiCommand>>    m_bankCommands;
    std::vector<std::shared_ptr<DxuiCommand>>    m_ignoreCommands;
    std::string                                  m_ignoreWords;
    std::vector<HeatMapOptions::Bank>            m_banks;
    uint64_t                                     m_appliedSerial = 0;

    //  The point the tip over the map is for, and whether it is pending until
    //  a snapshot brings the cell's last writer and reader.
    std::optional<POINT>                         m_tipAt;
    bool                                         m_isTipPending  = false;
};
