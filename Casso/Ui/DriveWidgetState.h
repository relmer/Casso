#pragma once

#include "Pch.h"






////////////////////////////////////////////////////////////////////////////////
//
//  DriveWidgetState
//
//  Per-drive runtime state shared between the CPU thread (motor + track
//  read/write signals) and the UI thread (door animation, mounted path).
//  Owned per-drive by `EmulatorShell`; read each UI frame and pushed into
//  the corresponding `DriveWidgetElement` via `SyncFromState`.
//
//  Concurrency
//  -----------
//  Per data-model.md:
//      mountedImagePath      -- UI thread only (insert/eject path)
//      motorOn               -- atomic<bool>, written by CPU thread
//      diskActive            -- atomic<bool>, written by CPU thread
//      doorState             -- UI thread only
//      animationStartTimeMs  -- UI thread only
//
//  The atomics are the existing pattern used by the audio system to
//  observe motor state from a second thread; no new sync primitives are
//  introduced (P6 constitution check gate).
//
//  Door animation FSM
//  --------------------------
//      Closed   -> Opening   on  BeginEject  (mounted == false set later)
//      Opening  -> Open      after kDoorAnimationMs elapsed
//      Open     -> Closing   on  BeginInsert (mountedImagePath set first)
//      Closing  -> Closed    after kDoorAnimationMs elapsed
//
//  The pure-logic helpers `BeginEject`, `BeginInsert`, and
//  `TickDoorAnimation` make the state machine unit-testable without an
//  RmlUi context (UnitTest/UiTests/DriveWidgetStateTests.cpp).
//
////////////////////////////////////////////////////////////////////////////////

struct DriveWidgetState
{
    enum class Door
    {
        Closed,
        Opening,
        Open,
        Closing,
    };

    // FR-021 / FR-025 door animation duration in ms.
    static constexpr int64_t  kDoorAnimationMs = 200;

    std::wstring      mountedImagePath;
    std::atomic<bool> motorOn              { false };
    std::atomic<bool> diskActive           { false };
    Door              doorState            = Door::Closed;
    int64_t           animationStartTimeMs = 0;
    uint64_t          lastSyncEventId      = 0;

    // ---- UI-thread mutators (pure logic) ----------------------------

    // Records a new mount and starts close-door animation if needed.
    void BeginInsert       (const std::wstring & path, int64_t nowMs)
    {
        mountedImagePath = path;

        if (doorState == Door::Open || doorState == Door::Opening)
        {
            doorState            = Door::Closing;
            animationStartTimeMs = nowMs;
        }
        else if (doorState == Door::Closed)
        {
            // Already closed -- nothing to animate. Keep timestamp 0.
            animationStartTimeMs = 0;
        }
    }

    // Records an eject and starts open-door animation if needed.
    void BeginEject        (int64_t nowMs)
    {
        mountedImagePath.clear();

        if (doorState == Door::Closed || doorState == Door::Closing)
        {
            doorState            = Door::Opening;
            animationStartTimeMs = nowMs;
        }
    }

    // Advances Opening->Open and Closing->Closed after the delay.
    void TickDoorAnimation (int64_t nowMs)
    {
        int64_t  elapsed = nowMs - animationStartTimeMs;

        if (elapsed < kDoorAnimationMs)
        {
            return;
        }

        if (doorState == Door::Opening)
        {
            doorState = Door::Open;
        }
        else if (doorState == Door::Closing)
        {
            doorState = Door::Closed;
        }
    }

    bool IsMounted         () const
    {
        return !mountedImagePath.empty();
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  IsSupportedDiskImageExtension
//
//  Case-insensitive check for the four supported disk image extensions.
//
////////////////////////////////////////////////////////////////////////////////

inline bool IsSupportedDiskImageExtension (const std::wstring & path)
{
    static const wchar_t * const  kExts[] = { L".dsk", L".nib", L".woz", L".po" };

    size_t  dot = path.find_last_of (L'.');

    if (dot == std::wstring::npos)
    {
        return false;
    }

    std::wstring  ext = path.substr (dot);

    for (wchar_t & c : ext)
    {
        if (c >= L'A' && c <= L'Z')
        {
            c = static_cast<wchar_t> (c + (L'a' - L'A'));
        }
    }

    for (const wchar_t * candidate : kExts)
    {
        if (ext == candidate)
        {
            return true;
        }
    }

    return false;
}
