#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"
#include "Machines/Apple2/Common/Disk2Controller.h"





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
//  The drive turns at the same speed as on an Apple II, so the Disk II
//  logic counts one cycle per 14 master clock periods, whatever the CPU's
//  speed.
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
    static constexpr uint32_t  kMasterPerCycle  = 14;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('A', '3', 'D', 'K');
    static constexpr uint16_t  kStateVersion = 1;

    Apple3DiskController();

    Byte     Read                 (Word address);
    void     Write                (Word address, Byte value);
    bool     IsControllerAddress  (Word address) const;

    //  Brings the Disk II logic to a time base's master clock count.
    void     SyncToMasterClocks   (uint64_t masterClocks);

    bool     IsInternalSelected   () const { return m_isInternalSelected; }
    bool     IsExternal1Set       () const { return m_isExternal1; }
    bool     IsExternal2Set       () const { return m_isExternal2; }

    //  The external drive the two lines select, 2 to 4, or 0 for none.
    int      GetSelectedExternal  () const;

    Disk2Controller        & GetDiskII ()       { return *m_diskII; }
    const Disk2Controller  & GetDiskII () const { return *m_diskII; }

    HRESULT  SaveState            (StateWriter & writer) const override;
    HRESULT  LoadState            (StateReader & reader) override;

private:
    void     HandleSelect         (Word address);

    std::unique_ptr<Disk2Controller>  m_diskII;
    uint64_t                          m_diskCycles         = 0;
    uint64_t                          m_lastMasterClocks   = 0;
    bool                              m_isInternalSelected = false;
    bool                              m_isExternal1        = false;
    bool                              m_isExternal2        = false;
};
