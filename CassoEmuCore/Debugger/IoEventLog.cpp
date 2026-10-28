#include "Pch.h"

#include "Debugger/IoEventLog.h"
#include "Debugger/Reply.h"
#include "Devices/Acia6551.h"





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



    switch (entry.kind)
    {
    case IoEventKind::VideoMode:
        line = std::format ("  cycle {:>12}  line {:>3}  h {:>2}  ${:04X}  {}", entry.cycle, entry.scanline, entry.cycleInLine, entry.address, FormatEvent (entry));
        break;

    case IoEventKind::Speaker:
    case IoEventKind::ViaWrite:
        line = std::format ("cycle {:>12}  {:>8}  ${:04X}  {}", entry.cycle, sinceText, entry.address, FormatEvent (entry));
        break;

    case IoEventKind::AyWrite:
        line = std::format ("cycle {:>12}  {:>8}         {}", entry.cycle, sinceText, FormatEvent (entry));
        break;

    default:
        line = std::format ("cycle {:>12}  {}", entry.cycle, FormatEvent (entry));
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLog::FormatEvent
//
////////////////////////////////////////////////////////////////////////////////

std::string IoEventLog::FormatEvent (const IoEvent & entry)
{
    std::string  text;



    switch (entry.kind)
    {
    case IoEventKind::VideoMode:
        FormatModeBits (entry.turnedOn,  "on",  text);
        FormatModeBits (entry.turnedOff, "off", text);
        break;

    case IoEventKind::Speaker:
        text = "speaker";
        break;

    case IoEventKind::ViaWrite:
        text = std::format ("Mockingboard chip {} {} = ${:02X}", entry.chip + 1, s_kViaRegisterNames[entry.reg & kRegisterMask], entry.value);
        break;

    case IoEventKind::AyWrite:
        text = std::format ("Mockingboard chip {} AY R{:02} = ${:02X}", entry.chip + 1, entry.reg, entry.value);
        break;

    case IoEventKind::SerialSettings:
        text = std::format ("{} command ${:02X} control ${:02X}", GetSerialPortName (entry.chip), entry.command, entry.control);
        break;

    case IoEventKind::SerialLines:
        text = std::format ("{} lines ${:02X}", GetSerialPortName (entry.chip), entry.lines);
        break;

    case IoEventKind::SerialSent:
        text = std::format ("{} sent ${:02X}", GetSerialPortName (entry.chip), entry.value);
        break;

    case IoEventKind::SerialReceived:
        text = std::format ("{} received ${:02X}", GetSerialPortName (entry.chip), entry.value);
        break;

    case IoEventKind::SerialOverrun:
        text = std::format ("{} overrun", GetSerialPortName (entry.chip));
        break;

    case IoEventKind::SocketState:
        text = std::format ("socket {} state ${:02X} from ${:02X}", entry.chip, entry.value, entry.reg);
        break;

    case IoEventKind::SocketSent:
        text = std::format ("socket {} sent {} bytes", entry.chip, entry.length);
        break;

    case IoEventKind::SocketReceived:
        text = std::format ("socket {} received {} bytes", entry.chip, entry.length);
        break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLog::GetSerialPortName
//
////////////////////////////////////////////////////////////////////////////////

std::string IoEventLog::GetSerialPortName (Byte port)
{
    if (port & Acia6551::kCommPortBuiltIn)
    {
        return std::format ("Port {}", port & ~Acia6551::kCommPortBuiltIn);
    }

    return std::format ("Slot {}", port);
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
