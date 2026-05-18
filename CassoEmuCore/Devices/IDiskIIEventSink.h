#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDiskIIEventSink
//
//  Abstract notification interface fired by DiskIIController whenever a
//  user-visible event happens on the controller surface. Implemented by
//  Casso/DiskIIDebugDialog and exercised by the spec-006 debug window;
//  the controller has zero awareness of who is listening.
//
//  Contract:
//    * All methods are void and infallible. They MUST NOT throw and MUST
//      NOT block. Implementers route notifications into a lock-free
//      SPSC ring so the producing CPU thread stays wait-free.
//    * The controller fires each method synchronously from its own call
//      sites (see plan.md "Event Trigger Specification"). When no sink
//      is attached (m_eventSink == nullptr) the controller fast-paths
//      around the call so behavior is byte-identical to the pre-feature
//      controller (FR-007, FR-020, SC-007).
//
//  Motor lifecycle (four events):
//    * OnMotorCommandOn  -- every $C0E9 strobe (including no-op re-strobes)
//    * OnMotorEngaged    -- m_motorOn false -> true edge only
//    * OnMotorCommandOff -- every $C0E8 strobe (arms spindown)
//    * OnMotorDisengaged -- m_motorOn true -> false after spindown
//
//  Head / surface:
//    * OnHeadStep   -- qtDelta != 0 AND head not pinned at a stop
//    * OnHeadBump   -- qtDelta != 0 AND head clamped at track 0 or
//                       kMaxQuarterTrack. Mutually exclusive with
//                       OnHeadStep for any single HandlePhase call.
//
//  Address / data marks (fired by DiskIIAddressMarkWatcher):
//    * OnAddressMark   -- after D5 AA 96 + 4-and-4 fields + good XOR
//                          checksum
//    * OnDataMarkRead  -- after D5 AA AD + 342 nibbles + DE AA EB
//    * OnDataMarkWrite -- DEFINED FOR FORWARD COMPATIBILITY ONLY.
//                          DOES NOT FIRE in v1 (A-010). A symmetric
//                          write-side watcher lands with issue #67
//                          (bit-level write path through Q6/Q7).
//
//  Drive / image lifecycle:
//    * OnDriveSelect   -- active drive index flipped via $C0EA / $C0EB
//    * OnDiskInserted  -- DiskImage mounted on a drive slot
//    * OnDiskEjected   -- DiskImage unmounted from a drive slot
//
//  Future (NOT in v1; intentionally not declared yet to avoid an
//  unfired vtable slot):
//    * OnMotorAtSpeed  -- motor reached rotational lock; will land
//                          alongside spec-005's spinup-suppression
//                          work (A-009). Added when needed; no
//                          forward-compat stub here.
//
////////////////////////////////////////////////////////////////////////////////

class IDiskIIEventSink
{
public:
    virtual ~IDiskIIEventSink() = default;

    virtual void OnMotorCommandOn   () = 0;
    virtual void OnMotorEngaged     () = 0;
    virtual void OnMotorCommandOff  () = 0;
    virtual void OnMotorDisengaged  () = 0;

    virtual void OnHeadStep         (int prevQt, int newQt) = 0;
    virtual void OnHeadBump         (int atQt) = 0;

    virtual void OnAddressMark      (int track, int sector, int volume) = 0;
    virtual void OnDataMarkRead     (int track, int sector, int volume, int byteCount) = 0;
    virtual void OnDataMarkWrite    (int track, int sector, int volume, int byteCount) = 0;

    virtual void OnDriveSelect      (int drive) = 0;
    virtual void OnDiskInserted     (int drive) = 0;
    virtual void OnDiskEjected      (int drive) = 0;
};
