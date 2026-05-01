#include "Pch.h"

#include "WasapiAudio.h"
#include "Audio/AudioGenerator.h"

#pragma comment(lib, "ole32.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  WasapiAudio
//
////////////////////////////////////////////////////////////////////////////////

WasapiAudio::WasapiAudio ()
    : m_enumerator      (nullptr),
      m_device          (nullptr),
      m_audioClient     (nullptr),
      m_renderClient    (nullptr),
      m_bufferFrames    (0),
      m_sampleRate      (44100),
      m_samplesPerFrame (735),
      m_channels        (1),
      m_initialized     (false)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~WasapiAudio
//
////////////////////////////////////////////////////////////////////////////////

WasapiAudio::~WasapiAudio ()
{
    Shutdown ();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WasapiAudio::Initialize ()
{
    HRESULT hr = S_OK;

    // Create device enumerator
    hr = CoCreateInstance (
        __uuidof (MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof (IMMDeviceEnumerator),
        reinterpret_cast<void **> (&m_enumerator));
    CHRA (hr);

    // Get default audio endpoint
    hr = m_enumerator->GetDefaultAudioEndpoint (eRender, eConsole, &m_device);
    CHRA (hr);

    // Activate audio client
    hr = m_device->Activate (
        __uuidof (IAudioClient),
        CLSCTX_ALL,
        nullptr,
        reinterpret_cast<void **> (&m_audioClient));
    CHRA (hr);

    // Get mix format and try float32 mono
    {
        WAVEFORMATEX * mixFormat = nullptr;
        hr = m_audioClient->GetMixFormat (&mixFormat);
        CHRA (hr);

        m_sampleRate = mixFormat->nSamplesPerSec;

        // Try float32 mono at the mix format sample rate
        WAVEFORMATEX desiredFormat = {};
        desiredFormat.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
        desiredFormat.nChannels       = 1;
        desiredFormat.nSamplesPerSec  = m_sampleRate;
        desiredFormat.wBitsPerSample  = 32;
        desiredFormat.nBlockAlign     = 4;
        desiredFormat.nAvgBytesPerSec = m_sampleRate * 4;

        REFERENCE_TIME bufferDuration = 330000;  // ~33ms

        m_channels = 1;

        hr = m_audioClient->Initialize (
            AUDCLNT_SHAREMODE_SHARED,
            0,
            bufferDuration,
            0,
            &desiredFormat,
            nullptr);

        if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT)
        {
            // Fallback: use mix format directly (typically stereo float32)
            m_channels = mixFormat->nChannels;

            hr = m_audioClient->Initialize (
                AUDCLNT_SHAREMODE_SHARED,
                0,
                bufferDuration,
                0,
                mixFormat,
                nullptr);
        }

        CoTaskMemFree (mixFormat);
        CHRA (hr);
    }

    // Get buffer size and render client
    hr = m_audioClient->GetBufferSize (&m_bufferFrames);
    CHRA (hr);

    hr = m_audioClient->GetService (
        __uuidof (IAudioRenderClient),
        reinterpret_cast<void **> (&m_renderClient));
    CHRA (hr);

    // Calculate samples per emulation frame (60 fps)
    m_samplesPerFrame = m_sampleRate / 60;

    // Pre-fill buffer with silence to avoid initial noise
    {
        BYTE * buffer = nullptr;
        hr = m_renderClient->GetBuffer (m_bufferFrames, &buffer);
        CHRA (hr);
        hr = m_renderClient->ReleaseBuffer (m_bufferFrames, AUDCLNT_BUFFERFLAGS_SILENT);
        CHRA (hr);
    }

    // Start the audio stream
    hr = m_audioClient->Start ();
    CHRA (hr);

    m_initialized = true;

Error:
    if (FAILED (hr))
    {
        DEBUGMSG (L"WASAPI initialization failed (hr=0x%08X). Audio disabled.\n", hr);
        Shutdown ();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
////////////////////////////////////////////////////////////////////////////////

void WasapiAudio::Shutdown ()
{
    if (m_audioClient)
    {
        m_audioClient->Stop ();
    }

    if (m_renderClient) { m_renderClient->Release (); m_renderClient = nullptr; }
    if (m_audioClient)  { m_audioClient->Release ();  m_audioClient  = nullptr; }
    if (m_device)       { m_device->Release ();       m_device       = nullptr; }
    if (m_enumerator)   { m_enumerator->Release ();   m_enumerator   = nullptr; }

    m_initialized = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SubmitFrame
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WasapiAudio::SubmitFrame (
    const std::vector<uint32_t> & toggleTimestamps,
    uint32_t totalCyclesThisFrame,
    float currentSpeakerState)
{
    HRESULT hr = S_OK;

    if (!m_initialized || m_renderClient == nullptr)
    {
        return S_OK;
    }

    // Get available buffer space
    {
        UINT32 padding = 0;
        hr = m_audioClient->GetCurrentPadding (&padding);
        CHRA (hr);

        UINT32 available = m_bufferFrames - padding;

        if (available == 0)
        {
            return S_OK;
        }

        // Cap submission to one frame's worth of samples
        UINT32 toWrite = (available < m_samplesPerFrame) ? available : m_samplesPerFrame;

        // Get buffer — WASAPI frames include all channels
        BYTE * buffer = nullptr;
        hr = m_renderClient->GetBuffer (toWrite, &buffer);
        CHRA (hr);

        {
            float * samples = reinterpret_cast<float *> (buffer);

            if (m_channels == 1)
            {
                // Mono — write directly
                AudioGenerator::GeneratePCM (
                    toggleTimestamps,
                    totalCyclesThisFrame,
                    currentSpeakerState,
                    samples,
                    toWrite);
            }
            else
            {
                // Stereo (or more) — generate mono into a temp buffer,
                // then duplicate to all channels
                std::vector<float> mono (toWrite);

                AudioGenerator::GeneratePCM (
                    toggleTimestamps,
                    totalCyclesThisFrame,
                    currentSpeakerState,
                    mono.data (),
                    toWrite);

                for (UINT32 i = 0; i < toWrite; i++)
                {
                    for (UINT32 ch = 0; ch < m_channels; ch++)
                    {
                        samples[i * m_channels + ch] = mono[i];
                    }
                }
            }
        }

        hr = m_renderClient->ReleaseBuffer (toWrite, 0);
        CHRA (hr);
    }

Error:
    return hr;
}
