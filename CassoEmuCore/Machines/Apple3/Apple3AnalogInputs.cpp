#include "Pch.h"

#include "Apple3AnalogInputs.h"
#include "Apple3TimeBase.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"





static constexpr Word      s_kSelect0Off   = 0xC058;
static constexpr Word      s_kSelect0On    = 0xC059;
static constexpr Word      s_kSelect2Off   = 0xC05A;
static constexpr Word      s_kSelect2On    = 0xC05B;
static constexpr Word      s_kCharge       = 0xC05C;
static constexpr Word      s_kStart        = 0xC05D;
static constexpr Word      s_kSelect1Off   = 0xC05E;
static constexpr Word      s_kSelect1On    = 0xC05F;
static constexpr Byte      s_kSelect0      = 0x01;
static constexpr Byte      s_kSelect1      = 0x02;
static constexpr Byte      s_kSelect2      = 0x04;
static constexpr uint64_t  s_kMicrosPerSec = 1000000;

static constexpr Byte      s_kBattery      = 200;
static constexpr Byte      s_kFullScale    = 255;

enum : Byte
{
    s_kGround    = 0,
    s_kPortBX    = 1,
    s_kPortBY    = 2,
    s_kPortAX    = 3,
    s_kPortAY    = 4,
    s_kBatteryIn = 5,
    s_kOpen      = 6,
    s_kReference = 7,
};

static constexpr int  s_kAxisAX = 0;
static constexpr int  s_kAxisBX = 1;
static constexpr int  s_kAxisAY = 2;
static constexpr int  s_kAxisBY = 3;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3AnalogInputs
//
////////////////////////////////////////////////////////////////////////////////

Apple3AnalogInputs::Apple3AnalogInputs (const Apple3TimeBase & timeBase)
    : m_timeBase (timeBase)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleSwitch
//
//  Charging stops a ramp in progress; starting one times it from now.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3AnalogInputs::HandleSwitch (Word address)
{
    uint64_t  now    = m_timeBase.GetMasterClocks();
    uint64_t  length = (uint64_t) GetRampMicros() * Apple3TimeBase::kMasterClockHz / s_kMicrosPerSec;



    switch (address)
    {
    case s_kSelect0Off: m_selection &= (Byte) ~s_kSelect0; break;
    case s_kSelect0On:  m_selection |= s_kSelect0;         break;
    case s_kSelect1Off: m_selection &= (Byte) ~s_kSelect1; break;
    case s_kSelect1On:  m_selection |= s_kSelect1;         break;
    case s_kSelect2Off: m_selection &= (Byte) ~s_kSelect2; break;
    case s_kSelect2On:  m_selection |= s_kSelect2;         break;
    case s_kCharge:     m_rampEnd    = 0;                  break;
    case s_kStart:      m_rampStart  = now; m_rampEnd = now + length; break;
    default:                                               break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRampMicros
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Apple3AnalogInputs::GetRampMicros() const
{
    Byte  value    = 0;
    bool  isGround = m_selection == s_kGround;



    switch (m_selection)
    {
    case s_kPortAX:     value = m_axes[s_kAxisAX]; break;
    case s_kPortBX:     value = m_axes[s_kAxisBX]; break;
    case s_kPortAY:     value = m_axes[s_kAxisAY]; break;
    case s_kPortBY:     value = m_axes[s_kAxisBY]; break;
    case s_kBatteryIn:  value = s_kBattery;        break;
    case s_kOpen:       value = s_kFullScale;      break;
    case s_kReference:  value = s_kFullScale;      break;
    default:                                       break;
    }

    return isGround ? 0 : kOffsetMicros + kMicrosPerStep * value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsTiming
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3AnalogInputs::IsTiming() const
{
    return m_timeBase.GetMasterClocks() < m_rampEnd;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsButtonPressed
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3AnalogInputs::IsButtonPressed (Word address) const
{
    bool  isButton = address >= kFirstButton && address <= kLastButton;



    return isButton && m_buttons[(size_t) (address - kFirstButton)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetAxis
//
////////////////////////////////////////////////////////////////////////////////

void Apple3AnalogInputs::SetAxis (int axis, Byte value)
{
    if (axis >= 0 && axis < kAxisCount)
    {
        m_axes[(size_t) axis] = value;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetButton
//
//  `button` is the switch's offset from $C060.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3AnalogInputs::SetButton (int button, bool isPressed)
{
    if (button >= 0 && button < kButtonCount)
    {
        m_buttons[(size_t) button] = isPressed;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3AnalogInputs::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteByte   (m_selection);
    writer.WriteUInt64 (m_rampStart);
    writer.WriteUInt64 (m_rampEnd);
    writer.WriteBytes  (m_axes.data(), m_axes.size());

    for (bool button : m_buttons)
    {
        writer.WriteBool (button);
    }

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3AnalogInputs::LoadState (StateReader & reader)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadByte   (m_selection);
    reader.ReadUInt64 (m_rampStart);
    reader.ReadUInt64 (m_rampEnd);
    reader.ReadBytes  (m_axes.data(), m_axes.size());

    for (bool & button : m_buttons)
    {
        reader.ReadBool (button);
    }

    hr = reader.EndSection();
    CHR (hr);

Error:
    return hr;
}
