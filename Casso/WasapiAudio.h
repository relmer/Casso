#pragma once

#include "Pch.h"

#include "Audio/AudioEndpointNotifier.h"


class DriveAudioMixer;





////////////////////////////////////////////////////////////////////////////////
//
//  WasapiAudio
//
//  WASAPI shared-mode pull-mode audio streaming.
//  Float32 mono at device mix format sample rate.
//
////////////////////////////////////////////////////////////////////////////////

// The render side runs on its own event-driven thread: WASAPI signals the
// event each device quantum, and the pump moves pending samples into the
// endpoint independent of the emulation thread's cadence. When the pending
// queue runs dry the pump writes a short fade-out of the last frame instead
// of letting the device insert hard silence -- an underrun becomes a ramp,
// not a click (#125).
class WasapiAudio
{
public:
    WasapiAudio();
    ~WasapiAudio();

    HRESULT Initialize();
    void    Shutdown   ();

    // CPU thread, once per frame, even while IsInitialized() is false.
    void  ServiceEndpointChanges();

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
        DriveAudioMixer *               driveMixer         = nullptr,
        uint64_t                        currentCycleCount  = 0,
        DriveAudioMixer *               mockingboardMixer  = nullptr);
    void    RecordDriveDoorSyncEvent (int drive, int64_t timestampMs);
    int64_t GetLastDriveDoorSyncEventMs (int drive) const;

    bool IsInitialized() const { return m_initialized; }
    UINT32 GetSampleRate() const { return m_sampleRate; }

    // Master output gain (0..1) applied to the completed mix, so every source
    // -- speaker, drives, printer, Mockingboard -- scales together (the chrome
    // toolbar's volume slider; mute passes 0 while the UI keeps the slider
    // value). Set on the UI thread, read on the submitting thread: atomic.
    void  SetMasterGain (float gain01) { m_masterGain.store (gain01, std::memory_order_relaxed); }

private:
    // How long to wait after losing the endpoint before trying to open the
    // (possibly new) default device again. Long enough that a device switch
    // mid-teardown is not hammered, short enough that audio returns before
    // anyone reaches for the volume control.
    static constexpr int64_t  kReinitRetryMs = 1000;

    void    RenderPump ();
    void    DrainFrames (UINT32 toWrite, BYTE * buffer);

    // Tear the client down and arm the throttled reopen. Two ways in, one
    // recovery path.
    //
    // NoteEndpointLoss is the endpoint dying under us: dock/undock, driver
    // reset, the device going away (AUDCLNT_E_DEVICE_INVALIDATED and
    // friends). An expected runtime condition, never an assert.
    //
    // NoteDefaultDeviceChange is the user selecting a different output
    // device. That does NOT invalidate the endpoint already held -- it stays
    // present and every call on it keeps succeeding -- so nothing fails and
    // the loss path never fires. An IMMNotificationClient is the only way to
    // hear about it (GH #137).
    void     ArmEndpointReopen      ();
    void     NoteEndpointLoss       (HRESULT hrLoss);
    void     NoteDefaultDeviceChange();
    int64_t  GetNowMs               () const;

    // PUMP THREAD's half of that: record the FIRST failing hr and let the
    // pump stop. A later failure must not overwrite it, or the reason the
    // endpoint went away would be replaced by a consequence of its going.
    void  ReportEndpointLoss (HRESULT hrLoss);

    ComPtr<IMMDeviceEnumerator>  m_enumerator;
    ComPtr<IMMDevice>            m_device;
    ComPtr<IAudioClient>         m_audioClient;
    ComPtr<IAudioRenderClient>   m_renderClient;

    // Registered on m_enumerator for the life of the stream and unregistered
    // before it is released. Held through a ComPtr because it is
    // reference-counted: a notification still in flight on an MMDevice
    // thread keeps the object alive by itself.
    ComPtr<AudioEndpointNotifier>  m_endpointNotifier;

    UINT32  m_bufferFrames    = 0;
    UINT32  m_sampleRate      = 44100;
    UINT32  m_samplesPerFrame = 735;
    UINT32  m_channels        = 1;
    bool    m_initialized     = false;

    // Pending interleaved-STEREO samples waiting for the render pump.
    // Written by the emulation thread in SubmitFrame, consumed by the
    // pump thread; m_pendingMutex guards every access.
    vector<float> m_pendingSamples;
    std::mutex    m_pendingMutex;

    // Render pump thread state: the WASAPI buffer-ready event, the stop
    // flag, and the last stereo frame written -- the seed for the fade-out
    // filler that replaces hard underrun silence.
    std::thread         m_renderThread;
    HANDLE              m_renderEvent = nullptr;
    std::atomic<bool>   m_renderStop { false };
    float               m_lastL = 0.0f;
    float               m_lastR = 0.0f;

    // Frames left in the filler-to-real resume crossfade (see DrainFrames).
    UINT32              m_resumeRamp = 0;

    // Per-frame scratch buffers. Reused across SubmitFrame() calls
    // to avoid per-frame allocation. m_mixScratch holds the completed
    // stereo mix before it is appended to the pending queue under lock.
    vector<float> m_speakerScratch;
    vector<float> m_driveScratch;
    vector<float> m_mixScratch;

    std::atomic<float>  m_masterGain { 1.0f };   // see SetMasterGain

    // Endpoint loss and the throttled reopen. The PUMP ONLY REPORTS: it
    // records the failing hr and stops, because Shutdown joins the render
    // thread and calling it from inside that thread would join itself. The
    // teardown and the retry both run on the CPU thread, in
    // ServiceEndpointChanges.
    std::atomic<HRESULT>  m_endpointLossHr { S_OK };
    bool                  m_deviceLost = false;
    int64_t               m_reinitAtMs = 0;

    // Diagnostic tap: when CASSO_AUDIO_DUMP names a file, every generated
    // stereo sample is appended to it as raw float32 pairs -- the exact
    // stream handed to the device, captured before the device can touch
    // it. Opened lazily on first use; empty when the variable is unset.
    FILE *  m_dumpFile    = nullptr;
    bool    m_dumpChecked = false;
    std::array<int64_t, 2> m_lastDriveDoorSyncMs { 0, 0 };
};




