#pragma once

#include "Pch.h"

#include "Core/IIoAccessSink.h"
#include "Debugger/IoEventLog.h"

class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventDecoder
//
//  The entries of the video and sound logs, from the I/O accesses a
//  machine's bus reports: an access to a video switch that changes the
//  video mode, a speaker toggle, a write to the Mockingboard's page, and
//  the AY register write the card makes of a change on port B's control
//  lines. Each access comes after the device has acted on it, so the mode
//  and the card's ports are read back from the machine.
//
//  Seed takes the video mode, the card's page and its control lines from
//  the machine as it is, so the first access is decoded right, on the
//  running machine and on one replaying history from a keyframe alike.
//
////////////////////////////////////////////////////////////////////////////////

class IoEventDecoder
{
public:
    //  A write to port B and the AY write it makes.
    static constexpr size_t  kMaxEvents = 2;

    using Events = std::array<IoEvent, kMaxEvents>;

    void    Seed   (MachineHost & machine);

    //  The entries one access makes, at most kMaxEvents, in outEvents; the
    //  count is returned. A speaker toggle, much the commonest, is filled in
    //  here, from the cycle counter Seed took.
    size_t  Decode (MachineHost & machine, Word address, Byte value, bool isWrite, Events & outEvents)
    {
        if (!IoEventLog::IsSpeaker (address))
        {
            return DecodeOther (machine, address, value, isWrite, outEvents);
        }

        outEvents[0]         = IoEvent();
        outEvents[0].cycle   = *m_cycles;
        outEvents[0].kind    = IoEventKind::Speaker;
        outEvents[0].address = address;
        return 1;
    }

    //  The accesses Decode makes entries of, for the bus's filter: video
    //  switches, the speaker, and writes to the card's page as seeded.
    void    GetFilter (IoAccessFilter & outFilter) const;

    static uint16_t  GetVideoModeBits (MachineHost & machine);

private:
    static constexpr int   kChipCount   = 2;
    static constexpr int   kNoBoard     = -1;
    static constexpr int   kPageShift   = 8;
    static constexpr Word  kChipSelect  = 0x80;
    static constexpr Byte  kRegMask     = 0x0F;
    static constexpr Byte  kRegOrb      = 0x0;
    static constexpr Byte  kAyControl   = 0x03;
    static constexpr Byte  kAyWrite     = 0x02;
    static constexpr Byte  kAyResetLow  = 0x04;

    size_t  DecodeOther (MachineHost & machine, Word address, Byte value, bool isWrite, Events & outEvents);
    size_t  DecodeBoard (MachineHost & machine, Word address, Byte value, uint64_t cycle, Events & outEvents);

    const uint64_t                 m_noCycles    = 0;
    const uint64_t               * m_cycles      = &m_noCycles;
    uint16_t                       m_videoBits   = 0;
    int                            m_boardPage   = kNoBoard;
    std::array<Byte, kChipCount>   m_lastControl = {};
};
