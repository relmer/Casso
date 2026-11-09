#pragma once

#include "Pch.h"

#include "../Dxui/MockDxuiTextRenderer.h"
#include "CassoExplorer/Window/IExplorerWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeExplorerWindow
//
//  The window's operations with no window: children are added to a plain
//  panel, and each other call is counted or stored.
//
////////////////////////////////////////////////////////////////////////////////

class FakeExplorerWindow : public IExplorerWindow
{
public:
    HWND                           GetHwnd         () const override                    { return nullptr; }
    void                           Invalidate      () override                          { invalidates++; }
    DxuiHwndSource               * GetPopupHost    () const override                    { return nullptr; }
    IDxuiTextRenderer            * GetTextRenderer () const override                    { return &text; }
    void                           SetTheme        (const IDxuiTheme *) override        { themesSet++; }
    DxuiPanel                    & GetChildPanel   () override                          { return panel; }
    CassoExplorerPrefs::Placement  GetPlacement    () const override                    { return {}; }
    void                           ShowMessage     (const std::wstring & message, UINT) override { messages.push_back (message); }
    void                           ShowAbout       () override                          {}
    void                           SaveSession     () override                          { sessionsSaved++; }
    void                           ArmTick         () override                          {}
    void                           CloseWindow     () override                          { closed = true; }

    DxuiPanel                      panel;
    mutable MockDxuiTextRenderer   text;
    std::vector<std::wstring>      messages;
    int                            invalidates   = 0;
    int                            themesSet     = 0;
    int                            sessionsSaved = 0;
    bool                           closed        = false;
};
