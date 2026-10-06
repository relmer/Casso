#include "Pch.h"

#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/Reverse/HeldInputWatch.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"
#include "Devices/IInputEventSink.h"





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
//  are the floating bus (modeled as zero here). An attached Joyport answers
//  first; when it declines, the staged button does.
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleGamePort::ReadButton (Word address)
{
    int   idx      = static_cast<int> (address - s_kwFirstButtonAddress);
    bool  pressed  = false;
    Byte  value    = 0;
    Byte  joyValue = 0;



    if (m_joyport != nullptr && m_joyport->TryReadButton (idx, joyValue))
    {
        value = joyValue;
    }
    else
    {
        pressed = m_buttonState[idx].load (memory_order_acquire);
        value   = pressed ? 0x80 : 0x00;

        if (m_inputJournal != nullptr)
        {
            ObserveButton (idx, pressed);
        }

        if (m_heldInputWatch != nullptr)
        {
            m_heldInputWatch->CheckButton (static_cast<size_t> (idx), pressed);
        }
    }

    return value;
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
//  With a Joyport attached and nothing in its rear sockets there is no
//  potentiometer on any input, so the one-shot never times out and PDL(n)
//  reads 255.
//
////////////////////////////////////////////////////////////////////////////////

Byte AppleGamePort::ReadPaddle (Word address)
{
    int       axis    = static_cast<int> (address - s_kwPaddle0Address);
    Byte      pos     = m_paddlePosition[axis].load (memory_order_acquire);
    uint64_t  elapsed = UINT64_MAX;
    Byte      value   = 0;



    if (m_inputJournal != nullptr)
    {
        ObservePaddle (axis, pos);
    }

    if (m_cpuCycleSource != nullptr)
    {
        elapsed = *m_cpuCycleSource - m_paddleTriggerCycle;
    }

    value = (elapsed < static_cast<uint64_t> (pos) * s_knPaddleCyclesPerUnit) ? s_knPaddleTiming : 0x00;

    if (m_joyport != nullptr && m_joyport->IsDrivingPaddles())
    {
        value = s_knPaddleTiming;
    }
    else if (m_heldInputWatch != nullptr)
    {
        m_heldInputWatch->CheckPaddle (static_cast<size_t> (axis), pos, elapsed, s_knPaddleCyclesPerUnit);
    }

    return value;
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
    EmitHostPaddle (axis, position);

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
    if (index < s_knHostButtonCount)
    {
        EmitHostButton (index, pressed);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetInputJournal
//
//  What is staged at the moment of attaching is what a keyframe taken then
//  holds, so it counts as seen.
//
////////////////////////////////////////////////////////////////////////////////

void AppleGamePort::SetInputJournal (InputJournal * journal)
{
    m_inputJournal = journal;
    SyncObservedInputs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyInput
//
////////////////////////////////////////////////////////////////////////////////

bool AppleGamePort::ApplyInput (const InputRecord & record)
{
    constexpr int  kFirstLine = static_cast<int> (InputLine::GamePortButton0);
    int            button     = static_cast<int> (record.detail) - kFirstLine;
    bool           isApplied  = false;



    if (record.kind == InputKind::Paddle && record.detail < s_knPaddleAxisCount)
    {
        m_paddlePosition[record.detail].store (record.value, memory_order_release);
        m_observedPaddle[record.detail] = record.value;
        isApplied = true;
    }
    else if (record.kind == InputKind::Button && button >= 0 && button < s_knButtonCount)
    {
        m_buttonState[button].store (record.value != 0, memory_order_release);
        m_observedButton[button] = record.value != 0;
        isApplied = true;
    }

    return isApplied;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ObserveButton
//
//  Journal attached. Records a button another thread changed since the last
//  read that saw it. Kept out of line so the read pays only the null test
//  while the journal is off.
//
////////////////////////////////////////////////////////////////////////////////

__declspec (noinline) void AppleGamePort::ObserveButton (int index, bool pressed)
{
    constexpr int  kFirstLine = static_cast<int> (InputLine::GamePortButton0);



    if (pressed != m_observedButton[index])
    {
        m_inputJournal->RecordObserved (m_inputJournal->GetCycle(), InputKind::Button, pressed ? 1 : 0, static_cast<uint16_t> (kFirstLine + index), 0);
        m_observedButton[index] = pressed;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ObservePaddle
//
////////////////////////////////////////////////////////////////////////////////

__declspec (noinline) void AppleGamePort::ObservePaddle (int axis, Byte position)
{
    if (position != m_observedPaddle[axis])
    {
        m_inputJournal->RecordObserved (m_inputJournal->GetCycle(), InputKind::Paddle, position, static_cast<uint16_t> (axis), 0);
        m_observedPaddle[axis] = position;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SampleHostInputs
//
////////////////////////////////////////////////////////////////////////////////

void AppleGamePort::SampleHostInputs()
{
    int  i = 0;



    for (i = 0; i < s_knButtonCount; i++)
    {
        ObserveButton (i, m_buttonState[i].load (memory_order_acquire));
    }

    for (i = 0; i < s_knPaddleAxisCount; i++)
    {
        ObservePaddle (i, m_paddlePosition[i].load (memory_order_acquire));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncObservedInputs
//
//  The CPU thread changed the staged state itself (attach, reset, load), so
//  the next read that sees it is not seeing another thread's input.
//
////////////////////////////////////////////////////////////////////////////////

void AppleGamePort::SyncObservedInputs()
{
    int  i = 0;



    for (i = 0; i < s_knButtonCount; i++)
    {
        m_observedButton[i] = m_buttonState[i].load (memory_order_acquire);
    }

    for (i = 0; i < s_knPaddleAxisCount; i++)
    {
        m_observedPaddle[i] = m_paddlePosition[i].load (memory_order_acquire);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmitHostPaddle
//
//  Host UI thread. Coalesced emit for a host-set analog axis: fires only
//  when the staged axis value changed since the last host-input emit.
//
////////////////////////////////////////////////////////////////////////////////

void AppleGamePort::EmitHostPaddle (int axis, Byte value)
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (m_inputSink == nullptr,                    S_OK);
    BAIL_OUT_IF (m_lastEmittedHostPaddle[axis] == value,    S_OK);

    m_lastEmittedHostPaddle[axis] = value;
    m_inputSink->OnHostPaddle (axis, value);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmitHostButton
//
//  Host UI thread. Coalesced emit for a host-set joystick button: fires
//  only when the staged button state changed since the last host-input emit.
//
////////////////////////////////////////////////////////////////////////////////

void AppleGamePort::EmitHostButton (int index, bool pressed)
{
    HRESULT  hr    = S_OK;
    int      value = pressed ? 1 : 0;



    BAIL_OUT_IF (m_inputSink == nullptr,                      S_OK);
    BAIL_OUT_IF (m_lastEmittedHostButton[index] == value,     S_OK);

    m_lastEmittedHostButton[index] = value;
    m_inputSink->OnHostButton (index, pressed);

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

    for (int & last : m_lastEmittedHostButton)
    {
        last = -1;
    }

    for (int & last : m_lastEmittedHostPaddle)
    {
        last = -1;
    }

    SyncObservedInputs();
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





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleGamePort::SaveState (StateWriter & writer) const
{
    int  i = 0;



    writer.BeginSection (kStateTag, kStateVersion);
    writer.WriteUInt64  (m_paddleTriggerCycle);

    for (i = 0; i < s_knButtonCount; i++)
    {
        writer.WriteBool (m_buttonState[i].load (memory_order_acquire));
    }

    for (i = 0; i < s_knPaddleAxisCount; i++)
    {
        writer.WriteByte (m_paddlePosition[i].load (memory_order_acquire));
    }

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleGamePort::LoadState (StateReader & reader)
{
    HRESULT   hr                                = S_OK;
    uint16_t  version                           = 0;
    bool      buttons[s_knButtonCount]          = {};
    Byte      paddles[s_knPaddleAxisCount]      = {};
    int       i                                 = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadUInt64 (m_paddleTriggerCycle);

    for (i = 0; i < s_knButtonCount; i++)
    {
        reader.ReadBool (buttons[i]);
    }

    for (i = 0; i < s_knPaddleAxisCount; i++)
    {
        reader.ReadByte (paddles[i]);
    }

    hr = reader.EndSection();
    CHR (hr);

    for (i = 0; i < s_knButtonCount; i++)
    {
        m_buttonState[i].store (buttons[i], memory_order_release);
        m_lastEmittedButton[i] = -1;
    }

    for (i = 0; i < s_knPaddleAxisCount; i++)
    {
        m_paddlePosition[i].store (paddles[i], memory_order_release);
        m_lastEmittedPaddle[i] = -1;
    }

    SyncObservedInputs();

Error:
    return hr;
}
