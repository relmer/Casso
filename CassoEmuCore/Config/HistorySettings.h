#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistorySettings
//
//  The machine history's settings, read when the emulator starts: whether
//  history recording is on, the memory the keyframes may use, and how many
//  video frames apart they are taken. A step back replays at most one
//  interval. The budget's upper limit is the settings page's, from the
//  memory available; a value above kMaxBudgetMb is invalid on any machine.
//  The emulator's preferences load and save them under the keys the
//  debugger's options used, so builds that share the preferences file read
//  the same settings.
//
////////////////////////////////////////////////////////////////////////////////

struct HistorySettings
{
    static constexpr int  kMinBudgetMb       = 1;
    static constexpr int  kMaxBudgetMb       = 1024 * 1024;
    static constexpr int  kMaxIntervalFrames = 600;
    static constexpr int  kDefaultBudgetMb   = 64;
    static constexpr int  kDefaultFrames     = 10;

    bool  isRecording    = true;
    int   budgetMb       = kDefaultBudgetMb;
    int   intervalFrames = kDefaultFrames;

    bool  operator== (const HistorySettings &) const = default;
};
