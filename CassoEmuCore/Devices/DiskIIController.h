#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"
#include "Disk/DiskImage.h"
#include "Disk/DiskIINibbleEngine.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIController
//
//  Phase 9 rewrite per audit §7. True bit-stream LSS controller:
//  $C0Ex/$C0Fx soft switches own phase magnets, motor on/off, drive
//  select, and Q6/Q7 latches. Reads/writes go through a per-drive
//  DiskIINibbleEngine that streams the active DiskImage bit-stream at
//  the standard 4-cycles-per-bit data rate.
//
//  Slot 6 ROM ($C600-$C6FF) is owned by the AppleIIeMmu's CxxxRomRouter
//  (audit C1: unshadowed when INTCXROM=0). The controller itself only
//  responds to its 16-byte $C0Ex page.
//
////////////////////////////////////////////////////////////////////////////////

class DiskIIController : public MemoryDevice
{
public:
    static constexpr int    kDriveCount      = 2;
    static constexpr int    kPhaseCount      = 4;
    static constexpr int    kMaxQuarterTrack = 139;

    explicit DiskIIController (int slot);

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return m_ioStart; }
    Word GetEnd   () const override { return m_ioEnd; }
    void Reset    () override;
    void SoftReset  () override;
    void PowerCycle (Prng & prng) override;

    HRESULT       MountDisk        (int drive, const string & path);
    void          EjectDisk        (int drive);
    DiskImage *   GetDisk          (int drive);
    void          SetExternalDisk  (int drive, DiskImage * external);
    bool          HasExternalDisk  (int drive) const;

    // Cycle-driven advance. EmuCpu pumps cycles per Step.
    void   Tick (uint32_t cpuCycles);

    // Inspectors used by Phase 9 tests.
    int    GetActiveDrive    () const { return m_activeDrive; }
    bool   IsMotorOn         () const { return m_motorOn; }
    int    GetQuarterTrack   () const { return m_quarterTrack; }
    int    GetCurrentTrack   () const { return m_quarterTrack / 4; }
    bool   IsQ6              () const { return m_q6; }
    bool   IsQ7              () const { return m_q7; }
    DiskIINibbleEngine &  GetEngine (int drive)  { return m_engine[drive]; }

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    void   HandleSwitch         (int offset);
    void   HandlePhase          (int phase, bool on);
    void   UpdateEngineSelection ();
    Byte   HandleReadDispatch  ();

    int                  m_slot;
    Word                 m_ioStart;
    Word                 m_ioEnd;

    uint8_t              m_phases       = 0;
    int                  m_quarterTrack = 0;

    bool                 m_motorOn      = false;

    // Real Disk II hardware: writing $C0E8 (motor off) starts a ~1
    // second spindown timer; the disk physically keeps spinning during
    // this window so DOS RWTS (which toggles motor off between
    // commands and back on a few hundred cycles later for the next
    // read) doesn't lose rotational sync. Tracked in CPU cycles
    // remaining; ticks down in Tick(); reaches 0 and we actually
    // stop the engine. UTAIIe ch. 9 / AppleWin SPINNING_CYCLES.
    static constexpr uint32_t  kMotorSpindownCycles = 1'000'000U;
    uint32_t             m_motorSpindownCycles = 0;

    int                  m_activeDrive  = 0;
    bool                 m_q6           = false;
    bool                 m_q7           = false;

    DiskImage            m_disks[kDriveCount];
    DiskImage *          m_activeDisk[kDriveCount] = { nullptr, nullptr };
    DiskIINibbleEngine   m_engine[kDriveCount];
};
