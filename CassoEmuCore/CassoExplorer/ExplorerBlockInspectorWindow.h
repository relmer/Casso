#pragma once

#include "Pch.h"

#include "CassoExplorer/InspectorWindowSet.h"

class BlockInspectorWindow;
class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerBlockInspectorWindow
//
//  A BlockInspectorWindow as InspectorWindowSet drives it, for an image file
//  of blocks: opened over the Explorer window with that window's icon. It
//  has no walkthrough and no saved state, and Compare is not offered on it.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerBlockInspectorWindow : public InspectorWindowSet::Window
{
public:
    ExplorerBlockInspectorWindow  (const std::wstring & path, IDiskFileIo & fileIo);
    ~ExplorerBlockInspectorWindow () override;

    HRESULT  Create          (HINSTANCE hInstance, HWND anchor, const DxuiTheme * theme, IDiskInspectorHost * host) override;
    void     Front           () override;
    void     ApplyPrefs      (const DiskInspectorPrefs & prefs) override { (void) prefs; }
    void     GetPrefs        (DiskInspectorPrefs & inOut) const override { (void) inOut; }
    void     OpenWalkthrough () override {}
    HRESULT  ShowDisk        () override { return S_OK; }
    HRESULT  RenderFrame     () override;
    void     ReloadDisk      () override;
    void     NoteFileGone    () override;
    void     CompareWith     (const std::wstring & pathB) override { (void) pathB; }
    void     ChangeTheme     (const DxuiTheme * theme) override;
    HWND     GetHwnd         () const override;

private:
    std::wstring                           m_path;
    IDiskFileIo                          & m_fileIo;
    std::unique_ptr<BlockInspectorWindow>  m_window;
};
