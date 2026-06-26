#include "Pch.h"

#include "AppleGamePort.h"
#include "IInputEventSink.h"
#include "Ehm.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  Dispatches the game-I/O strip: PB0-PB2 status ($C061-$C063), PDL0-PDL3
//  analog timer reads ($C064-$C067) and the PTRIG strobe ($C070). Any other
//  address inside the claimed range reads as floating-bus zero.
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleGamePort::Read (Word address)
{
    Byte result = 0;



    if (address >= s_kwFirstButtonAddress &&
        address <  s_kwFirstButtonAddress + s_knButtonCount)
    {
        result = ReadButton (address);

        EmitButtonRead (address, result);
    }
    else if (address >= s_kwPaddle0Address &&
             address <  s_kwPaddle0Address + s_knPaddleAxisCount)
    {
        result = ReadPaddle (address);

        EmitPaddleRead (address, result);
    }
    else if (address == s_kwPaddleTimerStrobe)
    {
        m_paddleTriggerCycle = (m_cpuCycleSource != nullptr) ? *m_cpuCycleSource : 0;

        EmitPaddleTrigger();
    }

    return result;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
//  A write to PTRIG ($C070) arms the paddle one-shots exactly like a read;
//  the analog/button addresses ignore writes.
//
////////////////////////////////////////////////////////////////////////////////

void AppleGamePort::Write (Word address, Byte value)
{
    UNREFERENCED_PARAMETER (value);

    if (address == s_kwPaddleTimerStrobe)
    {
        m_paddleTriggerCycle = (m_cpuCycleSource != nullptr) ? *m_cpuCycleSource : 0;

        EmitPaddleTrigger();
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  ReadButton
//
//  Pushbutton status read: bit 7 is the pressed state; the low seven bits
//  are the floating bus (modeled as zero here).
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleGamePort::ReadButton (Word address) const
{
    int   idx     = static_cast<int> (address - s_kwFirstButtonAddress);
    bool  pressed = m_buttonState[idx].load (memory_order_acquire);

    return pressed ? 0x80 : 0x00;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ReadPaddle
//
//  Models the 558 one-shot: after a $C070 strobe each axis holds bit 7
//  high for a span proportional to its position, so PREAD's poll loop
//  counts up to the position value. With no cycle source wired (tests) the
//  timer reads as already expired so a poll loop can never hang.
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleGamePort::ReadPaddle (Word address) const
{
    int       axis    = static_cast<int> (address - s_kwPaddle0Address);
    Byte      pos     = m_paddlePosition[axis].load (memory_order_acquire);
    uint64_t  elapsed = UINT64_MAX;



    if (m_cpuCycleSource != nullptr)
    {
        elapsed = *m_cpuCycleSource - m_paddleTriggerCycle;
    }

    return (elapsed < static_cast<uint64_t> (pos) * s_knPaddleCyclesPerUnit) ? 0x80 : 0x00;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetPaddle
//
//  Host UI thread. Stages an axis position; the CPU thread observes it on
//  the next $C064-$C067 read. axis 0/1 = joystick X/Y, 2/3 = paddles 2/3;
//  callers always pass an in-range axis, so an out-of-range value asserts.
//
////////////////////////////////////////////////////////////////////////////////

void AppleGamePort::SetPaddle (int axis, Byte position)
{
    HRESULT  hr = S_OK;



    CBRA (axis >= 0 && axis < s_knPaddleAxisCount);

    m_paddlePosition[axis].store (position, memory_order_release);

Error:
    return;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetButton
//
//  Host UI thread. Stages a pushbutton state; the CPU thread observes it on
//  the next $C061-$C063 read. Callers always pass an in-range index, so an
//  out-of-range value asserts.
//
////////////////////////////////////////////////////////////////////////////////

void AppleGamePort::SetButton (int index, bool pressed)
{
    HRESULT  hr = S_OK;



    CBRA (index >= 0 && index < s_knButtonCount);

    m_buttonState[index].store (pressed, memory_order_release);

Error:
    return;
}




////////////////////////////////////////////////////////////////////////////////
//
//  EmitButtonRead
//
//  CPU thread. Coalesced emit for a guest read of $C061-$C063: fires only
//  when that button's returned byte (bit 7 = pressed) changed since the
//  last emit, so a tight button-poll loop yields one event per edge.
//
////////////////////////////////////////////////////////////////////////////////

void AppleGamePort::EmitButtonRead (Word address, Byte value)
{
    HRESULT  hr  = S_OK;
    int      idx = static_cast<int> (address - s_kwFirstButtonAddress);



    BAIL_OUT_IF (m_inputSink == nullptr,            S_OK);
    BAIL_OUT_IF (m_lastEmittedButton[idx] == value, S_OK);

    m_lastEmittedButton[idx] = value;
    m_inputSink->OnButtonRead (address, value);

Error:
    return;
}




////////////////////////////////////////////////////////////////////////////////
//
//  EmitPaddleTrigger
//
//  CPU thread. Fires a PaddleTrigger event on each $C070 PTRIG strobe so the
//  input-debug panel can show the program arming the game-port one-shots.
//
////////////////////////////////////////////////////////////////////////////////

void AppleGamePort::EmitPaddleTrigger()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (m_inputSink == nullptr, S_OK);

    m_inputSink->OnPaddleTrigger (s_kwPaddleTimerStrobe);

Error:
    return;
}




////////////////////////////////////////////////////////////////////////////////
//
//  EmitPaddleRead
//
//  CPU thread. Coalesced emit for a guest read of $C064-$C067: fires only
//  when that axis's returned byte (bit 7 = timer still counting) changed
//  since the last emit, so PREAD's tight poll loop yields one event per
//  timer transition rather than one per read.
//
////////////////////////////////////////////////////////////////////////////////

void AppleGamePort::EmitPaddleRead (Word address, Byte value)
{
    HRESULT  hr  = S_OK;
    int      idx = static_cast<int> (address - s_kwPaddle0Address);



    BAIL_OUT_IF (m_inputSink == nullptr,            S_OK);
    BAIL_OUT_IF (m_lastEmittedPaddle[idx] == value, S_OK);

    m_lastEmittedPaddle[idx] = value;
    m_inputSink->OnPaddleRead (address, value);

Error:
    return;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  Power-on / hard reset recenters the paddles, releases the buttons and
//  disarms the one-shot.
//
////////////////////////////////////////////////////////////////////////////////

void AppleGamePort::Reset()
{
    m_paddleTriggerCycle = 0;

    for (atomic<Byte> & axis : m_paddlePosition)
    {
        axis.store (s_knPaddleCenter, memory_order_release);
    }

    for (atomic<bool> & button : m_buttonState)
    {
        button.store (false, memory_order_release);
    }

    for (int & last : m_lastEmittedButton)
    {
        last = -1;
    }

    for (int & last : m_lastEmittedPaddle)
    {
        last = -1;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  A ][/][+ /RESET does not disturb the staged analog/button input, so the
//  soft reset only disarms the one-shot timer.
//
////////////////////////////////////////////////////////////////////////////////

void AppleGamePort::SoftReset()
{
    m_paddleTriggerCycle = 0;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> AppleGamePort::Create (const DeviceConfig & config, MemoryBus & bus)
{
    unique_ptr<AppleGamePort>  device = make_unique<AppleGamePort>();

    UNREFERENCED_PARAMETER (config);
    UNREFERENCED_PARAMETER (bus);



    device->Reset();

    return device;
}
