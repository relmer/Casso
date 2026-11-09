#pragma once

#include "Pch.h"

#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorPrefs
//
//  The disk inspector window's saved state (FR-080): its placement and
//  size, its mode and timing range, the overlays and "Show deleted
//  files", Follow head and its zoom, the last kind of "Go to", and in Casso the last drive and
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
    double  timingRange       = 0.05;
    bool    isAlignment       = false;
    bool    isFiles           = false;
    bool    isReads           = false;
    bool    isWrites          = false;
    bool    isScans           = false;
    bool    isShowDeleted     = false;
    bool    isFollowHead      = false;
    double  zoom              = 1.0;
    double  tilt              = 0.0;
    int     lastDrive         = 0;
    bool    isOpen            = false;
    bool    isWalkthroughSeen = false;
    int     goToKind          = 0;

    //  Whether the window was ever opened, so there is state to write.
    bool       IsSaved  () const { return w > 0 || isWalkthroughSeen; }
    JsonValue  ToJson   (bool hasDriveState) const;
    void       FromJson (const JsonValue & obj);

private:
    static void  ReadBool   (const JsonValue & obj, const char * key, bool   & inOut);
    static void  ReadInt    (const JsonValue & obj, const char * key, int    & inOut);
    static void  ReadNumber (const JsonValue & obj, const char * key, double & inOut);
};
