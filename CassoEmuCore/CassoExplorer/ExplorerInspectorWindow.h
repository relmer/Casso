#pragma once

#include "Pch.h"

#include "CassoExplorer/InspectorWindowSet.h"

class DiskInspectorWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorWindow
//
//  A DiskInspectorWindow as InspectorWindowSet drives it: opened over the
//  Explorer window with that window's icon, and on its one disk.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerInspectorWindow : public InspectorWindowSet::Window
{
public:
    ExplorerInspectorWindow  ();
    ~ExplorerInspectorWindow () override;

    HRESULT  Create          (HINSTANCE hInstance, HWND anchor, const DxuiTheme * theme, IDiskInspectorHost * host) override;
    void     Front           () override;
    void     ApplyPrefs      (const DiskInspectorPrefs & prefs) override;
    void     GetPrefs        (DiskInspectorPrefs & inOut) const override;
    void     OpenWalkthrough () override;
    HRESULT  ShowDisk        () override;
    HRESULT  RenderFrame     () override;
    void     ReloadDisk      () override;
    void     NoteFileGone    () override;
    void     ChangeTheme     (const DxuiTheme * theme) override;
    HWND     GetHwnd         () const override;

private:
    std::unique_ptr<DiskInspectorWindow>  m_window;
};
