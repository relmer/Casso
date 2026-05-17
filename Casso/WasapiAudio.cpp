#include "Pch.h"

#include "WasapiAudio.h"
#include "Audio/AudioGenerator.h"
#include "Audio/DriveAudioMixer.h"
#include "Core/MachineConfig.h"

#pragma comment(lib, "ole32.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  Constants
//
////////////////////////////////////////////////////////////////////////////////

// Equal-power "center" pan coefficient (= sqrt(0.5)). Placing the
// //e speaker at the stereo center via this coefficient (FR-011 /
// FR-012) preserves total acoustic power when the device is stereo
// and yields the same drained amplitude as the pre-feature mono path
// after the m_channels==1 downmix at drain time.
static constexpr float  s_kfSpeakerCenter = 1.0f / (float) std::numbers::sqrt2;





////////////////////////////////////////////////////////////////////////////////
//
//  WasapiAudio
//
////////////////////////////////////////////////////////////////////////////////

WasapiAudio::WasapiAudio()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~WasapiAudio
//
////////////////////////////////////////////////////////////////////////////////

WasapiAudio::~WasapiAudio()
{
    Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WasapiAudio::Initialize()
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

    // Try float32 stereo at the mix format sample rate. The drive
    // mixer (spec 005-disk-ii-audio FR-010 / FR-012) requires a
    // stereo render endpoint to carry per-drive panning. Fallback
    // to the device's native mix format below if stereo float is
    // rejected; the downmix-to-mono path inside SubmitFrame keeps
    // mono devices working.
    desiredFormat.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
    desiredFormat.nChannels       = 2;
    desiredFormat.nSamplesPerSec  = m_sampleRate;
    desiredFormat.wBitsPerSample  = 32;
    desiredFormat.nBlockAlign     = 8;
    desiredFormat.nAvgBytesPerSec = m_sampleRate * 8;

    m_channels = 2;

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

    // Calculate samples per emulation frame:
    // sampleRate * cyclesPerFrame / clockSpeed = exact samples per frame
    m_samplesPerFrame = m_sampleRate * kAppleCyclesPerFrame / kAppleCpuClock;

    // Pre-fill buffer with silence to avoid initial noise
    hr = m_renderClient->GetBuffer (m_bufferFrames, &buffer);
    CHRA (hr);
    hr = m_renderClient->ReleaseBuffer (m_bufferFrames, AUDCLNT_BUFFERFLAGS_SILENT);
    CHRA (hr);

    // Start the audio stream
    hr = m_audioClient->Start();
    CHRA (hr);

    m_initialized = true;

Error:
    if (FAILED (hr))
    {
        DEBUGMSG (L"WASAPI initialization failed (hr=0x%08X). Audio disabled.\n", hr);
        Shutdown();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
////////////////////////////////////////////////////////////////////////////////

void WasapiAudio::Shutdown()
{
    if (m_audioClient)
    {
        m_audioClient->Stop();
    }

    m_renderClient.Reset();
    m_audioClient.Reset();
    m_device.Reset();
    m_enumerator.Reset();

    m_initialized = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SubmitFrame
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
//  SubmitFrame
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WasapiAudio::SubmitFrame (
    const vector<uint32_t>   & toggleTimestamps,
    uint32_t                        totalCyclesThisSlice,
    float                           currentSpeakerState,
    uint32_t                        numSamplesToGenerate,
    DriveAudioMixer *               driveMixer,
    uint64_t                        currentCycleCount)
{
    HRESULT    hr             = S_OK;
    size_t     prevFrames     = 0;
    size_t     stereoFloats   = 0;
    UINT32     padding        = 0;
    UINT32     available      = 0;
    UINT32     pendingFrames  = 0;
    UINT32     toWrite        = 0;
    UINT32     i              = 0;
    BYTE     * buffer         = nullptr;
    float    * samples        = nullptr;
    float    * monoPtr        = nullptr;
    float    * stereoPtr      = nullptr;



    if (!m_initialized || m_renderClient == nullptr)
    {
        return S_OK;
    }

    // m_pendingSamples is interleaved stereo regardless of the
    // device's channel count -- mono devices downmix at drain time.
    // Cap pending buffer to ~3 frames worth to avoid unbounded
    // growth (numSamples == frames, so 3 frames == 6*frames floats).
    prevFrames = m_pendingSamples.size() / 2;

    if (numSamplesToGenerate > 0 && prevFrames < m_samplesPerFrame * 3)
    {
        if (m_speakerScratch.size() < numSamplesToGenerate)
        {
            m_speakerScratch.resize (numSamplesToGenerate);
        }

        if (m_driveScratch.size() < numSamplesToGenerate * 2)
        {
            m_driveScratch.resize (numSamplesToGenerate * 2);
        }

        AudioGenerator::GeneratePCM (toggleTimestamps,
                                     totalCyclesThisSlice,
                                     currentSpeakerState,
                                     m_speakerScratch.data(),
                                     numSamplesToGenerate);

        // Speaker mono -> centered stereo (equal-power center).
        stereoFloats = m_pendingSamples.size();
        m_pendingSamples.resize (stereoFloats + numSamplesToGenerate * 2);
        stereoPtr = &m_pendingSamples[stereoFloats];

        for (i = 0; i < numSamplesToGenerate; i++)
        {
            float  s = m_speakerScratch[i] * s_kfSpeakerCenter;

            stereoPtr[2 * i]     = s;
            stereoPtr[2 * i + 1] = s;
        }

        if (driveMixer != nullptr)
        {
            driveMixer->Tick (currentCycleCount);
            driveMixer->GeneratePCM (m_driveScratch.data(), numSamplesToGenerate);

            DriveAudioMixer::MixDriveIntoSpeakerStereo (
                stereoPtr, m_driveScratch.data(), numSamplesToGenerate);
        }
    }

    // Drain as many pending frames as WASAPI can accept.
    hr = m_audioClient->GetCurrentPadding (&padding);
    CHRA (hr);

    available     = m_bufferFrames - padding;
    pendingFrames = static_cast<UINT32> (m_pendingSamples.size() / 2);
    toWrite       = (available < pendingFrames) ? available : pendingFrames;

    if (toWrite > 0)
    {
        hr = m_renderClient->GetBuffer (toWrite, &buffer);
        CHRA (hr);

        samples = reinterpret_cast<float *> (buffer);
        monoPtr = m_pendingSamples.data();

        if (m_channels == 2)
        {
            memcpy (samples, monoPtr, toWrite * 2 * sizeof (float));
        }
        else if (m_channels == 1)
        {
            // Mono device: average L+R for a clean downmix without
            // an amplitude jump (FR-010).
            for (i = 0; i < toWrite; i++)
            {
                samples[i] = (monoPtr[2 * i] + monoPtr[2 * i + 1]) * 0.5f;
            }
        }
        else
        {
            // Surround-or-greater: broadcast L,R into the first two
            // channels of each frame; remaining channels stay silent.
            memset (samples, 0, toWrite * m_channels * sizeof (float));

            for (i = 0; i < toWrite; i++)
            {
                samples[i * m_channels]     = monoPtr[2 * i];
                samples[i * m_channels + 1] = monoPtr[2 * i + 1];
            }
        }

        hr = m_renderClient->ReleaseBuffer (toWrite, 0);
        CHRA (hr);

        // Remove consumed (interleaved-stereo) samples from front.
        m_pendingSamples.erase (m_pendingSamples.begin(),
                                m_pendingSamples.begin() + toWrite * 2);
    }

Error:
    return hr;
}





