#pragma once

#include "Pch.h"

#include "Audio/IDriveAudioEventSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EventCategory
//
//  Tags a DiskIIEvent ring entry as a controller-side event or an
//  audio-side event so the UI-thread formatter routes to the right
//  detail template (FR-023). Stays uint8_t to keep DiskIIEvent at 24
//  bytes total.
//
////////////////////////////////////////////////////////////////////////////////

enum class EventCategory : uint8_t
{
    Controller = 0,
    Audio      = 1,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIEventType
//
//  Concrete event identifier. Controller-side and audio-side event
//  types share one enum so the consumer can switch on a single field;
//  the EventCategory tag tells the formatter which sub-table to use
//  for the Detail column.
//
//  EventsLost is a UI-thread synthetic entry inserted by the projection
//  helper when the producer's overflow counter is non-zero on drain
//  (FR-010). It never originates from the controller or audio side.
//
////////////////////////////////////////////////////////////////////////////////

enum class DiskIIEventType : uint8_t
{
    // Controller-side
    MotorCommandOn      = 0,
    MotorEngaged        = 1,
    MotorCommandOff     = 2,
    MotorDisengaged     = 3,
    HeadStep            = 4,
    HeadBump            = 5,
    AddrMark            = 6,
    DataRead            = 7,
    DataWrite           = 8,    // declared but never fires in v1 (A-010)
    DriveSelect         = 9,
    DiskInserted        = 10,
    DiskEjected         = 11,
    EventsLost          = 12,   // synthetic; producer-side overflow marker

    // Audio-side
    AudioStarted        = 13,
    AudioRestarted      = 14,
    AudioContinued      = 15,
    AudioSilent         = 16,
    AudioLoopStarted    = 17,
    AudioLoopStopped    = 18,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIEvent
//
//  Single ring-buffer entry. Fixed-size POD; the SPSC ring stores these
//  by value in a contiguous power-of-two array. Layout (24 bytes total
//  on the v145 toolchain):
//
//      offset 0  : EventCategory   category   (uint8_t)
//      offset 1  : DiskIIEventType type       (uint8_t)
//      offset 2  : uint16_t        reserved   (alignment padding)
//      offset 4  : uint32_t        reserved2  (alignment padding for cycle)
//      offset 8  : uint64_t        cycle      (CPU cycle snapshot)
//      offset 16 : payload union              (12 bytes max)
//
//  Payload variants are tagged implicitly by `type`. The consumer
//  picks the correct union member based on `type` at format time;
//  there is no runtime tagged-union check (the producer is trusted
//  to write the variant matching the type).
//
//  The static_assert at the bottom of this header is normative: any
//  future enlargement of the payload MUST be accompanied by a
//  documented relaxation of the size bound and a benchmark of the
//  ring's memory footprint (4096 * sizeof(DiskIIEvent)).
//
////////////////////////////////////////////////////////////////////////////////

struct DiskIIEvent
{
    struct StepPayload
    {
        int                 prevQt;
        int                 newQt;
    };

    struct BumpPayload
    {
        int                 atQt;
    };

    struct AddrMarkPayload
    {
        int                 track;
        int                 sector;
        int                 volume;
    };

    struct DataMarkPayload
    {
        int                 sector;
        int                 byteCount;
    };

    struct DrivePayload
    {
        int                 drive;
    };

    struct LostPayload
    {
        uint32_t            count;
    };

    struct AudioPayload
    {
        SoundKind           kind;     // 1 byte
        SilentReason        reason;   // 1 byte (only meaningful for AudioSilent)
        uint16_t            pad;      // alignment
        int                 drive;    // 4 bytes
    };

    union Payload
    {
        StepPayload         step;
        BumpPayload         bump;
        AddrMarkPayload     addrMark;
        DataMarkPayload     dataMark;
        DrivePayload        drive;
        LostPayload         lost;
        AudioPayload        audio;
    };

    EventCategory           category;
    DiskIIEventType         type;
    uint16_t                reserved;
    uint32_t                reserved2;
    uint64_t                cycle;
    Payload                 payload;
};

static_assert (sizeof (DiskIIEvent) <= 32,
               "DiskIIEvent must stay <= 32 bytes (NFR-003 ring footprint). "
               "Tasks.md T002 target was 24 bytes, but 8-byte alignment for "
               "cycle (uint64_t) plus the 12-byte payload union pads to 32. "
               "4096 * 32 = 128 KiB ring is still well within budget.");
