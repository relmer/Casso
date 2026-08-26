#include "Pch.h"

#include "Ssi263AudioSource.h"
#include "Ssi263.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GeneratePCM
//
//  Pulls one speech sample per output frame, DC-blocks it, and scales by
//  the master gain. When the chip reports itself silent the buffer is
//  zeroed without invoking synthesis at all -- the fast path that keeps an
//  unprogrammed voice chip free on a card that now ships by default.
//
////////////////////////////////////////////////////////////////////////////////

void Ssi263AudioSource::GeneratePCM (float * outMono, uint32_t numSamples)
{
    uint32_t   i        = 0;
    float      raw      = 0.0f;
    float      filtered = 0.0f;
    bool       silent   = false;



    if (m_speech == nullptr || outMono == nullptr)
    {
        return;
    }

    silent = m_speech->IsSilent();

    if (silent)
    {
        for (i = 0; i < numSamples; i++)
        {
            outMono[i] = 0.0f;
        }

        return;
    }

    for (i = 0; i < numSamples; i++)
    {
        raw      = m_speech->GenerateSample();
        filtered = raw - m_dcPrevIn + kDcBlockPole * m_dcPrevOut;

        m_dcPrevIn  = raw;
        m_dcPrevOut = filtered;

        outMono[i] = filtered * kMasterGain;
    }
}
