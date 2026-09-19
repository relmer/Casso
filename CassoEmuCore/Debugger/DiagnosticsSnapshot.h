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

struct DiagnosticsMemoryMap
{
    static constexpr size_t  kPageCount = 256;

    struct Page
    {
        MemorySource  read  = MemorySource::None;
        MemorySource  write = MemorySource::None;
    };

    std::array<Page, kPageCount>  pages = {};
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
