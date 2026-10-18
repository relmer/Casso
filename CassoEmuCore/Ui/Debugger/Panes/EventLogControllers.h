#pragma once

#include "Debugger/Events/DebugLogDelta.h"
#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"

class DiskLogPane;
class InputLogPane;
class ToolbarBoxEntry;





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogController
//
//  An event log pane: its bar over its list, with the list's rows from the
//  log's model. Each pane is closed until the user first opens it; its
//  options text then has "open=1". The machine's thread sends the rows as
//  deltas, which the window takes each frame and passes to each log.
//
////////////////////////////////////////////////////////////////////////////////

class EventLogController : public IDebuggerPane
{
public:
    EventLogController (IDebuggerPaneHost & host, std::wstring id, std::wstring title, DebugLogKind kind);
    ~EventLogController() override;

    DxuiToolbar                * GetBar             () const { return m_bar.GetBar(); }

    std::wstring                 GetId              () const override { return m_id; }
    std::wstring                 GetTitle           () const override { return m_title; }
    bool                         TryGetOpened       (bool & isOpened) const override { isOpened = m_isOpened; return true; }
    void                         Open               () override;
    IDxuiControl               * GetContent         () const override;
    std::vector<DxuiListView *>  GetLists           () const override { return { m_list }; }
    std::vector<DxuiToolbar *>   GetBars            () const override { return { m_bar.GetBar() }; }
    bool                         HasOwnColumnWidths () const override { return true; }
    void                         Apply              (const DebuggerViewSnapshot & snapshot) override;
    void                         OnProjectSwitched  () override;
    void                         OnWindowClose      () override;
    void                         PlaceBar           () override { m_bar.Place(); }
    bool                         RouteBarMouse      (const DxuiMouseEvent & ev) override { return m_bar.RouteMouse (ev); }
    bool                         RouteEditKey       (const DxuiKeyEvent & ev, IDxuiControl * focused) override;
    const wchar_t              * GetBarTip          (POINT clientPx, RECT & anchor) const override { return m_bar.GetTip (clientPx, anchor); }
    bool                         AddMenuItems       (DxuiListView * list, int row, PaneMenuItems & items) override;
    bool                         AddColumnMenuItems (DxuiListView * list, std::vector<std::shared_ptr<DxuiCommand>> & commands) override;

protected:
    //  The model's own text, options and visibility, and the column that
    //  is always in the list.
    virtual std::wstring         GetModelText       (bool isSelectedOnly) const = 0;
    virtual std::string          GetOptionsText     () const = 0;
    virtual void                 SetOptionsText     (const std::string & text) = 0;
    virtual void                 SetModelShown      (bool isShown) = 0;
    virtual size_t               GetTextColumn      () const = 0;

    //  Builds the drop-downs' rows again, before each opens.
    virtual void                 SetMenus           () = 0;

    //  Creates the list and then the bar, which paints over it; sets up the
    //  bar as the other panes' bars, its menus built before each press; and
    //  reads the options and whether the pane was ever opened.
    void                         CreateListAndBar   ();
    void                         ConfigureBar       ();
    std::string                  LoadOptions        ();
    void                         SaveOptions        ();

    //  Adds an entry to the bar's list of entries, and its command to the
    //  pane's commands.
    std::shared_ptr<DxuiCommand> AddEntry           (std::vector<DxuiToolbar::Entry> & entries, int id, const wchar_t * label, const wchar_t * tip,
                                                     DxuiToolbar::Kind kind, int group);

    bool                         CopyRows           (bool isAll);

    IDebuggerWindowHost        * GetWindowHost      () const { return m_host.GetWindowHost(); }

    std::wstring                                 m_id;
    std::wstring                                 m_title;
    DebugLogKind                                 m_kind;
    PaneBarSlot                                  m_bar;
    DxuiListView                               * m_list     = nullptr;
    std::unique_ptr<DebuggerPaneFrame>           m_frame;
    std::vector<std::shared_ptr<DxuiCommand>>    m_commands;
    bool                                         m_isOpened = false;

    //  From the last snapshot: the position the machine has been taken back
    //  to, none while live; whether the machine is a //c; and whether it has
    //  a Disk II controller.
    uint64_t                                     m_shownBefore = UINT64_MAX;
    bool                                         m_isTwoC      = false;
    bool                                         m_hasDisk     = true;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskLogController
//
//  The Disk II event log: its bar has its kinds, its drive, its severities,
//  and track and sector boxes, and a line under it lists the filter tokens
//  not taken, or that the machine has no Disk II controller.
//
////////////////////////////////////////////////////////////////////////////////

class DiskLogController : public EventLogController
{
public:
    explicit DiskLogController (IDebuggerPaneHost & host);
    ~DiskLogController() override;

    DiskLogPane                     & GetModel         () const { return *m_model; }

    void                              CreateControls   () override;
    void                              Configure        () override;
    void                              AddToDock        (DxuiDockSite & dock) override;
    std::vector<IDxuiControl *>       GetControls      () const override;
    std::vector<IDxuiControl *>       GetPressTargets  () const override { return { m_trackBox, m_sectorBox }; }
    std::vector<DxuiTextInput *>      GetTextBoxes     () const override { return { m_trackBox, m_sectorBox }; }
    void                              ApplyLogDeltas   (const DebugLogDeltas & deltas, bool isWindowVisible) override;
    void                              PlaceBar         () override;
    bool                              RouteBarMouse    (const DxuiMouseEvent & ev) override;
    bool                              IsMonospaceTipAt (POINT clientPx) const override;
    bool                              AddMenuItems     (DxuiListView * list, int row, PaneMenuItems & items) override;

protected:
    std::wstring                      GetModelText     (bool isSelectedOnly) const override;
    std::string                       GetOptionsText   () const override;
    void                              SetOptionsText   (const std::string & text) override;
    void                              SetModelShown    (bool isShown) override;
    size_t                            GetTextColumn    () const override;
    void                              SetMenus         () override;

private:
    static constexpr int  kBoxDip         = 90;
    static constexpr int  kTrackLabelDip  = 48;
    static constexpr int  kSectorLabelDip = 56;
    static constexpr int  kErrorDip       = 20;

    void  SetBarEntries ();
    void  ReadBoxes     ();
    bool  IsInBox       (POINT pointDip) const;

    std::unique_ptr<DiskLogPane>        m_model;
    DxuiTextInput                     * m_trackBox  = nullptr;
    DxuiTextInput                     * m_sectorBox = nullptr;
    DxuiLabel                         * m_error     = nullptr;
    std::unique_ptr<ToolbarBoxEntry>    m_trackEntry;
    std::unique_ptr<ToolbarBoxEntry>    m_sectorEntry;
};





////////////////////////////////////////////////////////////////////////////////
//
//  InputLogController
//
//  The input log: its bar has the guest's and the host's input and each
//  paddle pair's view.
//
////////////////////////////////////////////////////////////////////////////////

class InputLogController : public EventLogController
{
public:
    explicit InputLogController (IDebuggerPaneHost & host);
    ~InputLogController() override;

    InputLogPane                & GetModel       () const { return *m_model; }

    void                          CreateControls () override;
    void                          Configure      () override;
    void                          AddToDock      (DxuiDockSite & dock) override;
    std::vector<IDxuiControl *>   GetControls    () const override;
    void                          ApplyLogDeltas (const DebugLogDeltas & deltas, bool isWindowVisible) override;

    //  The guest's and the host's input, chosen on the Source column's Filter
    //  cascade.
    std::vector<DxuiPopupMenuItem>  GetColumnFilterItems (DxuiListView * list, size_t column, std::vector<std::shared_ptr<DxuiCommand>> & commands) override;

protected:
    std::wstring                  GetModelText   (bool isSelectedOnly) const override;
    std::string                   GetOptionsText () const override;
    void                          SetOptionsText (const std::string & text) override;
    void                          SetModelShown  (bool isShown) override;
    size_t                        GetTextColumn  () const override;
    void                          SetMenus       () override;

private:
    void          SetBarEntries   ();
    void          ShowSourceFilter ();
    std::wstring  GetPairLabel    (int pair) const;

    std::unique_ptr<InputLogPane>   m_model;
};
