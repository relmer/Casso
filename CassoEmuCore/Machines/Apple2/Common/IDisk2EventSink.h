#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IDisk2EventSink
//
//  Abstract notification interface fired by Disk2Controller whenever a
//  user-visible event happens on the controller surface. Implemented by
//  whatever observes the controller; the controller has zero awareness
//  of who is listening.
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
//  Address and data fields (fired by Disk2AddressMarkWatcher), 16-sector
//  or 13-sector:
//    * OnAddressMark          -- after D5 AA 96 or B5 + 4-and-4 fields +
//                                 good XOR checksum
//    * OnAddressChecksumError -- the same with a checksum that fails
//    * OnDataMarkRead         -- after D5 AA AD + body + DE AA, the
//                                 decoded bytes with a good checksum
//    * OnDataChecksumError    -- the same with a checksum that fails
//    * OnDataFieldCutOff      -- a data field with no epilogue, or one
//                                 ended at a length neither encoding has
//    * OnDataWithoutAddress   -- a whole data field with no address
//                                 field read before it
//    * OnDataMarkWrite        -- after D5 AA AD + body + DE AA written
//                                 through the write latch, with its bytes
//    * OnReadStall            -- two revolutions of nibbles read with no
//                                 address field decoded and no head step;
//                                 once per stall
//
//  Writing:
//    * OnWriteMode            -- Q7 turned on or off, with the selected
//                                 drive
//    * OnWriteProtected       -- the program turned on writing with a
//                                 write-protected disk in the drive
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

class IDisk2EventSink
{
public:
    virtual ~IDisk2EventSink() = default;

    virtual void OnMotorCommandOn() = 0;
    virtual void OnMotorEngaged() = 0;
    virtual void OnMotorCommandOff() = 0;
    virtual void OnMotorDisengaged() = 0;

    virtual void OnHeadStep (int prevQt, int newQt) = 0;
    virtual void OnHeadBump (int atQt) = 0;

    virtual void OnAddressMark (int track, int sector, int volume) = 0;
    virtual void OnDataMarkRead (int track, int sector, int volume, std::span<const Byte> bytes) = 0;
    virtual void OnDataMarkWrite (int track, int sector, int volume, std::span<const Byte> bytes) = 0;
    virtual void OnReadStall (int quarterTrack) {}

    virtual void OnAddressChecksumError (int track, int sector, int volume)                              { (void) track; (void) sector; (void) volume; }
    virtual void OnDataChecksumError    (int track, int sector, int volume, std::span<const Byte> bytes) { (void) track; (void) sector; (void) volume; (void) bytes; }
    virtual void OnDataFieldCutOff      (int track, int sector, int volume)                              { (void) track; (void) sector; (void) volume; }
    virtual void OnDataWithoutAddress   (std::span<const Byte> bytes)                                    { (void) bytes; }
    virtual void OnWriteProtected       ()                                                                {}
    virtual void OnWriteMode            (int drive, bool isOn)                                            { (void) drive; (void) isOn; }

    virtual void OnDriveSelect (int drive) = 0;
    virtual void OnDiskInserted (int drive) = 0;
    virtual void OnDiskEjected (int drive) = 0;
};
