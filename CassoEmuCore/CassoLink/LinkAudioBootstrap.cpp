#include "Pch.h"

#include "CassoLink/LinkAudioBootstrap.h"
#include "Devices/Tape/WavCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkAudioBootstrap::BuildAudio
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkAudioBootstrap::BuildAudio (const LinkShimOptions & options, TapeAudio & outAudio, std::string & outCommand)
{
    HRESULT              hr    = S_OK;
    std::vector<Byte>    shim;
    std::vector<double>  halfCycles;
    Word                 start = LinkProgram::GetShimAddress (options.chip);



    outCommand.clear();

    hr = LinkProgram::BuildShim (options, shim);
    CHR (hr);

    AppendRecord (shim, halfCycles);
    Render (halfCycles, outAudio);

    outCommand = std::format ("{:04X}.{:04X}R {:04X}G\r", start, start + shim.size() - 1, start);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkAudioBootstrap::BuildWav
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkAudioBootstrap::BuildWav (const LinkShimOptions & options, std::vector<Byte> & outWav, std::string & outCommand)
{
    HRESULT    hr = S_OK;
    TapeAudio  audio;



    outWav.clear();

    hr = BuildAudio (options, audio, outCommand);
    CHR (hr);

    WavCodec::Encode (audio, outWav);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkAudioBootstrap::AppendRecord
//
//  One closing half-cycle follows the checksum, so its last edge is
//  followed by another edge rather than by silence.
//
////////////////////////////////////////////////////////////////////////////////

void LinkAudioBootstrap::AppendRecord (std::span<const Byte> data, std::vector<double> & halfCyclesUs)
{
    constexpr double  kUsPerSecond = 1000000.0;
    size_t            leaderHalves = (size_t) (kLeaderSeconds * kUsPerSecond / kLeaderHalfUs);
    Byte              checksum     = kChecksumSeed;



    halfCyclesUs.insert (halfCyclesUs.end(), leaderHalves, kLeaderHalfUs);
    halfCyclesUs.push_back (kSyncFirstUs);
    halfCyclesUs.push_back (kSyncSecondUs);

    for (Byte value : data)
    {
        AppendByte (value, halfCyclesUs);
        checksum ^= value;
    }

    AppendByte (checksum, halfCyclesUs);
    halfCyclesUs.push_back (kOneHalfUs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkAudioBootstrap::AppendByte
//
////////////////////////////////////////////////////////////////////////////////

void LinkAudioBootstrap::AppendByte (Byte value, std::vector<double> & halfCyclesUs)
{
    constexpr int   kBitsPerByte = 8;
    constexpr Byte  kHighBit     = 0x80;



    for (int bit = 0; bit < kBitsPerByte; bit++)
    {
        double  half = (value & (kHighBit >> bit)) != 0 ? kOneHalfUs : kZeroHalfUs;

        halfCyclesUs.push_back (half);
        halfCyclesUs.push_back (half);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkAudioBootstrap::Render
//
////////////////////////////////////////////////////////////////////////////////

void LinkAudioBootstrap::Render (std::span<const double> halfCyclesUs, TapeAudio & outAudio)
{
    constexpr double  kUsPerSecond = 1000000.0;
    double            totalUs      = kTailSeconds * kUsPerSecond * 2.0;
    double            startUs      = kTailSeconds * kUsPerSecond;
    double            sign         = 1.0;
    size_t            total        = 0;



    for (double half : halfCyclesUs)
    {
        totalUs += half;
    }

    total = (size_t) (totalUs * kSampleRate / kUsPerSecond);

    outAudio.sampleRate    = kSampleRate;
    outAudio.bitsPerSample = 16;
    outAudio.samples.assign (total, 0.0f);

    for (double half : halfCyclesUs)
    {
        size_t  first = (size_t) ceil (startUs * kSampleRate / kUsPerSecond);
        size_t  last  = (size_t) ceil ((startUs + half) * kSampleRate / kUsPerSecond);

        for (size_t n = first; n < last && n < total; n++)
        {
            double  t = ((double) n * kUsPerSecond / kSampleRate - startUs) / half;

            outAudio.samples[n] = (float) (sign * sin (std::numbers::pi * t) * kAmplitude);
        }

        startUs += half;
        sign     = -sign;
    }
}
