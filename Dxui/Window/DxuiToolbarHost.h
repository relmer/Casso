#pragma once

#include "Pch.h"
#include "Widgets/DxuiDockSite.h"
#include "Widgets/DxuiToolbar.h"
#include "Widgets/DxuiToolbarDock.h"
#include "Widgets/DxuiToolbarDropPreview.h"
#include "Window/DxuiToolbarDockGroup.h"
#include "Window/DxuiToolbarDragSession.h"
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
//  turning to suit the band a floating drag comes near, and docking where
//  the button comes up over a band.
//
//  The owner hands it the window, the toolbar (a child of that window), and
//  optionally the dock site whose edges the toolbar shares. It calls Layout
//  from its own layout and Sync once a frame. Everything about a drag is the
//  host's own: it takes the owner's mouse and key input ahead of the
//  owner's routing while a drag lasts, so no toolbar needs wiring of its
//  own for any of it. Where the toolbar is goes back to the owner as
//  DxuiToolbarDock text through the save callback, for it to keep.
//
//  A DRAG IS ONE DxuiToolbarDragSession, from the press to the release,
//  with the owner window holding the mouse throughout: the floating window
//  is moved by the host, never by the system's move loop, so tearing off
//  hands nothing over. Where it docks follows the pointer alone, a preview
//  shows it while the button is down, and only the release docks it.
//  Escape puts it back.
//
//  A window with more than one dockable toolbar puts their hosts in one
//  DxuiToolbarDockGroup, which lays them out in bands together: a toolbar
//  dragged over another's band joins it, and one dragged to the boundary
//  between two bands, or to either side of them all, makes a band of its
//  own there. While it is carried, its band is highlighted.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiToolbarHost : private IDxuiToolbarDragSite
{
    friend class DxuiToolbarDockGroup;

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

    DxuiToolbarHost  ();
    ~DxuiToolbarHost () override;

    DxuiToolbarHost             (const DxuiToolbarHost &) = delete;
    DxuiToolbarHost & operator= (const DxuiToolbarHost &) = delete;

    void  Attach (DxuiWindow * owner, DxuiToolbar * toolbar, DxuiDockSite * dockSite, HINSTANCE hInstance);

    //  Lays this host's toolbar out with the other toolbars of `group`,
    //  which outlives it, in place of a group of its own.
    void                    JoinGroup (DxuiToolbarDockGroup & group);
    DxuiToolbarDockGroup &  GetGroup  () const { return *m_group; }

    //  Run when the docked toolbar has moved and the owner must lay out
    //  again, when its place should be saved, for mouse input over the
    //  floating window less the grab handle, on each tick of a system move
    //  or size loop of the floating window in place of the owner's own
    //  modal tick, when a drag of the grab handle starts, once a floating
    //  window has been created (before it is shown), and once the toolbar
    //  has floated or docked.
    void  SetOnLayout        (ClosedFn fn) { m_onLayout       = std::move (fn); }
    void  SetOnSave          (SaveFn fn)   { m_onSave         = std::move (fn); }
    void  SetOnFloatMouse    (MouseFn fn)  { m_onFloatMouse   = std::move (fn); }
    void  SetOnMoveLoopFrame (ClosedFn fn) { m_onMoveFrame    = std::move (fn); }
    void  SetOnDragStart     (ClosedFn fn) { m_onDragStart    = std::move (fn); }
    void  SetOnFloatCreated  (WindowFn fn) { m_onFloatCreated = std::move (fn); }
    void  SetOnFloatChanged  (ClosedFn fn) { m_onFloatChanged = std::move (fn); }

    const DxuiToolbarDock  &  GetDock () const { return m_dock; }
    void                      SetDock (const DxuiToolbarDock & dock) { m_dock = dock; }

    //  A toolbar that runs the whole length of whichever edge it docks to,
    //  such as a strip of pictures with an entry that fills. Torn off, it
    //  keeps the length it had, and its floating window resizes along that
    //  length only; the length is saved with the rest of its place.
    void  SetFillsEdge  (bool fills) { m_fillsEdge = fills; }
    bool  IsFillingEdge () const     { return m_fillsEdge; }

    //  The default place, saved.
    void  ResetDock ();

    //  Whether a lift plays as an animation, in place of the system's
    //  setting, for a test that must see the same thing on every machine.
    void  SetAnimationsEnabled (bool on) { m_animations = on; }

    bool                             IsFloating     () const { return m_float != nullptr; }
    DxuiToolbarWindow             *  GetFloatWindow () const { return m_float.get(); }
    bool                             IsDragging     () const { return m_session.IsActive(); }
    const DxuiToolbarDragSession  &  GetSession     () const { return m_session; }

    //  Lays out the host's group against `area`, in client pixels: docked,
    //  the toolbar takes its place in its band and gives the dock site its
    //  stretch of the edge; floating, gives the dock site its edges back.
    void  Layout (const RECT & area, const RECT & hostClient, const DxuiDpiScaler & scaler);

    //  A press on the grab handle and the drag that follows, in the owner's
    //  client pixels; true while the host takes the event. Attach puts it
    //  ahead of the owner's routing.
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

    //  The dock site's side for a toolbar's edge.
    static DxuiDockSide  EdgeToDockSide (DxuiToolbarDock::Edge edge);

    //  The length a toolbar docked in `bar` keeps when it is torn off, in
    //  DIPs, and the floating window's length in pixels for `dock`: its kept
    //  length for a toolbar that fills its edge, or 0 for the natural length.
    static int  GetTearOffLengthDip (const RECT & bar, bool vertical, int dpi);
    static int  GetFloatLengthPx    (const DxuiToolbarDock & dock, bool fillsEdge, int dpi);

    //  The rectangle a floating window takes when its DPI changes with no
    //  drag under way: where the system suggests, at `sizePx`, the size the
    //  toolbar has at the new scale.
    static RECT  GetDpiChangedRect  (const RECT & suggestedPx, SIZE sizePx);

private:
    //  IDxuiToolbarDragSite
    bool  IsPulledOut     (POINT screenPx) override;
    void  SlideDocked     (POINT screenPx) override;
    bool  TryTearOff      (const RECT & rectPx, bool vertical) override;
    UINT  GetFloatDpi     () override;
    SIZE  GetFloatSizePx  (bool vertical, UINT dpi) override;
    int   GetFloatInsetPx (UINT dpi) override;
    bool  PickVertical    (POINT screenPx, bool current) override;
    void  PlaceFloat      (const RECT & rectPx, bool vertical) override;
    bool  TryPickDrop     (POINT screenPx, int grabAlongDip, DxuiToolbarDock & outDock, bool & outNewBand) override;
    void  ShowDropPreview (bool show, const DxuiToolbarDock & dock, bool isNewBand) override;
    void  DropDocked      (const DxuiToolbarDock & dock, bool isNewBand) override;
    void  LeaveFloating   () override;
    void  PutDown         () override;
    void  Restore         () override;

    bool  BeginLayout      (const RECT & area, const RECT & hostClient, const DxuiDpiScaler & scaler);
    void  PlaceDocked      (const RECT & bar, const RECT & band);
    void  BeginCarry       (POINT clientPx);
    void  BeginFloatDrag   (POINT screenPx);
    void  BeginDrag        ();
    bool  OnDragKey        (WPARAM vk);
    void  OnMouseLost      (UINT message);
    void  Feed             (UINT message, POINT screenPx, bool buttonDown, WPARAM key);
    void  OnFloatDpi       (UINT newDpi, RECT & inOutRectPx);
    void  MoveAcrossBands  (POINT clientPx, DxuiToolbarDock & dock);
    bool  TryPickDropAt    (POINT clientPx, POINT grab, DxuiToolbarDock & outDock, bool & outNewBand);
    void  TakeDrop         (const DxuiToolbarDock & dock, bool isNewBand);
    bool  Float            (const RECT & rectPx);
    void  DockBack         ();
    void  RunMoveLoopFrame ();
    void  UpdateLift       ();
    void  FitFloatWindow   ();
    int   AdoptFloatLength (const RECT & window, UINT dpi);
    int   GetKeptLengthDip () const;
    void  Save             ();
    RECT  GetFloatingRect  (POINT topLeftPx);
    HWND  GetOwnerHwnd     () const;
    bool  IsOwnerShowing   () const;
    POINT ToScreen         (POINT clientPx) const;
    POINT ToClient         (POINT screenPx) const;
    void  ShowMoveCursor   () const;
    static bool  IsLeftButtonDown ();
    int   GetBandDipOfBar  () const { return (m_toolbar != nullptr) ? m_toolbar->GetBandDp() : DxuiToolbar::GetBandDip(); }

    DxuiWindow                          * m_owner     = nullptr;
    DxuiToolbar                         * m_toolbar   = nullptr;
    DxuiDockSite                        * m_dockSite  = nullptr;
    HINSTANCE                             m_hInstance = nullptr;
    DxuiDpiScaler                         m_scaler;
    bool                                  m_fillsEdge = false;
    std::optional<bool>                   m_animations;

    //  The toolbar's place, the region it docks around, and a carry of the
    //  docked toolbar in progress, with where along it the handle was taken.
    DxuiToolbarDock                       m_dock;
    RECT                                  m_area     = {};
    bool                                  m_dragging = false;
    POINT                                 m_grab     = {};

    //  The drag, from press to release, and what it puts back when it is
    //  canceled: every place in the group, and the floating window.
    DxuiToolbarDragSession                m_session { *this };
    std::vector<DxuiToolbarDock>          m_startDocks;
    RECT                                  m_startFloatRect = {};

    //  Where a release would dock a floating toolbar, shown in the owner.
    DxuiToolbarDropPreview                m_preview;
    bool                                  m_previewShown = false;
    RECT                                  m_previewRect  = {};

    std::unique_ptr<DxuiToolbarWindow>    m_float;

    //  The group the toolbar is laid out with: its own until it joins its
    //  window's.
    std::unique_ptr<DxuiToolbarDockGroup>    m_ownGroup = std::make_unique<DxuiToolbarDockGroup>();
    DxuiToolbarDockGroup                   * m_group    = m_ownGroup.get();

    ClosedFn                              m_onLayout;
    SaveFn                                m_onSave;
    MouseFn                               m_onFloatMouse;
    ClosedFn                              m_onMoveFrame;
    ClosedFn                              m_onDragStart;
    WindowFn                              m_onFloatCreated;
    ClosedFn                              m_onFloatChanged;
};
