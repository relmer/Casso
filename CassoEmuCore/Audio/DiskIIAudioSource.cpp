#include "Pch.h"

#include "Audio/DiskIIAudioSource.h"

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
//  DiskIIAudioSource
//
////////////////////////////////////////////////////////////////////////////////

DiskIIAudioSource::DiskIIAudioSource()
{
}


DiskIIAudioSource::~DiskIIAudioSource()
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

HRESULT DiskIIAudioSource::LoadSamples (
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
            slots[i].clear ();
            continue;
        }

        hrSlot = DecodeWavToMonoFloat (fullPath.wstring ().c_str (),
                                       targetSampleRate, slots[i]);

        if (FAILED (hrSlot))
        {
            DEBUGMSG (
                L"DiskIIAudioSource: failed to load %s (hr=0x%08X) -- sound muted.\n",
                fullPath.wstring ().c_str (), hrSlot);
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

void DiskIIAudioSource::SetSampleBufferForTest (
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

void DiskIIAudioSource::SetPan (float panLeft, float panRight)
{
    m_panLeft  = panLeft;
    m_panRight = panRight;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMotorStart / OnMotorStop
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIAudioSource::OnMotorStart()
{
    m_motorRunning = true;
}


void DiskIIAudioSource::OnMotorStop()
{
    m_motorRunning = false;
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

void DiskIIAudioSource::OnHeadStep (int newQt)
{
    (void) newQt;

    bool      withinSeekWindow = false;
    bool      previousStillPlaying = false;
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHeadBump
//
//  Track-0 / max-track wall-bang. Always restarts the stop one-shot
//  (a bump is a discrete event, never collapsed into the seek-burst
//  state). Clears seek mode so a subsequent step starts fresh.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIAudioSource::OnHeadBump()
{
    m_seekMode      = false;
    m_headBuf       = &m_stopBuf;
    m_headPos       = 0;
    m_lastStepCycle = m_currentCycle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDiskInserted / OnDiskEjected
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIAudioSource::OnDiskInserted()
{
    m_doorBuf = &m_doorCloseBuf;
    m_doorPos = 0;
}


void DiskIIAudioSource::OnDiskEjected()
{
    m_doorBuf = &m_doorOpenBuf;
    m_doorPos = 0;
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

void DiskIIAudioSource::Tick (uint64_t currentCycle)
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
//  Loop the motor buffer additively into `out` while m_motorRunning.
//  Wraps at the buffer end. Empty buffer == silent (FR-009).
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIAudioSource::MixMotor (float * out, uint32_t n)
{
    uint32_t  len = static_cast<uint32_t> (m_motorBuf.size());
    uint32_t  i   = 0;

    if (!m_motorRunning || len == 0)
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

void DiskIIAudioSource::MixHead (float * out, uint32_t n)
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

void DiskIIAudioSource::MixDoor (float * out, uint32_t n)
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

void DiskIIAudioSource::GeneratePCM (float * outMono, uint32_t numSamples)
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
