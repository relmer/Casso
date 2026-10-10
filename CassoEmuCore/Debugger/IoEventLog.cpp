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
//  IoEventLog::IsVideoSwitch
//
//  The accesses that can change the video mode: any access to $C050-$C05F
//  (the display switches and, on the //e, double hi-res with annunciator
//  3), and a write to $C000-$C00F (80STORE, 80COL and ALTCHARSET). Reads in
//  $C000-$C00F are the keyboard, which changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool IoEventLog::IsVideoSwitch (Word address, bool isWrite)
{
    constexpr Word  kDisplayFirst = 0xC050;
    constexpr Word  kDisplayLast  = 0xC05F;
    constexpr Word  kIieFirst     = 0xC000;
    constexpr Word  kIieLast      = 0xC00F;



    return (address >= kDisplayFirst && address <= kDisplayLast) ||
           (isWrite && address >= kIieFirst && address <= kIieLast);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLog::IsSpeaker
//
//  $C030-$C03F: any access toggles the speaker.
//
////////////////////////////////////////////////////////////////////////////////

bool IoEventLog::IsSpeaker (Word address)
{
    constexpr Word  kSpeakerFirst = 0xC030;
    constexpr Word  kSpeakerLast  = 0xC03F;



    return address >= kSpeakerFirst && address <= kSpeakerLast;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLog::Clear
//
//  The entries, the total and the cycle each kind last happened at go; the
//  ports the Mockingboard decode follows stay, since the card keeps them.
//
////////////////////////////////////////////////////////////////////////////////

void IoEventLog::Clear()
{
    m_entries.clear();
    m_total      = 0;
    m_hasVideo   = false;
    m_hasSpeaker = false;
    m_hasBoard   = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLog::Add
//
////////////////////////////////////////////////////////////////////////////////

void IoEventLog::Add (IoEvent & entry, uint64_t & lastOfKind, bool & hasLastOfKind)
{
    if (hasLastOfKind && entry.cycle >= lastOfKind)
    {
        entry.sinceLast    = entry.cycle - lastOfKind;
        entry.hasSinceLast = true;
    }

    lastOfKind    = entry.cycle;
    hasLastOfKind = true;

    if (m_entries.size() >= kCapacity)
    {
        m_entries.pop_front();
    }

    m_entries.push_back (entry);
    ++m_total;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLog::RecordVideo
//
////////////////////////////////////////////////////////////////////////////////

void IoEventLog::RecordVideo (
    uint64_t  cycle,
    uint32_t  cycleInFrame,
    uint32_t  scanline,
    uint32_t  cycleInLine,
    Word      address,
    uint16_t  before,
    uint16_t  after)
{
    IoEvent  entry;



    if (!m_isOn || before == after)
    {
        return;
    }

    entry.cycle        = cycle;
    entry.cycleInFrame = cycleInFrame;
    entry.scanline     = scanline;
    entry.cycleInLine  = cycleInLine;
    entry.kind         = IoEventKind::VideoMode;
    entry.address      = address;
    entry.turnedOn     = (uint16_t) (after & ~before);
    entry.turnedOff    = (uint16_t) (before & ~after);

    Add (entry, m_lastVideo, m_hasVideo);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLog::RecordSpeaker
//
////////////////////////////////////////////////////////////////////////////////

void IoEventLog::RecordSpeaker (uint64_t cycle, Word address)
{
    IoEvent  entry;



    if (!m_isOn)
    {
        return;
    }

    entry.cycle   = cycle;
    entry.kind    = IoEventKind::Speaker;
    entry.address = address;

    Add (entry, m_lastSpeaker, m_hasSpeaker);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLog::RecordMockingboard
//
//  A write to the card's page: bit 7 of the address picks the chip and the
//  low four bits the 6522 register. Port A is followed even while the log is
//  off, so the first AY write after it is turned on carries the right value.
//
////////////////////////////////////////////////////////////////////////////////

void IoEventLog::RecordMockingboard (uint64_t cycle, Word address, Byte value)
{
    IoEvent  entry;
    int      chip  = (address & kChipSelect) ? 1 : 0;
    Byte     reg   = (Byte) (address & kRegisterMask);



    if (reg == kRegOra || reg == kRegOraNh)
    {
        m_portA[chip] = value;
    }

    if (!m_isOn)
    {
        return;
    }

    entry.cycle   = cycle;
    entry.kind    = IoEventKind::ViaWrite;
    entry.address = address;
    entry.chip    = (Byte) chip;
    entry.reg     = reg;
    entry.value   = value;

    Add (entry, m_lastBoard, m_hasBoard);

    if (reg == kRegOrb)
    {
        DecodeAy (chip, value, cycle);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLog::DecodeAy
//
//  Port B's control lines, as the card's SyncPsg follows them: reset low
//  clears the decode; a change to latch takes the register from port A, and
//  a change to write records the write.
//
////////////////////////////////////////////////////////////////////////////////

void IoEventLog::DecodeAy (int chip, Byte portB, uint64_t cycle)
{
    Byte     control = (Byte) (portB & kAyControl);
    IoEvent  entry;



    if ((portB & kAyResetLow) == 0)
    {
        m_lastControl[chip] = 0;
        return;
    }

    if (control == m_lastControl[chip])
    {
        return;
    }

    m_lastControl[chip] = control;

    if (control == kAyLatch)
    {
        m_latched[chip] = m_portA[chip];
        return;
    }

    if (control != kAyWrite)
    {
        return;
    }

    entry.cycle = cycle;
    entry.kind  = IoEventKind::AyWrite;
    entry.chip  = (Byte) chip;
    entry.reg   = m_latched[chip];
    entry.value = m_portA[chip];

    Add (entry, m_lastBoard, m_hasBoard);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLog::Format
//
//  A video entry is grouped under the frame it fell in, by the cycle the
//  frame began at, so a frame's changes read together.
//
////////////////////////////////////////////////////////////////////////////////

void IoEventLog::Format (size_t count, std::vector<std::string> & lines) const
{
    size_t       shown      = std::min (count, m_entries.size());
    size_t       first      = m_entries.size() - shown;
    uint64_t     frameStart = 0;
    bool         hasFrame   = false;
    std::string  line;



    if (m_total == 0)
    {
        lines.push_back ("No entries.");
        return;
    }

    lines.push_back (std::format ("{} entries since the log was cleared; the last {}:", m_total, shown));

    for (size_t index = first; index < m_entries.size(); ++index)
    {
        const IoEvent  & entry = m_entries[index];

        if (entry.kind == IoEventKind::VideoMode && (!hasFrame || entry.cycle - entry.cycleInFrame != frameStart))
        {
            frameStart = entry.cycle - entry.cycleInFrame;
            hasFrame   = true;
            lines.push_back (std::format ("Frame from cycle {}:", frameStart));
        }

        FormatEntry (entry, line);
        lines.push_back (line);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLog::FormatEntry
//
////////////////////////////////////////////////////////////////////////////////

void IoEventLog::FormatEntry (const IoEvent & entry, std::string & line)
{
    std::string  since = !entry.hasSinceLast ? std::string() : std::format ("+{}", entry.sinceLast);
    std::string  modes;



    switch (entry.kind)
    {
    case IoEventKind::VideoMode:
        FormatModeBits (entry.turnedOn,  "on",  modes);
        FormatModeBits (entry.turnedOff, "off", modes);
        line = std::format ("  cycle {:>12}  line {:>3}  h {:>2}  ${:04X}  {}", entry.cycle, entry.scanline, entry.cycleInLine, entry.address, modes);
        break;

    case IoEventKind::Speaker:
        line = std::format ("cycle {:>12}  {:>8}  ${:04X}  speaker", entry.cycle, since, entry.address);
        break;

    case IoEventKind::ViaWrite:
        line = std::format ("cycle {:>12}  {:>8}  ${:04X}  Mockingboard chip {} {} = ${:02X}",
                            entry.cycle, since, entry.address, entry.chip + 1, s_kViaRegisterNames[entry.reg & kRegisterMask], entry.value);
        break;

    case IoEventKind::AyWrite:
        line = std::format ("cycle {:>12}  {:>8}         Mockingboard chip {} AY R{:02} = ${:02X}",
                            entry.cycle, since, entry.chip + 1, entry.reg, entry.value);
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
