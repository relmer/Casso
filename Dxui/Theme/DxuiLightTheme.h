#pragma once

#include "Pch.h"
#include "Theme/DxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiLightTheme
//
//  The Windows 11 light palette, for an application that follows the system
//  rather than carrying a look of its own.
//
//  The surfaces and text come from the Fluent solid-fill and text-fill
//  tokens, flattened to opaque colors over the base fill, since Dxui paints
//  each surface once rather than stacking translucent layers. The caption
//  buttons take the same tokens DxuiWindowsThemeColors reads, so the chrome
//  matches the system's own windows beside it.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiLightTheme : public DxuiTheme
{
    DxuiLightTheme();
};
