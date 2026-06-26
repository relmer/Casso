#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"

class IInputEventSink;




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

class AppleGamePort : public MemoryDevice
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

    // Attach the input-debug notification sink (CPU thread, lock-free ring).
    void SetInputEventSink (IInputEventSink * sink) noexcept { m_inputSink = sink; }

    // Stage an analog axis position (0-255, s_knPaddleCenter = neutral).
    void SetPaddle (int axis, Byte position);

    // Stage a pushbutton state (button 0 = PB0/fire, 1 = PB1, 2 = PB2).
    void SetButton (int index, bool pressed);

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

    static constexpr Byte s_knPaddleCenter = 127;

private:
    static constexpr Word     s_kwFirstButtonAddress = 0xC061;
    static constexpr int      s_knButtonCount        = 3;
    static constexpr Word     s_kwPaddle0Address     = 0xC064;
    static constexpr int      s_knPaddleAxisCount    = 4;
    static constexpr Word     s_kwPaddleTimerStrobe  = 0xC070;

    // PREAD's poll loop advances its counter once per ~11 CPU cycles, so an
    // axis holds bit 7 for position*11 cycles to yield a returned count equal
    // to the position. Matches the //e game-port full-scale read (~2.82 ms).
    static constexpr uint64_t s_knPaddleCyclesPerUnit = 11;

    Byte ReadButton        (Word address) const;
    Byte ReadPaddle        (Word address) const;
    void EmitButtonRead    (Word address, Byte value);
    void EmitPaddleTrigger ();
    void EmitPaddleRead    (Word address, Byte value);

    IInputEventSink *    m_inputSink          = nullptr;
    const uint64_t *     m_cpuCycleSource     = nullptr;
    uint64_t             m_paddleTriggerCycle = 0;
    atomic<bool>         m_buttonState[s_knButtonCount]    = {};
    atomic<Byte>         m_paddlePosition[s_knPaddleAxisCount];
    int                  m_lastEmittedButton[s_knButtonCount]   = { -1, -1, -1 };
    int                  m_lastEmittedPaddle[s_knPaddleAxisCount] = { -1, -1, -1, -1 };
};
