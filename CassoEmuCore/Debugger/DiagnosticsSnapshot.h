#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Diagnostics rows
//
//  What one device publishes for its panel: named groups of rows, each a label
//  and a value, and optionally a decode giving each bit of the value a name.
//
////////////////////////////////////////////////////////////////////////////////

struct DiagnosticsBit
{
    std::string  name;
    bool         set = false;
};

struct DiagnosticsRow
{
    std::string                  label;
    std::string                  value;
    std::vector<DiagnosticsBit>  bits;
    bool                         isSwitch = false;     // value is on or off
    std::string                  tip;                  // what the row's setting does; empty for none
};

struct DiagnosticsGroup
{
    std::string                  title;
    std::vector<DiagnosticsRow>  rows;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Diagnostics visuals
//
//  The payloads the panel graphics draw: which bank each page reads and writes,
//  where the disk head is, a set of levels from 0 to 1, and a sound card's chips.
//
////////////////////////////////////////////////////////////////////////////////

enum class MemorySource
{
    None,       // a write that goes nowhere
    Main,
    Aux,
    LcBank1,
    LcBank2,
    Rom,
    SlotRom,
    Io,
    Count,
};

//  One of a device's bus registers and what it is, for the debugger's tips:
//  "Super Serial Card, slot 2: 6551 status register".
struct DiagnosticsIoRegister
{
    Word         address = 0;
    std::string  description;
};

struct DiagnosticsMemoryMap
{
    static constexpr size_t  kPageCount = 256;

    struct Page
    {
        MemorySource  read  = MemorySource::None;
        MemorySource  write = MemorySource::None;
    };

    std::array<Page, kPageCount>  pages         = {};
    int                           expansionSlot = 0;     // the card's slot at $C800-$CEFF, 0 for none
};

struct DiagnosticsDiskHead
{
    int   quarterTrack    = 0;
    int   maxQuarterTrack = 0;
    Byte  phases          = 0;      // bit n is phase magnet n
    bool  motorOn         = false;
    int   drive           = 0;

    //  Each drive's quarter track: the selected drive's is quarterTrack,
    //  and the other's is where its head was left.
    std::array<int, 2>  quarterTracks = {};
};

struct DiagnosticsMeters
{
    struct Level
    {
        std::string  name;
        float        level = 0.0f;  // 0 to 1

        //  When set, a steady state shown in place of the bar, for a value
        //  that moves too fast or has nothing to measure against.
        std::string  status;
    };

    std::vector<Level>  levels;
};

//  One AY channel in a sound board. The scope has the channel's last samples,
//  oldest first, as they are before any mute.
struct DiagnosticsSoundChannel
{
    double              frequencyHz  = 0.0;
    int                 period       = 0;
    int                 level        = 0;        // 0 to 15
    bool                isToneOn     = false;
    bool                isNoiseOn    = false;
    bool                usesEnvelope = false;
    bool                isMuted      = false;
    std::vector<float>  scope;
};

struct DiagnosticsSoundPsg
{
    static constexpr int  kChannelCount = 3;

    std::array<DiagnosticsSoundChannel, kChannelCount>  channels;

    Byte      envelopeRegister = 0;
    int       envelopePeriod   = 0;
    int       envelopeLevel    = 0;      // 0 to 15
    double    envelopeRampMs   = 0.0;
    int       noisePeriod      = 0;
    double    noiseHz          = 0.0;
    Byte      mixer            = 0;
    uint32_t  writeCount       = 0;
    bool      hasLastWrite     = false;
    Byte      lastWriteReg     = 0;
    Byte      lastWriteValue   = 0;
};

struct DiagnosticsSoundVia
{
    Byte      ora        = 0;
    Byte      orb        = 0;
    Byte      ddra       = 0;
    Byte      ddrb       = 0;
    Byte      acr        = 0;
    Byte      pcr        = 0;
    Byte      ifr        = 0;
    Byte      ier        = 0;
    uint16_t  t1Count    = 0;
    uint16_t  t1Latch    = 0;
    uint16_t  t2Count    = 0;
    double    t1RateHz   = 0.0;          // 0 unless T1 is free running
    bool      isRunning  = false;
    bool      isFired    = false;
    bool      isT1Irq    = false;
    bool      isIrq      = false;
};

//  One half of the card: a 6522 and the AY it drives.
struct DiagnosticsSoundHalf
{
    int                  index       = 0;    // from 0
    Word                 baseAddress = 0;
    std::string          busState;
    DiagnosticsSoundVia  via;
    DiagnosticsSoundPsg  psg;
};

struct DiagnosticsSoundVoice
{
    struct Phoneme
    {
        std::string  name;
        float        seconds = 0.0f;
    };

    static constexpr int  kFormantCount = 3;

    std::string                         phoneme;    // its name and example word
    Byte                                phonemeCode      = 0;
    std::vector<Phoneme>                recent;         // oldest first
    std::array<double, kFormantCount>   formantsHz       = {};
    double                              pitchHz          = 0.0;
    float                               level            = 0.0f;     // 0 to 1
    Byte                                durationSelect   = 0;
    double                              durationMs       = 0.0;
    Byte                                rateSelect       = 0;
    double                              frameMs          = 0.0;
    Byte                                articulation     = 0;
    Byte                                amplitude        = 0;
    Byte                                filterRegister   = 0;
    double                              filterHz         = 0.0;
    uint16_t                            inflection       = 0;
    std::string                         mode;
    bool                                isPoweredDown    = false;
    bool                                isSounding       = false;
    bool                                isRequesting     = false;
    bool                                isMuted          = false;
    std::vector<float>                  scope;
};

struct DiagnosticsSoundBoard
{
    int                                   slot       = 0;
    uint32_t                              sampleRate = 0;   // of the scopes' samples
    std::vector<DiagnosticsSoundHalf>     halves;
    std::optional<DiagnosticsSoundVoice>  voice;
};

using DiagnosticsVisual = std::variant<std::monostate, DiagnosticsMemoryMap, DiagnosticsDiskHead, DiagnosticsMeters, DiagnosticsSoundBoard>;





////////////////////////////////////////////////////////////////////////////////
//
//  DiagnosticsSnapshot
//
//  One device's panel as plain data, built on the CPU thread and read on the UI
//  thread. `id` is the provider's stable id and `device` the panel's title.
//
////////////////////////////////////////////////////////////////////////////////

struct DiagnosticsSnapshot
{
    std::string                    id;
    std::string                    device;
    std::vector<DiagnosticsGroup>  groups;
    DiagnosticsVisual              visual;
};
