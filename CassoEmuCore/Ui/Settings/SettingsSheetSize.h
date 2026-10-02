#pragma once

#include "Pch.h"


struct GlobalUserPrefs;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsSheetSize
//
//  The Settings sheet's size as the user leaves it, kept in the global prefs
//  in DIPs, and the size it opens at from them.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsSheetSize
{
public:
    // The remembered size, or the design size when there is none, never
    // below the minimum.
    static SIZE  GetInitialSizeDip (const GlobalUserPrefs & prefs, const SIZE & designDip, const SIZE & minDip);

    // Records a size; false when it is the one already recorded.
    static bool  TryStoreSizeDip   (GlobalUserPrefs & prefs, const SIZE & sizeDip);

    // A size in pixels at one DPI as DIPs, and back.
    static SIZE  PxToDip           (const SIZE & sizePx, UINT dpi);
    static SIZE  DipToPx           (const SIZE & sizeDip, UINT dpi);
};