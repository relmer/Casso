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

    // Submit one frame of audio from speaker toggle timestamps
    HRESULT SubmitFrame (
        const std::vector<uint32_t> & toggleTimestamps,
        uint32_t totalCyclesThisFrame,
        float currentSpeakerState);

    bool IsInitialized () const { return m_initialized; }

private:
    IMMDeviceEnumerator * m_enumerator;
    IMMDevice *           m_device;
    IAudioClient *        m_audioClient;
    IAudioRenderClient *  m_renderClient;

    UINT32  m_bufferFrames;
    UINT32  m_sampleRate;
    UINT32  m_samplesPerFrame;
    UINT32  m_channels;
    bool    m_initialized;
};
