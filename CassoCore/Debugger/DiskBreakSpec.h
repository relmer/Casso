#pragma once

#include "DiskFieldKind.h"
#include "DiskMarkPattern.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk breakpoint types
//
//  What BPDISK was given, parsed in CassoCore so every mode's parser builds
//  the same value. The machine has one Disk II controller, so a breakpoint
//  takes no slot.
//
////////////////////////////////////////////////////////////////////////////////

enum class DiskEvent
{
    SectorRead,
    AddressField,
    Head,
    WriteMode,
    WritePrologue,
    WriteBlocked,
    DosSector,
    ProDosBlock,
    RwtsCall,
    DriverCall,
    Motor,
    DriveSelect,
    Inserted,
    Ejected,
};

enum class DiskDriveFilter
{
    Either,
    Drive1,
    Drive2,
};

enum class DiskHeadFilter
{
    Any,
    Range,
    Half,
};

enum class DiskMotorFilter
{
    Any,
    On,
    Off,
    Stopped,
};

enum class DiskMotorEvent
{
    On,
    Off,
    Stopped,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakSpec
//
//  The event and its filters. A track, sector or volume left out matches any;
//  the sector is physical for a sector read, an address field and a written
//  prologue, logical for a DOS sector, and the IOB's for an RWTS call. A T
//  range is stored as its whole tracks' quarter tracks with isWholeTracks set,
//  so only positions that are a multiple of four match. Custom marks replace
//  the standard ones for this breakpoint, and format is the length of a data
//  field after custom address marks; with the standard marks the length
//  follows from each field's prologue.
//
////////////////////////////////////////////////////////////////////////////////

struct DiskBreakSpec
{
    static constexpr int  kMaxQuarterTrack = 139;

    DiskEvent                       event          = DiskEvent::SectorRead;
    DiskDriveFilter                 drive          = DiskDriveFilter::Either;
    std::optional<Byte>             track;
    std::optional<Byte>             sector;
    std::optional<Byte>             volume;
    std::optional<Word>             block;
    DiskHeadFilter                  head           = DiskHeadFilter::Any;
    int                             qtFirst        = 0;
    int                             qtLast         = kMaxQuarterTrack;
    bool                            isWholeTracks  = false;
    DiskMotorFilter                 motor          = DiskMotorFilter::Any;
    std::optional<Byte>             command;
    bool                            isPassed       = false;     // PASSED: every data field read in full counts
    bool                            isBadChecksum  = false;     // BADSUM
    bool                            isWrongTrack   = false;     // WRONGTRACK
    bool                            isBump         = false;     // BUMP
    bool                            isEvery        = false;     // EVERY, on SELECT
    std::optional<DiskMarkPattern>  addressMarks;
    std::optional<DiskMarkPattern>  dataMarks;
    DiskFieldKind                   format         = DiskFieldKind::Sixteen;

    bool  operator== (const DiskBreakSpec & other) const = default;
};
