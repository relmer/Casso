#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDriveAudioSink
//
//  Abstract notification interface fired by a drive controller (Disk II
//  in v1; //c internal floppy, ProFile, etc. in future features) into
//  its audio source. Decouples controller state changes from the
//  concrete audio source.
//
//  Drive types that don't use a given event simply never fire it.
//
////////////////////////////////////////////////////////////////////////////////

class IDriveAudioSink
{
public:
    virtual ~IDriveAudioSink() = default;

    // Motor transitions. OnMotorStart is fired on the off->on edge
    // (case 0x9 of the Disk II soft switches). OnMotorStop fires on
    // the on->off edge, which for the Disk II is the spindown-timer
    // expiry inside Tick(), NOT the raw $C0E8 access (FR-001/FR-002).
    virtual void OnMotorStart   () = 0;
    virtual void OnMotorStop    () = 0;

    // Head movement. OnHeadStep fires when the head actually moves a
    // quarter-track (qtDelta != 0 AND head not pinned at a stop).
    // OnHeadBump fires when the head WOULD have moved but is clamped
    // at the travel stop (track 0 or kMaxQuarterTrack). The two are
    // mutually exclusive for any single phase event (FR-003/FR-004).
    virtual void OnHeadStep     (int newQt) = 0;
    virtual void OnHeadBump     () = 0;

    // Disk image mount/unmount. Fired by the shell mount/eject path.
    // Cold-boot mounts (command-line / restored / autoload) MUST be
    // suppressed by the shell (FR-013) -- the sink itself unconditionally
    // plays the close sound when invoked.
    virtual void OnDiskInserted() = 0;
    virtual void OnDiskEjected  () = 0;
};
