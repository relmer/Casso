#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"

class Apple3TimeBase;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3AnalogInputs
//
//  The A/D converter and the joystick switches. $C058/9, $C05E/F and
//  $C05A/B clear and set the three input select bits; $C05C charges the
//  ramp and $C05D starts it. $C066 bit 7 is 1 from the start until the ramp
//  passes the selected input's voltage, a time proportional to the voltage.
//
//  SOS times the ramp with VIA D's timer 2 from 360 and divides the count by
//  8 (SOS 1.3, AD.READ), so an input that reads v from 0 to 255 times out
//  360 + 8v microseconds after the start, and ground at once. The battery,
//  the open input and the reference voltage have no documented reading:
//  they read 200, 255 and 255.
//
//  $C060-$C063 bit 7 is a joystick switch, 1 while pressed: $C060 port B
//  switch 1, $C061 port A switch 0, $C062 port B switch 0, $C063 port A
//  switch 1.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3AnalogInputs : public IMachineState
{
public:
    static constexpr Word      kFirstSwitch      = 0xC058;
    static constexpr Word      kLastSwitch       = 0xC05F;
    static constexpr Word      kFirstButton      = 0xC060;
    static constexpr Word      kLastButton       = 0xC063;
    static constexpr int       kAxisCount        = 4;          // port A X, port B X, port A Y, port B Y
    static constexpr int       kButtonCount      = 4;
    static constexpr uint32_t  kOffsetMicros     = 360;
    static constexpr uint32_t  kMicrosPerStep    = 8;
    static constexpr Byte      kCentered         = 128;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('A', '3', 'A', 'D');
    static constexpr uint16_t  kStateVersion = 1;

    explicit Apple3AnalogInputs (const Apple3TimeBase & timeBase);

    bool      IsSwitchAddress (Word address) const { return address >= kFirstSwitch && address <= kLastSwitch; }
    void      HandleSwitch    (Word address);

    bool      IsTiming        () const;
    bool      IsButtonPressed (Word address) const;

    //  Host input: an axis 0-255, and a switch.
    void      SetAxis         (int axis, Byte value);
    void      SetButton       (int button, bool isPressed);

    Byte      GetSelection    () const { return m_selection; }

    //  The microseconds the selected input's ramp takes.
    uint32_t  GetRampMicros   () const;

    HRESULT   SaveState       (StateWriter & writer) const override;
    HRESULT   LoadState       (StateReader & reader) override;

private:
    const Apple3TimeBase         & m_timeBase;
    Byte                           m_selection   = 0;
    uint64_t                       m_rampStart   = 0;
    uint64_t                       m_rampEnd     = 0;
    std::array<Byte, kAxisCount>   m_axes        = { kCentered, kCentered, kCentered, kCentered };
    std::array<bool, kButtonCount> m_buttons     = {};
};
