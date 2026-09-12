#pragma once

#include "Pch.h"
#include "Theme/DxuiTheme.h"
#include "Theme/DxuiWindowsThemeColors.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDarkTheme
//
//  The Windows 11 dark palette, for an application that follows the system
//  rather than carrying a look of its own.
//
//  The surfaces and text come from the Fluent solid-fill and text-fill
//  tokens, flattened to opaque colors over the base fill, since Dxui paints
//  each surface once rather than stacking translucent layers. The caption
//  buttons take the same tokens DxuiWindowsThemeColors reads, so the chrome
//  matches the system's own windows beside it.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiDarkTheme : public DxuiTheme
{
    DxuiDarkTheme();

    //  Overwrites the values Windows publishes -- the list surface and the
    //  accent -- keeping the built-in ones for whatever it does not. Not done
    //  in the constructor, so a theme built in a test does not depend on the
    //  machine it runs on.
    void  ApplySystemColors (const DxuiWindowsThemeColors::SystemColors & colors);
};
