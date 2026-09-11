#pragma once

#include "Pch.h"

#include "Cassque/CassqueBrowser.h"
#include "Cassque/CassqueCommands.h"
#include "Cassque/Model/CassquePrefs.h"
#include "Theme/DxuiDarkTheme.h"
#include "Theme/DxuiLightTheme.h"
#include "Widgets/DxuiFramebufferView.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiMenuBar.h"
#include "Widgets/DxuiSplitter.h"
#include "Widgets/DxuiStatusBar.h"
#include "Widgets/DxuiTreeView.h"
#include "Window/DxuiWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow
//
//  The browser's top-level window: menu bar, tree, splitter, file list,
//  splitter, preview, status bar.
//
//  EVERY BRANCH FORWARDS. A click or key is turned into a call on the browser
//  controller or a command id, and the widgets are then refilled from what
//  the controller holds. What to list, what to preview and what the status
//  says are all decided there, where tests reach them.
//
//  Keyboard focus is one of the three panes, moved by Tab and by a press.
//  The window is laid out by hand: the dock layout sizes a slab from the
//  child's current bounds, which a splitter changes on every drag.
//
////////////////////////////////////////////////////////////////////////////////

class CassqueWindow : public DxuiWindow
{
public:
    CassqueWindow (CassqueBrowser & browser, CassquePrefs & prefs);
    ~CassqueWindow() override;

    HRESULT  Open (HINSTANCE instance, const std::wstring & title, int showCommand);

    //  The window's placement in the preferences' terms, for saving on exit.
    void  StorePlacement();

    void    Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void    Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool    OnMouse           (const DxuiMouseEvent & ev) override;
    bool    OnKey             (const DxuiKeyEvent & ev) override;
    LPCWSTR GetCursorForPoint (POINT clientPx) const override;

    static constexpr int  kMinTreeWidthDip    = 140;
    static constexpr int  kMinListWidthDip    = 220;
    static constexpr int  kMinPreviewWidthDip = 160;

protected:
    void  OnCreate        () override;
    void  OnWindowClose   () override;

private:
    enum class Pane { Tree, List, Preview };

    void  ConfigureWidgets();
    void  ApplyTheme();
    void  RecomputeLayout();
    void  FillList();
    void  FillPreview();
    void  FillStatus();
    void  SetFocusPane (Pane pane);
    void  Dispatch     (int id);
    bool  IsEnabled    (int id) const;
    bool  IsChecked    (int id) const;
    void  ShowAbout();

    static bool  Contains (const RECT & rect, POINT point);
    static DxuiMouseEvent  ToLocal (const DxuiMouseEvent & ev, const RECT & bounds);

    CassqueBrowser         & m_browser;
    CassquePrefs           & m_prefs;
    CassqueCommands          m_commands;
    DxuiLightTheme           m_lightTheme;
    DxuiDarkTheme            m_darkTheme;
    const DxuiTheme        * m_theme          = nullptr;
    DxuiDpiScaler            m_scaler;
    RECT                     m_client         = {};
    Pane                     m_focus          = Pane::Tree;

    DxuiMenuBar          * m_menuBar         = nullptr;
    DxuiTreeView         * m_tree            = nullptr;
    DxuiSplitter         * m_treeSplitter    = nullptr;
    DxuiListView         * m_list            = nullptr;
    DxuiLabel            * m_listMessage     = nullptr;
    DxuiSplitter         * m_previewSplitter = nullptr;
    DxuiListView         * m_previewList     = nullptr;
    DxuiFramebufferView  * m_picture         = nullptr;
    DxuiLabel            * m_previewMessage  = nullptr;
    DxuiStatusBar        * m_status          = nullptr;
};
