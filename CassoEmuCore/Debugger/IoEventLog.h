#pragma once

struct SoftSwitch;
struct Reply;





////////////////////////////////////////////////////////////////////////////////
//
//  IoEvent
//
//  One entry in a VIDEOLOG or SOUNDLOG: the machine's cycle count at the
//  access, the beam position then, and what happened. A video entry holds
//  the mode bits that went on and off; a Mockingboard entry the chip, the
//  register and the byte written.
//
////////////////////////////////////////////////////////////////////////////////

enum class IoEventKind
{
    VideoMode,
    Speaker,
    ViaWrite,
    AyWrite,
};

struct IoEvent
{
    uint64_t     cycle        = 0;
    uint64_t     sinceLast    = 0;      // cycles since the last entry of its kind, when hasSinceLast
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
    bool         hasSinceLast = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLog
//
//  The entries behind VIDEOLOG and SOUNDLOG, oldest first, up to kCapacity;
//  past that the oldest go. The total counts every entry since the log was
//  cleared, kept or not. A log that is off records nothing.
//
//  The video mode is the set of soft switches that change what the screen
//  shows, as bits (see GetVideoModeBits); an access that leaves them as they
//  were records nothing. Mockingboard writes are decoded as the card's AY
//  sees them: a write to port B whose control lines go to latch takes the
//  register from the last byte written to port A, and one that goes to write
//  records the AY register write, with port A's byte as its value.
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

    static constexpr size_t  kCapacity = 100000;

    //  Video mode bits, in the order the soft switch list gives them.
    static constexpr uint16_t  kText       = 0x01;
    static constexpr uint16_t  kMixed      = 0x02;
    static constexpr uint16_t  kPage2      = 0x04;
    static constexpr uint16_t  kHires      = 0x08;
    static constexpr uint16_t  k80Col      = 0x10;
    static constexpr uint16_t  kDhires     = 0x20;
    static constexpr uint16_t  kAltCharset = 0x40;
    static constexpr uint16_t  k80Store    = 0x80;

    static uint16_t  GetVideoModeBits   (const std::vector<SoftSwitch> & switches);
    static bool      IsVideoSwitch      (Word address, bool isWrite);
    static bool      IsSpeaker          (Word address);

    void    SetOn              (bool isOn) { m_isOn = isOn; }
    bool    IsOn               () const    { return m_isOn; }
    void    Clear              ();

    void    RecordVideo        (uint64_t cycle, uint32_t cycleInFrame, uint32_t scanline, uint32_t cycleInLine, Word address, uint16_t before, uint16_t after);
    void    RecordSpeaker      (uint64_t cycle, Word address);
    void    RecordMockingboard (uint64_t cycle, Word address, Byte value);

    const std::deque<IoEvent> &  GetEntries () const { return m_entries; }
    uint64_t                     GetTotal   () const { return m_total; }

    //  The last count entries, oldest first, after a line giving the total.
    void    Format             (size_t count, std::vector<std::string> & lines) const;

private:
    static constexpr int   kChipCount    = 2;
    static constexpr Word  kChipSelect   = 0x80;
    static constexpr Byte  kRegisterMask = 0x0F;
    static constexpr Byte  kRegOrb       = 0x0;
    static constexpr Byte  kRegOra       = 0x1;
    static constexpr Byte  kRegOraNh     = 0xF;
    static constexpr Byte  kAyControl    = 0x03;
    static constexpr Byte  kAyLatch      = 0x03;
    static constexpr Byte  kAyWrite      = 0x02;
    static constexpr Byte  kAyResetLow   = 0x04;

    void         Add            (IoEvent & entry, uint64_t & lastOfKind, bool & hasLastOfKind);
    void         DecodeAy       (int chip, Byte portB, uint64_t cycle);
    static void  FormatEntry    (const IoEvent & entry, std::string & line);
    static void  FormatModeBits (uint16_t bits, const char * state, std::string & text);

    std::deque<IoEvent>              m_entries;
    uint64_t                         m_total          = 0;
    uint64_t                         m_lastVideo      = 0;
    uint64_t                         m_lastSpeaker    = 0;
    uint64_t                         m_lastBoard      = 0;
    bool                             m_hasVideo       = false;
    bool                             m_hasSpeaker     = false;
    bool                             m_hasBoard       = false;
    std::array<Byte, kChipCount>     m_portA          = {};
    std::array<Byte, kChipCount>     m_latched        = {};
    std::array<Byte, kChipCount>     m_lastControl    = {};
    bool                             m_isOn           = false;
};
