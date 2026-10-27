#pragma once

#include "Pch.h"

#include "CassoExplorer/ICassoExplorerHost.h"

class CassoLinkConnections;
class FirewallMonitor;
class RunningCassoRegistry;
class ILinkAnnouncements;
struct CassoExplorerSessionWindow;
class CassoExplorerWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabStrip
//
//  The tab strip: its tabs, switching, closing, the tab's menu, and a tab
//  dragged within the strip, to another window's strip, or off into a new
//  window.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerTabStrip
{
public:
    explicit ExplorerTabStrip (CassoExplorerWindow & window);

    //  Opens a folder, a disk image, or a file's folder in a new tab.
    void  OpenPathInNewTab (const std::wstring & path);

    //  A tab dragged from another window of the app: whether the point is
    //  over this window's tab strip, the gap it opens while it is, and the
    //  tab arriving on the drop, at the gap.
    bool  IsOverTabStrip (POINT screen) const;
    void  OnTabHover     (bool over, POINT screen);
    void  AcceptTab      (BrowserModel::Tab tab);
    static constexpr int    kTabTopDip      = 4;
    static constexpr float  kTabLeadDip     = 8.67f;   // the first tab from the strip's left edge, measured at 150%
    static constexpr int    kTabWidthDip    = 240;
    static constexpr int    kTabMinWidthDip = 100;
    void  FillTabs();
    void  SwitchToTab (size_t index);

    //  A tab dragged off the strip: the card that moves with the pointer, the
    //  other window's strip it is over, and where it goes on the drop -- into
    //  that window, or into a window of its own.
    void   OnTabDragMove       (int index, POINT client, bool tornOff);
    void   OnTabDragEnd        (int index, POINT client, bool tornOff);
    void   CancelTabDrag       ();
    void   ShowTabDragVisual   (int index, POINT screen);
    CassoExplorerWindow *  FindTabStripAt (POINT screen) const;
    static HWND            FindWindowBelow (HWND above, POINT screen);
    void   SetTabDragTarget    (CassoExplorerWindow * target, POINT screen);
    void   TearOffTab          (int index, POINT screen);
    RECT   GetTearOffRect      (POINT screen) const;

    //  A tab closed by its button, its menu or Ctrl+W, or moved to another
    //  window. The last tab closes the window, as File Explorer's does.
    void   CloseTabAt          (size_t index);

    //  The tab strip's menu, and the pieces the menus share: one command row,
    //  Copy as path over the selection, and Properties, which is Windows' own
    //  sheet for a host item and the catalog details for an entry in an image.
    void  ShowTabContextMenu     (int x, int y, int index);
    void  OpenInNewWindow     (const std::wstring & path);

private:
    friend class CassoExplorerWindow;

    CassoExplorerWindow  & m_window;

    DxuiTabDragVisual        m_tabDragVisual;
    HWND                     m_tabDragTarget = nullptr;   // the other window whose strip a dragged tab is over
};
