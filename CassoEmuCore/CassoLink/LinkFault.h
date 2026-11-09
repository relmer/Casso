#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFault
//
//  A fault injected into a link, to test how the link recovers. A fault from
//  the PC changes a byte the PC sends: the host's, counted from the start of
//  the operation the fault is taken for, a serial bootstrap's, or the
//  cassette audio's. A fault from the Apple is sent to the Apple's program in
//  a Fault command, which the program has only when assembled for a Debug
//  build: a byte of the Apple's reply to one of the operation's requests, or
//  a sector, the first time the Apple reads it.
//
//  As text, the form --link-faults takes, a list separated by semicolons:
//
//      pc:flip@40              the PC's byte at offset 40, its low bit flipped
//      pc:drop@40              that byte left out
//      pc:stall@40:8           that byte sent after 8 quarter seconds
//      apple:flip@300#2        byte 300 of the reply to the operation's third
//                              request; drop and stall as for the PC
//      apple:unreadable@T17S3  the sector reported as not read
//      apple:baddata@T17S3     the sector's data changed, with a CRC that
//                              matches the change
//      apple:badcrc@T17S3      a wrong CRC for the sector in Read CRCs
//      bootstrap:flip@12       a serial bootstrap's byte; drop and stall too
//      cassette:flip@200       a byte of the cassette audio's data
//
////////////////////////////////////////////////////////////////////////////////

struct LinkFault
{
public:
    enum class Source { Pc, Apple, Bootstrap, Cassette };

    //  The values the Apple's program has for them.
    enum class Kind : Byte { Flip = 1, Drop = 2, Stall = 3, Unreadable = 4, BadData = 5, BadCrc = 6 };

    static constexpr DWORD   kQuarterMs       = 250;
    static constexpr Byte    kDefaultQuarters = 8;
    static constexpr Byte    kLastTrack       = 34;
    static constexpr Byte    kLastSector      = 15;
    static constexpr size_t  kPayloadSize     = 5;

    Source    source   = Source::Pc;
    Kind      kind     = Kind::Flip;
    uint32_t  offset   = 0;      // a byte's, from the first counted
    int       request  = 0;      // the Apple's: the operation's request whose reply it changes
    Byte      track    = 0;
    Byte      sector   = 0;
    Byte      quarters = 0;      // a stall's length

    bool  IsSectorFault () const { return kind >= Kind::Unreadable; }

    static HRESULT            Parse          (const std::string & text, std::vector<LinkFault> & outFaults);
    static HRESULT            ParseOne       (const std::string & text, LinkFault & outFault);
    static std::string        Describe       (const LinkFault & fault);
    static std::vector<Byte>  MakePayload    (const LinkFault & fault);

private:
    static HRESULT  ParseSector    (const std::string & where, LinkFault & inOutFault);
    static HRESULT  ParseByte      (const std::string & text, LinkFault & inOutFault);
    static bool     TryParseNumber (const std::string & text, uint32_t limit, uint32_t & outValue);
};





////////////////////////////////////////////////////////////////////////////////
//
//  LinkFaults
//
//  The faults armed in this process, taken one at a time: a link takes the
//  next PC or Apple fault as each operation starts, a serial bootstrap the
//  next bootstrap fault, and the cassette audio the next cassette fault.
//  Faults are armed only once Enable is called, which a Debug build of Casso
//  Explorer does; the Apple's program has the Fault command only then.
//
////////////////////////////////////////////////////////////////////////////////

class LinkFaults
{
public:
    using ReportFn = std::function<void (const std::string & text)>;

    static void  Enable      ();
    static void  Disable     ();   // and the armed faults cleared, for tests
    static bool  IsEnabled   ();
    static void  Arm         (const std::vector<LinkFault> & faults);
    static bool  TryTake     (std::initializer_list<LinkFault::Source> sources, LinkFault & outFault);
    static void  Clear       ();

    //  Where a fault applied outside a link is logged.
    static void  SetReporter (ReportFn report);
    static void  Report      (const std::string & text);

private:
    struct State
    {
        std::mutex              lock;
        bool                    enabled = false;
        std::deque<LinkFault>   armed;
        ReportFn                report;
    };

    static State &  GetState ();
};





////////////////////////////////////////////////////////////////////////////////
//
//  LinkByteFault
//
//  A byte fault applied to what is sent, counting the bytes from when it is
//  armed. Each write is passed through Apply, which returns the bytes to
//  send: as they are, or with the fault's byte flipped or left out, and where
//  to stall first.
//
////////////////////////////////////////////////////////////////////////////////

class LinkByteFault
{
public:
    static constexpr size_t  kNoStall = SIZE_MAX;

    void  Arm      (const LinkFault & fault);
    bool  IsArmed  () const { return m_armed; }

    //  outStallAt is the index of the byte to send after the stall, or
    //  kNoStall; outReport is the text to log when the fault was applied.
    void  Apply    (std::span<const Byte> data, std::vector<Byte> & outBytes, size_t & outStallAt, DWORD & outStallMs, std::string & outReport);

private:
    LinkFault  m_fault;
    bool       m_armed = false;
    uint64_t   m_sent  = 0;
};
