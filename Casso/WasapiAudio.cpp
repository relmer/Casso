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
//  Opens the default render endpoint in shared mode, preferring 32-bit float
//  stereo and falling back to whatever the device actually offers.
//
//  STEREO is requested rather than mono because the drive-audio mixer carries
//  per-drive panning (spec 005, FR-010 / FR-012) -- two drives on one channel
//  lose the separation that makes them distinguishable by ear.
//
//  The fallback to the device's own mix format is what keeps unusual endpoints
//  working. A device that rejects float stereo still gets audio, and
//  SubmitFrame's downmix path handles a mono result.
//
//  The sample rate is TAKEN from the mix format rather than requested, since
//  shared mode resamples anything else and asking for a rate the device does
//  not use only adds a conversion.
//
//  SHARED mode is deliberate: exclusive mode would give lower latency but take
//  the endpoint away from every other application, which is not a trade an
//  emulator should make on the user's behalf.
//
//  A 100 ms buffer is long enough to absorb a scheduling hiccup without an
//  audible dropout and short enough that speaker latency stays imperceptible.
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
    m_deviceLost  = false;

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
//  NoteEndpointLoss
//
////////////////////////////////////////////////////////////////////////////////

void WasapiAudio::NoteEndpointLoss (HRESULT hrLoss)
{
    DEBUGMSG (L"WASAPI endpoint lost (hr=0x%08X). Reopening the default device shortly.\n", hrLoss);

    Shutdown();

    m_deviceLost = true;
    m_reinitAtMs = NowMs() + kReinitRetryMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NowMs
//
////////////////////////////////////////////////////////////////////////////////

int64_t WasapiAudio::NowMs() const
{
    return (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
               std::chrono::steady_clock::now().time_since_epoch()).count();
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
//  Generates one slice of audio -- speaker, drives, Mockingboard -- mixes it,
//  and drains as much as the endpoint will currently take.
//
//  Generation and submission are DECOUPLED through a pending buffer, because
//  the two run at unrelated rates: the emulator produces samples per CPU
//  slice, while the endpoint accepts them only as its buffer drains. Anything
//  not written now is carried to the next call.
//
//  That buffer is capped at roughly three frames. If the endpoint stalls or
//  the emulator runs ahead at Maximum speed, unbounded growth would turn into
//  both a memory leak and seconds of audio latency; dropping generation
//  instead keeps the sound current, which is what matters for an emulator.
//
//  Everything is mixed as interleaved STEREO regardless of the device's actual
//  channel count, and mono devices downmix at drain time. One internal format
//  means the mixers need no per-device variants.
//
//  The speaker is centered with an EQUAL-POWER factor rather than being copied
//  to both channels at full amplitude, so it does not sound louder than the
//  panned drive sources beside it.
//
//  Scratch buffers are grown and reused rather than allocated per call, since
//  this runs on the CPU thread for every slice.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WasapiAudio::SubmitFrame (
    const vector<uint32_t>   & toggleTimestamps,
    uint32_t                        totalCyclesThisSlice,
    float                           currentSpeakerState,
    uint32_t                        numSamplesToGenerate,
    DriveAudioMixer *               driveMixer,
    uint64_t                        currentCycleCount,
    DriveAudioMixer *               mockingboardMixer)
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



    // Device-loss recovery: after the endpoint died (dock/undock, default-
    // device switch), periodically try to open the CURRENT default device.
    // Runs here because Initialize and SubmitFrame share the CPU thread --
    // no cross-thread state.
    if (m_deviceLost && NowMs() >= m_reinitAtMs)
    {
        m_reinitAtMs = NowMs() + kReinitRetryMs;

        HRESULT  hrReopen = Initialize();
        IGNORE_RETURN_VALUE (hrReopen, S_OK);
    }

    BAIL_OUT_IF (!m_initialized || m_renderClient == nullptr, S_OK);

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

        // Mockingboard PSG audio shares the same additive stereo mix but
        // runs through its own mixer so its Options toggle is independent
        // of Drive Audio. Its sources ignore the cycle-based Tick.
        if (mockingboardMixer != nullptr)
        {
            mockingboardMixer->GeneratePCM (m_driveScratch.data(), numSamplesToGenerate);

            DriveAudioMixer::MixDriveIntoSpeakerStereo (
                stereoPtr, m_driveScratch.data(), numSamplesToGenerate);
        }

        // Master volume: one gain over the completed mix so every source
        // scales together (mute == 0). Applied at generation, not drain, so
        // pending samples keep the gain they were produced under.
        {
            float  gain = m_masterGain.load (std::memory_order_relaxed);

            if (gain != 1.0f)
            {
                for (i = 0; i < numSamplesToGenerate * 2; i++)
                {
                    stereoPtr[i] *= gain;
                }
            }
        }
    }

    // Drain as many pending frames as WASAPI can accept. Endpoint calls can
    // fail at any time when the device disappears (AUDCLNT_E_DEVICE_
    // INVALIDATED on a default-device switch or undock) -- an environmental
    // condition, not a coding error: note the loss and report success, and
    // the recovery above reopens the new default shortly.
    hr = m_audioClient->GetCurrentPadding (&padding);

    if (FAILED (hr))
    {
        NoteEndpointLoss (hr);
    }

    BAIL_OUT_IF (FAILED (hr), S_OK);

    available     = m_bufferFrames - padding;
    pendingFrames = static_cast<UINT32> (m_pendingSamples.size() / 2);
    toWrite       = (available < pendingFrames) ? available : pendingFrames;

    if (toWrite > 0)
    {
        hr = m_renderClient->GetBuffer (toWrite, &buffer);

        if (FAILED (hr))
        {
            NoteEndpointLoss (hr);
        }

        BAIL_OUT_IF (FAILED (hr), S_OK);

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

        if (FAILED (hr))
        {
            NoteEndpointLoss (hr);
        }

        BAIL_OUT_IF (FAILED (hr), S_OK);

        // Remove consumed (interleaved-stereo) samples from front.
        m_pendingSamples.erase (m_pendingSamples.begin(),
                                m_pendingSamples.begin() + toWrite * 2);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecordDriveDoorSyncEvent / GetLastDriveDoorSyncEventMs
//
////////////////////////////////////////////////////////////////////////////////

void WasapiAudio::RecordDriveDoorSyncEvent (int drive, int64_t timestampMs)
{
    if (drive < 0 || drive >= static_cast<int> (m_lastDriveDoorSyncMs.size()))
    {
        return;
    }

    m_lastDriveDoorSyncMs[drive] = timestampMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLastDriveDoorSyncEventMs
//
////////////////////////////////////////////////////////////////////////////////

int64_t WasapiAudio::GetLastDriveDoorSyncEventMs (int drive) const
{
    bool  inRange = (drive >= 0 && drive < static_cast<int> (m_lastDriveDoorSyncMs.size()));



    // 0 is also the never-fired value, so an out-of-range drive reads the
    // same as a drive whose door has not moved this session.
    return inRange ? m_lastDriveDoorSyncMs[drive] : 0;
}




