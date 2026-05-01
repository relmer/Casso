#pragma once

#include "Pch.h"





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
    WasapiAudio ();
    ~WasapiAudio ();

    HRESULT Initialize ();
    void    Shutdown   ();

    // Submit one audio slice from speaker toggle timestamps
    HRESULT SubmitFrame (
        const std::vector<uint32_t>   & toggleTimestamps,
        uint32_t                        totalCyclesThisSlice,
        float                           currentSpeakerState,
        uint32_t                        numSamplesToGenerate);

    bool IsInitialized () const { return m_initialized; }
    UINT32 GetSampleRate () const { return m_sampleRate; }

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

    // Pending mono samples waiting to be drained into WASAPI
    std::vector<float> m_pendingSamples;
};





