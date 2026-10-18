#include "Pch.h"

#include "Debugger/IoEventLog.h"
#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kVideoModeNames
//
//  Each video mode bit and the soft switch it comes from, in bit order.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr IoEventLog::VideoModeName s_kVideoModeNames[] =
{
    { IoEventLog::kText,       "TEXT"       },
    { IoEventLog::kMixed,      "MIXED"      },
    { IoEventLog::kPage2,      "PAGE2"      },
    { IoEventLog::kHires,      "HIRES"      },
    { IoEventLog::k80Col,      "80COL"      },
    { IoEventLog::kDhires,     "DHIRES"     },
    { IoEventLog::kAltCharset, "ALTCHARSET" },
    { IoEventLog::k80Store,    "80STORE"    },
};





////////////////////////////////////////////////////////////////////////////////
//
//  s_kViaRegisterNames
//
//  The 6522's sixteen registers, by number.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr const char * s_kViaRegisterNames[] =
{
    "ORB",  "ORA",  "DDRB", "DDRA",
    "T1CL", "T1CH", "T1LL", "T1LH",
    "T2CL", "T2CH", "SR",   "ACR",
    "PCR",  "IFR",  "IER",  "ORA (no handshake)",
};





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLog::GetVideoModeBits
//
////////////////////////////////////////////////////////////////////////////////

uint16_t IoEventLog::GetVideoModeBits (const std::vector<SoftSwitch> & switches)
{
    uint16_t  bits = 0;



    for (const SoftSwitch & entry : switches)
    {
        if (!entry.value)
        {
            continue;
        }

        for (const IoEventLog::VideoModeName & mode : s_kVideoModeNames)
        {
            if (entry.name == mode.name)
            {
                bits |= mode.bit;
            }
        }
    }

    return bits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLog::FormatEntries
//
//  A video entry is grouped under the frame it fell in, by the cycle the
//  frame began at, so a frame's changes read together. Each speaker or
//  Mockingboard entry has the cycles since the one before it of its own
//  kind, the card's writes counting as one kind. A gap in recording, or a
//  stretch that cannot be rebuilt, has a line of its own, and the entries
//  before it are not counted from.
//
////////////////////////////////////////////////////////////////////////////////

void IoEventLog::FormatEntries (
    const std::vector<DebugEventEntry<IoEvent>>  & entries,
    std::vector<std::string>                     & lines)
{
    uint64_t     frameStart  = 0;
    uint64_t     lastSpeaker = 0;
    uint64_t     lastBoard   = 0;
    bool         hasFrame    = false;
    bool         hasSpeaker  = false;
    bool         hasBoard    = false;
    std::string  line;



    for (const DebugEventEntry<IoEvent> & entry : entries)
    {
        const IoEvent  & event    = entry.event;
        bool             isVideo  = event.kind == IoEventKind::VideoMode;
        bool             isSound  = event.kind == IoEventKind::Speaker;
        uint64_t       & last     = isSound ? lastSpeaker : lastBoard;
        bool           & hasLast  = isSound ? hasSpeaker  : hasBoard;
        bool             hasSince = false;

        if (entry.kind != DebugEventEntryKind::Event)
        {
            lines.push_back ((entry.kind == DebugEventEntryKind::Gap) ? "History recording paused here." : "The entries before here cannot be rebuilt: other disks were in the drives.");

            hasFrame   = false;
            hasSpeaker = false;
            hasBoard   = false;
            continue;
        }

        if (isVideo && (!hasFrame || event.cycle - event.cycleInFrame != frameStart))
        {
            frameStart = event.cycle - event.cycleInFrame;
            hasFrame   = true;
            lines.push_back (std::format ("Frame from cycle {}:", frameStart));
        }

        hasSince = !isVideo && hasLast && event.cycle >= last;

        FormatEntry (event, hasSince ? event.cycle - last : 0, hasSince, line);
        lines.push_back (line);

        if (!isVideo)
        {
            last    = event.cycle;
            hasLast = true;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLog::FormatEntry
//
////////////////////////////////////////////////////////////////////////////////

void IoEventLog::FormatEntry (
    const IoEvent  & entry,
    uint64_t         since,
    bool             hasSince,
    std::string    & line)
{
    std::string  sinceText = hasSince ? std::format ("+{}", since) : std::string();
    std::string  modes;



    switch (entry.kind)
    {
    case IoEventKind::VideoMode:
        FormatModeBits (entry.turnedOn,  "on",  modes);
        FormatModeBits (entry.turnedOff, "off", modes);
        line = std::format ("  cycle {:>12}  line {:>3}  h {:>2}  ${:04X}  {}", entry.cycle, entry.scanline, entry.cycleInLine, entry.address, modes);
        break;

    case IoEventKind::Speaker:
        line = std::format ("cycle {:>12}  {:>8}  ${:04X}  speaker", entry.cycle, sinceText, entry.address);
        break;

    case IoEventKind::ViaWrite:
        line = std::format ("cycle {:>12}  {:>8}  ${:04X}  Mockingboard chip {} {} = ${:02X}",
                            entry.cycle, sinceText, entry.address, entry.chip + 1, s_kViaRegisterNames[entry.reg & kRegisterMask], entry.value);
        break;

    case IoEventKind::AyWrite:
        line = std::format ("cycle {:>12}  {:>8}         Mockingboard chip {} AY R{:02} = ${:02X}",
                            entry.cycle, sinceText, entry.chip + 1, entry.reg, entry.value);
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLog::FormatModeBits
//
////////////////////////////////////////////////////////////////////////////////

void IoEventLog::FormatModeBits (uint16_t bits, const char * state, std::string & text)
{
    for (const IoEventLog::VideoModeName & mode : s_kVideoModeNames)
    {
        if ((bits & mode.bit) == 0)
        {
            continue;
        }

        if (!text.empty())
        {
            text += ", ";
        }

        text += std::format ("{} {}", mode.name, state);
    }
}
