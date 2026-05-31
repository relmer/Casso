#pragma once

#include "Pch.h"




////////////////////////////////////////////////////////////////////////////////
//
//  IInputEventSink
//
//  Abstract notification interface fired by the keyboard devices whenever
//  a user-visible input event happens on the keyboard / button surface.
//  Implemented by the Casso Input Debug panel; the devices have zero
//  awareness of who is listening.
//
//  Contract:
//    * All methods are void and infallible. They MUST NOT throw and MUST
//      NOT block. Implementers route notifications into a lock-free SPSC
//      ring (for the CPU-thread callbacks) or a UI-owned staging buffer
//      (for the UI-thread callbacks) so the producing thread stays
//      wait-free.
//    * When no sink is attached (m_inputSink == nullptr) the device
//      fast-paths around the call so behavior is byte-identical to the
//      pre-feature device.
//    * The devices coalesce the guest reads producer-side: a callback
//      fires only when the observed value actually changed since the last
//      emit, so a tight poll loop reading $C000 millions of times a second
//      produces one event per latch transition, not millions.
//
//  THREADING -- the implementer MUST respect which thread each callback
//  arrives on, because the two lanes use different delivery mechanisms:
//
//    CPU (emulation) thread -- push to the SPSC ring:
//      * OnKbdDataRead   -- guest read of $C000-$C00F (latched key)
//      * OnKbdStrobe     -- guest access of $C010-$C01F (strobe clear)
//      * OnButtonRead    -- guest read of $C061-$C063 (Open/Closed-Apple,
//                            Shift)
//      * OnHostAutoRepeat-- the //e repeat timer re-latching a held key
//                            (originates in AppleKeyboard::Tick)
//
//    UI thread -- stage directly into the UI-owned buffer (these callbacks
//    are raised from the Windows message handlers via BeginKeyRepeat and
//    NEVER from the CPU thread, so they must NOT touch the SPSC ring,
//    whose single producer is the CPU thread):
//      * OnHostKeyDown   -- a real key press (first BeginKeyRepeat latch)
//      * OnHostKeyUp     -- the matching release (BeginKeyRepeat disarm)
//
//  Soft-switch addresses are passed through verbatim so the formatter can
//  render the exact $C0xx the program touched. `value` is the byte the
//  device returned to the CPU (bit 7 = strobe / any-key-down / button).
//
////////////////////////////////////////////////////////////////////////////////

class IInputEventSink
{
public:
    virtual ~IInputEventSink() = default;

    // CPU thread (SPSC ring).
    virtual void OnKbdDataRead (Word address, Byte value, bool strobeSet) = 0;
    virtual void OnKbdStrobe (Word address, Byte value, bool clearedStrobe) = 0;
    virtual void OnButtonRead (Word address, Byte value) = 0;
    virtual void OnHostAutoRepeat (Byte asciiChar) = 0;

    // UI thread (staging buffer).
    virtual void OnHostKeyDown (Byte asciiChar) = 0;
    virtual void OnHostKeyUp (Byte asciiChar) = 0;
};
