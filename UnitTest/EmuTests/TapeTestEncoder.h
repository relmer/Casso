#pragma once

#include "Devices/Tape/TapeAudio.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeEncodeOptions
//
//  How a synthetic test tape is rendered and stored. speed above 1 plays the
//  tape fast (every half-cycle shorter); noiseSnrDb of 0 means no noise.
//
////////////////////////////////////////////////////////////////////////////////

struct TapeEncodeOptions
{
    uint32_t  sampleRate    = 44100;
    double    leaderSeconds = 5.0;
    double    tailSeconds   = 0.5;
    double    speed         = 1.0;
    double    amplitude     = 0.8;
    double    gainEnd       = 1.0;      // gain multiplier reached by the end of the tape
    double    dcOffset      = 0.0;
    double    noiseSnrDb    = 0.0;
    bool      isInverted    = false;
    bool      isSquare      = false;
    Word      channels      = 1;
    Word      bitsPerSample = 16;
    bool      isFloat       = false;
    uint32_t  noiseSeed     = 0xCA55E77E;
    size_t    weakHalfEvery = 0;        // every this many half-cycles is weak; 0 for none
    double    weakHalfGain  = 1.0;      // and is this fraction of the others' height
};





////////////////////////////////////////////////////////////////////////////////
//
//  TapeTestEncoder
//
//  Test-only generator of the Apple II cassette signal, so load tests can feed
//  known bytes through the real ROM. A record is a 770 Hz leader, a short sync
//  half-cycle pair, the data bytes most significant bit first, and an XOR
//  checksum seeded with $FF. A 0 bit is one 2 kHz cycle and a 1 bit one 1 kHz
//  cycle. Written for this project with Egan Ford's c2t
//  (https://github.com/datajerk/c2t, BSD-3-Clause) as the timing reference.
//
//  This knowledge of the byte format is allowed here and nowhere in the
//  emulator: the emulator's load path only ever sees transitions.
//
////////////////////////////////////////////////////////////////////////////////

class TapeTestEncoder
{
public:
    static constexpr double  kLeaderHalfUs = 650.0;
    static constexpr double  kSyncFirstUs  = 200.0;
    static constexpr double  kSyncSecondUs = 250.0;
    static constexpr double  kZeroHalfUs   = 250.0;
    static constexpr double  kOneHalfUs    = 500.0;
    static constexpr Byte    kChecksumSeed = 0xFF;

    static void  AppendRecord (std::vector<double> & halfCyclesUs, std::span<const Byte> data, const TapeEncodeOptions & options);
    static void  Render       (std::span<const double> halfCyclesUs, const TapeEncodeOptions & options, TapeAudio & audio);
    static void  EncodeWav    (const TapeAudio & audio, const TapeEncodeOptions & options, std::vector<Byte> & bytes);
    static void  EncodeAiff   (const TapeAudio & audio, const TapeEncodeOptions & options, std::vector<Byte> & bytes);

    static void  MakeWav      (const std::vector<std::vector<Byte>> & records, const TapeEncodeOptions & options, std::vector<Byte> & bytes);

private:
    static void  AppendByte   (std::vector<double> & halfCyclesUs, Byte value);
    static void  AddNoise     (TapeAudio & audio, const TapeEncodeOptions & options);
    static void  WriteSample  (std::vector<Byte> & bytes, double sample, const TapeEncodeOptions & options, bool isBigEndian);
};
