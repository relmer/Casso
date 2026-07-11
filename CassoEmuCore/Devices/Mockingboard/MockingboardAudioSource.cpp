#include "Pch.h"

#include "MockingboardAudioSource.h"
#include "Ay8910.h"




////////////////////////////////////////////////////////////////////////////////
//
//  GeneratePCM
//
//  Pulls one PSG sample per output frame, DC-blocks the unipolar DAC
//  output into an AC-coupled signal centred on zero, and scales by the
//  master gain. The mixer has already zeroed the buffer, so an unbound
//  source contributes silence.
//
////////////////////////////////////////////////////////////////////////////////

void MockingboardAudioSource::GeneratePCM (float * outMono, uint32_t numSamples)
{
    uint32_t   i        = 0;
    float      raw      = 0.0f;
    float      filtered = 0.0f;

    if (m_psg == nullptr || outMono == nullptr)
    {
        return;
    }

    for (i = 0; i < numSamples; i++)
    {
        raw      = m_psg->GenerateSample ();
        filtered = raw - m_dcPrevIn + kDcBlockPole * m_dcPrevOut;

        m_dcPrevIn  = raw;
        m_dcPrevOut = filtered;

        outMono[i] = filtered * kMasterGain;
    }
}
