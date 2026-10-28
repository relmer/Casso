#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  IFloatingPanesHost
//
//  The debugger window's part in floating panes: the dock site, each pane's
//  controls, title and close, the routing of an event as the window's own,
//  and the calls made when a pane floats, docks or gets the focus.
//
////////////////////////////////////////////////////////////////////////////////

class IFloatingPanesHost
{
public:
    virtual ~IFloatingPanesHost() = default;

    virtual HWND                           GetFloatOwner     () const = 0;
    virtual HINSTANCE                      GetFloatInstance  () const = 0;
    virtual DxuiDockSite *                 GetFloatDockSite  () const = 0;
    virtual const DxuiTheme *              GetTheme          () const = 0;
    virtual const DxuiKeyMap *             GetFloatKeyMap    () const = 0;
    virtual const DxuiDpiScaler &          GetFloatScaler    () const = 0;

    virtual std::vector<IDxuiControl *>    GetPaneControls   (const std::wstring & pane) const = 0;
    virtual IDxuiControl *                 GetPaneContent    (const std::wstring & pane) const = 0;
    virtual std::wstring                   GetPaneTitle      (const std::wstring & pane) const = 0;
    virtual bool                           IsPaneShown       (const std::wstring & pane) const = 0;
    virtual bool                           CanClosePane      (const std::wstring & pane) const = 0;
    virtual void                           ClosePane         (const std::wstring & pane) = 0;
    virtual void                           ShowDockToMenu    (const std::wstring & pane, POINT clientPx) = 0;
    virtual void                           SaveLayout        () = 0;
    virtual void                           Repaint           () = 0;

    //  Sync is due: run it soon, outside any window's message. The frame
    //  that runs it stops while a modal dialog runs the message loop.
    virtual void                           RequestFloatSync  () = 0;

    //  The controls moved out of the window into a floating one, and back.
    virtual std::unique_ptr<IDxuiControl>  DetachFloatControl (IDxuiControl * control) = 0;
    virtual void                           AttachFloatControl (std::unique_ptr<IDxuiControl> control) = 0;

    //  An event in a floating window, routed as the window's own are, with
    //  GetRoutingPane indicating the pane it came from.
    virtual bool  RouteFloatedMouse   (const DxuiMouseEvent & ev) = 0;
    virtual bool  RouteFloatedKey     (const DxuiKeyEvent & ev) = 0;
    virtual bool  RunFloatedCommand   (int commandId) = 0;
    virtual bool  DropFloatedFiles    (const std::vector<std::wstring> & paths) = 0;

    //  Called when a floating window's focus changes; when one is about to
    //  be destroyed, its pane's controls back in the debugger window; when
    //  the windows match the layout again; and when a tip is about to be
    //  destroyed.
    virtual void  OnFloatFocusChanged (const std::wstring & pane, bool focused) = 0;
    virtual void  OnFloatDocked       (const std::wstring & pane, DxuiDockedWindow * window) = 0;
    virtual void  OnFloatsSynced      () = 0;
    virtual void  OnFloatTipErased    (const DxuiTooltip * tip) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FloatingPanes
//
//  Each floating window a DxuiDockedWindow, holding one pane or a group of
//  them as tabs (FR-205), their controls moved there whole, with a tooltip
//  and a focused control of its own; the drag that tears a pane or a group
//  off or docks it back; and the window the event being routed came from,
//  which every event in the debugger is filtered by.
//
//  WINDOWS ARE KEYED AS THE LAYOUT KEYS THEM (DxuiPaneLayout::Floating::
//  window), and a pane finds its window through the panes each holds. The
//  routing pane of an event from a window of several is the tab it shows,
//  the only one whose controls are in view.
//
////////////////////////////////////////////////////////////////////////////////

class FloatingPanes
{
public:
    explicit FloatingPanes (IFloatingPanesHost & host) : m_host (host) {}

    //  Sets the routing pane for the life of the scope, and restores the
    //  previous one.
    class RouteScope
    {
    public:
        RouteScope (FloatingPanes & floats, const std::wstring & pane) : m_floats (floats), m_saved (floats.m_routingPane) { floats.m_routingPane = pane; }
        ~RouteScope () { m_floats.m_routingPane = m_saved; }

        RouteScope (const RouteScope &)             = delete;
        RouteScope & operator= (const RouteScope &) = delete;

    private:
        FloatingPanes  & m_floats;
        std::wstring     m_saved;
    };

    //  The pane the event being routed came from, empty for the debugger
    //  window itself.
    const std::wstring &  GetRoutingPane () const { return m_routingPane; }
    bool                  IsRoutedToFloat () const { return !m_routingPane.empty() && Contains (m_routingPane); }

    //  Whether an event now being routed is passed to a control in `pane`,
    //  or in no pane for an empty one; and the routing pane for mouse events
    //  on a pane's bar.
    bool          IsRoutable        (const std::wstring & pane) const;
    std::wstring  GetBarRoutingPane (const std::wstring & pane) const;

    //  The floating window, popup host, tooltip and HWND of the event being
    //  routed, null for the debugger window's own.
    DxuiHwndSource *  GetRoutedPopupHost () const;
    DxuiTooltip *     GetRoutedTip       () const;
    HWND              GetRoutedHwnd      () const;

    //  An event routed as the debugger window's own under `pane`: a floating
    //  pane's, or one over a pane slid out from an edge; and a key-scheme
    //  command from a floating window's key map.
    bool  RouteMouse   (const std::wstring & pane, const DxuiMouseEvent & ev);
    bool  RouteKey     (const std::wstring & pane, const DxuiKeyEvent & ev);
    bool  RouteCommand (const std::wstring & pane, int commandId);

    //  The focused control of the floating window the event came from.
    IDxuiControl *  GetRoutedFocus  () const;
    void            SetRoutedFocus  (IDxuiControl * control);
    void            ForgetFocus     (const std::vector<IDxuiControl *> & controls);

    //  A pane's window, or a window by its key.
    bool                Contains    (const std::wstring & pane) const { return !GetWindowOf (pane).empty(); }
    DxuiDockedWindow *  Find        (const std::wstring & pane) const;
    DxuiDockSite *      FindSite    (const std::wstring & pane) const;

    //  The layout's floating places change; the windows match it at the next
    //  Sync, run from the frame.
    void  MarkSyncNeeded () { m_isSyncNeeded = true; m_host.RequestFloatSync(); }
    void  Sync           ();

    void  RequestFloat (const std::wstring & pane, POINT clientPx);
    void  TearOff      (const std::wstring & pane, POINT clientPx);
    void  TearOffGroup (const std::vector<std::wstring> & panes, const std::wstring & active, POINT clientPx);
    void  CarryTornOff ();
    void  DockBack     (const std::wstring & pane);
    void  Show         (const std::wstring & pane);

    //  Each floating window's place, read back into the layout, since moving
    //  or sizing one is not a layout operation.
    void  SaveRects (DxuiPaneLayout & layout) const;

    //  A tooltip by its key, a pane's or the floating command bar's.
    void  SetTip   (const std::wstring & key, std::unique_ptr<DxuiTooltip> tip) { m_tips[key] = std::move (tip); }
    void  EraseTip (const std::wstring & key);
    DxuiTooltip *  FindTip (const std::wstring & key) const;

    //  Every floating window at once.
    void  ApplyTheme       (const DxuiTheme * theme);
    void  ApplyKeyMap      (const DxuiKeyMap * map);
    void  HideAll          ();
    void  PollCaptionDrags ();
    void  InvalidateAll    ();
    void  Tick             (int64_t now);
    void  SetFocusedLooks  (const std::wstring & focusedPane, const DxuiTheme & theme);
    bool  IsAnyMenuOpen    () const;

    static std::wstring  GetMonitorKey (const RECT & rectPx);

private:
    //  A point outside every dock site, where a drag over another floating
    //  window is taken to be.
    static constexpr POINT  s_kOffSite = { INT_MIN / 2, INT_MIN / 2 };

    void          Float          (const std::wstring & window);
    void          Dock           (const std::wstring & window);
    void          PlaceUnderGrab (const std::wstring & pane);
    void          OnDrag         (const std::wstring & window, POINT screenPx, bool ended);
    bool          IsOverAnotherWindow (const std::wstring & window, POINT screenPx) const;
    void          ShowDragMarks  ();
    void          HideDragMarks  ();
    void          DropCarriedTab (const std::wstring & pane);
    void          SetFade        (const std::wstring & pane, bool on);
    void          TearOut        (const std::wstring & window, const std::wstring & pane, POINT windowPx);
    void          DockBackWindow (const std::wstring & window);
    RECT          GetTearOffRect (const std::wstring & pane, POINT clientPx) const;
    std::wstring  GetWindowOf    (const std::wstring & pane) const;
    std::wstring  GetKeyOf       (const std::wstring & pane) const;
    std::wstring  GetActivePane  (const std::wstring & window) const;

    IFloatingPanesHost                                         & m_host;
    std::map<std::wstring, std::unique_ptr<DxuiDockedWindow>>    m_windows;
    std::map<std::wstring, std::vector<std::wstring>>            m_windowPanes;
    std::map<std::wstring, IDxuiControl *>                       m_focus;         // each floating pane's focused control, by pane
    std::map<std::wstring, std::unique_ptr<DxuiTooltip>>         m_tips;
    DxuiDragOverlay                                              m_dragOverlay;
    std::wstring                                                 m_routingPane;
    std::wstring                                                 m_tornOffPane;
    POINT                                                        m_tearGrab    = {};
    bool                                                         m_tearFromTab = false;
    bool                                                         m_isSyncNeeded = false;
};
