#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiDisplay
//
//  The monitors, their DPI and the window under a point: the queries that
//  code makes about the desktop. Each method has its Win32 call's name.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiDisplay
{
public:
    virtual ~IDxuiDisplay() = default;

    virtual HMONITOR  MonitorFromPoint           (POINT pointPx, DWORD flags)                          = 0;
    virtual HMONITOR  MonitorFromRect            (LPCRECT rectPx, DWORD flags)                         = 0;
    virtual HMONITOR  MonitorFromWindow          (HWND hwnd, DWORD flags)                              = 0;
    virtual BOOL      GetMonitorInfoW            (HMONITOR monitor, LPMONITORINFO info)                = 0;
    virtual BOOL      EnumDisplayMonitors        (HDC dc, LPCRECT clip, MONITORENUMPROC proc, LPARAM param) = 0;
    virtual UINT      GetDpiForSystem            ()                                                    = 0;
    virtual UINT      GetDpiForWindow            (HWND hwnd)                                           = 0;
    virtual HWND      WindowFromPoint            (POINT pointPx)                                       = 0;
    virtual BOOL      SystemParametersInfoForDpi (UINT action, UINT param, PVOID pvParam, UINT winIni, UINT dpi) = 0;
    virtual int       GetSystemMetricsForDpi     (int index, UINT dpi)                                 = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDisplay
//
//  The display the code queries: the system's until a test sets one, so a
//  unit test lays out and places windows on a display it defines and doesn't
//  read the desktop's.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDisplay
{
public:
    static IDxuiDisplay &  Get ();

    //  Null restores the system's.
    static void            Set (IDxuiDisplay * display);

private:
    class SystemDisplay;

    static inline IDxuiDisplay  * s_display = nullptr;
};
