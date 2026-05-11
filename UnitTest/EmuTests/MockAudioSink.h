#pragma once

#include "../../CassoEmuCore/Pch.h"

#include "Core/IHostShell.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MockAudioSink
//
//  IAudioSink test double. Drops samples (no audio device opened) and
//  counts the number of zero-crossings (toggles) seen across pushed
//  samples — that count reflects CPU $C030 toggles when the speaker is
//  wired into the audio path in a later phase.
//
//  In F0 the speaker is not yet wired through IAudioSink; the count is
//  exercised by tests that push synthetic samples directly. The toggle
//  counter is a public testing API.
//
////////////////////////////////////////////////////////////////////////////////

class MockAudioSink : public IAudioSink
{
public:
    HRESULT             PushSamples (const float * samples, size_t count) override;

    void                RecordToggle    () { m_toggleCount += 1; }
    uint64_t            GetToggleCount  () const { return m_toggleCount; }
    uint64_t            GetSampleCount  () const { return m_sampleCount; }
    void                Clear           ();

private:
    uint64_t            m_toggleCount = 0;
    uint64_t            m_sampleCount = 0;
    float               m_lastSample  = 0.0f;
    bool                m_haveLast    = false;
};
