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
    HRESULT          hr             = S_OK;
    WAVEFORMATEX   * mixFormat      = nullptr;
    WAVEFORMATEX     desiredFormat  = {};
    REFERENCE_TIME   bufferDuration = 1000000;  // 100ms
    BYTE           * buffer         = nullptr;



    // Create device enumerator
    hr = CoCreateInstance (__uuidof (MMDeviceEnumerator),
                           nullptr,
                           CLSCTX_ALL,
                           IID_PPV_ARGS (&m_enumerator));
    CHRA (hr);

    // Get default audio endpoint
    hr = m_enumerator->GetDefaultAudioEndpoint (eRender, eConsole, &m_device);
    CHRA (hr);

    // Activate audio client
    hr = m_device->Activate (__uuidof (IAudioClient),
                              CLSCTX_ALL,
                              nullptr,
                              &m_audioClient);
    CHRA (hr);

    // Get mix format and try float32 mono
    hr = m_audioClient->GetMixFormat (&mixFormat);
    CHRA (hr);

    m_sampleRate = mixFormat->nSamplesPerSec;

    // Try float32 mono at the mix format sample rate
    desiredFormat.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
    desiredFormat.nChannels       = 1;
    desiredFormat.nSamplesPerSec  = m_sampleRate;
    desiredFormat.wBitsPerSample  = 32;
    desiredFormat.nBlockAlign     = 4;
    desiredFormat.nAvgBytesPerSec = m_sampleRate * 4;

    m_channels = 1;

    hr = m_audioClient->Initialize (AUDCLNT_SHAREMODE_SHARED,
                                    0,
                                    bufferDuration,
                                    0,
                                    &desiredFormat,
                                    nullptr);

    if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT)
    {
        // Fallback: use mix format directly (typically stereo float32)
        m_channels = mixFormat->nChannels;

        hr = m_audioClient->Initialize (AUDCLNT_SHAREMODE_SHARED,
                                        0,
                                        bufferDuration,
                                        0,
                                        mixFormat,
                                        nullptr);
    }

    CoTaskMemFree (mixFormat);
    CHRA (hr);

    // Get buffer size and render client
    hr = m_audioClient->GetBufferSize (&m_bufferFrames);
    CHRA (hr);

    hr = m_audioClient->GetService (IID_PPV_ARGS (&m_renderClient));
    CHRA (hr);

    // Calculate samples per emulation frame (60 fps)
    m_samplesPerFrame = m_sampleRate / 60;

    // Pre-fill buffer with silence to avoid initial noise
    hr = m_renderClient->GetBuffer (m_bufferFrames, &buffer);
    CHRA (hr);
    hr = m_renderClient->ReleaseBuffer (m_bufferFrames, AUDCLNT_BUFFERFLAGS_SILENT);
    CHRA (hr);

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

    m_renderClient.Reset ();
    m_audioClient.Reset ();
    m_device.Reset ();
    m_enumerator.Reset ();

    m_initialized = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SubmitFrame
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WasapiAudio::SubmitFrame (
    const std::vector<uint32_t>   & toggleTimestamps,
    uint32_t                        totalCyclesThisSlice,
    float                           currentSpeakerState,
    uint32_t                        numSamplesToGenerate)
{
    HRESULT    hr        = S_OK;
    size_t     prevSize  = 0;
    UINT32     padding   = 0;
    UINT32     available = 0;
    UINT32     pending   = 0;
    UINT32     toWrite   = 0;
    BYTE     * buffer    = nullptr;
    float    * samples   = nullptr;



    if (!m_initialized || m_renderClient == nullptr)
    {
        return S_OK;
    }

    // Generate this slice's mono samples into the pending buffer.
    // This decouples generation (fixed per slice) from WASAPI drain
    // (variable based on available buffer space), preventing stutter.
    prevSize = m_pendingSamples.size ();

    // Cap pending buffer to ~3 frames to avoid unbounded growth
    if (numSamplesToGenerate > 0 && prevSize < m_samplesPerFrame * 3)
    {
        m_pendingSamples.resize (prevSize + numSamplesToGenerate);

        AudioGenerator::GeneratePCM (toggleTimestamps,
                                     totalCyclesThisSlice,
                                     currentSpeakerState,
                                     &m_pendingSamples[prevSize],
                                     numSamplesToGenerate);
    }

    // Drain as many pending samples as WASAPI can accept
    hr = m_audioClient->GetCurrentPadding (&padding);
    CHRA (hr);

    available = m_bufferFrames - padding;
    pending   = static_cast<UINT32> (m_pendingSamples.size ());
    toWrite   = (available < pending) ? available : pending;

    if (toWrite > 0)
    {
        hr = m_renderClient->GetBuffer (toWrite, &buffer);
        CHRA (hr);

        samples = reinterpret_cast<float *> (buffer);

        if (m_channels == 1)
        {
            memcpy (samples, m_pendingSamples.data (),
                    toWrite * sizeof (float));
        }
        else
        {
            for (UINT32 i = 0; i < toWrite; i++)
            {
                for (UINT32 ch = 0; ch < m_channels; ch++)
                {
                    samples[i * m_channels + ch] = m_pendingSamples[i];
                }
            }
        }

        hr = m_renderClient->ReleaseBuffer (toWrite, 0);
        CHRA (hr);

        // Remove consumed samples from front
        m_pendingSamples.erase (m_pendingSamples.begin (),
                                m_pendingSamples.begin () + toWrite);
    }

Error:
    return hr;
}





