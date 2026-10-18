#pragma once

#include "Ui/Debugger/Panes/IDebuggerPane.h"

class MemoryAddressEntry;
class MemoryBarCommands;
class MemoryPane;
class MemoryWindowGroup;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowPane
//
//  One memory window: a place at its top for the bar the windows share,
//  over its bytes. The machine opens and closes the windows after the
//  first, as the snapshot lists them.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryWindowPane : public IDebuggerPane
{
public:
    MemoryWindowPane (IDebuggerPaneHost & host, MemoryWindowGroup & group, int id);
    ~MemoryWindowPane() override;

    int                              GetWindowId  () const { return m_windowId; }
    MemoryPane                     & GetModel     () const { return *m_model; }
    DxuiHexView                    * GetView      () const { return m_view; }
    DebuggerPaneFrame              * GetSlot      () const { return m_slot.get(); }
    bool                             IsOpen       () const { return m_isOpen; }
    void                             SetOpen      (bool isOpen) { m_isOpen = isOpen; }

    std::wstring                     GetId               () const override;
    std::wstring                     GetTitle            () const override;
    void                             CreateControls      () override;
    void                             Configure           () override;
    void                             AddToDock           (DxuiDockSite & dock) override;
    IDxuiControl                   * GetContent          () const override;
    std::vector<IDxuiControl *>      GetControls         () const override;
    std::optional<PaneColorKeySpec>  GetColorKey         () const override;
    bool                             TryGetOpened        (bool & isOpened) const override { isOpened = m_isOpen; return true; }
    bool                             RequestOpen         () override;
    bool                             RequestClose        () override;
    bool                             IsFixed             () const override { return m_windowId == 1; }
    void                             AddContentMenuItems (POINT clientPx, PaneMenuItems & items) override;

private:
    MemoryWindowGroup                   & m_group;
    int                                   m_windowId = 0;
    DxuiHexView                         * m_view     = nullptr;
    std::unique_ptr<MemoryPane>           m_model;
    std::unique_ptr<DebuggerPaneFrame>    m_slot;
    std::unique_ptr<DebuggerPaneFrame>    m_frame;
    bool                                  m_isOpen   = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryWindowGroup
//
//  What the memory windows share: Visual Studio's memory window bar, with
//  the Address box, Refresh, Columns, the grouping, Undo and Redo and Heat,
//  which sits in the bar of the window it acts on and goes with it into a
//  floating window; the Address box's history; the window last clicked in;
//  and a Find's or a comparison's results the machine has yet to send. The
//  group routes the mouse and the keys across every window, since a drag
//  in one has the mouse until it ends. It is in the pane list with no dock
//  pane of its own.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryWindowGroup : public IDebuggerPane
{
public:
    explicit MemoryWindowGroup (IDebuggerPaneHost & host);
    ~MemoryWindowGroup() override;

    //  Creates the windows, for the window to add to its pane list after the
    //  group.
    std::vector<std::unique_ptr<IDebuggerPane>>  CreateWindows ();

    DxuiToolbar                        * GetBar          () const { return m_bar; }
    DxuiTextInput                      * GetBox          () const { return m_box; }
    const std::vector<std::wstring>    & GetHistory      () const { return m_history; }
    MemoryWindowPane                   * GetWindow       (int id) const;
    std::vector<MemoryWindowPane *>      GetOpenWindows  () const;
    MemoryWindowPane                   * GetActive       () const;
    MemoryWindowPane                   * GetFocused      () const;
    bool                                 IsAnyHeatOn     () const;
    void                                 ClearHistories  ();

    //  The Address box's text gone to in the active window: a hex address at
    //  once, anything else resolved on the machine's thread.
    void                                 SubmitBox       ();
    void                                 RunBarEntry     (int id);
    void                                 ChooseColumns   (int columns);
    void                                 ChooseGrouping  (int grouping);
    bool                                 IsBarEnabled    (int id) const;
    void                                 Undo            (MemoryWindowPane * window, bool redo);

    //  A window's menu on its bytes.
    void                                 AddWindowMenuItems (MemoryWindowPane & window, POINT clientPx, PaneMenuItems & items);

    std::wstring                     GetId                  () const override { return L"memory-group"; }
    std::wstring                     GetTitle               () const override { return {}; }
    void                             CreateControls         () override;
    void                             Configure              () override;
    void                             AddToDock              (DxuiDockSite & dock) override { (void) dock; }
    IDxuiControl                   * GetContent             () const override { return nullptr; }
    std::vector<IDxuiControl *>      GetControls            () const override { return {}; }
    std::vector<DxuiToolbar *>       GetBars                () const override { return { m_bar }; }
    std::vector<IDxuiControl *>      GetPressTargets        () const override { return { m_box }; }
    std::vector<DxuiTextInput *>     GetTextBoxes           () const override { return { m_box }; }
    std::wstring                     GetPaneOfSharedControl (const IDxuiControl * control) const override;
    bool                             TakesTyping            (const IDxuiControl * control) const override;
    void                             Apply                  (const DebuggerViewSnapshot & snapshot) override;
    bool                             Tick                   (int64_t nowMs) override;
    void                             SetTextZoom            (float zoom) override;
    void                             OnFloatClosed          (DxuiWindow * window) override;
    void                             PlaceBar               () override;
    bool                             RouteBarMouse          (const DxuiMouseEvent & ev) override;
    bool                             RouteContentMouse      (const DxuiMouseEvent & ev) override;
    bool                             RouteEditKey           (const DxuiKeyEvent & ev, IDxuiControl * focused) override;
    bool                             OnMappedAction         (DebuggerKeySchemes::Action action, IDxuiControl * focused) override;
    bool                             SubmitTextBox          (DxuiTextInput * box) override;
    const wchar_t                  * GetBarTip              (POINT clientPx, RECT & anchor) const override;
    bool                             TryGetTip              (POINT clientPx, PaneTip & tip) override;

private:
    //  A Find's window, the bytes a match covers, the hit it is on, and the
    //  serial of the snapshot's hits it waits to replace; and a Compare
    //  with... waiting for its differences the same way.
    struct Find
    {
        int       window     = -1;
        size_t    length     = 1;
        int       at         = -1;
        uint64_t  awaited    = 0;
        bool      isAwaiting = false;
    };

    struct Compare
    {
        int       window     = -1;
        uint64_t  awaited    = 0;
        bool      isAwaiting = false;
    };

    void              SetBarMenus      ();
    std::wstring      GetBarLabel      (int id) const;
    std::wstring      GetBarTip        (int id) const;
    void              AddHistory       (const std::wstring & text);
    void              MoveBar          (DxuiWindow * to);
    void              ApplyResults     (const DebuggerViewSnapshot & snapshot);
    void              FindIn           (MemoryWindowPane * window);
    void              StepFind         (bool forward);
    void              FillSelection    (MemoryWindowPane * window);
    void              CopySelection    (MemoryWindowPane * window);
    void              CompareSelection (MemoryWindowPane * window);
    static size_t     GetSearchLength  (const std::wstring & items);

    std::array<MemoryWindowPane *, DebuggerViewState::kMaxMemoryWindows>  m_windows   = {};
    DxuiToolbar                                                         * m_bar       = nullptr;
    DxuiTextInput                                                       * m_box       = nullptr;
    std::unique_ptr<MemoryBarCommands>                                    m_commands;
    std::unique_ptr<MemoryAddressEntry>                                   m_addressEntry;
    std::vector<std::wstring>                                             m_history;
    MemoryWindowPane                                                    * m_active    = nullptr;
    std::wstring                                                          m_barPane;
    DxuiWindow                                                          * m_barHost   = nullptr;
    //  From the last snapshot: whether the machine is stopped, and a Find's
    //  hits and the serials of the results the machine sent last.
    bool               m_isPaused      = false;
    bool               m_hasSnapshot   = false;
    std::vector<Word>  m_searchHits;
    uint64_t           m_searchSerial  = 0;
    uint64_t           m_compareSerial = 0;
    uint32_t           m_goToSerial    = 0;
    Find               m_find;
    Compare            m_compare;
};
