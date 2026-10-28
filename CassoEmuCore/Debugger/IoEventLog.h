#pragma once

#include "Pch.h"

#include "Debugger/Events/DebugEventCache.h"

struct SoftSwitch;





////////////////////////////////////////////////////////////////////////////////
//
//  IoEvent
//
//  One entry in the video, sound, serial or network log: the machine's cycle
//  count at the access, the beam position then, and what happened. A video
//  entry has the mode bits that went on and off; a Mockingboard entry the
//  chip, the register and the byte written.
//
//  A serial entry has the port in `chip`, its 6551's base in `address`, the
//  byte in `value`, and the control and command registers and the line
//  levels (the kLine bits) as they were. A byte sent is stamped where its
//  start bit begins, and a byte received where its stop bit ends, with its
//  receive errors in `reg`. A connection entry has 1 in `value` when an
//  attachment's connection opened and 0 when it closed, and the far end's
//  address as text in the cache entry's bytes. A network
//  entry has the socket in `chip`; a state change the new status in `value`
//  and the old in `reg`, and data sent or received its length in `length`,
//  the bytes in the cache entry. Fixed in size, so an event cache stores
//  them without allocating.
//
////////////////////////////////////////////////////////////////////////////////

enum class IoEventKind : uint8_t
{
    VideoMode,
    Speaker,
    ViaWrite,
    AyWrite,
    SerialSettings,
    SerialLines,
    SerialSent,
    SerialReceived,
    SerialOverrun,
    SerialConnection,
    SocketState,
    SocketSent,
    SocketReceived,
};

struct IoEvent
{
    uint64_t     cycle        = 0;
    uint32_t     cycleInFrame = 0;
    uint32_t     scanline     = 0;
    uint32_t     cycleInLine  = 0;
    IoEventKind  kind         = IoEventKind::Speaker;
    Word         address      = 0;
    uint16_t     turnedOn     = 0;
    uint16_t     turnedOff    = 0;
    Byte         chip         = 0;
    Byte         reg          = 0;
    Byte         value        = 0;
    Byte         control      = 0;
    Byte         command      = 0;
    Byte         lines        = 0;
    uint16_t     length       = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLog
//
//  What the video and sound logs have in common: which accesses they take,
//  the video mode as bits, and the text VIDEOLOG and SOUNDLOG list and save.
//
//  The video mode is the set of display soft switches that are on, as bits
//  (see GetVideoModeBits).
//
////////////////////////////////////////////////////////////////////////////////

class IoEventLog
{
public:
    struct VideoModeName
    {
        uint16_t      bit;
        const char  * name;
    };

    //  Video mode bits, in the order the soft switch list gives them.
    static constexpr uint16_t  kText       = 0x01;
    static constexpr uint16_t  kMixed      = 0x02;
    static constexpr uint16_t  kPage2      = 0x04;
    static constexpr uint16_t  kHires      = 0x08;
    static constexpr uint16_t  k80Col      = 0x10;
    static constexpr uint16_t  kDhires     = 0x20;
    static constexpr uint16_t  kAltCharset = 0x40;
    static constexpr uint16_t  k80Store    = 0x80;

    //  A serial entry's line levels, each bit set while that line is on.
    static constexpr Byte  kLineDtr = 0x01;
    static constexpr Byte  kLineRts = 0x02;
    static constexpr Byte  kLineCts = 0x04;
    static constexpr Byte  kLineDsr = 0x08;
    static constexpr Byte  kLineDcd = 0x10;
    static constexpr Byte  kLineIrq = 0x20;

    //  The accesses that can change the video mode: any access to
    //  $C050-$C05F (the display switches and, on the //e, double hi-res with
    //  annunciator 3), and a write to $C000-$C00F (80STORE, 80COL and
    //  ALTCHARSET); reads there are the keyboard. Any access to $C030-$C03F
    //  toggles the speaker.
    static constexpr Word  kIieFirst     = 0xC000;
    static constexpr Word  kIieLast      = 0xC00F;
    static constexpr Word  kSpeakerFirst = 0xC030;
    static constexpr Word  kSpeakerLast  = 0xC03F;
    static constexpr Word  kDisplayFirst = 0xC050;
    static constexpr Word  kDisplayLast  = 0xC05F;

    static uint16_t  GetVideoModeBits (const std::vector<SoftSwitch> & switches);

    static bool  IsVideoSwitch (Word address, bool isWrite)
    {
        return (address >= kDisplayFirst && address <= kDisplayLast) || (isWrite && address >= kIieFirst && address <= kIieLast);
    }

    static bool  IsSpeaker (Word address)
    {
        return address >= kSpeakerFirst && address <= kSpeakerLast;
    }

    //  A line for each entry, oldest first, a video entry under a line for
    //  the frame it fell in.
    static void      FormatEntries    (const std::vector<DebugEventEntry<IoEvent>> & entries, std::vector<std::string> & lines);

    //  What one entry did, as its line in a listing ends: the modes a video
    //  switch turned on and off, "speaker", or a Mockingboard register write.
    static std::string  FormatEvent   (const IoEvent & entry);

    //  "CTS DSR DCD" for the kLine bits set, or "none"; and ", framing
    //  error" or ", parity and framing errors" for a received byte's
    //  receive errors, empty for none.
    static std::string  FormatSerialLines   (Byte lines);
    static std::string  FormatReceiveErrors (Byte errors);

    //  A serial entry's port: "Slot 2" for a card, "Port 1" for one of the
    //  //c's built-in ports.
    static std::string  GetSerialPortName   (Byte port);

private:
    static constexpr Byte  kRegisterMask = 0x0F;

    static void  FormatEntry    (const IoEvent & entry, uint64_t since, bool hasSince, std::string & line);
    static void  FormatModeBits (uint16_t bits, const char * state, std::string & text);
};
