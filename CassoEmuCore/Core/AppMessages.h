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
    ShellUpdateTitle    = WM_APP + 0x21,   // a window-title refresh, from the CPU thread to the UI thread
    ShellNotifyUser     = WM_APP + 0x22,
    ShellReportDamage   = WM_APP + 0x23,
    ShellMountCompleted = WM_APP + 0x25,
    ShellChangeReport   = WM_APP + 0x26,
    ShellChangeAsk      = WM_APP + 0x27,
    ShellShowNotice     = WM_APP + 0x28,
    ShellGameportFlush  = WM_APP + 0x29,
    ShellControllerPick = WM_APP + 0x2A,
    ShellUpdateResult   = WM_APP + 0x2B,
    ShellIntentReply    = WM_APP + 0x2C,
};