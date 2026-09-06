#include "Pch.h"

#include "AppleKeyboard.h"
#include "IInputEventSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleKeyboard
//
////////////////////////////////////////////////////////////////////////////////

AppleKeyboard::AppleKeyboard()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  The keyboard's two soft switches: read the latch, or clear the strobe.
//
//  Both ranges are decoded 16 addresses wide because the hardware decodes only
//  the high address bits. Software genuinely uses the mirrors -- $C010 and
//  $C01F are the same switch -- so responding only to the canonical address
//  breaks programs that do.
//
//  Reading $C000 has NO side effect; it is the strobe clear at $C010 that
//  acknowledges the key. That asymmetry is why they are separate switches, and
//  why the read path can be polled freely.
//
//  Bit 7 means two different things across the two ranges, which is the
//  detail most easily gotten wrong. On the data read it is the STROBE -- a key
//  is waiting. On the strobe clear it is rewritten to reflect ANY-KEY-DOWN, so
//  the same bit answers "is a key held" once the strobe has been consumed.
//
//  The latch is atomic because it is written from the UI thread and read from
//  the CPU thread; fetch_and does the clear and the read as one operation, so
//  a key arriving mid-clear cannot be lost.
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleKeyboard::Read (Word address)
{
    Byte  value         = 0;
    Byte  old           = 0;
    bool  clearedStrobe = false;



    if (address >= 0xC000 && address <= 0xC00F)
    {
        // $C000-$C00F: Read keyboard data (bit 7 = strobe)
        value = m_latchedKey.load (memory_order_acquire);
        EmitKbdDataRead (address, value);
    }
    else if (address >= 0xC010 && address <= 0xC01F)
    {
        // $C010-$C01F: Clear keyboard strobe (bit 7)
        old           = m_latchedKey.fetch_and (0x7F, memory_order_acq_rel);
        clearedStrobe = (old & 0x80) != 0;
        value         = old & 0x7F;

        // Return the key with bit 7 reflecting any-key-down state
        if (m_anyKeyDown.load (memory_order_acquire))
        {
            value = value | 0x80;
        }

        EmitKbdStrobe (address, value, clearedStrobe);
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void AppleKeyboard::Write (Word address, Byte value)
{
    UNREFERENCED_PARAMETER (value);

    // Writing to $C010 also clears strobe
    if (address >= 0xC010 && address <= 0xC01F)
    {
        m_latchedKey.fetch_and (0x7F, memory_order_release);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void AppleKeyboard::Reset()
{
    m_latchedKey.store (0, memory_order_release);
    m_anyKeyDown.store (false, memory_order_release);
    m_repeatKey.store  (0, memory_order_release);
    m_repeatAccumUs = 0;
    m_repeatStarted = false;
    m_lastRepeatKey = 0;

    m_lastEmittedKbdData   = -1;
    m_lastEmittedStrobe    = -1;
    m_lastHostKeyDownAscii = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  Phase 4: clear the latched key and the any-key-down indicator so the
//  ROM's `]` prompt sees no pending strobe.
//
////////////////////////////////////////////////////////////////////////////////

void AppleKeyboard::SoftReset()
{
    Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PressKey
//
////////////////////////////////////////////////////////////////////////////////

void AppleKeyboard::PressKey (Byte asciiChar)
{
    // Store key with bit 7 set (strobe) in a single atomic write
    m_latchedKey.store (
        TranslateTypedChar (asciiChar) | 0x80, memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginKeyRepeat
//
//  Arms (or disarms with 0) the //e auto-repeat for the freshly-pressed
//  key, then raises the UI-thread host key-down / key-up notification on
//  the input sink. Called only from the Windows message handlers, never
//  from the CPU thread, so the host notifications are safe to stage
//  directly into the panel's UI-owned buffer. A repeated press of the
//  same held key (host OS key repeat that slips through) is coalesced.
//
////////////////////////////////////////////////////////////////////////////////

void AppleKeyboard::BeginKeyRepeat (Byte asciiChar)
{
    Byte  released = 0;



    m_repeatKey.store (asciiChar, memory_order_release);

    if (m_inputSink == nullptr)
    {
        // Nothing to notify; the repeat arming above is the whole job.
    }
    else if (asciiChar != 0)
    {
        // Coalesce a host OS repeat that slipped through: the same key
        // arriving twice is still one key-down.
        if (asciiChar != m_lastHostKeyDownAscii)
        {
            m_lastHostKeyDownAscii = asciiChar;
            m_inputSink->OnHostKeyDown (asciiChar);
        }
    }
    else if (m_lastHostKeyDownAscii != 0)
    {
        // Disarm (asciiChar 0) is the key-up edge, but only once.
        released               = m_lastHostKeyDownAscii;
        m_lastHostKeyDownAscii = 0;
        m_inputSink->OnHostKeyUp (released);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmitKbdDataRead
//
//  CPU thread. Coalesced emit for a guest read of $C000-$C00F: fires only
//  when the latched-key byte (data bits + strobe) changed since the last
//  emit, collapsing a million-poll wait loop into one event per latch
//  transition. Bit 7 carries the strobe state.
//
////////////////////////////////////////////////////////////////////////////////

void AppleKeyboard::EmitKbdDataRead (Word address, Byte value)
{
    if (m_inputSink != nullptr && m_lastEmittedKbdData != value)
    {
        m_lastEmittedKbdData = value;
        m_inputSink->OnKbdDataRead (address, value, (value & 0x80) != 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmitKbdStrobe
//
//  CPU thread. Coalesced emit for a guest access of $C010-$C01F. Always
//  fires when this access actually cleared a set strobe (a real edge the
//  program cares about); otherwise fires only when the returned value
//  (any-key-down bit 7 + data bits) changed since the last emit.
//
////////////////////////////////////////////////////////////////////////////////

void AppleKeyboard::EmitKbdStrobe (Word address, Byte value, bool clearedStrobe)
{
    // A real strobe-clearing edge always reports; anything else is subject
    // to the same last-value coalescing as EmitKbdDataRead.
    if (m_inputSink != nullptr && (clearedStrobe || m_lastEmittedStrobe != value))
    {
        m_lastEmittedStrobe = value;
        m_inputSink->OnKbdStrobe (address, value, clearedStrobe);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TickAutoRepeat
//
//  Regenerates the authentic //e keyboard auto-repeat. The shell suppresses
//  the host OS repeat and arms a single character via BeginKeyRepeat; this
//  drives the delay-then-rate cadence in REAL time. Re-latches the held key
//  (re-arming the $C000 strobe) once the delay elapses and then at the steady
//  repeat interval -- but only while the key is still physically down, which
//  the shell signals through SetKeyDown / any-key-down.
//
//  The cadence deliberately ignores the emulated clock. A typist's finger
//  rests in real seconds, and the //e's own repeat comes out of the keyboard
//  encoder rather than the 6502, so neither Double nor Maximum speed should
//  move it. Counting guest cycles here made Maximum -- uncapped, tens of times
//  real -- fire hundreds of characters a second and left the machine
//  impossible to type on.
//
////////////////////////////////////////////////////////////////////////////////

void AppleKeyboard::TickAutoRepeat (uint32_t elapsedMicroseconds)
{
    Byte      key       = m_repeatKey.load (memory_order_acquire);
    bool      keyHeld   = m_anyKeyDown.load (memory_order_acquire);
    uint32_t  elapsed   = elapsedMicroseconds;
    uint32_t  threshold = 0;



    if (key == 0 || !keyHeld)
    {
        // No armed key, or the physical key was released: stand down and reset
        // the cadence so the next press starts a fresh delay window.
        m_repeatAccumUs = 0;
        m_repeatStarted = false;
        m_lastRepeatKey = 0;
    }
    else if (key != m_lastRepeatKey)
    {
        // A newly-armed key. The shell already latched the first strobe on the
        // physical press, so begin timing the pre-repeat delay without latching
        // again here.
        m_lastRepeatKey = key;
        m_repeatAccumUs = 0;
        m_repeatStarted = false;
    }
    else
    {
        // One tick can never be worth more than one repeat. A pause, a
        // breakpoint or a stalled thread hands us an arbitrarily long
        // interval, and banking it would spill a burst of characters the
        // moment the machine ran again.
        if (elapsed > kKeyRepeatDelayUs)
        {
            elapsed = kKeyRepeatDelayUs;
        }

        m_repeatAccumUs += elapsed;

        threshold = m_repeatStarted ? kKeyRepeatIntervalUs
                                    : kKeyRepeatDelayUs;

        if (m_repeatAccumUs >= threshold)
        {
            m_repeatAccumUs -= threshold;
            m_repeatStarted  = true;
            PressKey (key);

            if (m_inputSink != nullptr)
            {
                m_inputSink->OnHostAutoRepeat (key);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TranslateTypedChar
//
//  The ][ / ][+ keyboard is uppercase only, so a typed letter latches in
//  upper case whatever the host sent.
//
//  This route NEVER drops a character, and the distinction is load-bearing.
//  A typed character is whatever the encoder produced, and on this keyboard
//  Ctrl+letter reaches $01-$1A: Ctrl+I sends $09, Ctrl+J $0A, Ctrl+K $0B --
//  the same codes as the //e's TAB and up / down arrows, from keys the ][+
//  really does have. Judging by the code would swallow them, so which keys a
//  machine HAS is answered by MapSpecialKey, against the key, never here.
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleKeyboard::TranslateTypedChar (Byte ch) const
{
    return (ch >= 'a' && ch <= 'z') ? (Byte) (ch - ('a' - 'A')) : ch;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MapSpecialKey
//
//  The code the ][ / ][+ keyboard sends for a named key, or 0 when the
//  keyboard has no such key.
//
//  Its cursor control is left and right ONLY. Four-way arrows, TAB and
//  DELETE all arrived with the //e, so a host pressing one of those is
//  pressing a key this machine does not have, and nothing should reach the
//  guest -- not the latch, and not any-key-down.
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleKeyboard::MapSpecialKey (AppleSpecialKey key) const
{
    Byte  code = 0;



    switch (key)
    {
        case AppleSpecialKey::Left:
            code = kAppleKeyLeft;
            break;

        case AppleSpecialKey::Right:
            code = kAppleKeyRight;
            break;

        case AppleSpecialKey::Escape:
            code = kAppleKeyEscape;
            break;

        default:
            // Up, Down, Tab and Delete are //e keys; this keyboard has none
            // of them, so they stay 0.
            break;
    }

    return code;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PressSpecialKey
//
//  Latch a named key, doing nothing at all when this machine's keyboard has
//  no such key. Returns the code latched, or 0, so the caller can tell the
//  two apart and skip arming auto-repeat for a key that was never pressed.
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleKeyboard::PressSpecialKey (AppleSpecialKey key)
{
    Byte  code = MapSpecialKey (key);



    if (code != 0)
    {
        PressKey (code);
    }

    return code;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> AppleKeyboard::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (config);
    UNREFERENCED_PARAMETER (bus);

    return make_unique<AppleKeyboard> ();
}
