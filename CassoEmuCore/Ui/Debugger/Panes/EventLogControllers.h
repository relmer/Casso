#pragma once

#include "Debugger/Events/DebugLogDelta.h"
#include "Ui/Debugger/PaneBarSlot.h"
#include "Ui/Debugger/Panes/EventLogPane.h"
#include "Ui/Debugger/Panes/IDebuggerPane.h"

class DiskLogPane;
class InputLogPane;
class IoLogPane;





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

    const std::wstring         & GetId              () const override { return m_id; }
    std::wstring                 GetTitle           () const override { return m_title; }
    bool                         TryGetOpened       (bool & isOpened) const override { isOpened = m_isOpened; return true; }
    void                         Open               () override;
    IDxuiControl               * GetContent         () const override;
    std::vector<DxuiListView *>  GetLists           () const override { return { m_list }; }
    std::vector<DxuiToolbar *>   GetBars            () const override { return { m_bar.GetBar() }; }
    IDxuiFindTarget            * GetFindView        () const override { return m_list; }
    DebuggerPaneFrame          * GetFindFrame       () const override { return m_frame.get(); }
    bool                         HasOwnColumnWidths () const override { return true; }
    void                         Apply              (const DebuggerViewSnapshot & snapshot) override;
    void                         OnProjectSwitched  () override;
    void                         OnWindowClose      () override;
    void                         PlaceBar           () override { m_bar.Place(); }
    bool                         RouteBarMouse      (const DxuiMouseEvent & ev) override { return m_bar.RouteMouse (ev); }
    bool                         RouteEditKey       (const DxuiKeyEvent & ev, IDxuiControl * focused) override;
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

    //  The model's column filters, which the headings' chevrons open.
    virtual IEventLogFilter    * GetColumnFilters   () const = 0;

    //  Each filterable heading's chevron and funnel, and a chevron's menu: the
    //  column's values as check boxes, or its comparisons.
    void                         ConfigureColumnFilters ();
    void                         ShowColumnFilter       (size_t column, RECT chevronPx);
    void                         AskComparison          (size_t column, IEventLogFilter::Op op);
    void                         ShowFilterState        ();

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
    std::vector<std::shared_ptr<DxuiCommand>>    m_filterCommands;   // the open chevron menu's rows
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
//  The Disk II event log: its columns' chevrons filter its rows, and a line
//  under its bar says when the machine has no Disk II controller.
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
    void                              ApplyLogDeltas   (const DebugLogDeltas & deltas, bool isWindowVisible) override;
    bool                              AddMenuItems     (DxuiListView * list, int row, PaneMenuItems & items) override;

protected:
    std::wstring                      GetModelText     (bool isSelectedOnly) const override;
    std::string                       GetOptionsText   () const override;
    void                              SetOptionsText   (const std::string & text) override;
    void                              SetModelShown    (bool isShown) override;
    size_t                            GetTextColumn    () const override;
    void                              SetMenus         () override {}
    IEventLogFilter                 * GetColumnFilters () const override;

private:
    static constexpr int  kErrorDip = 20;

    void  SetBarEntries ();

    std::unique_ptr<DiskLogPane>    m_model;
    DxuiLabel                     * m_error = nullptr;
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


protected:
    std::wstring       GetModelText     (bool isSelectedOnly) const override;
    std::string        GetOptionsText   () const override;
    void               SetOptionsText   (const std::string & text) override;
    void               SetModelShown    (bool isShown) override;
    size_t             GetTextColumn    () const override;
    void               SetMenus         () override;
    IEventLogFilter  * GetColumnFilters () const override;

private:
    void          SetBarEntries    ();
    std::wstring  GetPairLabel     (int pair) const;

    std::unique_ptr<InputLogPane>   m_model;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IoLogController
//
//  The video log or the sound log (FR-198): its bar has Clear and Save,
//  which run VIDEOLOG or SOUNDLOG CLEAR and SAVE, so the pane and the
//  console agree on what the log holds.
//
////////////////////////////////////////////////////////////////////////////////

class IoLogController : public EventLogController
{
public:
    IoLogController (IDebuggerPaneHost & host, DebugLogKind kind);
    ~IoLogController() override;

    IoLogPane                   & GetModel       () const { return *m_model; }

    void                          CreateControls () override;
    void                          Configure      () override;
    void                          AddToDock      (DxuiDockSite & dock) override;
    std::vector<IDxuiControl *>   GetControls    () const override;
    void                          ApplyLogDeltas (const DebugLogDeltas & deltas, bool isWindowVisible) override;


    //  What Clear and Save run, for the log this pane shows.
    void                          Clear          ();
    void                          Save           ();

protected:
    std::wstring       GetModelText     (bool isSelectedOnly) const override;
    std::string        GetOptionsText   () const override;
    void               SetOptionsText   (const std::string & text) override;
    void               SetModelShown    (bool isShown) override;
    size_t             GetTextColumn    () const override;
    void               SetMenus         () override {}
    IEventLogFilter  * GetColumnFilters () const override;

private:
    bool          IsVideo          () const { return m_kind == DebugLogKind::Video; }
    void          SetBarEntries    ();

    std::unique_ptr<IoLogPane>    m_model;
};
