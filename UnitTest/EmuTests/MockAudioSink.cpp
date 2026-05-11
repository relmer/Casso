#include "../CassoEmuCore/Pch.h"

#include "MockAudioSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MockAudioSink::PushSamples
//
//  Drops the audio data; counts a toggle every time the sample sign
//  changes (positive <-> negative) so tests can verify the audio path
//  saw CPU-driven speaker activity.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MockAudioSink::PushSamples (const float * samples, size_t count)
{
    size_t      i;
    float       s;
    bool        prevSign;
    bool        curSign;

    if (samples == nullptr && count != 0)
    {
        return E_INVALIDARG;
    }

    for (i = 0; i < count; i++)
    {
        s        = samples[i];
        curSign  = (s >= 0.0f);

        if (m_haveLast)
        {
            prevSign = (m_lastSample >= 0.0f);

            if (prevSign != curSign)
            {
                m_toggleCount += 1;
            }
        }

        m_lastSample = s;
        m_haveLast   = true;
        m_sampleCount += 1;
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MockAudioSink::Clear
//
////////////////////////////////////////////////////////////////////////////////

void MockAudioSink::Clear ()
{
    m_toggleCount = 0;
    m_sampleCount = 0;
    m_lastSample  = 0.0f;
    m_haveLast    = false;
}
