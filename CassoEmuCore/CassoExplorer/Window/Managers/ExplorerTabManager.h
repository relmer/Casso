#pragma once

#include "Pch.h"

#include "CassoExplorer/ICassoExplorerHost.h"
struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTabManager
//
//  The tab strip: its tabs, switching, closing, the tab's menu, and a tab
//  dragged within the strip, to another window's strip, or off into a new
//  window. The window routes pointer and key events over the strip here.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerTabManager
{
public:
    explicit ExplorerTabManager (ExplorerContext & context);

    static constexpr int    kTabWidthDip    = 240;
    static constexpr int    kTabMinWidthDip = 100;

    void                   Create             ();
    void                   Configure          ();
    void                   ApplyTheme         (const DxuiTheme & theme);
    void                   Layout             (const RECT & rect);
    void                   ClearHoverOnLeave  (const DxuiMouseEvent & ev);
    IDxuiControl         * GetControl         () const { return m_tabs; }
    RECT                   GetBounds          () const { return m_tabs->GetBounds(); }
    int                    HitTest            (POINT client) const;
    bool                   IsDragging         () const { return m_tabs->IsDragging(); }
    bool                   IsSliding          () const { return m_tabs->IsSliding(); }
    void                   OpenPathInNewTab   (const std::wstring & path);
    bool                   IsOverTabStrip     (POINT screen) const;
    void                   AcceptTab          (BrowserModel::Tab tab);
    void                   FillTabs           ();
    void                   SwitchToTab        (size_t index);
    void                   CancelTabDrag      ();
    void                   ShowTabDragVisual  (int index, POINT screen);
    CassoExplorerWindow  * FindTabStripAt     (POINT screen) const;
    static HWND            FindWindowBelow    (HWND above, POINT screen);
    void                   SetTabDragTarget   (CassoExplorerWindow * target, POINT screen);
    void                   TearOffTab         (int index, POINT screen);
    RECT                   GetTearOffRect     (POINT screen) const;
    void                   CloseTabAt         (size_t index);
    void                   ShowTabContextMenu (int x, int y, int index);
    void                   OpenInNewWindow    (const std::wstring & path);

    bool  HandleCapturedMouse (const DxuiMouseEvent & ev);
    bool  HandleMouse         (const DxuiMouseEvent & ev);
    bool  HandleEscapeKey     (const DxuiKeyEvent & ev);
    bool  HandleKey           (const DxuiKeyEvent & ev) { return m_tabs->OnKey (ev); }
    void  HandleFocusChanged  (bool focused) { m_tabs->OnFocusChanged (focused); }
    void  HandleTabHover      (bool over, POINT screen);
    void  HandleTabDragMove   (int index, POINT client, bool tornOff);
    void  HandleTabDragEnd    (int index, POINT client, bool tornOff);

private:
    ExplorerContext    & m_context;
    DxuiTabStrip       * m_tabs          = nullptr;
    DxuiTabDragVisual    m_tabDragVisual;
    HWND                 m_tabDragTarget = nullptr;   // the other window whose strip a dragged tab is over
};
