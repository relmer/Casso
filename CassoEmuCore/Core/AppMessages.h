#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppMessage
//
//  Every private window message the app posts, in one enum so that no two
//  have the same value. A message posted between threads is handled by the
//  window it is posted to.
//
////////////////////////////////////////////////////////////////////////////////

enum class AppMessage : UINT
{
    ShellUpdateTitle       = WM_APP + 0x21,   // a window-title refresh, from the CPU thread to the UI thread
    ShellNotifyUser        = WM_APP + 0x22,
    ShellReportDamage      = WM_APP + 0x23,
    ShellMountCompleted    = WM_APP + 0x25,
    ShellChangeReport      = WM_APP + 0x26,
    ShellChangeAsk         = WM_APP + 0x27,
    ShellShowNotice        = WM_APP + 0x28,
    ShellGameportFlush     = WM_APP + 0x29,
    ShellControllerPick    = WM_APP + 0x2A,
    ShellUpdateResult      = WM_APP + 0x2B,
    ShellIntentReply       = WM_APP + 0x2C,

    ExplorerReply          = WM_APP + 0x31,
    ExplorerFolderChanged  = WM_APP + 0x32,
    ExplorerRunCommand     = WM_APP + 0x33,
    ExplorerDropMenu       = WM_APP + 0x34,
    ExplorerIconsLoaded    = WM_APP + 0x35,
    ExplorerInfoTip        = WM_APP + 0x36,
    ExplorerRecycleBin     = WM_APP + 0x37,
    ExplorerShellListed    = WM_APP + 0x38,
    ExplorerContentDetails = WM_APP + 0x39,
    ExplorerShellPreview   = WM_APP + 0x3A,
    PreviewHelperReady     = WM_APP + 0x3B,   // the preview helper's window exists, posted to the Explorer window
    PreviewHelperResult    = WM_APP + 0x3C,   // a preview handler's load finished, posted to the Explorer window
    ExplorerNewChoices     = WM_APP + 0x3D,
    ExplorerCassoLink      = WM_APP + 0x3E,
    ExplorerRunningCasso   = WM_APP + 0x3F,
    ExplorerLaunch         = WM_APP + 0x41,
    PreviewHelperRunLoad   = WM_APP + 0x42,   // to the preview helper's window
    PreviewHelperUnload    = WM_APP + 0x43,   // to the preview helper's window
    ShellPreviewLoad       = WM_APP + 0x44,   // to the shell preview's thread
};
