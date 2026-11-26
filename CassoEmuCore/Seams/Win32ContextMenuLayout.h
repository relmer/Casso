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
    bool  showMore           = false;   // Show more options at the bottom
    bool  showExtensions     = true;    // the App extensions submenu
    bool  showCustomize      = true;    // Customize menu at the bottom
    bool  inlinePrimary      = false;   // cut, copy, paste, rename, share and delete as rows, not icons
    bool  propertiesAtBottom = false;   // Properties just above the bottom section
    bool  cloudSection       = true;    // a sync client's commands in a separate section
    bool  copyAsPath         = true;    // the file actions
    bool  sendTo             = false;
    bool  createShortcut     = false;
    bool  print              = false;
    bool  rotate             = false;

    //  Returns the page's choices from the registry.
    static Win32ContextMenuLayout  Read ();

    //  Returns whether the page puts the app's commands in the main menu
    //  rather than in the App extensions submenu.
    static bool  IsExtensionInMainMenu (const std::wstring & packageFamily);

private:
    static bool  ReadSetting (const wchar_t * setting, bool fallback);
    static bool  ReadEnabled (const std::wstring & key, bool & outEnabled);
};
