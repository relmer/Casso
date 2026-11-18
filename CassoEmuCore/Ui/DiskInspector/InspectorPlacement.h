#pragma once

#include "Pch.h"

#include "Config/DiskInspectorPrefs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorPlacement
//
//  An inspector window's saved placement: its restored position and size, in
//  the workspace coordinates SetWindowPlacement takes back, and whether it
//  was maximized. The first inspector window opened takes the saved
//  position; the others open where Windows places them, with the saved size.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorPlacement
{
public:
    static void  Get   (HWND hwnd, DiskInspectorPrefs & inOut);
    static void  Apply (HWND hwnd, const DiskInspectorPrefs & prefs, bool isPositioned);
};
