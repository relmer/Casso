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
//  the machine runs at 2x / max (the visible head is capped at the real
//  ~250 cps / 25 in-per-sec, so the carriage loop plays at its natural rate --
//  no time-stretch, just gate on/off and loop).
//
//  Sounds are the pre-sliced CC BY 4.0 grains recorded from a real ImageWriter
//  II by Scott Lawrence (github.com/BleuLlama/ImageWriterIISimulator):
//    * a carriage-printing loop per print quality (draft / medium / NLQ),
//      looped while the head sweeps;
//    * line-feed clacks (three variants, rotated) fired at each new line;
//    * page-feed one-shots (short / medium / long) picked by feed distance for
//      Form Feed;
//    * paper-tear one-shots (five, chosen at random) for Discard.
//
//  Threading: LoadSounds / SetQuality / PublishReveal / PlayFormFeed /
//  PlayTearOff / SetVolume / SetMuted / SetPan run on the UI (or CPU) thread;
//  GeneratePCM runs on the audio mix thread. Reveal state, user-action triggers,
//  and pan cross the boundary through atomics; the grain buffers load once
//  before mixing. Empty grains == silent, so a missing asset never faults.
//
////////////////////////////////////////////////////////////////////////////////

class PrinterAudioSource : public IDriveAudioSource
{
public:
    // Carriage-loop print quality (which loop plays while the head sweeps).
    enum class Quality { Draft = 0, Medium = 1, NLQ = 2 };

    static constexpr float   kDefaultVolume      = 0.80f;

    // Carriage-loop hold after the last inked reveal advance (seconds). Long
    // enough to BRIDGE the brief ink=false gap of a line feed (carriage return +
    // one-line paper advance). The buzz gate is edge-triggered: ink under the
    // head arms it, ink ending releases it -- this hold is only the release
    // time, sized to bridge frame quantization (an ink=false wrap frame between
    // two inked lines) and nothing more. It must stay well under a carriage
    // pass: a line that is just a thin border strike at each margin has to fall
    // SILENT while the head crosses the blank middle (~0.3 s), or two columns of
    // dots sound like a full line of print. A form feed / tear is an explicit
    // action: its one-shot latch cuts this hold (GeneratePCM) so it feeds
    // silently under its own grain.
    static constexpr double  kPrintHoldSec       = 0.05;

    // Forced articulation gap at each completed pass (the reveal stepping into
    // the next pin band while the buzz is live). The real machine stops
    // printing for the line feed between passes, so each line must read as its
    // own burst -- a 19-line CATALOG is nineteen distinct print noises, not one
    // long zip (the hold above would otherwise bridge a text line's ~20 ms
    // blank feed and fuse consecutive lines). The buzz is cut for this long
    // and re-arms by itself when the next band is inked. Sized above a couple
    // of mix buffers and well under a pass (~40-90 ms for a logic-seeking
    // text line).
    static constexpr double  kLineFeedGapSec     = 0.035;

    // Minimum spacing between line-feed one-shots, so a burst of column wraps (a
    // fast catch-up) cannot machine-gun the clack.
    static constexpr double  kFeedMinIntervalSec = 0.09;

    // A revealed-column drop larger than half the pass's sweep width (never
    // less than this floor) counts as a new-line wrap, which fires a line-feed
    // clack. Scaled, not fixed: a logic-seeking pass over a short line is
    // narrower than any fixed full-width threshold.
    static constexpr int     kLineWrapDropFloor  = 50;

    static constexpr int     kNumLineFeeds       = 3;
    static constexpr int     kNumPageFeeds       = 3;   // short, medium, long
    static constexpr int     kNumTears           = 5;

    PrinterAudioSource ();
    ~PrinterAudioSource () override;

    // Decode the sound set from `dir` (absolute path to the "ImageWriter II
    // Sounds" folder) to mono float at `targetSampleRate` via Media Foundation.
    // A missing / unreadable file leaves that slot empty (silent). Returns the
    // MFStartup HRESULT; per-file decode failures do NOT propagate (best-effort,
    // matching Disk2AudioSource::LoadSamples).
    HRESULT  LoadSounds (const wchar_t * dir, uint32_t targetSampleRate);

    // Which carriage loop plays while the head sweeps.
    void  SetQuality (Quality quality);

    // Publish the paced on-screen reveal (UI thread). `progressDots` is a
    // monotonic revealed-dot counter (row * dotsPerRow + column); `colDots` is
    // the within-line sweep column. `inkActive` is true when the paced head is
    // actually laying ink (vs. merely feeding paper or traversing a blank
    // margin) -- the audio thread gates the carriage BUZZ on progress advancing
    // AND ink, so a form feed / blank line feed feeds silently under its own
    // one-shot instead of buzzing like a print. The line-feed clack still fires
    // on a column wrap regardless (that IS the feed sound).
    // `sweepWidthDots` is the current pass's span (logic seeking): the wrap
    // detector scales its column-drop threshold to it, so a short catalog
    // line's wrap still clacks even though its whole pass is narrower than a
    // fixed full-width threshold.
    void  PublishReveal (int64_t progressDots, int colDots, bool inkActive = true,
                         int sweepWidthDots = 1280);

    // User-action one-shots (UI thread; consumed on the audio thread). Form Feed
    // picks the page-feed grain by how much of the page will feed (`unusedPage01`
    // in 0..1 -> less == shorter); Discard picks a random paper-tear.
    void  PlayFormFeed (float unusedPage01);
    void  PlayTearOff  ();

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
    void    SetLoopForTest     (Quality quality, vector<float> && samples, uint32_t sampleRate);
    void    SetLineFeedForTest (int index, vector<float> && samples);
    void    SetPageFeedForTest (int index, vector<float> && samples);
    void    SetTearForTest     (int index, vector<float> && samples);

    bool    IsPrinting     () const { return m_printHoldSamples > 0; }
    bool    IsFeedPlaying  () const { return m_lineFeedBuf != nullptr; }
    bool    IsActionPlaying () const { return m_actionBuf != nullptr; }
    Quality CurrentQuality () const { return m_quality; }

private:
    void  MixCarriage (float * out, uint32_t n);
    void  MixLineFeed (float * out, uint32_t n);
    void  MixAction   (float * out, uint32_t n);

    uint32_t       m_sampleRate = 0;
    // Default matches the on-screen head: it is paced at real DRAFT speed
    // (~250 cps, single pass), so the draft loop reads in sync with the sweep.
    // NLQ / Medium stay wired for when the head model tracks the guest quality.
    Quality        m_quality    = Quality::Draft;

    vector<float>  m_loops[3];                    // Draft, Medium, NLQ carriage loops
    vector<float>  m_lineFeeds[kNumLineFeeds];
    vector<float>  m_pageFeeds[kNumPageFeeds];    // short, medium, long
    vector<float>  m_tears[kNumTears];

    uint32_t       m_carriagePos  = 0;

    const vector<float> *  m_lineFeedBuf  = nullptr;   // active line-feed one-shot
    uint32_t               m_lineFeedPos  = 0;
    int                    m_lineFeedNext = 0;          // rotation cursor

    const vector<float> *  m_actionBuf = nullptr;       // active form-feed / tear one-shot
    uint32_t               m_actionPos = 0;

    // Sync channel: UI thread writes, audio thread reads.
    std::atomic<int64_t>  m_revealProgress { 0 };
    std::atomic<int32_t>  m_revealCol      { 0 };
    std::atomic<int32_t>  m_revealInk      { 1 };      // 1 = head laying ink (buzz eligible)
    std::atomic<int32_t>  m_revealSweepW   { 1280 };   // pass span for wrap detection

    // User-action request (UI thread writes, audio thread exchanges to 0):
    // 0 = none, 1..3 = page-feed short/medium/long, 4..8 = tear 0..4.
    std::atomic<int32_t>  m_pendingAction  { 0 };

    // Audio-thread-local reveal tracking + gate timers (in samples).
    int64_t  m_lastProgress     = 0;
    int32_t  m_lastCol          = 0;
    int64_t  m_lastRows         = 0;
    int32_t  m_printHoldSamples = 0;
    int32_t  m_lineGapSamples   = 0;      // live pass-boundary articulation gap
    bool     m_pendingBuzz      = false;  // re-arm the buzz when the gap ends
    int32_t  m_feedThrottle     = 0;

    // Random cursor for the tear pick (UI thread only).
    uint32_t m_rng = 0x1234abcdu;

    float  m_volume = kDefaultVolume;
    bool   m_muted  = false;

    std::atomic<float>  m_panLeft  { IDriveAudioSource::kCenterPan };
    std::atomic<float>  m_panRight { IDriveAudioSource::kCenterPan };
};
