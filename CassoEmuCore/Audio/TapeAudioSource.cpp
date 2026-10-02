#include "Pch.h"

#include "Audio/TapeAudioSource.h"
#include "Devices/Tape/TapeDeck.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeAudioSource::Attach
//
////////////////////////////////////////////////////////////////////////////////

void TapeAudioSource::Attach (const TapeDeck * deck, NowFn now)
{
    m_deck    = deck;
    m_now     = std::move (now);
    m_hasLast = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeAudioSource::GeneratePCM
//
//  The slice being mixed ran from the cycle of the previous call to now; each
//  output sample takes the tape's level at its point in that span.
//
////////////////////////////////////////////////////////////////////////////////

void TapeAudioSource::GeneratePCM (float * outMono, uint32_t numSamples)
{
    uint64_t  now       = m_now ? m_now() : 0;
    uint64_t  first     = m_hasLast && m_lastCycle <= now ? m_lastCycle : now;
    double    span      = (double) (now - first);
    bool      isPlaying = m_deck != nullptr && m_deck->GetTransport() == TapeTransport::Playing;



    for (uint32_t i = 0; i < numSamples; i++)
    {
        uint64_t  cycle = first + (uint64_t) (span * (double) (i + 1) / (double) numSamples);

        outMono[i] = !isPlaying ? 0.0f : (m_deck->PeekLevel (cycle) ? kAmplitude : -kAmplitude);
    }

    m_lastCycle = now;
    m_hasLast   = true;
}
