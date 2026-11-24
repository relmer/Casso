#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ContextMenuLayout
//
//  The choices on Settings' Context menu page that Casso Explorer's menus
//  follow. The defaults are File Explorer's when the page is at its defaults.
//
////////////////////////////////////////////////////////////////////////////////

struct Win32ContextMenuLayout
{
    bool  showMore       = false;   // Show more options at the bottom
    bool  showExtensions = true;    // the App extensions submenu
    bool  showCustomize  = true;    // Customize menu at the bottom

    //  Returns the page's choices from the registry.
    static Win32ContextMenuLayout  Read ();

private:
    static bool  ReadSetting (const wchar_t * setting, bool fallback);
};
