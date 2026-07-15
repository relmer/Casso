#pragma once

#include "Pch.h"
#include "Audio/IDriveAudioSource.h"




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterAudioSource
//
//  Mechanical-sound source for the emulated ImageWriter II, mixed through the
//  same DriveAudioMixer bus as the Disk II drives. The sound is Option A
//  (research T038): the audio follows the PACED on-screen carriage rather than
//  the raw guest byte stream, so what you hear matches what you see even when
//  the machine runs at 2x / max (the visible head is capped at the real 250 cps
//  / 25 in-per-sec, so the carriage grain plays at its natural pitch -- no
//  time-stretch, just gate on/off and loop).
//
//  Grains are sliced from a single CC0 recording of a real ImageWriter II
//  self-test (freesound.org/s/662714 by jewettg, CC0): a sustained
//  bidirectional-printing segment loops while the carriage sweeps, and a short
//  segment fires as a one-shot line-feed clack at each new line.
//
//  Threading: PublishReveal / SetVolume / SetMuted / SetPan run on the UI (or
//  CPU) thread; GeneratePCM runs on the audio mix thread. The reveal state and
//  pan cross the boundary through atomics; the grain buffers are loaded once
//  before mixing starts and then only read. Empty grains == silent, so a
//  missing / unreadable asset never faults (matching the Disk II loader).
//
////////////////////////////////////////////////////////////////////////////////

class PrinterAudioSource : public IDriveAudioSource
{
public:
    // Grain boundaries (seconds) into the self-test recording. The sustained
    // bidirectional-line segment is the carriage-printing loop; the short
    // segment is the line-feed clack.
    static constexpr double  kPrintLoopBeginSec  = 8.4;   // -> end of file
    static constexpr double  kFeedBeginSec       = 3.7;
    static constexpr double  kFeedEndSec         = 3.9;

    static constexpr float   kDefaultVolume      = 0.80f;

    // Carriage-loop hold after the last reveal advance (seconds). Bridges the
    // UI publish / audio consume rate mismatch so the loop does not gate on and
    // off between reveal updates; a longer silence than this = head stopped.
    static constexpr double  kPrintHoldSec       = 0.05;

    // Minimum spacing between line-feed one-shots, so a burst of column wraps (a
    // fast catch-up) cannot machine-gun the clack.
    static constexpr double  kFeedMinIntervalSec = 0.09;

    // A revealed-column drop larger than this counts as a new-line wrap (the
    // carriage returning to the left margin), which fires a line-feed clack.
    static constexpr int     kLineWrapDropDots   = 400;

    PrinterAudioSource ();
    ~PrinterAudioSource () override;

    // Decode `path` (any Media Foundation format, incl. MP3) to mono float at
    // `targetSampleRate`, then slice the print-loop + line-feed grains. A
    // missing / unreadable file leaves the grains empty -> silent. Returns the
    // MFStartup result; per-file decode failure does NOT propagate (best-effort,
    // matching Disk2AudioSource::LoadSamples).
    HRESULT  LoadSound (const wchar_t * path, uint32_t targetSampleRate);

    // Publish the paced on-screen reveal (UI thread). `progressDots` is a
    // monotonic revealed-dot counter (row * dotsPerRow + column); `colDots` is
    // the within-line sweep column. The audio thread gates the carriage loop on
    // progress advancing and fires a line-feed clack when the column wraps back
    // toward the left margin.
    void  PublishReveal (int64_t progressDots, int colDots);

    // Master printer-audio gain (0..1) and mute (UI / CPU thread).
    void  SetVolume (float volume);
    void  SetMuted  (bool muted);

    // IDriveAudioSource:
    void   GeneratePCM (float * outMono, uint32_t numSamples) override;
    float  PanLeft  () const override { return m_panLeft.load  (std::memory_order_relaxed); }
    float  PanRight () const override { return m_panRight.load (std::memory_order_relaxed); }
    void   SetPan   (float panLeft, float panRight) override;

    // IDriveAudioSink -- the printer fires none of these disk events (it is a
    // non-drive source on a generic bus, FR-016); they are inert.
    void   OnMotorEngaged    () override {}
    void   OnMotorDisengaged () override {}
    void   OnHeadStep        (int) override {}
    void   OnHeadBump        () override {}
    void   OnDiskInserted    () override {}
    void   OnDiskEjected     () override {}

    // Test seams / introspection (no Media Foundation in unit tests).
    void    SetGrainsForTest     (vector<float> && printLoop, vector<float> && feed, uint32_t sampleRate);
    void    SetFullBufferForTest (vector<float> && full, uint32_t sampleRate);
    size_t  PrintLoopSamples () const { return m_printLoop.size (); }
    size_t  FeedSamples      () const { return m_feed.size (); }
    bool    IsPrinting       () const { return m_printHoldSamples > 0; }
    bool    IsFeedPlaying    () const { return m_feedActive; }

private:
    void  SliceGrains  (const vector<float> & full, uint32_t sampleRate);
    void  MixPrintLoop (float * out, uint32_t n);
    void  MixFeed      (float * out, uint32_t n);

    uint32_t       m_sampleRate = 0;

    vector<float>  m_printLoop;     // looped while the carriage is sweeping
    vector<float>  m_feed;          // one-shot at each line boundary
    uint32_t       m_printPos   = 0;
    uint32_t       m_feedPos    = 0;
    bool           m_feedActive = false;

    // Sync channel: UI thread writes, audio thread reads.
    std::atomic<int64_t>  m_revealProgress { 0 };
    std::atomic<int32_t>  m_revealCol      { 0 };

    // Audio-thread-local reveal tracking + gate timers (in samples).
    int64_t  m_lastProgress     = 0;
    int32_t  m_lastCol          = 0;
    int32_t  m_printHoldSamples = 0;
    int32_t  m_feedThrottle     = 0;

    float  m_volume = kDefaultVolume;
    bool   m_muted  = false;

    std::atomic<float>  m_panLeft  { IDriveAudioSource::kCenterPan };
    std::atomic<float>  m_panRight { IDriveAudioSource::kCenterPan };
};
