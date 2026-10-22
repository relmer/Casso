#pragma once

#include "Pch.h"

#include "CassoLink/LinkProgram.h"
#include "Devices/Tape/TapeAudio.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkAudioBootstrap
//
//  Starts the link on a ][+ or //e with no copy of it, through the cassette
//  input (FR-092, US15/AC2). At the BASIC prompt, the user types CALL -151
//  for the monitor, then the monitor command this builds, such as
//  3D00.3FFFR 3D00G, and Return; the monitor reads the shim from the tape
//  input and runs it. The audio is the shim as one cassette record: a 770 Hz
//  leader, the sync half-cycles, the bytes most significant bit first as one
//  2 kHz cycle for a 0 and one 1 kHz cycle for a 1, and the XOR checksum
//  seeded with $FF, as Apple's monitor writes and reads a tape.
//
//  The leader is long enough to cover the monitor's wait of about 3.5
//  seconds after the first edge it reads.
//
////////////////////////////////////////////////////////////////////////////////

class LinkAudioBootstrap
{
public:
    static constexpr uint32_t  kSampleRate    = 44100;
    static constexpr double    kLeaderSeconds = 6.0;
    static constexpr double    kTailSeconds   = 0.5;
    static constexpr double    kAmplitude     = 0.8;

    static constexpr double    kLeaderHalfUs  = 650.0;
    static constexpr double    kSyncFirstUs   = 200.0;
    static constexpr double    kSyncSecondUs  = 250.0;
    static constexpr double    kZeroHalfUs    = 250.0;
    static constexpr double    kOneHalfUs     = 500.0;
    static constexpr Byte      kChecksumSeed  = 0xFF;

    //  The audio for the shim the options build, and the monitor command
    //  that reads it and runs it, ending in a carriage return.
    static HRESULT  BuildAudio (const LinkShimOptions & options, TapeAudio & outAudio, std::string & outCommand);

    //  The same audio as a 16-bit mono WAV file's bytes.
    static HRESULT  BuildWav   (const LinkShimOptions & options, std::vector<Byte> & outWav, std::string & outCommand);

    //  The half-cycles of one record, in microseconds.
    static void     AppendRecord (std::span<const Byte> data, std::vector<double> & halfCyclesUs);

    //  Each half-cycle as a half sine of alternating sign, after a short
    //  silence and followed by one.
    static void     Render       (std::span<const double> halfCyclesUs, TapeAudio & outAudio);

private:
    static void     AppendByte   (Byte value, std::vector<double> & halfCyclesUs);
};
