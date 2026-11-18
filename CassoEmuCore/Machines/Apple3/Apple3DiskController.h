#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"
#include "Machines/Apple2/Common/Disk2Controller.h"

class Prng;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3DiskController
//
//  The Apple ///'s floppy controller. The disk register at $C0E0-$C0EF is
//  a Disk II controller at the slot 6 addresses: phases, motor power,
//  $C0EA and $C0EB to connect the internal or the external drives, and
//  Q6 and Q7. $C0D0-$C0D5 are three motor select latches: $C0D4 and $C0D5
//  set and clear the internal drive's, $C0D3 and $C0D2 the first external
//  select line, and $C0D1 and $C0D0 the second. The second line alone
//  selects .D2, the first alone .D3, and both .D4.
//
//  The Disk II logic has two drives: the built-in drive and one external
//  drive. The external drives share the second, and the one the select
//  lines choose is swapped into it with its head position and rotation, so
//  each drive's place is restored when it is connected again. A drive past the
//  number plugged in reads as an empty drive.
//
//  The drive turns at the same speed as on an Apple II, so the Disk II
//  logic counts one cycle per two of the time base's ticks, whatever the
//  CPU's speed. A tick is seven master clock periods, and the last of each
//  line is nine, as the Apple II's long cycle is: the disk clock stretches
//  with the CPU's 1 MHz cycle, so a write loop's nibbles land on the bit
//  cells it times them for.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3DiskController : public IMachineState
{
public:
    static constexpr Word      kFirstSelect     = 0xC0D0;
    static constexpr Word      kLastSelect      = 0xC0D5;
    static constexpr Word      kFirstRegister   = 0xC0E0;
    static constexpr Word      kLastRegister    = 0xC0EF;
    static constexpr int       kDiskIiSlot      = 6;
    static constexpr uint32_t  kTicksPerCycle   = 2;
    static constexpr int       kMaxExternal     = 3;
    static constexpr int       kDriveCount      = 1 + kMaxExternal;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('A', '3', 'D', 'K');
    static constexpr uint16_t  kStateVersion = 4;

    Apple3DiskController();

    Byte     Read                 (Word address);
    void     Write                (Word address, Byte value);
    bool     IsControllerAddress  (Word address) const;

    void     PowerOn              (Prng & prng);
    void     Reset                ();

    //  Brings the Disk II logic to a time base's tick count.
    void     SyncToTicks          (uint64_t ticks);

    //  The CPU's bus cycle and the cycle its instruction started on, so an
    //  access to the disk register brings the Disk II logic to the cycle of
    //  the access rather than to the end of the instruction before it: a
    //  write loop's nibbles are 32 cycles apart only when each write lands on
    //  its cycle. The difference is the access's cycle within the
    //  instruction. In 1 MHz mode a CPU cycle is a disk cycle, and in 2 MHz
    //  mode half of one.
    void     SetCycleSources      (const uint64_t * busCycle, const uint64_t * instructionStart) { m_busCycle = busCycle; m_instructionStart = instructionStart; }
    void     SetOneMhz            (bool isOneMhz)             { m_isOneMhz = isOneMhz; }

    bool     IsInternalSelected   () const { return m_isInternalSelected; }
    bool     IsExternal1Set       () const { return m_isExternal1; }
    bool     IsExternal2Set       () const { return m_isExternal2; }

    //  The external drive the two lines select, 2 to 4, or 0 for none.
    int      GetSelectedExternal  () const;

    //  How many external drives are plugged in, 0 to 3.
    void     SetExternalDriveCount (int count);
    int      GetExternalDriveCount () const { return m_externalCount; }

    //  The disk in drive 0 (the built-in drive) to 3 (.D4), or null for an
    //  empty drive.
    void     SetDisk              (int drive, DiskImage * disk);

    //  The external drive on the Disk II logic's second drive, 0 to 2.
    int      GetConnectedExternal () const { return m_connected; }

    //  A drive's motor, head and nibble counts, for the drive widgets: the
    //  Disk II logic's for a connected drive, and what a disconnected drive
    //  had when it was swapped out. Drive 0 is the built-in drive.
    struct DriveActivity
    {
        bool      isMotorOn    = false;
        int       quarterTrack = -1;
        uint64_t  readNibbles  = 0;
        uint64_t  writeNibbles = 0;
    };

    DriveActivity  GetDriveActivity (int drive) const;

    //  The disk in drive 0 to 3, or null for an empty drive.
    DiskImage    * GetDisk          (int drive) const;

    Disk2Controller        & GetDiskII ()       { return *m_diskII; }
    const Disk2Controller  & GetDiskII () const { return *m_diskII; }

    HRESULT  SaveState            (StateWriter & writer) const override;
    HRESULT  LoadState            (StateReader & reader) override;

private:
    //  A disconnected external drive's head, disk and rotation.
    struct External
    {
        DiskImage          * disk         = nullptr;
        int                  quarterTrack = 0;
        uint64_t             readNibbles  = 0;
        uint64_t             writeNibbles = 0;
        std::vector<Byte>    engineState;
    };

    void     HandleSelect         (Word address);
    void     CatchUpToAccess      ();
    void     ConnectExternal      (int index);

    std::unique_ptr<Disk2Controller>     m_diskII;
    uint64_t                             m_diskCycles         = 0;
    uint64_t                             m_syncedDiskCycles   = 0;
    uint64_t                             m_lastTicks          = 0;
    const uint64_t                     * m_busCycle           = nullptr;
    const uint64_t                     * m_instructionStart   = nullptr;
    bool                                 m_isOneMhz           = true;
    bool                                 m_isInternalSelected = false;
    bool                                 m_isExternal1        = false;
    bool                                 m_isExternal2        = false;
    std::array<External, kMaxExternal>   m_externals;
    int                                  m_connected          = 0;
    int                                  m_externalCount      = 1;
};
