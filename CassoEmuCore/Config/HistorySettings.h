#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistorySettings
//
//  The machine history's settings, read when the emulator starts: whether
//  history recording is on, and the memory the keyframes may use. The
//  budget's upper limit is the settings page's, from the memory available;
//  a value above kMaxBudgetMb is invalid on any machine.
//  The emulator's preferences load and save them under the keys the
//  debugger's options used, so builds that share the preferences file read
//  the same settings.
//
////////////////////////////////////////////////////////////////////////////////

struct HistorySettings
{
    static constexpr int  kMinBudgetMb     = 1;
    static constexpr int  kMaxBudgetMb     = 1024 * 1024;
    static constexpr int  kDefaultBudgetMb = 64;

    bool  isRecording = true;
    int   budgetMb    = kDefaultBudgetMb;

    bool  operator== (const HistorySettings &) const = default;
};
