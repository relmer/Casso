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
//  The payloads the three panel graphics draw: which bank each page reads and
//  writes, where the disk head is, and a set of levels from 0 to 1.
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

using DiagnosticsVisual = std::variant<std::monostate, DiagnosticsMemoryMap, DiagnosticsDiskHead, DiagnosticsMeters>;





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
