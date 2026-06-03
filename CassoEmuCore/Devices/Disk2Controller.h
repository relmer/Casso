#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"
#include "Disk/DiskImage.h"
#include "Disk/Disk2NibbleEngine.h"
#include "Disk2AddressMarkWatcher.h"


class IDriveAudioSink;
class IDisk2EventSink;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Controller
//
//  Phase 9 rewrite per audit §7. True bit-stream LSS controller:
//  $C0Ex/$C0Fx soft switches own phase magnets, motor on/off, drive
//  select, and Q6/Q7 latches. Reads/writes go through a per-drive
//  Disk2NibbleEngine that streams the active DiskImage bit-stream at
//  the standard 4-cycles-per-bit data rate.
//
//  Slot 6 ROM ($C600-$C6FF) is owned by the Apple2eMmu's CxxxRomRouter
//  (audit C1: unshadowed when INTCXROM=0). The controller itself only
//  responds to its 16-byte $C0Ex page.
//
////////////////////////////////////////////////////////////////////////////////

class Disk2Controller : public MemoryDevice
{
public:
    static constexpr int    kDriveCount      = 2;
    static constexpr int    kPhaseCount      = 4;
    static constexpr int    kMaxQuarterTrack = 139;

    // Real Disk II: writing $C0E8 (motor off) starts a ~1-second
    // spindown so DOS RWTS can toggle the motor off between
    // commands without losing rotational sync. UTAIIe ch. 9 /
    // AppleWin SPINNING_CYCLES.
    static constexpr uint32_t  kMotorSpindownCycles = 1'000'000U;

    // Issue #67 deliverable 1: LSS stability window. For roughly
    // 0x2EC CPU cycles after the motor's off->on edge the Disk II
    // Logic State Sequencer hasn't latched a stable nibble yet, so
    // any read returns 0x80 (MSB set, meaningless data). Copy-
    // protection schemes deliberately read during this window and
    // verify the absence of valid sync nibbles. Matches AppleWin's
    // MOTOR_ON_UNTIL_LSS_STABLE_CYCLES (GH#864); the per-card range
    // is 0x2EC-0x990 cycles. The bit cursor still advances during
    // the window -- only the CPU-visible latch is overridden -- so
    // rotational position stays accurate. Note: physical disk spin-
    // up takes ~500 ms, but the firmware doesn't care; it only
    // cares about the LSS-stable bit, which is much shorter.
    static constexpr uint32_t  kMotorSpinupCycles = 0x2EC;

    explicit Disk2Controller (int slot);

    Byte Read (Word address) override;
    void Write (Word address, Byte value) override;
    Word GetStart() const override { return m_ioStart; }
    Word GetEnd() const override { return m_ioEnd; }
    void Reset() override;
    void SoftReset() override;
    void PowerCycle (Prng & prng) override;

    HRESULT       MountDisk (int drive, const string & path);
    void          EjectDisk (int drive);
    DiskImage *   GetDisk (int drive);
    void          SetExternalDisk (int drive, DiskImage * external);
    bool          HasExternalDisk (int drive) const;

    // Spec-006 bug 14b. When EmulatorShell drives mount/eject through
    // DiskImageStore + SetExternalDisk (bypassing this class's own
    // MountDisk / EjectDisk), the controller's own load path never
    // runs and the IDisk2EventSink never sees the user-facing
    // insert/eject. These notify-only entrypoints let the shell fire
    // those events without re-routing the actual image bytes.
    void          NotifyDiskInserted (int drive);
    void          NotifyDiskEjected (int drive);

    // Audio sink wiring (spec 005-disk-ii-audio FR-001..FR-004).
    // Caller-owned; controller never deletes it. Single sink covers
    // both drives (per-drive routing happens at the source-mixer
    // level via separate IDriveAudioSource instances).
    void          SetAudioSink (IDriveAudioSink * sink) { m_audioSink = sink; }
    IDriveAudioSink * GetAudioSink() const                 { return m_audioSink; }

    // Spec-006 debug-window event sink wiring. Caller-owned;
    // controller never deletes it. Pass nullptr to detach (the
    // controller fast-paths around the per-fire-site guard when
    // unattached so behavior is byte-identical to the pre-feature
    // path, FR-007 / FR-020 / SC-007). Propagated to the embedded
    // Disk2AddressMarkWatcher so the watcher fires its own
    // address-mark / data-mark events through the same sink.
    void          SetEventSink (IDisk2EventSink * sink) noexcept;

    // Cycle-driven advance. EmuCpu pumps cycles per Step.
    void   Tick (uint32_t cpuCycles);

    // Issue #67: catch-up cycle source. When set, every Read/Write of
    // the $C0Ex page first walks the active drive's bit-stream engine
    // forward to the CPU's current cycle count BEFORE the soft-switch
    // dispatch fires, mirroring AppleWin's CpuCalcCycles-at-top-of-
    // handler pattern. The counter is the per-instruction cycle
    // accumulator (m_totalCycles), so the engine is current to the end
    // of the previous instruction at the moment of the access -- the
    // same effective granularity AppleWin provides.
    //
    // When a source is attached, the per-instruction Tick path no
    // longer advances the engine bit cursor (the catch-up does it on
    // demand). Pass nullptr for tests that drive the controller
    // without a real CPU.
    void   SetCpuCycleSource (const uint64_t * cycleSource) noexcept { m_cpuCycleSource = cycleSource; m_lastCpuSync = (cycleSource != nullptr) ? *cycleSource : 0; }

    // Inspectors used by Phase 9 tests.
    int    GetActiveDrive() const { return m_activeDrive; }
    bool   IsMotorOn() const { return m_motorOn; }
    bool   IsMotorAtSpeed() const { return m_motorOn && m_motorSpinupRemaining == 0; }
    uint32_t  GetMotorSpinupRemaining() const { return m_motorSpinupRemaining; }
    int    GetQuarterTrack() const { return m_quarterTrack; }
    int    GetCurrentTrack() const { return m_quarterTrack / 4; }
    bool   IsQ6() const { return m_q6; }
    bool   IsQ7() const { return m_q7; }
    Disk2NibbleEngine &  GetEngine (int drive)  { return m_engine[drive]; }

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    void   HandleSwitch (int offset);
    void   HandlePhase (int phase, bool on);
    void   UpdateEngineSelection();
    Byte   HandleReadDispatch();
    void   CatchUpToCpu();

    int                  m_slot;
    Word                 m_ioStart;
    Word                 m_ioEnd;

    uint8_t              m_phases       = 0;
    int                  m_phase        = 0;
    int                  m_quarterTrack = 0;

    bool                 m_motorOn      = false;

    // Real Disk II hardware: writing $C0E8 (motor off) starts a ~1
    // second spindown timer; the disk physically keeps spinning during
    // this window so DOS RWTS (which toggles motor off between
    // commands and back on a few hundred cycles later for the next
    // read) doesn't lose rotational sync. Tracked in CPU cycles
    // remaining; ticks down in Tick(); reaches 0 and we actually
    // stop the engine. UTAIIe ch. 9 / AppleWin SPINNING_CYCLES.
    // Constant kMotorSpindownCycles lives in the public section
    // for test access.
    uint32_t             m_motorSpindownCycles = 0;

    // Issue #67 deliverable 1: motor spin-up window remainder.
    // Constant kMotorSpinupCycles lives in the public section
    // for test access. See the public-section comment for the
    // protection-fidelity rationale.
    uint32_t             m_motorSpinupRemaining = 0;

    int                  m_activeDrive  = 0;
    bool                 m_q6           = false;
    bool                 m_q7           = false;

    DiskImage            m_disks[kDriveCount];
    DiskImage *          m_activeDisk[kDriveCount] = { nullptr, nullptr };
    Disk2NibbleEngine    m_engine[kDriveCount];

    IDriveAudioSink *    m_audioSink   = nullptr;

    IDisk2EventSink *         m_eventSink       = nullptr;
    Disk2AddressMarkWatcher   m_addrMarkWatcher;

    const uint64_t *          m_cpuCycleSource = nullptr;
    uint64_t                  m_lastCpuSync    = 0;
};
