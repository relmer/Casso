#include "Pch.h"

#include "Audio/PrinterAudioSource.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "propsys.lib")




////////////////////////////////////////////////////////////////////////////////
//
//  DecodeToMonoFloat
//
//  Open `path` via IMFSourceReader, force float32 mono at `targetSampleRate`,
//  read every sample into `outSamples`. Media Foundation decodes MP3 the same
//  way it decodes the Disk II WAVs. Empty outSamples on any failure (caller
//  treats empty == silent).
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT DecodeToMonoFloat (
    const wchar_t *  path,
    uint32_t         targetSampleRate,
    vector<float> &  outSamples)
{
    HRESULT                  hr         = S_OK;
    ComPtr<IMFSourceReader>  reader;
    ComPtr<IMFMediaType>     pcmType;
    ComPtr<IMFSample>        sample;
    ComPtr<IMFMediaBuffer>   buffer;
    DWORD                    flags      = 0;
    DWORD                    bufLen     = 0;
    BYTE *                   bufPtr     = nullptr;
    const float *            srcFloat   = nullptr;
    DWORD                    floatCount = 0;
    DWORD                    i          = 0;

    outSamples.clear ();

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
        MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pcmType.Get ());
    CHR (hr);

    hr = reader->SetStreamSelection (MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
    CHR (hr);

    for (;;)
    {
        sample.Reset ();
        buffer.Reset ();
        flags = 0;

        hr = reader->ReadSample (
            MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0, nullptr, &flags, nullptr, &sample);
        CHR (hr);

        if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
        {
            break;
        }

        if (sample.Get () == nullptr)
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

        hr = buffer->Unlock ();
        CHR (hr);
    }

Error:
    if (FAILED (hr))
    {
        outSamples.clear ();
    }

    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterAudioSource
//
////////////////////////////////////////////////////////////////////////////////

PrinterAudioSource::PrinterAudioSource ()
{
}


PrinterAudioSource::~PrinterAudioSource ()
{
}




////////////////////////////////////////////////////////////////////////////////
//
//  LoadSound
//
//  Best-effort decode + slice. Returns the MFStartup HRESULT; a decode failure
//  leaves the grains empty (silent) but is not itself an error.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PrinterAudioSource::LoadSound (const wchar_t * path, uint32_t targetSampleRate)
{
    HRESULT        hr        = S_OK;
    bool           mfStarted = false;
    vector<float>  full;

    if (path == nullptr || targetSampleRate == 0)
    {
        return E_INVALIDARG;
    }

    hr = MFStartup (MF_VERSION, MFSTARTUP_LITE);
    CHR (hr);
    mfStarted = true;

    // Decode is best-effort: on failure `full` is empty and slicing yields no
    // grains, so the source is simply silent.
    if (FAILED (DecodeToMonoFloat (path, targetSampleRate, full)))
    {
        full.clear ();
    }

    SliceGrains (full, targetSampleRate);

Error:
    if (mfStarted)
    {
        MFShutdown ();
    }

    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SliceGrains
//
//  Copy the print-loop (sustained bidirectional line -> end) and line-feed
//  (short clack) sub-ranges out of the fully decoded buffer. Out-of-range
//  timestamps clamp; an empty / too-short buffer yields empty grains (silent).
//
////////////////////////////////////////////////////////////////////////////////

void PrinterAudioSource::SliceGrains (const vector<float> & full, uint32_t sampleRate)
{
    m_printLoop.clear ();
    m_feed.clear ();
    m_sampleRate = sampleRate;
    m_printPos   = 0;
    m_feedPos    = 0;
    m_feedActive = false;

    if (full.empty () || sampleRate == 0)
    {
        return;
    }

    size_t  total = full.size ();

    auto  idx = [&] (double sec) -> size_t
    {
        double  s = sec * (double) sampleRate;
        if (s < 0.0) { s = 0.0; }
        size_t  n = (size_t) (s + 0.5);
        return (n > total) ? total : n;
    };

    size_t  printBegin = idx (kPrintLoopBeginSec);
    if (printBegin < total)
    {
        m_printLoop.assign (full.begin () + printBegin, full.end ());
    }

    size_t  feedBegin = idx (kFeedBeginSec);
    size_t  feedEnd   = idx (kFeedEndSec);
    if (feedBegin < feedEnd)
    {
        m_feed.assign (full.begin () + feedBegin, full.begin () + feedEnd);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PublishReveal
//
////////////////////////////////////////////////////////////////////////////////

void PrinterAudioSource::PublishReveal (int64_t progressDots, int colDots)
{
    m_revealProgress.store (progressDots, std::memory_order_relaxed);
    m_revealCol.store      ((int32_t) colDots, std::memory_order_relaxed);
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetVolume / SetMuted / SetPan
//
////////////////////////////////////////////////////////////////////////////////

void PrinterAudioSource::SetVolume (float volume)
{
    m_volume = std::clamp (volume, 0.0f, 1.0f);
}


void PrinterAudioSource::SetMuted (bool muted)
{
    m_muted = muted;
}


void PrinterAudioSource::SetPan (float panLeft, float panRight)
{
    m_panLeft.store  (panLeft,  std::memory_order_relaxed);
    m_panRight.store (panRight, std::memory_order_relaxed);
}




////////////////////////////////////////////////////////////////////////////////
//
//  GeneratePCM
//
//  Clear, read the published reveal, gate the carriage loop on the head still
//  advancing, fire a line-feed clack on a column wrap, and mix. All timers are
//  in samples so playback is sample-rate independent.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterAudioSource::GeneratePCM (float * outMono, uint32_t numSamples)
{
    if (outMono == nullptr || numSamples == 0)
    {
        return;
    }

    memset (outMono, 0, sizeof (float) * numSamples);

    int64_t  progress = m_revealProgress.load (std::memory_order_relaxed);
    int32_t  col      = m_revealCol.load (std::memory_order_relaxed);

    // Head advanced this frame -> (re)arm the carriage loop for one hold window.
    if (progress > m_lastProgress)
    {
        m_printHoldSamples = (int32_t) (kPrintHoldSec * (double) m_sampleRate);
    }

    // Column wrapped back toward the left margin -> a new line began: clack.
    if (col + kLineWrapDropDots < m_lastCol &&
        m_feedThrottle <= 0 &&
        !m_feed.empty ())
    {
        m_feedActive  = true;
        m_feedPos     = 0;
        m_feedThrottle = (int32_t) (kFeedMinIntervalSec * (double) m_sampleRate);
    }

    if (!m_muted && m_volume > 0.0f)
    {
        MixPrintLoop (outMono, numSamples);
        MixFeed      (outMono, numSamples);
    }

    m_printHoldSamples = (std::max) (0, m_printHoldSamples - (int32_t) numSamples);
    m_feedThrottle     = (std::max) (0, m_feedThrottle     - (int32_t) numSamples);
    m_lastProgress     = progress;
    m_lastCol          = col;
}




////////////////////////////////////////////////////////////////////////////////
//
//  MixPrintLoop
//
//  Loop the sustained carriage grain while the head is still sweeping (the hold
//  timer is live). Empty grain == silent.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterAudioSource::MixPrintLoop (float * out, uint32_t n)
{
    uint32_t  len = (uint32_t) m_printLoop.size ();

    if (m_printHoldSamples <= 0 || len == 0)
    {
        return;
    }

    for (uint32_t i = 0; i < n; i++)
    {
        if (m_printPos >= len)
        {
            m_printPos = 0;
        }

        out[i] += m_printLoop[m_printPos] * m_volume;
        m_printPos++;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  MixFeed
//
//  One-shot line-feed clack. Stops once the grain is exhausted.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterAudioSource::MixFeed (float * out, uint32_t n)
{
    uint32_t  len = (uint32_t) m_feed.size ();

    if (!m_feedActive || len == 0)
    {
        return;
    }

    for (uint32_t i = 0; i < n; i++)
    {
        if (m_feedPos >= len)
        {
            m_feedActive = false;
            break;
        }

        out[i] += m_feed[m_feedPos] * m_volume;
        m_feedPos++;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetGrainsForTest / SetFullBufferForTest
//
//  Test seams that avoid a Media Foundation decode (constitution Principle II).
//
////////////////////////////////////////////////////////////////////////////////

void PrinterAudioSource::SetGrainsForTest (
    vector<float> &&  printLoop,
    vector<float> &&  feed,
    uint32_t          sampleRate)
{
    m_printLoop  = std::move (printLoop);
    m_feed       = std::move (feed);
    m_sampleRate = sampleRate;
    m_printPos   = 0;
    m_feedPos    = 0;
    m_feedActive = false;
}


void PrinterAudioSource::SetFullBufferForTest (vector<float> && full, uint32_t sampleRate)
{
    vector<float>  local = std::move (full);
    SliceGrains (local, sampleRate);
}
