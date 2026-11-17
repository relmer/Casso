#include "Pch.h"

#include "Ui/DiskInspector/InspectorPlacement.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorPlacement::Get
//
//  The restored position, so a maximized window comes back to the size it
//  had before.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorPlacement::Get (HWND hwnd, DiskInspectorPrefs & inOut)
{
    WINDOWPLACEMENT  placement = { sizeof (WINDOWPLACEMENT) };



    if (hwnd != nullptr && GetWindowPlacement (hwnd, &placement))
    {
        inOut.x           = placement.rcNormalPosition.left;
        inOut.y           = placement.rcNormalPosition.top;
        inOut.w           = placement.rcNormalPosition.right - placement.rcNormalPosition.left;
        inOut.h           = placement.rcNormalPosition.bottom - placement.rcNormalPosition.top;
        inOut.isMaximized = placement.showCmd == SW_SHOWMAXIMIZED || IsZoomed (hwnd);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorPlacement::Apply
//
//  With isPositioned, the saved position is used while it is on a monitor;
//  otherwise, and for a position on a monitor since removed, the window
//  stays where Windows placed it. A positioned window is set twice: a move
//  onto a monitor of another DPI rescales the window, so the second call
//  sets the saved size at that monitor's DPI.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorPlacement::Apply (HWND hwnd, const DiskInspectorPrefs & prefs, bool isPositioned)
{
    WINDOWPLACEMENT  placement  = { sizeof (WINDOWPLACEMENT) };
    RECT             saved      = { prefs.x, prefs.y, prefs.x + prefs.w, prefs.y + prefs.h };
    bool             isOnScreen = isPositioned && MonitorFromRect (&saved, MONITOR_DEFAULTTONULL) != nullptr;



    if (hwnd != nullptr && prefs.w > 0 && prefs.h > 0 && GetWindowPlacement (hwnd, &placement))
    {
        if (isOnScreen)
        {
            placement.rcNormalPosition = saved;
        }
        else
        {
            placement.rcNormalPosition.right  = placement.rcNormalPosition.left + prefs.w;
            placement.rcNormalPosition.bottom = placement.rcNormalPosition.top + prefs.h;
        }

        placement.showCmd = prefs.isMaximized ? SW_SHOWMAXIMIZED : (IsWindowVisible (hwnd) ? SW_SHOWNOACTIVATE : SW_HIDE);
        SetWindowPlacement (hwnd, &placement);

        if (isOnScreen)
        {
            SetWindowPlacement (hwnd, &placement);
        }
    }
}
