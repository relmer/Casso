#include "Pch.h"

#include "Machines/Apple3/Apple3Sound.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"





static constexpr double  s_kMsPerSecond    = 1000.0;
static constexpr size_t  s_kMaxDacChanges  = 1 << 16;





////////////////////////////////////////////////////////////////////////////////
//
//  SetTickSource
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Sound::SetTickSource (const uint64_t * ticks, double ticksPerSecond)
{
    m_ticks          = ticks;
    m_ticksPerSecond = (ticksPerSecond > 0.0) ? ticksPerSecond : 1.0;
    m_renderedTo     = GetNow();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TriggerBeep
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Sound::TriggerBeep()
{
    m_beepStart = GetNow();
    m_beepEnd   = m_beepStart + (uint64_t) (m_ticksPerSecond * kBeepMs / s_kMsPerSecond);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsBeeping
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3Sound::IsBeeping() const
{
    uint64_t  now = GetNow();



    return now >= m_beepStart && now < m_beepEnd;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetDacPort
//
//  The log is bounded: when a program writes the DAC with no frames being
//  rendered, such as in a run with sound off, the log is cleared at its
//  limit and starts again from the latest level.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Sound::SetDacPort (Byte portB)
{
    Byte  level = (Byte) (portB & kDacMask);



    if (level == m_latestLevel)
    {
        return;
    }

    m_latestLevel = level;

    if (m_changes.size() >= s_kMaxDacChanges)
    {
        m_changes.clear();
        m_renderedLevel = level;
        m_renderedTo    = GetNow();
        return;
    }

    m_changes.push_back (DacChange { GetNow(), level });
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Sound::Reset()
{
    m_changes.clear();
    m_beepStart     = 0;
    m_beepEnd       = 0;
    m_renderedTo    = GetNow();
    m_renderedLevel = m_latestLevel;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneratePCM
//
//  The frame's samples cover the ticks from the last frame's end to now,
//  evenly. A gap longer than a second, such as after a pause, starts the
//  frame at now.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3Sound::GeneratePCM (float * outMono, uint32_t numSamples)
{
    uint64_t  end   = GetNow();
    uint64_t  start = m_renderedTo;
    size_t    next  = 0;



    if (numSamples == 0)
    {
        return;
    }

    if (start > end || (double) (end - start) > m_ticksPerSecond)
    {
        start = end;
    }

    for (uint32_t i = 0; i < numSamples; i++)
    {
        uint64_t  tick = start + (end - start) * (i + 1) / numSamples;



        while (next < m_changes.size() && m_changes[next].tick <= tick)
        {
            m_renderedLevel = m_changes[next].level;
            next++;
        }

        outMono[i] = RenderSample (tick);
    }

    m_changes.erase (m_changes.begin(), m_changes.begin() + (ptrdiff_t) next);
    m_renderedTo = end;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderSample
//
//  The DAC through a one-pole DC blocker, so an idle level is silence, and
//  the beeper's square wave while it sounds.
//
////////////////////////////////////////////////////////////////////////////////

float Apple3Sound::RenderSample (uint64_t tick)
{
    float     dac    = (float) m_renderedLevel / (float) (kDacLevels - 1) * kDacGain;
    float     out    = dac - m_dcPrevIn + kDcBlockPole * m_dcPrevOut;
    float     beep   = 0.0f;
    uint64_t  halves = 0;



    m_dcPrevIn  = dac;
    m_dcPrevOut = out;

    if (tick >= m_beepStart && tick < m_beepEnd)
    {
        halves = (uint64_t) ((double) (tick - m_beepStart) * 2.0 * kBeepHz / m_ticksPerSecond);
        beep   = ((halves & 1) == 0) ? kBeepGain : -kBeepGain;
    }

    return out + beep;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
//  The DAC level and the beeper's window. The speaker's state is the
//  AppleSpeaker's section.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3Sound::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteByte   (m_latestLevel);
    writer.WriteUInt64 (m_beepStart);
    writer.WriteUInt64 (m_beepEnd);

    hr = writer.EndSection();
    CHR (hr);

    hr = m_speaker.SaveState (writer);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3Sound::LoadState (StateReader & reader)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadByte   (m_latestLevel);
    reader.ReadUInt64 (m_beepStart);
    reader.ReadUInt64 (m_beepEnd);

    hr = reader.EndSection();
    CHR (hr);

    hr = m_speaker.LoadState (reader);
    CHR (hr);

    m_changes.clear();
    m_renderedLevel = m_latestLevel;
    m_renderedTo    = GetNow();

Error:
    return hr;
}
