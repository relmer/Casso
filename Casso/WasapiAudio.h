#pragma once

#include "Pch.h"


class DriveAudioMixer;





////////////////////////////////////////////////////////////////////////////////
//
//  WasapiAudio
//
//  WASAPI shared-mode pull-mode audio streaming.
//  Float32 mono at device mix format sample rate.
//
////////////////////////////////////////////////////////////////////////////////

class WasapiAudio
{
public:
    WasapiAudio();
    ~WasapiAudio();

    HRESULT Initialize();
    void    Shutdown   ();

    // Submit one audio slice from speaker toggle timestamps. The
    // optional `driveMixer` parameter is the host's per-frame
    // stereo drive-audio source (FR-010); pass nullptr to retain
    // pre-feature speaker-only behavior (FR-011 / SC-006). When
    // provided, the mixer is ticked then asked for `numSamplesToGenerate`
    // samples of stereo PCM, additively summed into the speaker
    // signal and clamped per channel to [-1, +1].
    HRESULT SubmitFrame (
        const vector<uint32_t>   & toggleTimestamps,
        uint32_t                        totalCyclesThisSlice,
        float                           currentSpeakerState,
        uint32_t                        numSamplesToGenerate,
        DriveAudioMixer *               driveMixer        = nullptr,
        uint64_t                        currentCycleCount = 0);

    bool IsInitialized() const { return m_initialized; }
    UINT32 GetSampleRate() const { return m_sampleRate; }

private:
    ComPtr<IMMDeviceEnumerator>  m_enumerator;
    ComPtr<IMMDevice>            m_device;
    ComPtr<IAudioClient>         m_audioClient;
    ComPtr<IAudioRenderClient>   m_renderClient;

    UINT32  m_bufferFrames    = 0;
    UINT32  m_sampleRate      = 44100;
    UINT32  m_samplesPerFrame = 735;
    UINT32  m_channels        = 1;
    bool    m_initialized     = false;

    // Pending interleaved samples waiting to be drained into WASAPI.
    // Layout matches m_channels: mono buffers store one float per
    // frame; stereo buffers store interleaved L,R pairs (2 floats
    // per frame). The drain loop divides size() by m_channels to
    // compute frame counts.
    vector<float> m_pendingSamples;

    // Per-frame scratch buffers. Reused across SubmitFrame() calls
    // to avoid per-frame allocation.
    vector<float> m_speakerScratch;
    vector<float> m_driveScratch;
};





