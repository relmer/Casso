#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3Access
//
//  What a CPU cycle accesses, for the 1 MHz rule. Fspace is the VIAs, the
//  6551 and the clock chip.
//
////////////////////////////////////////////////////////////////////////////////

enum class Apple3Access
{
    Ram,
    Rom,
    Io,
    Fspace,
};





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3TimeBase
//
//  The Apple ///'s time, counted in ticks of the 2 MHz clock. A line is 130
//  ticks, the last one stretched by two master clock periods, so a line is
//  912 master clock periods; a frame is 262 scan lines. A CPU cycle costs one tick,
//  or two when the cycle runs at 1 MHz: with the 1 MHz mode selected, for an
//  access to the VIAs, the 6551 or the clock chip, and for a RAM access while
//  the display or a RAM refresh is under way. The video counter runs at
//  1 MHz, so each of a line's 65 states is two ticks; states 25 to 64 of
//  lines 0 to 191 are displayed.
//
//  Devices count time in master clock periods, which do not depend on the
//  CPU's speed.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3TimeBase : public IMachineState
{
public:
    static constexpr uint32_t  kTicksPerLine         = 130;
    static constexpr uint32_t  kTicksPerState        = 2;
    static constexpr uint32_t  kStatesPerLine        = kTicksPerLine / kTicksPerState;
    static constexpr uint32_t  kFirstDisplayedState  = 25;
    static constexpr uint32_t  kLinesPerFrame        = 262;
    static constexpr uint32_t  kDisplayedLines       = 192;
    static constexpr uint32_t  kMasterPerTick        = 7;
    static constexpr uint32_t  kMasterPerLine        = 912;
    static constexpr uint32_t  kRefreshStatesPerLine = 16;
    static constexpr uint64_t  kMasterClockHz        = 14318180;
    static constexpr uint64_t  kTicksPerFrame        = (uint64_t) kTicksPerLine * kLinesPerFrame;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('A', '3', 'T', 'B');
    static constexpr uint16_t  kStateVersion = 1;

    void       SetOneMhz       (bool oneMhz)   { m_isOneMhz   = oneMhz; }
    void       SetScreenOn     (bool screenOn) { m_isScreenOn = screenOn; }
    bool       IsOneMhz        () const        { return m_isOneMhz; }
    bool       IsScreenOn      () const        { return m_isScreenOn; }

    uint32_t   ChargeCycle     (Apple3Access access);
    uint32_t   ChargeCycles    (uint32_t cycles, Apple3Access access);

    uint64_t   GetTicks        () const        { return m_ticks; }
    uint64_t   GetMasterClocks () const;
    uint32_t   GetLine         () const        { return m_line; }
    uint32_t   GetTickInLine   () const        { return m_tickInLine; }
    bool       IsDisplayActive () const;

    static bool  IsRefreshState (uint32_t state);

    HRESULT    SaveState       (StateWriter & writer) const override;
    HRESULT    LoadState       (StateReader & reader) override;

private:
    void       Advance         (uint32_t ticks);

    uint64_t   m_ticks      = 0;
    uint64_t   m_lines      = 0;          // lines completed since power on
    uint32_t   m_line       = 0;          // line within the frame
    uint32_t   m_tickInLine = 0;
    bool       m_isOneMhz   = false;
    bool       m_isScreenOn = false;
};
