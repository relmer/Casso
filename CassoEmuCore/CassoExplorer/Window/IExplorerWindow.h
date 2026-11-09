#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CassoExplorerPrefs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IExplorerWindow
//
//  The Casso Explorer window's operations its components call.
//
////////////////////////////////////////////////////////////////////////////////

class IExplorerWindow
{
public:
    virtual ~IExplorerWindow() = default;

    virtual HWND                           GetHwnd         () const                            = 0;
    virtual void                           Invalidate      ()                                  = 0;
    virtual DxuiHwndSource               * GetPopupHost    () const                            = 0;
    virtual IDxuiTextRenderer            * GetTextRenderer () const                            = 0;
    virtual void                           SetTheme        (const IDxuiTheme * theme)          = 0;
    virtual DxuiPanel                    & GetChildPanel   ()                                  = 0;
    virtual CassoExplorerPrefs::Placement  GetPlacement    () const                            = 0;
    virtual void                           ShowMessage     (const std::wstring & text, UINT icon) = 0;
    virtual void                           ShowAbout       ()                                  = 0;
    virtual void                           SaveSession     ()                                  = 0;
    virtual void                           ArmTick         ()                                  = 0;
    virtual void                           CloseWindow     ()                                  = 0;
    template <typename T, typename... Args>
    T *  CreateChild (Args &&... args) { return GetChildPanel().template CreateChild<T> (std::forward<Args> (args)...); }
};
