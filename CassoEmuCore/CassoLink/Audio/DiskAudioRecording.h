#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAudioRecording
//
//  A 140K disk as one recording. Sent from any player into a ][+ or //e's
//  cassette input, it writes the disk in drive 1 with no PC answering
//  (FR-207). It is the cassette link's audio laid out ahead of time:
//
//    1. The cassette build of the program as a monitor tape record, which
//       the monitor command outMonitorCommand reads and runs.
//    2. The 13-sector module, loaded a chunk at a time, then its check.
//    3. Each track's nibbles, by the module's Write nibble track: the
//       request, then the stream.
//
//  The Apple's replies go nowhere, so each request is preceded by a tone
//  long enough to cover the Apple's reply to the last one and its Ready,
//  and each track's stream by one long enough to cover the Apple's seek and
//  its Acknowledge; the first track's also covers the motor's start and the
//  arm's trip to track 0. Nothing is read back, as nothing could be sent
//  again.
//
////////////////////////////////////////////////////////////////////////////////

class DiskAudioRecording
{
public:
    static constexpr Byte    kDrive             = 1;
    static constexpr double  kAfterProgramMs    = 2000.0;
    static constexpr double  kRequestToneMs     = 1200.0;
    static constexpr double  kStreamToneMs      = 1200.0;
    static constexpr double  kFirstStreamToneMs = 3500.0;
    static constexpr double  kAfterTrackMs      = 800.0;
    static constexpr double  kAfterRequestMs    = 300.0;
    static constexpr double  kTailMs            = 3000.0;

    //  `sectors` is a 140K DOS-order buffer or a .d13 buffer, as
    //  VolumeImage::Load returns them. ERROR_BAD_LENGTH for any other size.
    static HRESULT  Build (const std::vector<Byte> & sectors, uint32_t sampleRate, std::vector<float> & outSamples, std::string & outMonitorCommand);

private:
    static void  AppendBurst   (std::span<const Byte> bytes, double toneMs, uint32_t sampleRate, std::vector<float> & inOutSamples);
    static void  AppendSilence (double ms, uint32_t sampleRate, std::vector<float> & inOutSamples);
};
