#pragma once

#include "Pch.h"
#include "Widgets/DxuiDockSite.h"
#include "Widgets/DxuiToolbar.h"
#include "Widgets/DxuiToolbarDock.h"
#include "Window/DxuiToolbarWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarHost
//
//  Makes one toolbar of a DxuiWindow dockable, as Visual Studio's toolbars
//  are: docked to any edge of a region of the window at any place along it,
//  sharing that edge with a dock site's auto-hide tabs; standing on end down
//  a side with its icons upright; torn off to float in a DxuiToolbarWindow;
//  and carried by its grab handle, sliding along its band until pulled away,
//  turning to suit the band a floating drag comes near, and snapping into a
//  band while the button is still down.
//
//  The owner hands it the window, the toolbar (a child of that window), and
//  optionally the dock site whose edges the toolbar shares. It calls Layout
//  from its own layout, RouteDrag first from its mouse routing for the
//  toolbar, and Sync once a frame. Where the toolbar is goes back to the
//  owner as DxuiToolbarDock text through the save callback, for it to keep.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiToolbarHost
{
public:
    using  ClosedFn = std::function<void ()>;
    using  MouseFn  = std::function<bool (const DxuiMouseEvent & ev)>;
    using  WindowFn = std::function<void (DxuiToolbarWindow & window)>;
    using  SaveFn   = std::function<void (const std::wstring & text)>;

    //  How near an edge, past the toolbar's own thickness, a drag has to
    //  stay to dock there rather than float, how far past its band a drag
    //  of the docked toolbar has to pull before it tears off, and the margin
    //  the toolbar keeps from the ends of the top and bottom edges.
    static constexpr int  kDockReachDp = 24;
    static constexpr int  kPullDp      = 32;
    static constexpr int  kMarginDp    = 8;

    void  Attach (DxuiWindow * owner, DxuiToolbar * toolbar, DxuiDockSite * dockSite, HINSTANCE hInstance);

    //  Run when the docked toolbar has moved and the owner must lay out
    //  again, when its place should be saved, for mouse input over the
    //  floating window less the grab handle, on each tick of a floating
    //  drag's move loop, when a drag of the grab handle starts, once a
    //  floating window has been created (before it is shown), and once the
    //  toolbar has floated or docked.
    void  SetOnLayout        (ClosedFn fn) { m_onLayout       = std::move (fn); }
    void  SetOnSave          (SaveFn fn)   { m_onSave         = std::move (fn); }
    void  SetOnFloatMouse    (MouseFn fn)  { m_onFloatMouse   = std::move (fn); }
    void  SetOnMoveLoopFrame (ClosedFn fn) { m_onMoveFrame    = std::move (fn); }
    void  SetOnDragStart     (ClosedFn fn) { m_onDragStart    = std::move (fn); }
    void  SetOnFloatCreated  (WindowFn fn) { m_onFloatCreated = std::move (fn); }
    void  SetOnFloatChanged  (ClosedFn fn) { m_onFloatChanged = std::move (fn); }

    const DxuiToolbarDock  &  GetDock () const { return m_dock; }
    void                      SetDock (const DxuiToolbarDock & dock) { m_dock = dock; }

    //  The default place, saved.
    void  ResetDock ();

    bool                  IsFloating     () const { return m_float != nullptr; }
    DxuiToolbarWindow  *  GetFloatWindow () const { return m_float.get(); }
    bool                  IsDragging     () const { return m_dragging; }

    //  Docked, places the toolbar against its edge of `area`, in client
    //  pixels, and gives the dock site that stretch of the edge; floating,
    //  gives the dock site its edges back.
    void  Layout (const RECT & area, const RECT & hostClient, const DxuiDpiScaler & scaler);

    //  A press on the grab handle and the drag that follows; true while the
    //  host takes the event.
    bool  RouteDrag (const DxuiMouseEvent & ev);

    //  Once a frame: a floating window while the place is floating and none
    //  otherwise, sized to the toolbar and shown while the owner is.
    void  Sync ();

    void  Hide ();

    //  The docked toolbar's rectangle for `dock` against `area`, `lengthPx`
    //  long at most and `bandPx` thick, `marginPx` in from the ends of the
    //  top and bottom edges.
    static RECT  GetDockedRect (const DxuiToolbarDock & dock, const RECT & area, int lengthPx, int bandPx, int marginPx, int dpi);

    //  How much of its edge strip the docked toolbar takes from the dock
    //  site: the whole band across the top, the band less the margin the
    //  site keeps from the window's other edges.
    static long  GetEdgeShareThickness (DxuiToolbarDock::Edge edge, int bandPx, int marginPx);

    //  The floating window's top left for a toolbar torn off at `screenPx`,
    //  with its grab handle centered under the pointer.
    static POINT  GetTearOffTopLeft (POINT screenPx, bool vertical, int gripPx, int bandPx);

    //  The dock site's side for a toolbar's edge.
    static DxuiDockSide  EdgeToDockSide (DxuiToolbarDock::Edge edge);

private:
    void  TearOff          (POINT clientPx);
    void  Float            ();
    void  DockBack         ();
    void  OnFloatDrag      (POINT screenPx);
    void  OnFloatDragEnd   (POINT screenPx);
    void  FinishSnap       ();
    void  ResumeSnapDrag   ();
    void  FitFloatWindow   ();
    void  Save             ();
    RECT  GetFloatingRect  (POINT topLeftPx);
    HWND  GetOwnerHwnd     () const;

    DxuiWindow                          * m_owner     = nullptr;
    DxuiToolbar                         * m_toolbar   = nullptr;
    DxuiDockSite                        * m_dockSite  = nullptr;
    HINSTANCE                             m_hInstance = nullptr;
    DxuiDpiScaler                         m_scaler;

    //  The toolbar's place, and a drag of its grab handle in progress, with
    //  where in the toolbar the handle was taken and the region it docks
    //  around.
    DxuiToolbarDock                       m_dock;
    RECT                                  m_area     = {};
    bool                                  m_dragging = false;
    POINT                                 m_grab     = {};

    //  A floating toolbar dragged into a band snaps into it: the place it
    //  takes once the move loop has ended, and, until the frame docks it,
    //  whether the drag goes on in the owner.
    DxuiToolbarDock                       m_snapDock;
    bool                                  m_snapping   = false;
    bool                                  m_snapDragOn = false;

    std::unique_ptr<DxuiToolbarWindow>    m_float;

    ClosedFn                              m_onLayout;
    SaveFn                                m_onSave;
    MouseFn                               m_onFloatMouse;
    ClosedFn                              m_onMoveFrame;
    ClosedFn                              m_onDragStart;
    WindowFn                              m_onFloatCreated;
    ClosedFn                              m_onFloatChanged;
};
