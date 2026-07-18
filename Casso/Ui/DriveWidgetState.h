#pragma once

#include "Pch.h"

#include "Devices/Disk/IDiskImage.h"    // WriteProtectInfo






////////////////////////////////////////////////////////////////////////////////
//
//  DriveWidgetState
//
//  Per-drive runtime state shared between the CPU thread (motor + track
//  read/write signals) and the UI thread (door animation, mounted path).
//  Owned per-drive by `EmulatorShell`; read each UI frame and pushed into
//  the corresponding chrome drive widget via `SyncFromState`.
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
//  context (UnitTest/UiTests/DriveWidgetStateTests.cpp).
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
    static constexpr int64_t  kDoorAnimationMs = 350;

    std::wstring      mountedImagePath;
    std::atomic<bool> motorOn              { false };
    std::atomic<bool> diskActive           { false };

    // Write-protect state of the mounted image, sampled each UI frame
    // from the DiskImage in DiskManager::UpdateDriveWidgets. Drives the
    // padlock cue and the hover tooltip. UI-thread only.
    WriteProtectInfo  writeProtect;

    // Default Open: an empty drive at rest shows the door open
    // (matches real Apple Disk II). Drives that auto-mount at boot
    // transition Open -> Closing via BeginInsert -- the brief 200 ms
    // animation reads as the disk being inserted, which is a nicer
    // cold-boot visual than the door snapping shut.
    Door              doorState            = Door::Open;
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

    // UI-only door transition that does NOT touch mountedImagePath.
    // Used when the user is browsing for a new disk: the chrome opens
    // the door for visual feedback while the file-open dialog is up,
    // then closes it again whether or not a disk was actually chosen.
    void StartDoorTransition (Door target, int64_t nowMs)
    {
        if (target == Door::Opening &&
            (doorState == Door::Closed || doorState == Door::Closing))
        {
            doorState            = Door::Opening;
            animationStartTimeMs = nowMs;
        }
        else if (target == Door::Closing &&
                 (doorState == Door::Open || doorState == Door::Opening))
        {
            doorState            = Door::Closing;
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
//  Case-insensitive check for the five supported disk image extensions.
//
////////////////////////////////////////////////////////////////////////////////

inline bool IsSupportedDiskImageExtension (const std::wstring & path)
{
    static const wchar_t * const  kExts[] = { L".dsk", L".do", L".nib", L".woz", L".po" };

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




////////////////////////////////////////////////////////////////////////////////
//
//  ComposeWriteProtectTooltip
//
//  Builds the single-line hover-tooltip text for a write-protected
//  drive, naming every active source so the user can tell a setting-
//  driven lock from an image flag or an unwritable backing file. Returns
//  an empty string when the disk is not protected (no tooltip shown).
//
////////////////////////////////////////////////////////////////////////////////

inline std::wstring ComposeWriteProtectTooltip (const WriteProtectInfo & wp)
{
    std::vector<std::wstring>  reasons;
    std::wstring               msg;
    size_t                     i = 0;


    if (!wp.Any())
    {
        return std::wstring();
    }

    if (wp.userSetting)  { reasons.push_back (L"the write-protect setting"); }
    if (wp.imageFlag)    { reasons.push_back (L"the image's write-protect flag"); }
    if (wp.readOnlyFile) { reasons.push_back (L"a read-only file"); }
    if (wp.noPermission) { reasons.push_back (L"no write permission for the file"); }

    msg = L"Disk is write-protected by ";

    for (i = 0; i < reasons.size(); ++i)
    {
        if (i > 0)
        {
            if (i + 1 == reasons.size())
            {
                // Final source: ", and " for a 3+ list (Oxford comma),
                // a bare " and " for exactly two.
                msg += (reasons.size() > 2) ? L", and " : L" and ";
            }
            else
            {
                msg += L", ";
            }
        }

        msg += reasons[i];
    }

    msg += L".";

    return msg;
}
