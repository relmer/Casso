#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskImageStore.h"    // IsMountableImageExtension
#include "Devices/Disk/IDiskImage.h"        // WriteProtectInfo





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
//      headQuarterTrack      -- atomic<int>, written by CPU thread
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

    // Where the head is sitting, in QUARTER-tracks (0 to 139), sampled each
    // UI frame from the drive's own nibble engine. Divide by four for a track
    // number.
    //
    // The units are the engine's, and its accessor is misnamed:
    // Disk2NibbleEngine::GetCurrentTrack returns quarter-tracks, while
    // Disk2Controller::GetCurrentTrack next door returns whole ones. See
    // GH #136, which renames them.
    //
    // Sampled per drive rather than from the controller's own quarter-track,
    // which is a single member for the whole card and so follows whichever
    // drive is selected. Drive 1's head drawn under drive 2's name would be a
    // worse fault than a coarse readout. GH #135 covers the underlying defect.
    //
    // -1 means the position is not known, which is every drive on a machine
    // with no Disk ][ controller. A reader must not render that as track 0.
    std::atomic<int>  headQuarterTrack     { -1 };

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

    // Set by BeginReinsert: the door opens, then closes on its own. A disk was
    // replaced under the running machine, which reads as it coming out and
    // another going in -- one gesture the path poll cannot see, since the file
    // in the drive did not change.
    bool              reinsertPending      = false;

    // UI-thread mutators (pure logic)

    // Records a new mount and starts close-door animation if needed.
    void BeginInsert       (const std::wstring & path, int64_t nowMs)
    {
        mountedImagePath = path;

        // A plain insert is not a reinsert. Clear a pending open-then-close
        // left by a swap this insert interrupts, or the door would roll into
        // an unasked-for close when the open half finishes.
        reinsertPending = false;

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

        // An eject is not a reinsert. Clear a pending open-then-close left by a
        // swap this eject interrupts, or the open half would roll into a close
        // and seal the door on a drive that is now empty.
        reinsertPending = false;

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
        // A browse open/close is not a reinsert. Clear a pending open-then-close
        // so a swap interrupted by the file dialog does not later roll shut.
        reinsertPending = false;

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

    // A disk replaced under the running machine: open, then close on our own.
    // The disk stays in the drive throughout -- the file behind it is what
    // changed -- so the path poll never sees it and this is the only way the
    // door moves. From a closed door the open half runs first; from an open
    // one (an empty drive that somehow took a swap) the close half is all that
    // is left to do.
    void BeginReinsert     (int64_t nowMs)
    {
        if (doorState == Door::Closed || doorState == Door::Closing)
        {
            doorState            = Door::Opening;
            animationStartTimeMs = nowMs;
            reinsertPending      = true;
        }
        else
        {
            doorState            = Door::Closing;
            animationStartTimeMs = nowMs;
            reinsertPending      = false;
        }
    }

    // Advances Opening->Open and Closing->Closed after the delay. A pending
    // reinsert turns the end of the open half straight into the close half,
    // so the door does not rest open between the two.
    void TickDoorAnimation (int64_t nowMs)
    {
        int64_t  elapsed = nowMs - animationStartTimeMs;

        if (elapsed < kDoorAnimationMs)
        {
            return;
        }

        if (doorState == Door::Opening)
        {
            if (reinsertPending)
            {
                reinsertPending      = false;
                doorState            = Door::Closing;
                animationStartTimeMs = nowMs;
            }
            else
            {
                doorState = Door::Open;
            }
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
//  Case-insensitive check for the disk image extensions this build mounts,
//  used by the drag-and-drop filter and the disk picker's file scan.
//
//  It keeps no list of its own. It used to, and the list said `.nib` while
//  the loader never has, so a dropped nibble image passed the filter and then
//  vanished without a word. The answer now comes from the routing table
//  itself, which is the only way the two cannot disagree again.
//
////////////////////////////////////////////////////////////////////////////////

inline bool IsSupportedDiskImageExtension (const std::wstring & path)
{
    return DiskImageStore::IsMountableImageExtension (path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComposeWriteProtectTooltip
//
//  Builds the hover-tooltip text for a write-protected drive on the
//  pattern "<subject> is write-protected (<specific cause>)". The subject
//  is the mounted image's quoted name when known; disk-borne causes (the
//  WOZ in-file flag, a read-only file, no write permission) merge into one
//  parenthetical, and the drive-level Settings preference -- which
//  protects the drive, not the disk -- gets its own sentence pointing at
//  where to change it. Returns an empty string when nothing is protected
//  (no tooltip shown).
//
//  A damaged image (its stored checksum did not match at load) takes
//  precedence over the cause list and explains itself: it is not a setting
//  the user can clear, so reporting it as plain write-protection would send
//  them hunting for a toggle that will refuse them.
//
////////////////////////////////////////////////////////////////////////////////

inline std::wstring ComposeWriteProtectTooltip (
    int                      driveNumber,
    const std::wstring     & imageName,
    const WriteProtectInfo & wp)
{
    std::vector<std::wstring>  causes;
    std::wstring               msg;
    size_t                     i = 0;


    if (!wp.Any())
    {
        return std::wstring();
    }

    if (wp.imageFlag)    { causes.push_back (L"WOZ write-protect flag"); }
    if (wp.readOnlyFile) { causes.push_back (L"file is read-only"); }
    if (wp.noPermission) { causes.push_back (L"no write permission"); }

    // A damaged image is a different and more worrying state than an ordinary
    // write-protect, so it leads with its own sentence instead of joining the
    // parenthetical list -- and it says why, because "write-protected" alone
    // invites the user to go looking for the toggle that would clear it.
    if (wp.checksumMismatch)
    {
        msg = imageName.empty() ? L"This disk image" : (L"\"" + imageName + L"\"");
        msg += L" is damaged: its stored checksum does not match its contents. "
               L"Casso will not write to it, because rewriting the file would "
               L"hide the damage.";

        // and nothing else. The other causes are true but immaterial: the disk
        // is unwritable because it is damaged, and no toggle or preference the
        // rest of this text would name can change that. Listing them invites
        // the user to go clear a flag that will not help.
        return msg;
    }

    if (!causes.empty())
    {
        if (!msg.empty())
        {
            msg += L" ";
        }

        msg += imageName.empty() ? L"Disk image" : (L"\"" + imageName + L"\"");
        msg += L" is write-protected (";

        for (i = 0; i < causes.size(); ++i)
        {
            if (i > 0)
            {
                msg += L"; ";
            }

            msg += causes[i];
        }

        msg += L").";
    }

    if (wp.userSetting)
    {
        if (!msg.empty())
        {
            msg += L" ";
        }

        msg += L"Drive " + std::to_wstring (driveNumber)
             + ((causes.empty() && !wp.checksumMismatch)
                    ? L" is write-protected in Settings > Disk."
                    : L" is also write-protected in Settings > Disk.");
    }

    return msg;
}
