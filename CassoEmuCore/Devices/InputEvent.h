#pragma once

#include "Pch.h"




////////////////////////////////////////////////////////////////////////////////
//
//  InputEventCategory
//
//  Tags an InputEvent ring entry by origin so the UI-thread formatter can
//  route to the right detail template and colorize the Source column.
//
//      Host   -- a real Windows key event observed on the UI thread, or a
//                CPU-thread auto-repeat re-latch driven by the //e repeat
//                timer. These describe what the *user* did.
//      Guest  -- the emulated program reading a keyboard / button soft
//                switch ($C000, $C010, $C061-$C063). These describe what
//                the *game* sees.
//      System -- a synthetic marker the projection helper inserts on the
//                UI thread (currently only EventsLost on ring overflow).
//
//  Stays uint8_t to keep InputEvent small.
//
////////////////////////////////////////////////////////////////////////////////

enum class InputEventCategory : uint8_t
{
    Host   = 0,
    Guest  = 1,
    System = 2,
};




////////////////////////////////////////////////////////////////////////////////
//
//  InputEventType
//
//  Concrete event identifier. Host-side and guest-side types share one
//  enum so the consumer switches on a single field; the category tag
//  tells the formatter which sub-table to use for the Meaning column.
//
//  HostKeyDown / HostKeyUp originate on the UI thread (real Windows
//  WM_KEY*/WM_CHAR), staged directly into the panel's UI-owned buffer.
//  HostAutoRepeat originates on the CPU thread (the //e repeat timer
//  re-latching a held key) and travels through the SPSC ring like the
//  guest reads.
//
//  KbdDataRead / KbdStrobe / ButtonRead are guest soft-switch accesses,
//  all on the CPU thread, all coalesced producer-side (emitted only when
//  the observed value actually changes) and all routed through the ring.
//
//  PaddleTrigger / PaddleRead are guest game-port accesses on the CPU
//  thread, routed through the ring. PaddleTrigger ($C070) fires on each
//  PTRIG strobe; PaddleRead ($C064-$C067) is coalesced producer-side,
//  emitted only when an axis timer's returned bit 7 changes.
//
//  EventsLost is a UI-thread synthetic entry inserted by the projection
//  helper when the producer's overflow counter is non-zero on drain. It
//  never originates from a device.
//
////////////////////////////////////////////////////////////////////////////////

enum class InputEventType : uint8_t
{
    // Host-side
    HostKeyDown     = 0,
    HostKeyUp       = 1,
    HostAutoRepeat  = 2,

    // Guest-side
    KbdDataRead     = 3,    // $C000 read: latched key + strobe bit
    KbdStrobe       = 4,    // $C010 access: clears strobe, returns AKD
    ButtonRead      = 5,    // $C061-$C063 read: Open/Closed-Apple/Shift

    // Game-port axis timers ($C064-$C067) and the PTRIG strobe ($C070)
    PaddleTrigger   = 6,    // $C070 strobe
    PaddleRead      = 7,    // $C064-$C067 read

    // Synthetic
    EventsLost      = 8,    // producer-side ring overflow marker
};




////////////////////////////////////////////////////////////////////////////////
//
//  InputEvent
//
//  Single ring-buffer entry. Fixed-size POD; the SPSC ring stores these
//  by value in a contiguous power-of-two array. Layout (24 bytes total
//  on the v145 toolchain):
//
//      offset 0  : InputEventCategory  category   (uint8_t)
//      offset 1  : InputEventType       type       (uint8_t)
//      offset 2  : uint8_t              reserved   (alignment padding)
//      offset 3  : uint8_t              reserved2  (alignment padding)
//      offset 4  : uint32_t             reserved3  (alignment padding for cycle)
//      offset 8  : uint64_t             cycle      (CPU cycle snapshot)
//      offset 16 : payload union                   (8 bytes max)
//
//  Payload variants are tagged implicitly by `type`. The consumer picks
//  the correct union member based on `type` at format time; there is no
//  runtime tagged-union check (the producer is trusted to write the
//  variant matching the type).
//
//  Flag bits in IoPayload.flags:
//      bit 0  -- KbdDataRead: the strobe (bit 7) was set on this read.
//                KbdStrobe:   this access actually cleared a set strobe.
//      bit 1  -- KbdStrobe:   any-key-down (//e AKD) was set.
//
////////////////////////////////////////////////////////////////////////////////

struct InputEvent
{
    static constexpr uint8_t    kFlagStrobe    = 0x01;
    static constexpr uint8_t    kFlagAnyKeyDown = 0x02;

    struct KeyPayload
    {
        uint8_t             ascii;
        uint8_t             reserved[3];
    };

    struct IoPayload
    {
        uint16_t            address;
        uint8_t             value;
        uint8_t             flags;
    };

    struct LostPayload
    {
        uint32_t            count;
    };

    union Payload
    {
        KeyPayload          key;
        IoPayload           io;
        LostPayload         lost;
    };

    InputEventCategory      category;
    InputEventType          type;
    uint8_t                 reserved;
    uint8_t                 reserved2;
    uint32_t                reserved3;
    uint64_t                cycle;
    Payload                 payload;
};

static_assert (sizeof (InputEvent) <= 32,
               "InputEvent must stay <= 32 bytes (SPSC ring footprint). "
               "4096 * 32 = 128 KiB ring is well within budget.");
