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
//  LoadSounds
//
//  Best-effort decode of the whole BleuLlama grain set from `dir`. Returns the
//  MFStartup HRESULT; a per-file decode failure leaves that slot empty (silent)
//  but does not itself fail the load (matches Disk2AudioSource::LoadSamples).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PrinterAudioSource::LoadSounds (const wchar_t * dir, uint32_t targetSampleRate)
{
    HRESULT  hr        = S_OK;
    bool     mfStarted = false;

    if (dir == nullptr || targetSampleRate == 0)
    {
        return E_INVALIDARG;
    }

    // Decode is best-effort per file: on failure the slot stays empty and that
    // sound is simply silent, so a partial asset set never faults playback.
    // Declared before the first CHR so its goto cannot skip the initialization.
    auto  decode = [&] (const wchar_t * name, vector<float> & dst)
    {
        wstring  path = wstring (dir) + L"\\" + name;
        if (FAILED (DecodeToMonoFloat (path.c_str (), targetSampleRate, dst)))
        {
            dst.clear ();
        }
    };

    hr = MFStartup (MF_VERSION, MFSTARTUP_LITE);
    CHR (hr);
    mfStarted = true;

    m_sampleRate = targetSampleRate;

    decode (L"print_draft_loop.mp3",  m_loops[(int) Quality::Draft]);
    decode (L"print_medium_loop.mp3", m_loops[(int) Quality::Medium]);
    decode (L"print_nlq_loop.mp3",    m_loops[(int) Quality::NLQ]);

    decode (L"line_feed_01.mp3", m_lineFeeds[0]);
    decode (L"line_feed_02.mp3", m_lineFeeds[1]);
    decode (L"line_feed_03.mp3", m_lineFeeds[2]);

    decode (L"page_feed_short.mp3",  m_pageFeeds[0]);
    decode (L"page_feed_medium.mp3", m_pageFeeds[1]);
    decode (L"page_feed_long.mp3",   m_pageFeeds[2]);

    decode (L"paper_tear_01.mp3", m_tears[0]);
    decode (L"paper_tear_02.mp3", m_tears[1]);
    decode (L"paper_tear_03.mp3", m_tears[2]);
    decode (L"paper_tear_04.mp3", m_tears[3]);
    decode (L"paper_tear_05.mp3", m_tears[4]);

Error:
    if (mfStarted)
    {
        MFShutdown ();
    }

    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetQuality
//
////////////////////////////////////////////////////////////////////////////////

void PrinterAudioSource::SetQuality (Quality quality)
{
    m_quality = quality;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PublishReveal
//
////////////////////////////////////////////////////////////////////////////////

void PrinterAudioSource::PublishReveal (int64_t progressDots, int colDots, bool inkActive)
{
    m_revealProgress.store (progressDots, std::memory_order_relaxed);
    m_revealCol.store      ((int32_t) colDots, std::memory_order_relaxed);
    m_revealInk.store      (inkActive ? 1 : 0, std::memory_order_relaxed);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PlayFormFeed / PlayTearOff
//
//  Fire a user-action one-shot. The grain is chosen here (UI thread) and the
//  choice is handed to the audio thread through m_pendingAction so the mix side
//  needs no RNG and no page-fraction math. Encoding: 1..3 = page-feed
//  short/medium/long, 4..8 = tear 0..4.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterAudioSource::PlayFormFeed (float unusedPage01)
{
    float  u = std::clamp (unusedPage01, 0.0f, 1.0f);

    // Less of the page unused -> a shorter feed to the tear bar -> shorter grain.
    int  slot = (u < (1.0f / 3.0f)) ? 0 : (u < (2.0f / 3.0f)) ? 1 : 2;

    m_pendingAction.store (1 + slot, std::memory_order_release);
}


void PrinterAudioSource::PlayTearOff ()
{
    // Cheap LCG; UI-thread only, so no synchronization needed on m_rng.
    m_rng = m_rng * 1664525u + 1013904223u;
    int  pick = (int) ((m_rng >> 24) % (uint32_t) kNumTears);

    m_pendingAction.store (1 + kNumPageFeeds + pick, std::memory_order_release);
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
//  advancing, fire a line-feed clack on a column wrap, latch any pending user
//  action, and mix the three channels. All timers are in samples so playback is
//  sample-rate independent.
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
    bool     ink      = m_revealInk.load (std::memory_order_relaxed) != 0;

    // Head advanced this frame AND is laying ink -> (re)arm the carriage buzz for
    // one hold window. Gating on ink (not just motion) keeps a form feed / blank
    // line feed / wide blank margin from buzzing like a print: those advance the
    // reveal but publish ink=false, so the hold decays and the buzz falls silent
    // while the one-shot feed grains carry the sound.
    if (progress > m_lastProgress && ink)
    {
        m_printHoldSamples = (int32_t) (kPrintHoldSec * (double) m_sampleRate);
    }

    // Column wrapped back toward the left margin -> a new line began: clack. Each
    // line rotates through the three recorded feed variants so repeats do not
    // sound machine-stamped; empty variants are skipped.
    if (col + kLineWrapDropDots < m_lastCol && m_feedThrottle <= 0)
    {
        for (int tries = 0; tries < kNumLineFeeds; tries++)
        {
            const vector<float> &  cand = m_lineFeeds[m_lineFeedNext];
            m_lineFeedNext = (m_lineFeedNext + 1) % kNumLineFeeds;
            if (!cand.empty ())
            {
                m_lineFeedBuf  = &cand;
                m_lineFeedPos  = 0;
                m_feedThrottle = (int32_t) (kFeedMinIntervalSec * (double) m_sampleRate);
                break;
            }
        }
    }

    // Latch a pending user action (form feed / tear). Newest wins if two land in
    // one window -- discard-then-tear reads naturally as the later sound.
    int32_t  action = m_pendingAction.exchange (0, std::memory_order_acquire);
    if (action >= 1 && action <= kNumPageFeeds)
    {
        const vector<float> &  buf = m_pageFeeds[action - 1];
        if (!buf.empty ())
        {
            m_actionBuf = &buf;
            m_actionPos = 0;
        }
    }
    else if (action >= 1 + kNumPageFeeds && action <= kNumPageFeeds + kNumTears)
    {
        const vector<float> &  buf = m_tears[action - 1 - kNumPageFeeds];
        if (!buf.empty ())
        {
            m_actionBuf = &buf;
            m_actionPos = 0;
        }
    }

    if (!m_muted && m_volume > 0.0f)
    {
        MixCarriage (outMono, numSamples);
        MixLineFeed (outMono, numSamples);
        MixAction   (outMono, numSamples);
    }

    m_printHoldSamples = (std::max) (0, m_printHoldSamples - (int32_t) numSamples);
    m_feedThrottle     = (std::max) (0, m_feedThrottle     - (int32_t) numSamples);
    m_lastProgress     = progress;
    m_lastCol          = col;
}




////////////////////////////////////////////////////////////////////////////////
//
//  MixCarriage
//
//  Loop the carriage grain for the selected quality while the head is still
//  sweeping (the hold timer is live). Empty grain == silent.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterAudioSource::MixCarriage (float * out, uint32_t n)
{
    const vector<float> &  loop = m_loops[(int) m_quality];
    uint32_t               len  = (uint32_t) loop.size ();

    if (m_printHoldSamples <= 0 || len == 0)
    {
        return;
    }

    for (uint32_t i = 0; i < n; i++)
    {
        if (m_carriagePos >= len)
        {
            m_carriagePos = 0;
        }

        out[i] += loop[m_carriagePos] * m_volume;
        m_carriagePos++;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  MixLineFeed
//
//  One-shot line-feed clack. Stops once the grain is exhausted.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterAudioSource::MixLineFeed (float * out, uint32_t n)
{
    if (m_lineFeedBuf == nullptr)
    {
        return;
    }

    uint32_t  len = (uint32_t) m_lineFeedBuf->size ();

    for (uint32_t i = 0; i < n; i++)
    {
        if (m_lineFeedPos >= len)
        {
            m_lineFeedBuf = nullptr;
            break;
        }

        out[i] += (*m_lineFeedBuf)[m_lineFeedPos] * m_volume;
        m_lineFeedPos++;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  MixAction
//
//  One-shot form-feed / paper-tear. Stops once the grain is exhausted.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterAudioSource::MixAction (float * out, uint32_t n)
{
    if (m_actionBuf == nullptr)
    {
        return;
    }

    uint32_t  len = (uint32_t) m_actionBuf->size ();

    for (uint32_t i = 0; i < n; i++)
    {
        if (m_actionPos >= len)
        {
            m_actionBuf = nullptr;
            break;
        }

        out[i] += (*m_actionBuf)[m_actionPos] * m_volume;
        m_actionPos++;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetLoopForTest / SetLineFeedForTest / SetPageFeedForTest / SetTearForTest
//
//  Test seams that avoid a Media Foundation decode (constitution Principle II).
//
////////////////////////////////////////////////////////////////////////////////

void PrinterAudioSource::SetLoopForTest (
    Quality           quality,
    vector<float> &&  samples,
    uint32_t          sampleRate)
{
    m_loops[(int) quality] = std::move (samples);
    m_sampleRate           = sampleRate;
    m_carriagePos          = 0;
}


void PrinterAudioSource::SetLineFeedForTest (int index, vector<float> && samples)
{
    if (index >= 0 && index < kNumLineFeeds)
    {
        m_lineFeeds[index] = std::move (samples);
    }
}


void PrinterAudioSource::SetPageFeedForTest (int index, vector<float> && samples)
{
    if (index >= 0 && index < kNumPageFeeds)
    {
        m_pageFeeds[index] = std::move (samples);
    }
}


void PrinterAudioSource::SetTearForTest (int index, vector<float> && samples)
{
    if (index >= 0 && index < kNumTears)
    {
        m_tears[index] = std::move (samples);
    }
}
