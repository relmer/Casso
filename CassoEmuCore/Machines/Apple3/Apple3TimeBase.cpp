#include "Pch.h"

#include "Apple3TimeBase.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"





static constexpr uint32_t  s_kFastCycleTicks = 1;
static constexpr uint32_t  s_kSlowCycleTicks = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  IsRefreshState
//
//  Whether a state of the line is a RAM refresh state. The decode ROM that
//  places them is not documented, so the refresh states are spread evenly
//  through the line, and their count is calibrated to Apple's averages of
//  1.8 MHz with the screen off and 1.4 MHz with it on. The state count and
//  the refresh count have no common factor, so the product walks every
//  residue once and exactly kRefreshStatesPerLine states qualify.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3TimeBase::IsRefreshState (uint32_t state)
{
    return (state * kRefreshStatesPerLine) % kStatesPerLine < kRefreshStatesPerLine;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDisplayActive
//
//  The DSPLY signal at the current tick: the unblanked part of the display
//  while the screen is on, or a refresh state.
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3TimeBase::IsDisplayActive() const
{
    uint32_t  state       = m_tickInLine / kTicksPerState;
    bool      isDisplayed = m_isScreenOn && m_line < kDisplayedLines && state >= kFirstDisplayedState;



    return isDisplayed || IsRefreshState (state);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChargeCycle
//
//  Advances time by one CPU cycle and returns its cost in ticks. The 1 MHz
//  rule is applied at the tick the cycle starts on.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Apple3TimeBase::ChargeCycle (Apple3Access access)
{
    bool      isRamDuringDisplay = access == Apple3Access::Ram && IsDisplayActive();
    bool      isSlow             = m_isOneMhz || access == Apple3Access::Fspace || isRamDuringDisplay;
    uint32_t  ticks              = isSlow ? s_kSlowCycleTicks : s_kFastCycleTicks;



    Advance (ticks);

    return ticks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChargeCycles
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Apple3TimeBase::ChargeCycles (uint32_t cycles, Apple3Access access)
{
    uint32_t  ticks = 0;



    for (uint32_t i = 0; i < cycles; i++)
    {
        ticks += ChargeCycle (access);
    }

    return ticks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMasterClocks
//
//  Master clock periods since power on. Every tick is seven periods but the
//  last of the line, which is nine, so the line is 912.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t Apple3TimeBase::GetMasterClocks() const
{
    return m_lines * kMasterPerLine + (uint64_t) m_tickInLine * kMasterPerTick;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Advance
//
////////////////////////////////////////////////////////////////////////////////

void Apple3TimeBase::Advance (uint32_t ticks)
{
    m_ticks      += ticks;
    m_tickInLine += ticks;

    while (m_tickInLine >= kTicksPerLine)
    {
        m_tickInLine -= kTicksPerLine;
        m_lines++;
        m_line = (m_line + 1) % kLinesPerFrame;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3TimeBase::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteUInt64 (m_ticks);
    writer.WriteUInt64 (m_lines);
    writer.WriteUInt32 (m_line);
    writer.WriteUInt32 (m_tickInLine);
    writer.WriteBool   (m_isOneMhz);
    writer.WriteBool   (m_isScreenOn);

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3TimeBase::LoadState (StateReader & reader)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadUInt64 (m_ticks);
    reader.ReadUInt64 (m_lines);
    reader.ReadUInt32 (m_line);
    reader.ReadUInt32 (m_tickInLine);
    reader.ReadBool   (m_isOneMhz);
    reader.ReadBool   (m_isScreenOn);

    hr = reader.EndSection();
    CHR (hr);

    CBREx (m_line < kLinesPerFrame && m_tickInLine < kTicksPerLine, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}
