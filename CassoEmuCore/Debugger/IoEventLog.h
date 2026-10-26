#pragma once

#include "Pch.h"

#include "Debugger/Events/DebugEventCache.h"

struct SoftSwitch;





////////////////////////////////////////////////////////////////////////////////
//
//  IoEvent
//
//  One entry in the video log or the sound log: the machine's cycle count
//  at the access, the beam position then, and what happened. A video entry
//  has the mode bits that went on and off; a Mockingboard entry the chip,
//  the register and the byte written. Fixed in size, so an event cache
//  stores them without allocating.
//
////////////////////////////////////////////////////////////////////////////////

enum class IoEventKind : uint8_t
{
    VideoMode,
    Speaker,
    ViaWrite,
    AyWrite,
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

private:
    static constexpr Byte  kRegisterMask = 0x0F;

    static void  FormatEntry    (const IoEvent & entry, uint64_t since, bool hasSince, std::string & line);
    static void  FormatModeBits (uint16_t bits, const char * state, std::string & text);
};
