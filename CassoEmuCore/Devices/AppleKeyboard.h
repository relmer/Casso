#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"

class IInputEventSink;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple II key codes
//
//  The Apple II keyboard maps special keys to ASCII control characters.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr Byte kAppleKeyLeft    = 0x08;   // Backspace / cursor left
static constexpr Byte kAppleKeyRight   = 0x15;   // NAK / cursor right
static constexpr Byte kAppleKeyUp      = 0x0B;   // VT / cursor up
static constexpr Byte kAppleKeyDown    = 0x0A;   // LF / cursor down
static constexpr Byte kAppleKeyEscape  = 0x1B;   // Escape
static constexpr Byte kAppleKeyDelete  = 0x7F;   // Delete





////////////////////////////////////////////////////////////////////////////////
//
//  AppleKeyboard
//
//  Apple II/II+ uppercase-only keyboard mapped at $C000/$C010.
//  $C000: Read returns last key pressed with bit 7 as strobe.
//  $C010: Any read clears the strobe (bit 7 of $C000).
//
////////////////////////////////////////////////////////////////////////////////

class AppleKeyboard : public MemoryDevice
{
public:
    AppleKeyboard ();

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return 0xC000; }
    Word GetEnd   () const override { return 0xC01F; }
    void Reset    () override;
    void SoftReset () override;

    // Called from EmulatorShell when a key event arrives (UI thread)
    void KeyPress (Byte asciiChar);

    // Check if the strobe is clear (CPU has consumed the previous key)
    bool IsStrobeClear () const { return (m_latchedKey.load (memory_order_acquire) & 0x80) == 0; }

    // Read-only floating-bus accessor for the //e soft-switch bank
    // ($C011-$C01F status reads): returns the data bits 0-6 of the
    // latched key without clearing the strobe. (Phase 6 / FR-001 /
    // audit §1.2, §4.) Independent of the strobe-bit-7 state.
    Byte GetLatchedKeyDataBits () const
    {
        return static_cast<Byte> (m_latchedKey.load (memory_order_acquire) & 0x7F);
    }

    // Called from EmulatorShell for special keys (UI thread)
    void SetKeyDown (bool down) { m_anyKeyDown.store (down, memory_order_release); }

    // Authentic Apple //e keyboard auto-repeat cadence: a held key arms
    // the $C000 strobe once, waits ~half a second, then re-arms it at
    // ~15 characters per second. Expressed in CPU cycles off the nominal
    // //e clock so the timing is deterministic and host-independent.
    static constexpr uint32_t kKeyRepeatClockHz      = 1020484;
    static constexpr uint32_t kKeyRepeatDelayMs      = 500;
    static constexpr uint32_t kKeyRepeatRateHz       = 15;
    static constexpr uint32_t kMillisecondsPerSecond = 1000;
    static constexpr uint32_t kKeyRepeatDelayCycles =
        static_cast<uint32_t> (static_cast<uint64_t> (kKeyRepeatClockHz) *
                               kKeyRepeatDelayMs / kMillisecondsPerSecond);
    static constexpr uint32_t kKeyRepeatIntervalCycles =
        kKeyRepeatClockHz / kKeyRepeatRateHz;

    // Arm the emulated //e keyboard auto-repeat for a freshly-pressed key
    // (UI thread). The host OS auto-repeat is suppressed by the shell; the
    // authentic //e delay-then-repeat cadence is regenerated here instead.
    // A value of 0 disarms (no key to repeat). Also raises the UI-thread
    // host key-down / key-up notifications on the attached input sink.
    void BeginKeyRepeat (Byte asciiChar);

    // Attach (or detach with nullptr) the Input Debug panel sink. Set from
    // the UI thread; read from the CPU thread on the device's null
    // fast-path, matching the Disk2 event-sink convention.
    void SetInputEventSink (IInputEventSink * sink) noexcept { m_inputSink = sink; }

    // Advance the auto-repeat timer by one instruction's worth of CPU
    // cycles (CPU thread). Re-arms the $C000 strobe with the held key
    // after the initial delay and then at the steady repeat rate, but
    // only while the key remains physically down (any-key-down set).
    void Tick (uint32_t cpuCycles);

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    Byte TranslateToUppercase (Byte ch) const;

    // Producer-side coalesced emit helpers (CPU thread). Each fires the
    // matching sink callback only when the observed value changed, so a
    // tight poll loop produces one event per transition.
    void EmitKbdDataRead (Word address, Byte value);
    void EmitKbdStrobe   (Word address, Byte value, bool clearedStrobe);

    // m_latchedKey bit 7 = strobe (new key available).  Atomic because
    // KeyPress is called from the UI thread while Read is called from
    // the CPU thread.
    atomic<Byte>   m_latchedKey{0};
    atomic<bool>   m_anyKeyDown{false};

    // Auto-repeat state. m_repeatKey is written by the UI thread (arm /
    // disarm) and read by the CPU thread (Tick); the cadence accumulator,
    // phase flag, and last-seen key are touched only by Tick.
    atomic<Byte>   m_repeatKey{0};
    uint32_t       m_repeatAccumCycles = 0;
    bool           m_repeatStarted     = false;
    Byte           m_lastRepeatKey     = 0;

protected:
    // Input Debug panel sink (null when no panel is open). Plain pointer
    // per the Disk2 event-sink convention; written on the UI thread,
    // read on the CPU thread.
    IInputEventSink * InputSink () const noexcept { return m_inputSink; }

private:
    IInputEventSink * m_inputSink = nullptr;

    // Last-emitted values for guest-read coalescing (CPU thread only).
    // -1 means "nothing emitted yet"; a Byte never matches it.
    int            m_lastEmittedKbdData = -1;
    int            m_lastEmittedStrobe  = -1;

    // Last host key-down ascii for host-down/up coalescing (UI thread
    // only). 0 means no key is currently held.
    Byte           m_lastHostKeyDownAscii = 0;
};
