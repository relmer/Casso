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
    HRESULT                  hr             = S_OK;
    WAVEFORMATEX           * mixFormat      = nullptr;
    WAVEFORMATEX             desiredFormat  = {};
    REFERENCE_TIME           bufferDuration = 1000000;  // 100ms
    BYTE                   * buffer         = nullptr;
    uint64_t                 frameSamples   = 0;
    AudioEndpointNotifier  * notifier       = nullptr;



    // Create device enumerator
    hr = CoCreateInstance (__uuidof (MMDeviceEnumerator),
                           nullptr,
                           CLSCTX_ALL,
                           IID_PPV_ARGS (&m_enumerator));
    CHRA (hr);

    // Watch for the default render endpoint moving before resolving it. The
    // endpoint below is resolved once, and a later switch of the default
    // output device leaves it valid, so nothing fails and no HRESULT reports
    // the change -- this callback is the only notice we get (GH #137).
    notifier = new AudioEndpointNotifier();
    CPRA (notifier);

    m_endpointNotifier.Attach (notifier);

    hr = m_enumerator->RegisterEndpointNotificationCallback (m_endpointNotifier.Get());
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
                                    AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                    bufferDuration,
                                    0,
                                    &desiredFormat,
                                    nullptr);

    if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT)
    {
        // Fallback: use mix format directly (typically stereo float32)
        m_channels = mixFormat->nChannels;

        hr = m_audioClient->Initialize (AUDCLNT_SHAREMODE_SHARED,
                                        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                        bufferDuration,
                                        0,
                                        mixFormat,
                                        nullptr);
    }

    CoTaskMemFree (mixFormat);
    CHRA (hr);

    // The render pump sleeps on this event; WASAPI signals it once per
    // device quantum when buffer space is ready.
    m_renderEvent = CreateEventW (nullptr, FALSE, FALSE, nullptr);
    CBRA (m_renderEvent != nullptr);

    hr = m_audioClient->SetEventHandle (m_renderEvent);
    CHRA (hr);

    // Get buffer size and render client
    hr = m_audioClient->GetBufferSize (&m_bufferFrames);
    CHRA (hr);

    hr = m_audioClient->GetService (IID_PPV_ARGS (&m_renderClient));
    CHRA (hr);

    // Samples per emulation frame, TRUNCATED rather than exact: the Apple's
    // frame rate is kAppleCpuClock / kAppleCyclesPerFrame = 60.05 Hz, not 60,
    // so 44100 Hz gives 734.3 and this keeps 734. The fraction does not matter
    // because the only reader is the pending-buffer cap in SubmitFrame.
    //
    // The product is taken in 64 bits because a 32-bit multiply wraps once the
    // endpoint rate passes 252,200 Hz, and pro interfaces do report 352800 and
    // 384000 shared-mode mix formats. A wrapped product stays positive and
    // merely looks small, so the cap would have sat at about a third of its
    // intended depth and starved the mixer.
    frameSamples      = static_cast<uint64_t> (m_sampleRate) * kAppleCyclesPerFrame / kAppleCpuClock;
    m_samplesPerFrame = static_cast<UINT32> (frameSamples);

    // Pre-fill buffer with silence to avoid initial noise
    hr = m_renderClient->GetBuffer (m_bufferFrames, &buffer);
    CHRA (hr);
    hr = m_renderClient->ReleaseBuffer (m_bufferFrames, AUDCLNT_BUFFERFLAGS_SILENT);
    CHRA (hr);

    // Start the audio stream
    hr = m_audioClient->Start();
    CHRA (hr);

    m_renderStop.store (false, std::memory_order_relaxed);
    m_renderThread = std::thread (&WasapiAudio::RenderPump, this);

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
    HRESULT  hrUnregister = S_OK;



    // Stop the pump before touching the client: set the flag, then signal
    // the event so the thread observes it without waiting out a timeout.
    m_renderStop.store (true, std::memory_order_relaxed);

    if (m_renderEvent != nullptr)
    {
        SetEvent (m_renderEvent);
    }

    if (m_renderThread.joinable())
    {
        m_renderThread.join();
    }

    if (m_audioClient)
    {
        m_audioClient->Stop();
    }

    // Unregister before the enumerator is released: the enumerator owns the
    // registration, so dropping it while notifications are still routed here
    // is a use-after-free waiting for the next device switch.
    if (m_enumerator && m_endpointNotifier)
    {
        hrUnregister = m_enumerator->UnregisterEndpointNotificationCallback (m_endpointNotifier.Get());
        IGNORE_RETURN_VALUE (hrUnregister, S_OK);
    }

    m_endpointNotifier.Reset();

    m_renderClient.Reset();
    m_audioClient.Reset();
    m_device.Reset();
    m_enumerator.Reset();

    if (m_renderEvent != nullptr)
    {
        CloseHandle (m_renderEvent);
        m_renderEvent = nullptr;
    }

    if (m_dumpFile != nullptr)
    {
        fclose (m_dumpFile);
        m_dumpFile = nullptr;
    }

    //  Beside its producer-side twin above. Left open, the tail of a capture
    //  sat in the stdio buffer and never reached the file -- and the end of
    //  the stream is exactly what this tap is opened to look at.
    if (m_devDumpFile != nullptr)
    {
        fclose (m_devDumpFile);
        m_devDumpFile = nullptr;
    }

    m_devDumpChecked = false;

    m_initialized = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArmEndpointReopen
//
//  CPU THREAD ONLY. The half both recovery routes share: drop the stream and
//  schedule one reopen. Shutdown joins the render pump, which is why the pump
//  only reports through m_endpointLossHr and the notification client only
//  sets a flag -- neither may tear the stream down from its own thread.
//
//  The delay is re-armed on every entry, so the burst of notifications
//  Windows sends while a device settles pushes the single reopen out instead
//  of producing one attempt each.
//
////////////////////////////////////////////////////////////////////////////////

void WasapiAudio::ArmEndpointReopen()
{
    Shutdown();

    m_deviceLost = true;
    m_reinitAtMs = GetNowMs() + kReinitRetryMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NoteEndpointLoss
//
//  CPU THREAD ONLY. The endpoint went away under us; the pump recorded the
//  failing hr and stopped.
//
////////////////////////////////////////////////////////////////////////////////

void WasapiAudio::NoteEndpointLoss (HRESULT hrLoss)
{
    DEBUGMSG (L"WASAPI endpoint lost (hr=0x%08X). Reopening the default device shortly.\n", hrLoss);

    ArmEndpointReopen();
}





////////////////////////////////////////////////////////////////////////////////
//
//  NoteDefaultDeviceChange
//
//  CPU THREAD ONLY. The user selected a different default output device. The
//  endpoint already held stays valid, so the notification is the only signal
//  there is; without it audio stays on the old device until the next restart
//  (GH #137).
//
////////////////////////////////////////////////////////////////////////////////

void WasapiAudio::NoteDefaultDeviceChange()
{
    DEBUGMSG (L"WASAPI default render endpoint changed. Reopening on the new device shortly.\n");

    ArmEndpointReopen();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReportEndpointLoss
//
//  PUMP THREAD ONLY. Records the FIRST failing hr; a later one does not
//  displace it, because the reason the endpoint went away is worth more than
//  a consequence of its going.
//
////////////////////////////////////////////////////////////////////////////////

void WasapiAudio::ReportEndpointLoss (HRESULT hrLoss)
{
    HRESULT  expected = S_OK;
    bool     stored   = false;



    stored = m_endpointLossHr.compare_exchange_strong (expected, hrLoss,
                                                       std::memory_order_release);
    IGNORE_RETURN_VALUE (stored, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NowMs
//
////////////////////////////////////////////////////////////////////////////////

int64_t WasapiAudio::GetNowMs() const
{
    return (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
               std::chrono::steady_clock::now().time_since_epoch()).count();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ServiceEndpointChanges
//
//  CPU THREAD ONLY, once per frame. The single recovery route, fed by two
//  reports: the pump's failing hr for an endpoint that went away, and the
//  notification client's flag for a default output device the user changed.
//
//  It lives OUTSIDE SubmitFrame because a teardown clears m_initialized, and
//  the owner reaches SubmitFrame only while audio is up: with the recovery
//  inside, the first teardown closed the only door the reopen could come
//  through and audio stayed dead until the next restart.
//
//  A loss outranks a device change, since the loss carries the hr worth
//  logging and both arm the same reopen anyway.
//
//  The reopen can land on a device with a different mix format, so the
//  sample rate is free to move across this call. Whatever the owner decoded
//  at the old rate has to be re-derived; GetSampleRate is what to compare
//  against.
//
////////////////////////////////////////////////////////////////////////////////

void WasapiAudio::ServiceEndpointChanges()
{
    HRESULT  hrLost       = S_OK;
    HRESULT  hrReopen     = S_OK;
    bool     defaultMoved = false;



    hrLost = m_endpointLossHr.exchange (S_OK, std::memory_order_acquire);

    if (FAILED (hrLost))
    {
        NoteEndpointLoss (hrLost);
    }
    else if (m_endpointNotifier)
    {
        defaultMoved = m_endpointNotifier->ConsumeDefaultRenderChange();

        if (defaultMoved)
        {
            NoteDefaultDeviceChange();
        }
    }

    if (m_deviceLost && GetNowMs() >= m_reinitAtMs)
    {
        m_reinitAtMs = GetNowMs() + kReinitRetryMs;

        hrReopen = Initialize();

        // Clearing the flag is what ends the retry. Leaving it set re-entered
        // Initialize every second over a live stream, and the second pass
        // assigned over a joinable render thread, which terminates the
        // process.
        if (SUCCEEDED (hrReopen))
        {
            m_deviceLost = false;
        }
    }
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
    UINT32     i              = 0;
    float    * stereoPtr      = nullptr;



    BAIL_OUT_IF (!m_initialized || m_renderClient == nullptr, S_OK);

    // m_pendingSamples is interleaved stereo regardless of the
    // device's channel count -- mono devices downmix in the pump.
    // Cap pending buffer to ~3 frames worth to avoid unbounded
    // growth (numSamples == frames, so 3 frames == 6*frames floats).
    {
        std::lock_guard<std::mutex>   lock (m_pendingMutex);

        prevFrames = m_pendingSamples.size() / 2;
    }

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

        // Speaker mono -> centered stereo (equal-power center), mixed in
        // scratch; the pending queue is touched only briefly at the end.
        if (m_mixScratch.size() < numSamplesToGenerate * 2)
        {
            m_mixScratch.resize (numSamplesToGenerate * 2);
        }

        stereoPtr = m_mixScratch.data();

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

        // Diagnostic tap (CASSO_AUDIO_DUMP): the generated mix exactly as
        // handed to the device, so an artifact heard from the speakers can
        // be attributed to our stream or to the device path by comparing
        // this dump against a loopback capture.
        if (!m_dumpChecked)
        {
            char    path[MAX_PATH] = {};
            size_t  len            = 0;

            m_dumpChecked = true;

            if (getenv_s (&len, path, sizeof (path), "CASSO_AUDIO_DUMP") == 0 && len > 1)
            {
                fopen_s (&m_dumpFile, path, "wb");
            }
        }

        if (m_dumpFile != nullptr)
        {
            fwrite (stereoPtr, sizeof (float), numSamplesToGenerate * 2, m_dumpFile);
        }

        // Hand the finished mix to the render pump.
        {
            std::lock_guard<std::mutex>   lock (m_pendingMutex);

            m_pendingSamples.insert (m_pendingSamples.end(),
                                     stereoPtr,
                                     stereoPtr + numSamplesToGenerate * 2);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderPump
//
//  The dedicated render thread (#125). WASAPI signals the event once per
//  device quantum; each wake feeds the endpoint from the pending queue.
//  Filler is a LAST RESORT, written only when the device is about to run
//  dry: an early version padded all free space with filler whenever the
//  queue was momentarily short, and every filler-to-real resume was a
//  small step -- a steady tick under sustained audio. Normally the pump
//  writes exactly what is pending and lets the device padding ride.
//
////////////////////////////////////////////////////////////////////////////////

void WasapiAudio::RenderPump()
{
    HRESULT   hr        = S_OK;
    DWORD     waitMs    = 200;
    UINT32    padding   = 0;
    UINT32    available = 0;
    UINT32    pending   = 0;
    UINT32    floorFr   = 0;
    UINT32    toWrite   = 0;
    BYTE    * buffer    = nullptr;



    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    // The depth at which the endpoint is treated as about to run dry, and the
    // only condition under which filler is written at all.
    //
    // This was 20 ms, and that was the whole problem. Logged over 3584 render
    // wakes the device queue sits at a mean of 19.2 ms and never once fell
    // below 10, while the pending queue is legitimately empty at 38% of wakes
    // -- a producer running at real time has nothing in hand most of the time.
    // A 20 ms floor against a 19 ms queue is therefore true almost whenever
    // pending is empty, so filler was spliced into an endpoint that was in no
    // danger whatsoever. 5 ms is a real emergency: the measured minimum is
    // twice that, so this should never fire in normal running, and if it does
    // the device genuinely was about to starve.
    floorFr = m_sampleRate / 200;

    while (!m_renderStop.load (std::memory_order_relaxed))
    {
        WaitForSingleObject (m_renderEvent, waitMs);

        if (m_renderStop.load (std::memory_order_relaxed))
        {
            break;
        }

        hr = m_audioClient->GetCurrentPadding (&padding);

        if (FAILED (hr))
        {
            ReportEndpointLoss (hr);
            break;
        }

        available = m_bufferFrames - padding;

        {
            std::lock_guard<std::mutex>   lock (m_pendingMutex);

            pending = static_cast<UINT32> (m_pendingSamples.size() / 2);
        }

        // Write everything pending, up to the free space.
        toWrite = (pending < available) ? pending : available;

        // Filler is a last resort, not a top-up. Extending every pass to reach
        // a queue floor splices a fade and a re-ramp into a stream that is
        // keeping pace perfectly well; what the device actually needs is
        // simply not to run dry, and while `padding` frames remain queued it
        // will not.
        //
        // This was tried once before and measured no better, because the CPU
        // thread's frame pacing was jittering hard enough to empty the queue
        // regardless -- see CpuManager::ThreadProc. With the pacing corrected,
        // the queue keeps a level and this is what removes the rest.
        //
        // WHAT IS QUEUED AFTER THIS PASS IS THE TEST, not whether anything is
        // pending. Gating on an empty pending queue read almost the same on
        // the measurements -- pending is empty at 38% of wakes and holds only
        // a handful of frames at most of the rest -- while leaving the case it
        // is supposed to cover wide open: a nearly dry endpoint with three
        // frames in hand would be sent those three and nothing else.
        if (padding + toWrite < floorFr)
        {
            toWrite = ((floorFr - padding) < available) ? (floorFr - padding)
                                                        : available;
        }

        if (toWrite == 0)
        {
            continue;
        }

        hr = m_renderClient->GetBuffer (toWrite, &buffer);

        if (FAILED (hr))
        {
            ReportEndpointLoss (hr);
            break;
        }

        DrainFrames (toWrite, buffer);

        m_renderClient->ReleaseBuffer (toWrite, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrainFrames
//
//  Fills `toWrite` device frames: pending samples first, then the fade-out
//  filler. The pending queue holds interleaved stereo; mono devices downmix
//  here and wider layouts take L,R in their first two channels.
//
////////////////////////////////////////////////////////////////////////////////

void WasapiAudio::DrainFrames (UINT32 toWrite, BYTE * buffer)
{
    // The fade coefficient takes the held frame to inaudible within a few
    // milliseconds -- fast enough to sound like a release, not an echo.
    // The resume ramp reconnects real audio to the faded state over ~2 ms,
    // so coming BACK from filler is as step-free as entering it.
    static constexpr float    kFillerDecay      = 0.995f;
    static constexpr UINT32   kResumeRampFrames = 96;



    UINT32   fromPending = 0;
    UINT32   i           = 0;
    float  * samples     = reinterpret_cast<float *> (buffer);
    float    blend       = 0.0f;
    float    left        = 0.0f;
    float    right       = 0.0f;



    std::lock_guard<std::mutex>   lock (m_pendingMutex);

    fromPending = static_cast<UINT32> (m_pendingSamples.size() / 2);

    if (fromPending > toWrite)
    {
        fromPending = toWrite;
    }

    for (i = 0; i < toWrite; i++)
    {
        if (i < fromPending)
        {
            left  = m_pendingSamples[2 * i];
            right = m_pendingSamples[2 * i + 1];

            if (m_resumeRamp > 0)
            {
                blend = static_cast<float> (m_resumeRamp) /
                        static_cast<float> (kResumeRampFrames);

                left  += (m_lastL - left)  * blend;
                right += (m_lastR - right) * blend;

                m_resumeRamp--;
            }

            m_lastL = left;
            m_lastR = right;
        }
        else
        {
            m_lastL *= kFillerDecay;
            m_lastR *= kFillerDecay;

            left  = m_lastL;
            right = m_lastR;

            m_resumeRamp = kResumeRampFrames;
        }

        if (m_channels == 1)
        {
            samples[i] = (left + right) * 0.5f;
        }
        else if (m_channels == 2)
        {
            samples[2 * i]     = left;
            samples[2 * i + 1] = right;
        }
        else
        {
            memset (&samples[i * m_channels], 0, m_channels * sizeof (float));

            samples[i * m_channels]     = left;
            samples[i * m_channels + 1] = right;
        }
    }

    // Diagnostic tap (CASSO_AUDIO_DUMP_DEVICE): the frames as handed to the
    // endpoint, filler included. CASSO_AUDIO_DUMP taps the producer side; the
    // two together say whether an artifact was generated or was introduced by
    // this queue. That comparison is what identified the filler cadence.
    if (!m_devDumpChecked)
    {
        char     path[MAX_PATH] = {};
        size_t   len            = 0;

        m_devDumpChecked = true;

        if (getenv_s (&len, path, sizeof (path), "CASSO_AUDIO_DUMP_DEVICE") == 0 && len > 1)
        {
            fopen_s (&m_devDumpFile, path, "wb");
        }
    }

    if (m_devDumpFile != nullptr)
    {
        fwrite (samples, sizeof (float), toWrite * m_channels, m_devDumpFile);
    }

    m_pendingSamples.erase (m_pendingSamples.begin(),
                            m_pendingSamples.begin() + fromPending * 2);
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




