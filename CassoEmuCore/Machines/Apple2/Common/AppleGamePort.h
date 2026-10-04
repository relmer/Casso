#pragma once

#include "Pch.h"
#include "Core/IMachineState.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"

class IInputEventSink;
class SiriusJoyport;
class InputJournal;
struct InputRecord;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleGamePort
//
//  Apple ][/][+ game-I/O strip: pushbuttons PB0-PB2 ($C061-$C063, bit 7 =
//  pressed), analog paddles PDL0-PDL3 ($C064-$C067) and the PTRIG strobe
//  ($C070). The original Apple ][/][+ has no game port in its soft-switch
//  bank (unlike the //e, whose Apple2eSoftSwitchBank owns the paddles and
//  whose Apple2eKeyboard owns the Open/Closed-Apple buttons), so this is a
//  standalone device wired in from the machine config.
//
//  Paddles use the same 558 one-shot model as Apple2eSoftSwitchBank: a
//  $C070 strobe arms the timer and each axis holds bit 7 high for a span
//  proportional to its staged position, so the program's PREAD poll loop
//  counts up to the position value.
//
////////////////////////////////////////////////////////////////////////////////

class AppleGamePort : public MemoryDevice, public IMachineState
{
public:
    AppleGamePort () = default;

    Byte Read      (Word address) override;
    void Write     (Word address, Byte value) override;
    Word GetStart  () const override { return s_kwFirstButtonAddress; }
    Word GetEnd    () const override { return s_kwPaddleTimerStrobe; }
    void Reset     () override;
    void SoftReset () override;

    // Wire the CPU bus-cycle accumulator that drives the PREAD paddle timer.
    void SetCpuCycleSource (const uint64_t * src) { m_cpuCycleSource = src; }

    // Attach the input-debug notification sink.
    void SetInputEventSink (IInputEventSink * sink) noexcept { m_inputSink = sink; }

    // Stage an analog axis position (0-255, s_knPaddleCenter = neutral).
    void SetPaddle (int axis, Byte position);

    // Stage a pushbutton state (button 0 = PB0/fire, 1 = PB1, 2 = PB2).
    void SetButton (int index, bool pressed);

    // The game-port adapter, asked first for every button and paddle read.
    void SetJoyport (const SiriusJoyport * joyport) { m_joyport = joyport; }

    // Reverse execution: while attached, the first read to see a button or
    // paddle another thread staged records it (see InputJournal). Null
    // detaches. ApplyInput puts a recorded value back for a replay and
    // returns false for a record this port does not hold.
    void SetInputJournal (InputJournal * journal);
    bool ApplyInput      (const InputRecord & record);

    // Journal attached, at a slice boundary: records what another thread
    // staged since it was last seen, as a read would.
    void SampleHostInputs();

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

    static constexpr Byte s_knPaddleCenter = 127;

    // IMachineState: the paddle trigger cycle and the staged buttons and
    // paddle positions.
    HRESULT  SaveState (StateWriter & writer) const override;
    HRESULT  LoadState (StateReader & reader) override;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('G', 'P', 'R', 'T');
    static constexpr uint16_t  kStateVersion = 1;

protected:
    static constexpr Word     s_kwFirstButtonAddress = 0xC061;
    static constexpr int      s_knButtonCount        = 3;
    static constexpr int      s_knHostButtonCount    = 2;
    static constexpr Word     s_kwPaddle0Address     = 0xC064;
    static constexpr int      s_knPaddleAxisCount    = 4;
    static constexpr Word     s_kwPaddleTimerStrobe  = 0xC070;

    // PREAD's poll loop advances its counter once per ~11 CPU cycles, so an
    // axis holds bit 7 for position*11 cycles to yield a returned count equal
    // to the position. Matches the //e game-port full-scale read (~2.82 ms).
    static constexpr uint64_t s_knPaddleCyclesPerUnit = 11;

    Byte ReadButton         (Word address);
    Byte ReadPaddle         (Word address);
    void ObserveButton      (int index, bool pressed);
    void ObservePaddle      (int axis, Byte position);
    void SyncObservedInputs ();
    void EmitHostPaddle    (int axis, Byte value);
    void EmitHostButton    (int index, bool pressed);
    void EmitButtonRead    (Word address, Byte value);
    void EmitPaddleTrigger ();
    void EmitPaddleRead    (Word address, Byte value);

    // Bit 7 of a paddle read whose one-shot is still timing: what an input
    // with no potentiometer connected reads forever.
    static constexpr Byte     s_knPaddleTiming = 0x80;

    IInputEventSink      * m_inputSink                                  = nullptr;
    const SiriusJoyport  * m_joyport                                    = nullptr;
    const uint64_t       * m_cpuCycleSource                             = nullptr;
    InputJournal         * m_inputJournal                               = nullptr;
    bool                   m_observedButton[s_knButtonCount]            = {};
    Byte                   m_observedPaddle[s_knPaddleAxisCount]        = {};
    uint64_t               m_paddleTriggerCycle                         = 0;
    atomic<bool>           m_buttonState[s_knButtonCount]               = {};
    atomic<Byte>           m_paddlePosition[s_knPaddleAxisCount];
    int                    m_lastEmittedButton[s_knButtonCount]         = { -1, -1, -1 };
    int                    m_lastEmittedPaddle[s_knPaddleAxisCount]     = { -1, -1, -1, -1 };
    int                    m_lastEmittedHostButton[s_knHostButtonCount] = { -1, -1 };
    int                    m_lastEmittedHostPaddle[s_knPaddleAxisCount] = { -1, -1, -1, -1 };
};
