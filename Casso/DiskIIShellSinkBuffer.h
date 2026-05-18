#pragma once

#include "Pch.h"

#include "../CassoEmuCore/Devices/IDiskIIEventSink.h"
#include "../CassoEmuCore/Devices/DiskIIEventRing.h"
#include "../CassoEmuCore/Audio/IDriveAudioEventSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIShellSinkBuffer
//
//  Spec-006 round-4 bug 3. EmulatorShell-owned, long-lived sink that
//  is wired onto the active DiskIIController and DiskIIAudioSource at
//  machine-build time -- BEFORE any user opens the debug dialog. It
//  implements both IDiskIIEventSink and IDriveAudioEventSink. Each
//  incoming event is stamped with the current cycle counter and the
//  tracked active drive, packed into a DiskIIEvent POD, and pushed
//  into an internal 4096-entry SPSC ring.
//
//  When the debug dialog opens for the first time, the shell calls
//  DrainInto to transfer everything currently buffered into the
//  dialog's display deque so the user sees pre-open events (boot
//  RWTS reads, the cold-boot OnDiskInserted, etc.) instead of an
//  empty log starting from the moment they hit the menu item.
//
//  After SetLiveSink is called (dialog has been opened), the buffer
//  forwards every incoming event to the live sink instead of pushing
//  into its own ring -- the dialog's own ring + drain machinery takes
//  over from there. Drive tracking (m_currentDrive) is updated on
//  every OnDriveSelect regardless of forward / buffer mode so a later
//  detach (if the dialog were ever destroyed and re-created) would
//  still see correct state.
//
////////////////////////////////////////////////////////////////////////////////

class DiskIIShellSinkBuffer : public IDiskIIEventSink,
                              public IDriveAudioEventSink
{
public:
    DiskIIShellSinkBuffer ()           = default;
    ~DiskIIShellSinkBuffer () override = default;

    // CPU thread pre-machine-start. The shell passes the CPU's
    // cycle-counter pointer; events stamped before the CPU exists
    // carry cycle 0.
    void   SetCycleCounter (const uint64_t * counter) noexcept
    {
        m_cycleCounter = counter;
    }

    // UI thread. Install the dialog (or any future live consumer)
    // as the downstream sink. From this point forward, incoming
    // events bypass the buffer's ring and are forwarded directly
    // to the live sinks. Pass nullptr to detach.
    void   SetLiveSink (IDiskIIEventSink *      controllerSink,
                        IDriveAudioEventSink *  audioSink) noexcept
    {
        m_liveSink      = controllerSink;
        m_liveAudioSink = audioSink;
    }

    // UI thread. Drain buffered events into `dest` in FIFO order.
    // After the drain, dest is capped to `maxRows` by pop_front'ing
    // anything past the cap (preserves the newest entries). Safe to
    // call multiple times; the second call sees only what landed in
    // the ring between calls.
    void   DrainInto (std::deque<DiskIIEvent> & dest, size_t maxRows) noexcept;

    // Test seam.
    DiskIIEventRing &       GetRing       () noexcept { return m_ring; }
    const DiskIIEventRing & GetRing       () const noexcept { return m_ring; }
    int                     GetCurrentDrive () const noexcept { return m_currentDrive; }

    // IDiskIIEventSink
    void OnMotorCommandOn   () override;
    void OnMotorEngaged     () override;
    void OnMotorCommandOff  () override;
    void OnMotorDisengaged  () override;
    void OnHeadStep         (int prevQt, int newQt) override;
    void OnHeadBump         (int atQt) override;
    void OnAddressMark      (int track, int sector, int volume) override;
    void OnDataMarkRead     (int track, int sector, int volume, int byteCount) override;
    void OnDataMarkWrite    (int track, int sector, int volume, int byteCount) override;
    void OnDriveSelect      (int drive) override;
    void OnDiskInserted     (int drive) override;
    void OnDiskEjected      (int drive) override;

    // IDriveAudioEventSink
    void OnAudioStarted     (SoundKind kind, int drive) override;
    void OnAudioRestarted   (SoundKind kind, int drive) override;
    void OnAudioContinued   (SoundKind kind, int drive) override;
    void OnAudioSilent      (SoundKind kind, int drive, SilentReason reason) override;
    void OnAudioLoopStarted (SoundKind kind, int drive) override;
    void OnAudioLoopStopped (SoundKind kind, int drive) override;

private:
    void   PushControllerEvent (DiskIIEventType type) noexcept;
    void   PushHeadStepEvent   (int prevQt, int newQt) noexcept;
    void   PushHeadBumpEvent   (int atQt) noexcept;
    void   PushAddrMarkEvent   (int track, int sector, int volume) noexcept;
    void   PushDataMarkEvent   (DiskIIEventType type, int track, int sector, int volume, int byteCount) noexcept;
    void   PushDriveEvent      (DiskIIEventType type, int drive) noexcept;
    void   PushAudioEvent      (DiskIIEventType type, SoundKind kind, int drive, SilentReason reason) noexcept;
    void   PublishToRing       (const DiskIIEvent & e) noexcept;

    DiskIIEventRing         m_ring;
    const uint64_t *        m_cycleCounter  = nullptr;
    int                     m_currentDrive  = 0;
    IDiskIIEventSink *      m_liveSink      = nullptr;
    IDriveAudioEventSink *  m_liveAudioSink = nullptr;
};
