#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SoundKind
//
//  Identifies the per-sound channel that a Disk II audio decision was
//  made on. Stays a small enum (fits in uint8_t) so Disk2Event's
//  payload union remains <= 12 bytes.
//
////////////////////////////////////////////////////////////////////////////////

enum class SoundKind : uint8_t
{
    MotorLoop  = 0,
    HeadStep   = 1,
    HeadStop   = 2,
    DoorOpen   = 3,
    DoorClose  = 4,
};





////////////////////////////////////////////////////////////////////////////////
//
//  SilentReason
//
//  Disposition for an OnAudioSilent event. Tells the debug log WHY no
//  audio played even though the underlying controller event fired.
//
////////////////////////////////////////////////////////////////////////////////

enum class SilentReason : uint8_t
{
    DriveAudioDisabled    = 0,   // user toggled drive audio off
    BufferMissing         = 1,   // sample WAV not loaded for this kind
    NoSourceRegistered    = 2,   // no Disk2AudioSource for active drive
    ColdBootSuppression   = 3,   // first-slice insert (spec-005 FR-013)
    NoDiskPresent         = 4,   // motor on but drive bay empty (spec-006)
};





////////////////////////////////////////////////////////////////////////////////
//
//  IDriveAudioEventSink
//
//  Abstract notification interface fired by Disk2AudioSource at every
//  audio-decision moment so the spec-006 debug window can show what the
//  audio subsystem decided to do (and why, when nothing played).
//
//  Distinct from IDriveAudioSink (the spec-005 controller-to-audio
//  contract). Both interfaces can be implemented on the same object;
//  they do not share a vtable. The audio-side OnAudio* methods fire
//  AFTER the audio source has already made its decision and updated
//  its own state, so a no-op sink leaves audio output byte-identical
//  to the pre-feature audio path (FR-022, FR-008 read-only invariant).
//
//  All methods are void and infallible; implementers route into a
//  lock-free SPSC ring on the same CPU thread that produced the
//  controller event.
//
//  Outcomes (mutually exclusive per controller-event delivery):
//    * OnAudioStarted    -- one-shot fired from sample 0; prior shot for
//                            this SoundKind was idle.
//    * OnAudioRestarted  -- one-shot fired while a prior shot was still
//                            playing; prior shot cut short.
//    * OnAudioContinued  -- event acknowledged; prior shot's tail kept
//                            (spec-005 FR-005 seek-mode HeadStep).
//    * OnAudioSilent     -- event fired but no audio played; reason
//                            says why.
//    * OnAudioLoopStarted -- looping sound (motor) began.
//    * OnAudioLoopStopped -- looping sound (motor) stopped.
//
////////////////////////////////////////////////////////////////////////////////

class IDriveAudioEventSink
{
public:
    virtual ~IDriveAudioEventSink() = default;

    virtual void OnAudioStarted     (SoundKind kind, int drive) = 0;
    virtual void OnAudioRestarted   (SoundKind kind, int drive) = 0;
    virtual void OnAudioContinued   (SoundKind kind, int drive) = 0;
    virtual void OnAudioSilent      (SoundKind kind, int drive, SilentReason reason) = 0;
    virtual void OnAudioLoopStarted (SoundKind kind, int drive) = 0;
    virtual void OnAudioLoopStopped (SoundKind kind, int drive) = 0;
};
