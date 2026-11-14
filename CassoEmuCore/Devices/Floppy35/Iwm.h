#pragma once

#include "Pch.h"

class IDiskDriveBus;





////////////////////////////////////////////////////////////////////////////////
//
//  Iwm
//
//  Apple's Integrated Woz Machine. Sixteen soft switches, an address's bit 0
//  the level and bits 1-3 the switch: the four phases, the drive enable, the
//  drive select, Q6 and Q7. Q6 and Q7 select what a read returns:
//
//      Q7 Q6
//      0  0   the data register: the last byte read off the disk, $FF
//             with the drive not enabled
//      0  1   status: bit 7 the selected drive's sense line, bit 5 the
//             enable, bits 4-0 the mode
//      1  0   write handshake: bit 7 the write buffer empty, bit 6 no
//             underrun
//
//  A write with Q6 and Q7 set goes to the mode register with the drive not
//  enabled, and to the write buffer with it enabled. In async mode, mode bit
//  1, reading a byte from the data register clears it.
//
//  Bits move one drive cell at a time, at the selected drive's cell time:
//  reading shifts each cell into a register that becomes the data byte when
//  its top bit is set; writing shifts the buffered byte out, top bit first.
//  Time is passed in nanoseconds by the owner, whose clock runs the chip.
//
//  The SWIM in its IWM mode works the same way. Writing mode bit 6 as 1, 0,
//  1, 1 switches a SWIM to its ISM mode, where an address's bit 3 selects a
//  read and bits 0-2 the register:
//
//      write 3, read 11   the 16-byte parameter RAM, through a counter that
//                         any write to the mode register resets
//      write 4, read 12   phases: bits 0-3 the levels, bits 4-7 the outputs
//      write 6            clears the mode bits set in the byte; clearing
//                         bit 6 returns to the IWM mode
//      write 7            sets the mode bits set in the byte
//      read 14            the mode register, bit 6 set
//
////////////////////////////////////////////////////////////////////////////////

class Iwm
{
public:
    static constexpr int   kDrives        = 2;
    static constexpr Byte  kModeLatch     = 0x01;
    static constexpr Byte  kModeAsync     = 0x02;
    static constexpr Byte  kModeNoTimer   = 0x04;
    static constexpr Byte  kModeMask      = 0x1F;

    void  AttachDrive (int index, IDiskDriveBus * drive) { m_drives[(size_t) index] = drive; }

    //  An access to soft switch `offset` (0-15) at `nowNs`; returns the byte
    //  a read returns.
    Byte  Access (int offset, bool isWrite, Byte value, uint64_t nowNs);

    //  Moves bits up to `nowNs`.
    void  AdvanceTo (uint64_t nowNs);

    void  Reset ();

    bool  IsEnabled     () const { return m_enabled; }
    bool  IsWriting     () const { return m_q7; }
    int   GetDrive      () const { return m_driveSelect; }
    Byte  GetMode       () const { return m_mode; }
    bool  GetPhase      (int phase) const { return m_phases[(size_t) phase]; }
    bool  IsIsmMode     () const { return m_ism; }

private:
    static constexpr uint64_t  kMotorOffNs    = 1'000'000'000;
    static constexpr size_t    kParamBytes    = 16;
    static constexpr Byte      kIsmBit        = 0x40;
    static constexpr Byte      kIsmEntry      = 0x0B;
    static constexpr Byte      kHistoryMask   = 0x0F;
    static constexpr Byte      kIsmPhaseReset = 0xF0;

    Byte   AccessIsm      (int offset, bool isWrite, Byte value);

    IDiskDriveBus  * GetSelected   () const { return m_drives[(size_t) m_driveSelect]; }
    void             SetSwitch     (int sw, bool on, uint64_t nowNs);
    Byte             ReadRegister  ();
    void             WriteRegister (Byte value);
    void             MoveCell      (uint64_t cellEndNs);
    void             UpdatePhases  ();

    std::array<IDiskDriveBus *, kDrives>  m_drives          = {};
    std::array<bool, 4>                   m_phases          = {};
    bool                                  m_enabled         = false;
    bool                                  m_motorOffPending = false;
    uint64_t                              m_motorOffAt      = 0;
    int                                   m_driveSelect     = 0;
    bool                                  m_q6              = false;
    bool                                  m_q7              = false;
    Byte                                  m_mode            = 0;
    Byte                                  m_data            = 0;
    Byte                                  m_shift           = 0;
    Byte                                  m_writeBuffer     = 0;
    bool                                  m_bufferFull      = false;
    int                                   m_writeBits       = 0;
    bool                                  m_underrun        = false;
    uint64_t                              m_lastNs          = 0;
    uint64_t                              m_cellCarryNs     = 0;
    bool                                  m_ism             = false;
    Byte                                  m_ismHistory      = 0;
    Byte                                  m_ismMode         = 0;
    Byte                                  m_ismPhase        = 0;
    std::array<Byte, kParamBytes>         m_param           = {};
    size_t                                m_paramIndex      = 0;
};
