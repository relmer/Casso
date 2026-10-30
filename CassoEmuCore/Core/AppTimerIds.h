#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppTimerId
//
//  Every timer the app sets, one value apiece, so no two collide on a
//  window they share. Dxui sets 1, 2, 0xDCE1 and 0xDCE2 on the windows it
//  creates, so none of those is used here.
//
////////////////////////////////////////////////////////////////////////////////

enum class AppTimerId : UINT_PTR
{
    ExplorerTooltip      = 0x5153,
    ExplorerFolder       = 0x5154,
    ExplorerRestore      = 0x5155,
    ExplorerRecycleBin   = 0x5156,
    ExplorerDrive        = 0x5157,
    ExplorerShellPreview = 0x5158,
    ExplorerAutoPreview  = 0x5259,
    ExplorerRefresh      = 0x5159,
    ExplorerUpdateIndicator = 0x515A,
    PreviewHelperOwner   = 0x5160,
    ControllerRescanSoon = 0x5161,
    ControllerRescanLate = 0x5162,
    ShellPrefsSave       = 0xCA55,
    ShellSharedCheck     = 0xCA56,
};