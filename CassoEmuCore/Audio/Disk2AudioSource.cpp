#include "Pch.h"

#include "Audio/Disk2AudioSource.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "propsys.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  File-scope helpers
//
////////////////////////////////////////////////////////////////////////////////

static const wchar_t * const s_kpszSampleFiles[] =
{
    L"MotorLoop.wav",
    L"HeadStep.wav",
    L"HeadStop.wav",
    L"DoorOpen.wav",
    L"DoorClose.wav",
};

static constexpr size_t  s_kcSampleFiles = _countof (s_kpszSampleFiles);

// Slot indices into the loader's local array -- kept in lockstep with
// s_kpszSampleFiles so a single loop populates everything.
static constexpr size_t  s_kSlotMotorLoop = 0;
static constexpr size_t  s_kSlotHeadStep  = 1;
static constexpr size_t  s_kSlotHeadStop  = 2;
static constexpr size_t  s_kSlotDoorOpen  = 3;
static constexpr size_t  s_kSlotDoorClose = 4;





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeWavToMonoFloat
//
//  Open `path` via IMFSourceReader, force float32 mono at
//  `targetSampleRate`, read every sample to `outSamples`. Empty
//  outSamples on any failure (caller treats empty == mute, FR-009).
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT DecodeWavToMonoFloat (
    const wchar_t *  path,
    uint32_t         targetSampleRate,
    vector<float> &  outSamples)
{
    HRESULT                  hr             = S_OK;
    ComPtr<IMFSourceReader>  reader;
    ComPtr<IMFMediaType>     pcmType;
    ComPtr<IMFSample>        sample;
    ComPtr<IMFMediaBuffer>   buffer;
    DWORD                    flags          = 0;
    DWORD                    bufLen         = 0;
    BYTE *                   bufPtr         = nullptr;
    const float *            srcFloat       = nullptr;
    DWORD                    floatCount     = 0;
    DWORD                    i              = 0;

    outSamples.clear();

    hr = MFCreateSourceReaderFromURL (path, nullptr, &reader);
    CHR (hr);

    hr = MFCreateMediaType (&pcmType);
    CHR (hr);

    hr = pcmType->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    CHR (hr);

    hr = pcmType->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_Float);
    CHR (hr);

    hr = pcmType->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, 1);
    CHR (hr);

    hr = pcmType->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, targetSampleRate);
    CHR (hr);

    hr = pcmType->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, 32);
    CHR (hr);

    hr = pcmType->SetUINT32 (MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
    CHR (hr);

    hr = pcmType->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, targetSampleRate * 4);
    CHR (hr);

    hr = pcmType->SetUINT32 (MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    CHR (hr);

    hr = reader->SetCurrentMediaType (
        MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pcmType.Get());
    CHR (hr);

    hr = reader->SetStreamSelection (MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
    CHR (hr);

    for (;;)
    {
        sample.Reset();
        buffer.Reset();
        flags = 0;

        hr = reader->ReadSample (
            MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0, nullptr, &flags, nullptr, &sample);
        CHR (hr);

        if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
        {
            break;
        }

        if (sample.Get() == nullptr)
        {
            continue;
        }

        hr = sample->ConvertToContiguousBuffer (&buffer);
        CHR (hr);

        hr = buffer->Lock (&bufPtr, nullptr, &bufLen);
        CHR (hr);

        srcFloat   = reinterpret_cast<const float *> (bufPtr);
        floatCount = bufLen / sizeof (float);

        for (i = 0; i < floatCount; i++)
        {
            outSamples.push_back (srcFloat[i]);
        }

        hr = buffer->Unlock();
        CHR (hr);
    }

Error:
    if (FAILED (hr))
    {
        outSamples.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2AudioSource
//
////////////////////////////////////////////////////////////////////////////////

Disk2AudioSource::Disk2AudioSource()
{
}


Disk2AudioSource::~Disk2AudioSource()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadSamples
//
//  Best-effort load of all five Disk II sample assets. Missing or
//  malformed files leave their buffer empty -- the mix path silently
//  skips empty buffers (FR-009). Returns S_OK whenever MediaFoundation
//  was reachable; per-file failures do NOT propagate up.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2AudioSource::LoadSamples (
    const wchar_t * devicesDir,
    const wchar_t * mechanism,
    uint32_t        targetSampleRate)
{
    HRESULT         hr            = S_OK;
    bool            mfStarted     = false;
    size_t          i             = 0;
    fs::path        baseDir;
    fs::path        mechDir;
    fs::path        fullPath;
    vector<float>   slots[s_kcSampleFiles];

    if (devicesDir == nullptr || mechanism == nullptr || targetSampleRate == 0)
    {
        hr = E_INVALIDARG;
        goto Error;
    }

    baseDir = fs::path (devicesDir);
    mechDir = baseDir / mechanism;

    hr = MFStartup (MF_VERSION, MFSTARTUP_LITE);
    CHR (hr);
    mfStarted = true;

    for (i = 0; i < s_kcSampleFiles; i++)
    {
        // Per-file precedence (FR-019): explicit override at
        // Devices/DiskII/<file>.wav wins over the per-mechanism copy
        // at Devices/DiskII/<Mechanism>/<file>.wav. Both missing ==
        // silent, FR-009.
        fs::path        overridePath = baseDir / s_kpszSampleFiles[i];
        fs::path        mechPath     = mechDir / s_kpszSampleFiles[i];
        HRESULT         hrSlot       = E_FAIL;
        error_code      ec;

        if (fs::exists (overridePath, ec))
        {
            fullPath = overridePath;
        }
        else if (fs::exists (mechPath, ec))
        {
            fullPath = mechPath;
        }
        else
        {
            slots[i].clear();
            continue;
        }

        hrSlot = DecodeWavToMonoFloat (fullPath.wstring().c_str(),
                                       targetSampleRate, slots[i]);

        if (FAILED (hrSlot))
        {
            DEBUGMSG (
                L"Disk2AudioSource: failed to load %s (hr=0x%08X) -- sound muted.\n",
                fullPath.wstring().c_str(), hrSlot);
            slots[i].clear();
        }
    }

    m_motorBuf     = std::move (slots[s_kSlotMotorLoop]);
    m_stepBuf      = std::move (slots[s_kSlotHeadStep]);
    m_stopBuf      = std::move (slots[s_kSlotHeadStop]);
    m_doorOpenBuf  = std::move (slots[s_kSlotDoorOpen]);
    m_doorCloseBuf = std::move (slots[s_kSlotDoorClose]);

Error:
    if (mfStarted)
    {
        MFShutdown();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSampleBufferForTest
//
//  Test-only seam that avoids host filesystem reads (constitution
//  Principle II). Slot key matches the WAV filename without ".wav".
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AudioSource::SetSampleBufferForTest (
    const wchar_t *    slot,
    vector<float> &&   samples)
{
    if (slot == nullptr)
    {
        return;
    }

    if (wcscmp (slot, L"MotorLoop") == 0)
    {
        m_motorBuf = std::move (samples);
    }
    else if (wcscmp (slot, L"HeadStep") == 0)
    {
        m_stepBuf = std::move (samples);
    }
    else if (wcscmp (slot, L"HeadStop") == 0)
    {
        m_stopBuf = std::move (samples);
    }
    else if (wcscmp (slot, L"DoorOpen") == 0)
    {
        m_doorOpenBuf = std::move (samples);
    }
    else if (wcscmp (slot, L"DoorClose") == 0)
    {
        m_doorCloseBuf = std::move (samples);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPan
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AudioSource::SetPan (float panLeft, float panRight)
{
    m_panLeft  = panLeft;
    m_panRight = panRight;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMotorEngaged / OnMotorDisengaged
//
//  Spec-006: also fire OnAudioLoopStarted / OnAudioLoopStopped on the
//  attached IDriveAudioEventSink so the debug window can show what
//  the audio path decided. Empty m_motorBuf maps to AudioSilent with
//  BufferMissing per FR-009 / FR-025. An empty drive bay (no disk
//  mounted) maps to AudioSilent with NoDiskPresent -- real Disk II
//  hardware spins the motor either way, but the media-noise sample
//  is only audible while a disk is loaded.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AudioSource::OnMotorEngaged()
{
    m_motorRunning = true;

    if (m_audioEventSink != nullptr)
    {
        if (m_motorBuf.empty())
        {
            m_audioEventSink->OnAudioSilent (SoundKind::MotorLoop, m_driveIndex,
                                             SilentReason::BufferMissing);
        }
        else if (!m_diskPresent)
        {
            m_audioEventSink->OnAudioSilent (SoundKind::MotorLoop, m_driveIndex,
                                             SilentReason::NoDiskPresent);
        }
        else
        {
            m_audioEventSink->OnAudioLoopStarted (SoundKind::MotorLoop, m_driveIndex);
        }
    }
}


void Disk2AudioSource::OnMotorDisengaged()
{
    m_motorRunning = false;

    if (m_audioEventSink != nullptr)
    {
        m_audioEventSink->OnAudioLoopStopped (SoundKind::MotorLoop, m_driveIndex);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHeadStep
//
//  Step-vs-seek discrimination (FR-005): if the previous step fired
//  within kSeekThresholdCycles, enter seek mode and DO NOT restart the
//  head one-shot (let the current sample's tail decay). Else clear
//  seek mode and restart the step one-shot from sample 0.
//
//  m_lastStepCycle == 0 is reserved as "no prior step", so the very
//  first step after construction / reset always starts a fresh shot
//  even if the cycle counter is small.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AudioSource::OnHeadStep (int newQt)
{
    (void) newQt;

    bool      withinSeekWindow     = false;
    bool      previousStillPlaying = false;
    bool      wasInSeekMode        = m_seekMode;
    uint64_t  gap                  = 0;
    uint32_t  headLen              = 0;

    if (m_lastStepCycle != 0 && m_currentCycle >= m_lastStepCycle)
    {
        gap              = m_currentCycle - m_lastStepCycle;
        withinSeekWindow = (gap < kSeekThresholdCycles);
    }

    if (m_headBuf != nullptr)
    {
        headLen              = static_cast<uint32_t> (m_headBuf->size());
        previousStillPlaying = (headLen > 0 && m_headPos < headLen);
    }

    if (withinSeekWindow && previousStillPlaying)
    {
        // Tight seek burst AND the previous head one-shot has not
        // finished decaying. Hold its tail; do not restart -- that
        // preserves the FR-005 "no click-click-click" invariant.
        m_seekMode = true;
    }
    else
    {
        // Either a fresh single step (gap >= kSeekThresholdCycles)
        // OR a seek burst whose previous sample already ran out.
        // In both cases restart from sample 0 so the listener hears
        // a continuous buzz, not silence-with-occasional-clicks.
        m_seekMode = withinSeekWindow;
        m_headBuf  = &m_stepBuf;
        m_headPos  = 0;
    }

    m_lastStepCycle = m_currentCycle;

    // A real step means the head left the track-0 wall, so the bump
    // ratchet is no longer in progress -- re-arm it from the top.
    m_lastEventWasBump = false;
    m_ratchetSlot      = 0;

    // Spec-006 audio-decision sink (FR-022 / FR-025). Mapping:
    //   * wasInSeekMode == true at entry  -> AudioContinued
    //   * empty step buffer               -> AudioSilent / BufferMissing
    //   * previous shot still playing     -> AudioRestarted
    //   * else                            -> AudioStarted
    if (m_audioEventSink != nullptr)
    {
        if (wasInSeekMode)
        {
            m_audioEventSink->OnAudioContinued (SoundKind::HeadStep, m_driveIndex);
        }
        else if (m_stepBuf.empty())
        {
            m_audioEventSink->OnAudioSilent (SoundKind::HeadStep, m_driveIndex,
                                             SilentReason::BufferMissing);
        }
        else if (previousStillPlaying)
        {
            m_audioEventSink->OnAudioRestarted (SoundKind::HeadStep, m_driveIndex);
        }
        else
        {
            m_audioEventSink->OnAudioStarted (SoundKind::HeadStep, m_driveIndex);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHeadBump
//
//  Track-0 / max-track wall-bang. An ISOLATED bump is a firm thunk
//  (the HeadStop one-shot, restarted). A rapid run of consecutive bumps
//  -- as the controller emits while the head is pinned against the
//  track-0 stop during a boot recalibrate -- is instead rendered through
//  a 4-slot ratchet pattern [thunk, pause, click, click] so it sounds
//  like a slow machine gun rather than a continuous buzz. Clears seek
//  mode either way.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AudioSource::OnHeadBump()
{
    bool      previousStillPlaying = false;
    bool      ratchet              = false;
    uint64_t  gap                  = 0;
    uint32_t  slot                 = 0;
    uint32_t  headLen              = 0;

    if (m_headBuf != nullptr)
    {
        headLen              = static_cast<uint32_t> (m_headBuf->size());
        previousStillPlaying = (headLen > 0 && m_headPos < headLen);
    }

    if (m_lastEventWasBump && m_lastStepCycle != 0 && m_currentCycle >= m_lastStepCycle)
    {
        gap     = m_currentCycle - m_lastStepCycle;
        ratchet = (gap < kHeadIdleCycles);
    }

    m_seekMode         = false;
    m_lastEventWasBump = true;
    m_lastStepCycle    = m_currentCycle;

    if (!ratchet)
    {
        // Isolated wall-bang: firm thunk. Re-arm the ratchet so a
        // following rapid burst renders [silent, click, click, thunk...].
        m_ratchetSlot = kRatchetSlotSilent;
        TriggerHeadShot (SoundKind::HeadStop, &m_stopBuf, previousStillPlaying);
        return;
    }

    slot          = m_ratchetSlot;
    m_ratchetSlot = (m_ratchetSlot + 1) % kRatchetPeriod;

    if (slot == kRatchetSlotThunk)
    {
        TriggerHeadShot (SoundKind::HeadStop, &m_stopBuf, previousStillPlaying);
    }
    else if (slot == kRatchetSlotSilent)
    {
        // Rhythmic pause: hold the decaying tail and emit nothing. This
        // silent slot is what breaks a steady 52 Hz buzz into the grouped
        // "slow machine gun" cadence of a real recalibrate.
    }
    else
    {
        // The two remaining slots are step clicks -> 2:1 click-to-thunk.
        TriggerHeadShot (SoundKind::HeadStep, &m_stepBuf, previousStillPlaying);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  TriggerHeadShot
//
//  Starts a head one-shot on `buf` from sample 0 and fires the matching
//  audio-decision event. Shared by the isolated-bump, ratchet-thunk and
//  ratchet-click paths in OnHeadBump.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AudioSource::TriggerHeadShot (
    SoundKind             kind,
    const vector<float> * buf,
    bool                  previousStillPlaying)
{
    m_headBuf = buf;
    m_headPos = 0;

    if (m_audioEventSink != nullptr)
    {
        if (buf->empty())
        {
            m_audioEventSink->OnAudioSilent (kind, m_driveIndex,
                                             SilentReason::BufferMissing);
        }
        else if (previousStillPlaying)
        {
            m_audioEventSink->OnAudioRestarted (kind, m_driveIndex);
        }
        else
        {
            m_audioEventSink->OnAudioStarted (kind, m_driveIndex);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDiskInserted / OnDiskEjected
//
//  Spec-006 bug 14: also track m_diskPresent so MixMotor can gate the
//  media-noise sample on disk presence (an empty drive bay with motor
//  on is silent on real hardware). When the motor is already running
//  at the insert/eject moment, fire the matching loop transition on
//  the audio-event sink so the debug window can show the change:
//
//    insert + motor on -> OnAudioRestarted (MotorLoop) -- loop resumes
//    eject  + motor on -> OnAudioLoopStopped + OnAudioSilent
//                          (MotorLoop, NoDiskPresent)
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AudioSource::OnDiskInserted()
{
    bool  wasPresent = m_diskPresent;

    m_diskPresent = true;
    m_doorBuf     = &m_doorCloseBuf;
    m_doorPos     = 0;

    if (m_audioEventSink != nullptr)
    {
        if (!wasPresent && m_motorRunning && !m_motorBuf.empty())
        {
            m_audioEventSink->OnAudioRestarted (SoundKind::MotorLoop, m_driveIndex);
        }

        if (m_doorCloseBuf.empty())
        {
            m_audioEventSink->OnAudioSilent (SoundKind::DoorClose, m_driveIndex,
                                             SilentReason::BufferMissing);
        }
        else
        {
            m_audioEventSink->OnAudioStarted (SoundKind::DoorClose, m_driveIndex);
        }
    }
}


void Disk2AudioSource::OnDiskEjected()
{
    bool  wasPresent = m_diskPresent;

    m_diskPresent = false;
    m_doorBuf     = &m_doorOpenBuf;
    m_doorPos     = 0;
    m_motorPos    = 0;

    if (m_audioEventSink != nullptr)
    {
        if (wasPresent && m_motorRunning && !m_motorBuf.empty())
        {
            m_audioEventSink->OnAudioLoopStopped (SoundKind::MotorLoop, m_driveIndex);
            m_audioEventSink->OnAudioSilent      (SoundKind::MotorLoop, m_driveIndex,
                                                  SilentReason::NoDiskPresent);
        }

        if (m_doorOpenBuf.empty())
        {
            m_audioEventSink->OnAudioSilent (SoundKind::DoorOpen, m_driveIndex,
                                             SilentReason::BufferMissing);
        }
        else
        {
            m_audioEventSink->OnAudioStarted (SoundKind::DoorOpen, m_driveIndex);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
//  Auto-clear seek mode after kHeadIdleCycles of no step activity
//  (FR-005). Snapshots the current cycle so OnHeadStep / OnHeadBump
//  can timestamp themselves without an extra parameter.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AudioSource::Tick (uint64_t currentCycle)
{
    m_currentCycle = currentCycle;

    if (m_seekMode && m_lastStepCycle != 0 && currentCycle >= m_lastStepCycle)
    {
        if ((currentCycle - m_lastStepCycle) > kHeadIdleCycles)
        {
            m_seekMode = false;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MixMotor
//
//  Loop the motor buffer additively into `out` while m_motorRunning
//  AND a disk is mounted. Empty bay maps to silence per spec-006
//  bug 14a (real Disk II motor is audibly silent without media).
//  Wraps at the buffer end. Empty buffer == silent (FR-009).
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AudioSource::MixMotor (float * out, uint32_t n)
{
    uint32_t  len = static_cast<uint32_t> (m_motorBuf.size());
    uint32_t  i   = 0;

    if (!m_motorRunning || !m_diskPresent || len == 0)
    {
        return;
    }

    for (i = 0; i < n; i++)
    {
        if (m_motorPos >= len)
        {
            m_motorPos = 0;
        }

        out[i] += m_motorBuf[m_motorPos] * kMotorVolume;
        m_motorPos++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MixHead
//
//  One-shot head sample (step or bump). Stops once the buffer is
//  exhausted. Empty buffer == silent (FR-009).
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AudioSource::MixHead (float * out, uint32_t n)
{
    uint32_t  len = 0;
    uint32_t  i   = 0;

    if (m_headBuf == nullptr)
    {
        return;
    }

    len = static_cast<uint32_t> (m_headBuf->size());

    if (len == 0)
    {
        return;
    }

    for (i = 0; i < n; i++)
    {
        if (m_headPos >= len)
        {
            break;
        }

        out[i] += (*m_headBuf)[m_headPos] * kHeadVolume;
        m_headPos++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MixDoor
//
//  One-shot door open/close sample.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AudioSource::MixDoor (float * out, uint32_t n)
{
    uint32_t  len = 0;
    uint32_t  i   = 0;

    if (m_doorBuf == nullptr)
    {
        return;
    }

    len = static_cast<uint32_t> (m_doorBuf->size());

    if (len == 0)
    {
        return;
    }

    for (i = 0; i < n; i++)
    {
        if (m_doorPos >= len)
        {
            break;
        }

        out[i] += (*m_doorBuf)[m_doorPos] * kDoorVolume;
        m_doorPos++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GeneratePCM
//
////////////////////////////////////////////////////////////////////////////////

void Disk2AudioSource::GeneratePCM (float * outMono, uint32_t numSamples)
{
    if (outMono == nullptr || numSamples == 0)
    {
        return;
    }

    memset (outMono, 0, sizeof (float) * numSamples);

    MixMotor (outMono, numSamples);
    MixHead  (outMono, numSamples);
    MixDoor  (outMono, numSamples);
}
