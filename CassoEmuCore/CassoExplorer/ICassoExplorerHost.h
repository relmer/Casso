#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/BrowserModel.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"


class CassoExplorerWindow;
struct DxuiTheme;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindowRequest
//
//  A window to open. With no tabs it opens on Home, and on `openPath` when
//  there is one; with tabs, on those, `activeTab` active, and `openPath` in
//  a tab after them. It goes where `screenRect` is when that is set, a
//  torn-off tab's window; else where `placement` is when that is valid,
//  stepped down and right with `cascade`; else at the default size. `owner`
//  is the emulator window it serves, if any.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoExplorerWindowRequest
{
    std::vector<BrowserModel::Tab>  tabs;
    size_t                          activeTab     = 0;
    std::wstring                    openPath;
    CassoExplorerPrefs::Placement   placement;
    bool                            cascade       = false;
    RECT                            screenRect    = {};
    bool                            hasScreenRect = false;
    HWND                            owner         = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ICassoExplorerHost
//
//  The app that owns every Casso Explorer window in the process. A window
//  calls it to open another window, to report that it closed, and to find
//  the other windows, to drag a tab to one.
//
////////////////////////////////////////////////////////////////////////////////

class ICassoExplorerHost
{
public:
    virtual ~ICassoExplorerHost() = default;

    virtual CassoExplorerWindow *  OpenWindow   (CassoExplorerWindowRequest request) = 0;

    //  The window that is the top-level window `hwnd`, or null.
    virtual CassoExplorerWindow *  GetWindowFor (HWND hwnd) = 0;

    virtual void  ForEachWindow (const std::function<void (CassoExplorerWindow & window)> & fn) = 0;

    //  `window` closed. The session is saved first: with it when it is the
    //  last window, without it otherwise.
    virtual void  CloseWindow   (CassoExplorerWindow & window) = 0;

    //  Every window is saved in the session and closed, as File > Exit and
    //  Windows ending the session do.
    virtual void  ExitAll       () = 0;
    virtual void  SaveSession   () = 0;

    //  Another launch of Casso Explorer, forwarded here with its arguments.
    virtual void  RunLaunch     (const std::wstring & arguments) = 0;

    //  Help > Check for updates, from the window `hwnd`.
    virtual void  CheckForUpdates (HWND hwnd) = 0;

    //  True when the update indicator in the window `hwnd` took the event.
    virtual bool  OfferMouseToUpdater (HWND hwnd, const DxuiMouseEvent & ev) = 0;

    //  A disk inspector window for each image, opened over the window `hwnd`
    //  in `theme`, or an image's window already open brought to the front
    //  (FR-073).
    virtual void  InspectDiskImages (HWND hwnd, const std::vector<std::wstring> & paths, const DxuiTheme & theme) { (void) hwnd; (void) paths; (void) theme; }

    //  One disk inspector window comparing image A with image B: A's window
    //  when it is open, a new one otherwise (FR-117).
    virtual void  CompareDiskImages (HWND hwnd, const std::wstring & pathA, const std::wstring & pathB, const DxuiTheme & theme) { (void) hwnd; (void) pathA; (void) pathB; (void) theme; }

    //  The theme the windows changed to, which the inspector windows follow
    //  (FR-078).
    virtual void  SetInspectorTheme (const DxuiTheme & theme) { (void) theme; }

    //  Called by a window when a disk image has been booted, or couldn't be
    //  for a machine's missing ROMs: those ROMs are offered once in a run,
    //  in a dialog on the window.
    virtual void  OfferBootRoms (HWND hwnd, const DxuiTheme * theme) { (void) hwnd; (void) theme; }
};
