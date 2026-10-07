#include "Pch.h"

#include "TapeTestEncoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeTestEncoder::AppendRecord
//
//  Leader, sync, data, checksum: the half-cycle durations, in microseconds at
//  nominal speed, that a cassette WRITE produces for one record.
//
////////////////////////////////////////////////////////////////////////////////

void TapeTestEncoder::AppendRecord (std::vector<double> & halfCyclesUs, std::span<const Byte> data, const TapeEncodeOptions & options)
{
    constexpr double  kUsPerSecond = 1000000.0;
    size_t            leaderHalves = (size_t) (options.leaderSeconds * kUsPerSecond / kLeaderHalfUs);
    Byte              checksum     = kChecksumSeed;



    halfCyclesUs.insert (halfCyclesUs.end(), leaderHalves, kLeaderHalfUs);
    halfCyclesUs.push_back (kSyncFirstUs);
    halfCyclesUs.push_back (kSyncSecondUs);

    for (Byte value : data)
    {
        AppendByte (halfCyclesUs, value);
        checksum ^= value;
    }

    AppendByte (halfCyclesUs, checksum);

    // One closing half-cycle, so the checksum's last edge is followed by
    // another edge rather than by silence.
    halfCyclesUs.push_back (kOneHalfUs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeTestEncoder::AppendByte
//
////////////////////////////////////////////////////////////////////////////////

void TapeTestEncoder::AppendByte (std::vector<double> & halfCyclesUs, Byte value)
{
    constexpr int  kBitsPerByte = 8;
    constexpr Byte kHighBit     = 0x80;



    for (int bit = 0; bit < kBitsPerByte; bit++)
    {
        double  half = (value & (kHighBit >> bit)) != 0 ? kOneHalfUs : kZeroHalfUs;

        halfCyclesUs.push_back (half);
        halfCyclesUs.push_back (half);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeTestEncoder::Render
//
//  Each half-cycle is a half sine (or a flat half of a square wave) of
//  alternating sign. The gain ramps linearly across the tape, the DC offset
//  is added everywhere, and noise is added last.
//
////////////////////////////////////////////////////////////////////////////////

void TapeTestEncoder::Render (std::span<const double> halfCyclesUs, const TapeEncodeOptions & options, TapeAudio & audio)
{
    constexpr double  kUsPerSecond = 1000000.0;
    double            totalUs      = options.tailSeconds * kUsPerSecond * 2.0;
    double            startUs      = options.tailSeconds * kUsPerSecond;
    double            sign         = options.isInverted ? -1.0 : 1.0;
    size_t            total        = 0;
    size_t            index        = 0;
    constexpr double  kClickDepth  = 0.6;



    for (double half : halfCyclesUs)
    {
        totalUs += half / options.speed;
    }

    total = (size_t) (totalUs * options.sampleRate / kUsPerSecond);
    audio.sampleRate = options.sampleRate;
    audio.samples.assign (total, (float) options.dcOffset);

    for (double half : halfCyclesUs)
    {
        double  lengthUs = half / options.speed;
        size_t  first    = (size_t) ceil (startUs * options.sampleRate / kUsPerSecond);
        size_t  last     = (size_t) ceil ((startUs + lengthUs) * options.sampleRate / kUsPerSecond);
        bool    isWeak   = options.weakHalfEvery != 0 && (index + 1) % options.weakHalfEvery == 0;
        bool    isClick  = options.clickEvery    != 0 && (index + 1) % options.clickEvery    == 0;
        double  height   = options.amplitude * (isWeak ? options.weakHalfGain : 1.0);



        for (size_t n = first; n < last && n < total; n++)
        {
            double  t     = ((double) n * kUsPerSecond / options.sampleRate - startUs) / lengthUs;
            double  shape = options.isSquare ? 1.0 : sin (std::numbers::pi * t);
            double  gain  = 1.0 + (options.gainEnd - 1.0) * (double) n / (double) total;

            audio.samples[n] = (float) (sign * shape * height * gain + options.dcOffset);
        }

        // A click: one sample at the half-cycle's peak thrown to the other
        // side of zero.
        if (isClick && (first + last) / 2 < total)
        {
            audio.samples[(first + last) / 2] = (float) (-sign * options.amplitude * kClickDepth + options.dcOffset);
        }

        index++;
        startUs += lengthUs;
        sign     = -sign;
    }

    AddNoise (audio, options);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeTestEncoder::AddNoise
//
//  Gaussian noise at the requested signal-to-noise ratio, measured against a
//  sine at the nominal amplitude. Seeded, so every run is identical.
//
////////////////////////////////////////////////////////////////////////////////

void TapeTestEncoder::AddNoise (TapeAudio & audio, const TapeEncodeOptions & options)
{
    constexpr double                  kDbPerDecade = 20.0;
    constexpr double                  kDecade      = 10.0;
    double                            signalRms    = options.amplitude / std::numbers::sqrt2;
    double                            noiseRms     = signalRms / pow (kDecade, options.noiseSnrDb / kDbPerDecade);
    std::mt19937                      engine         (options.noiseSeed);
    std::normal_distribution<double>  noise          (0.0, noiseRms);



    if (options.noiseSnrDb == 0.0)
    {
        return;
    }

    for (float & sample : audio.samples)
    {
        sample = (float) (sample + noise (engine));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeTestEncoder::MakeWav
//
//  Several records back to back, each with its own leader, as a SAVE writes
//  them, rendered and stored as a WAV.
//
////////////////////////////////////////////////////////////////////////////////

void TapeTestEncoder::MakeWav (const std::vector<std::vector<Byte>> & records, const TapeEncodeOptions & options, std::vector<Byte> & bytes)
{
    std::vector<double>  halfCycles;
    TapeAudio            audio;



    for (const std::vector<Byte> & record : records)
    {
        AppendRecord (halfCycles, record, options);
    }

    Render (halfCycles, options, audio);
    EncodeWav (audio, options, bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeTestEncoder::EncodeWav
//
//  Any depth and channel count, so the reader's whole format matrix can be
//  exercised. Every channel holds the same samples.
//
////////////////////////////////////////////////////////////////////////////////

void TapeTestEncoder::EncodeWav (const TapeAudio & audio, const TapeEncodeOptions & options, std::vector<Byte> & bytes)
{
    constexpr Word      kTagPcm      = 1;
    constexpr Word      kTagFloat    = 3;
    constexpr uint32_t  kFmtSize     = 16;
    constexpr uint32_t  kHeaderTail  = 36;
    uint32_t            blockAlign   = options.channels * (options.bitsPerSample / 8u);
    uint32_t            dataSize     = (uint32_t) audio.samples.size() * blockAlign;
    auto                put32        = [&] (uint32_t v) { for (int i = 0; i < 4; i++) { bytes.push_back ((Byte) (v >> (8 * i))); } };
    auto                put16        = [&] (Word v)     { bytes.push_back ((Byte) v); bytes.push_back ((Byte) (v >> 8)); };
    auto                putTag       = [&] (const char * t) { bytes.insert (bytes.end(), t, t + 4); };



    bytes.clear();
    putTag ("RIFF");
    put32  (kHeaderTail + dataSize);
    putTag ("WAVE");
    putTag ("fmt ");
    put32  (kFmtSize);
    put16  (options.isFloat ? kTagFloat : kTagPcm);
    put16  (options.channels);
    put32  (audio.sampleRate);
    put32  (audio.sampleRate * blockAlign);
    put16  ((Word) blockAlign);
    put16  (options.bitsPerSample);
    putTag ("data");
    put32  (dataSize);

    for (float sample : audio.samples)
    {
        for (Word c = 0; c < options.channels; c++)
        {
            WriteSample (bytes, sample, options, false);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeTestEncoder::EncodeAiff
//
//  Plain AIFF: COMM with an 80-bit extended sample rate, then SSND holding
//  signed big-endian PCM.
//
////////////////////////////////////////////////////////////////////////////////

void TapeTestEncoder::EncodeAiff (const TapeAudio & audio, const TapeEncodeOptions & options, std::vector<Byte> & bytes)
{
    constexpr int       kExponentBias = 16383;
    constexpr int       kMantissaTop  = 63;
    constexpr uint32_t  kCommSize     = 18;
    constexpr uint32_t  kSsndHeader   = 8;
    constexpr uint32_t  kFormTail     = 4 + 8 + kCommSize + 8 + kSsndHeader;
    uint32_t            frameSize     = options.channels * (options.bitsPerSample / 8u);
    uint32_t            dataSize      = (uint32_t) audio.samples.size() * frameSize;
    int                 exponent      = (int) floor (log2 ((double) audio.sampleRate));
    uint64_t            mantissa      = (uint64_t) audio.sampleRate << (kMantissaTop - exponent);
    auto                put32         = [&] (uint32_t v) { for (int i = 3; i >= 0; i--) { bytes.push_back ((Byte) (v >> (8 * i))); } };
    auto                put16         = [&] (Word v)     { bytes.push_back ((Byte) (v >> 8)); bytes.push_back ((Byte) v); };
    auto                putTag        = [&] (const char * t) { bytes.insert (bytes.end(), t, t + 4); };



    bytes.clear();
    putTag ("FORM");
    put32  (kFormTail + dataSize);
    putTag ("AIFF");
    putTag ("COMM");
    put32  (kCommSize);
    put16  (options.channels);
    put32  ((uint32_t) audio.samples.size());
    put16  (options.bitsPerSample);
    put16  ((Word) (exponent + kExponentBias));
    put32  ((uint32_t) (mantissa >> 32));
    put32  ((uint32_t) mantissa);
    putTag ("SSND");
    put32  (kSsndHeader + dataSize);
    put32  (0);
    put32  (0);

    for (float sample : audio.samples)
    {
        for (Word c = 0; c < options.channels; c++)
        {
            WriteSample (bytes, sample, options, true);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeTestEncoder::WriteSample
//
//  WAV 8-bit is unsigned; every other integer depth, and all of AIFF, is
//  signed.
//
////////////////////////////////////////////////////////////////////////////////

void TapeTestEncoder::WriteSample (std::vector<Byte> & bytes, double sample, const TapeEncodeOptions & options, bool isBigEndian)
{
    constexpr int  kByteBits = 8;
    double         clamped   = clamp (sample, -1.0, 1.0);
    int            byteCount = options.bitsPerSample / kByteBits;
    double         scale     = ldexp (1.0, options.bitsPerSample - 1);
    int64_t        value     = 0;
    float          asFloat   = (float) clamped;
    double         asDouble  = clamped;
    Byte           raw[8]    = {};



    if (options.isFloat)
    {
        if (options.bitsPerSample == 32)
        {
            memcpy (raw, &asFloat, sizeof (asFloat));
        }
        else
        {
            memcpy (raw, &asDouble, sizeof (asDouble));
        }

        bytes.insert (bytes.end(), raw, raw + byteCount);
        return;
    }

    value = (int64_t) llround (clamped * scale);
    value = min (value, (int64_t) scale - 1);

    if (byteCount == 1 && !isBigEndian)
    {
        value += (int64_t) scale;
    }

    for (int i = 0; i < byteCount; i++)
    {
        int  shift = isBigEndian ? (byteCount - 1 - i) * kByteBits : i * kByteBits;

        bytes.push_back ((Byte) (value >> shift));
    }
}
