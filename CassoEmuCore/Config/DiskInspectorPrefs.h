#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorPrefs
//
//  The disk inspector window's saved state (FR-080): its placement and
//  size, its mode, tabs and timing range, the overlays and "Show deleted
//  files", Follow head and its zoom, and in Casso the last drive and
//  whether the window was open at exit. A width of zero indicates the
//  state was never saved.
//
////////////////////////////////////////////////////////////////////////////////

struct DiskInspectorPrefs
{
    int     x                 = 0;
    int     y                 = 0;
    int     w                 = 0;
    int     h                 = 0;
    bool    isMaximized       = false;
    bool    isTimingMode      = false;
    bool    isDiskGroup       = false;
    int     trackTab          = 0;
    int     diskTab           = 0;
    double  timingRange       = 0.05;
    bool    isAlignment       = false;
    bool    isFiles           = false;
    bool    isReads           = false;
    bool    isShowDeleted     = false;
    bool    isFollowHead      = false;
    double  zoom              = 1.0;
    double  tilt              = 0.0;
    int     lastDrive         = 0;
    bool    isOpen            = false;
    bool    isWalkthroughSeen = false;
};
