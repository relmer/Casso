#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerCommands.h"
#include "CassoExplorer/CassoExplorerNamedControl.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"
#include "CassoExplorer/ICassoExplorerHost.h"
#include "CassoExplorer/Model/FocusRing.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "Config/IFileSystem.h"
#include "Seams/Win32HostDialogs.h"
#include "Seams/Win32IntentChannel.h"
#include "Seams/Win32ProcessLauncher.h"
#include "CassoExplorer/CrtPictureRenderer.h"
#include "Seams/Win32ContentDetails.h"
#include "Ui/Debugger/SourceSyntax.h"
#include "Ui/ThemeLoader.h"
#include "Config/CrtResolver.h"
#include "Config/CrtPresets.h"
#include "Seams/Win32ShellIcons.h"
#include "Seams/Win32InfoTips.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "Seams/Win32PackagedCommands.h"
#include "Ui/Chrome/CassoTheme.h"
#include "CassoExplorer/Model/FolderWatch.h"
#include "CassoExplorer/Model/RefreshAnchor.h"

class CassoLinkConnections;
class FirewallMonitor;
class RunningCassoRegistry;
class ILinkAnnouncements;
struct CassoExplorerSessionWindow;
class CassoExplorerWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInput
//
//  Pointer and keyboard input: each press, move and key routed to the
//  pane, toolbar or widget under it, and keyboard focus across the panes
//  and the toolbars' buttons.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerInput
{
public:
    explicit ExplorerInput (CassoExplorerWindow & window);

    bool    OnMouse           (const DxuiMouseEvent & ev);
    bool    OnKey             (const DxuiKeyEvent & ev);
    LPCWSTR GetCursorForPoint (POINT clientPx) const;

    static DxuiToolbar &  GetToolbarUnder (const RECT & commandBarBand, POINT point, DxuiToolbar & navToolbar, DxuiToolbar & commandBar);

    //  Keyboard focus. The toolbar is one pane, with its focused button in
    //  m_toolbarFocus; FocusRing defines the Tab order.
    enum class Pane { Toolbar, Address, Tabs, Tree, List, PreviewToolbar, GoTo, Search, Preview, CommandBar, LocationSearch };
    void  SetFocusPane (Pane pane);

    FocusStop               GetFocusStop     () const;
    void                    SetFocusStop     (const FocusStop & stop);
    std::vector<FocusStop>  BuildFocusStops  () const;
    bool                    RouteToolbarKey  (bool preview, const DxuiKeyEvent & ev);
    void                    StepToolbarFocus (bool preview, bool forward);

    //  The focused pane's control, where routing of a standard command such as
    //  Copy or Select all starts.
    IDxuiControl *  GetFocusedControl() const;
    bool  RouteToolbarMouse   (DxuiToolbar & toolbar, const DxuiMouseEvent & ev);
    bool  RouteCommandBarKey      (const DxuiKeyEvent & ev);

    //  A press on a drive's node in the tree, which drags the whole drive once
    //  the pointer moves far enough.
    struct TreeDrag
    {
        bool      armed    = false;
        POINT     start    = {};
        Location  location;
    };
    struct RightPress
    {
        bool   active  = false;
        bool   canDrag = false;   // pressed on a row
        int    group   = -1;      // pressed on a group's header
        POINT  start   = {};
    };

private:
    friend class CassoExplorerWindow;

    CassoExplorerWindow  & m_window;

    POINT                                     m_dragStart           = {};

    TreeDrag  m_treeDrag;
    int       m_toolbarFocus    = 0;
    int       m_commandBarFocus = 0;

    RightPress            m_rightPress;
    bool                  m_tipMuted    = false;
    RECT                  m_tipMuteRect = {};
};
