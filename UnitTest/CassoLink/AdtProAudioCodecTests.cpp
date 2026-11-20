#include "Pch.h"

#include "CassoLink/Audio/AdtProAudioCodec.h"
#include "CassoLink/Audio/CassetteCodec.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProAudioCodecTests
//
//  ADTPro's audio encoding: bursts Casso sends, decoded at both sample rates
//  and through an inverted cable, and a burst as ADTPro's client sent it in
//  a recording, with the client's trailer after its packet.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AdtProAudioCodecTests)
{
public:

    static constexpr float  kAmplitude = 0.7f;

    static std::vector<std::vector<Byte>>  Decode (const std::vector<float> & samples, uint32_t rate)
    {
        static constexpr size_t  kBlock = 441;

        AdtProAudioDecoder              decoder (rate);
        std::vector<std::vector<Byte>>  bursts;
        std::vector<Byte>               burst;
        std::vector<float>              padded ((size_t) rate / 20, 0.0f);



        padded.insert (padded.end(), samples.begin(), samples.end());
        padded.insert (padded.end(), (size_t) rate / 20, 0.0f);

        for (size_t at = 0; at < padded.size(); at += kBlock)
        {
            decoder.Push (std::span<const float> (padded).subspan (at, std::min (kBlock, padded.size() - at)));
        }

        while (decoder.PopBurst (burst))
        {
            bursts.push_back (burst);
        }

        return bursts;
    }

    static std::vector<Byte>  MakeBytes (size_t count)
    {
        std::vector<Byte>  bytes (count);



        for (size_t at = 0; at < count; at++)
        {
            bytes[at] = (Byte) (at * 37 + (at >> 3));
        }

        return bytes;
    }


    TEST_METHOD (ABurst_DecodesAtEachRate_AndInverted)
    {
        std::vector<Byte>    bytes = MakeBytes (600);
        std::vector<double>  halves;
        std::vector<float>   samples;



        AdtProAudioCodec::AppendBurst (bytes, halves);

        for (uint32_t rate : { 44100u, 48000u })
        {
            for (float amplitude : { kAmplitude, -kAmplitude })
            {
                std::vector<std::vector<Byte>>  bursts;

                CassetteCodec::Render (halves, rate, amplitude, samples);
                bursts = Decode (samples, rate);

                Assert::AreEqual<size_t> (1, bursts.size(), std::format (L"{} Hz, amplitude {}", rate, amplitude).c_str());
                Assert::IsTrue (bursts[0] == bytes, std::format (L"{} Hz, amplitude {}", rate, amplitude).c_str());
            }
        }
    }


    //  The client's Directory request as its cassette output toggled, in
    //  the Apple's cycles: a tone of 545 halves of 652 cycles, a sync of
    //  195 and 250, the packet, then a byte and the check.
    TEST_METHOD (TheClientsRecordedRequest_DecodesToItsPacket)
    {
        const Byte                      burst[]  = { 0xC1, 0x02, 0x00, 0xC4, 0x07, 0x00, 0x00, 0x00, 0x00, 0xFF };
        const Byte                      packet[] = { 0xC1, 0x02, 0x00, 0xC4, 0x07, 0x00, 0x00, 0x00 };
        std::vector<double>             halves (545, 652.0 * AdtProAudioCodec::kUsPerCycle);
        std::vector<float>              samples;
        std::vector<std::vector<Byte>>  bursts;
        std::span<const Byte>           found;



        halves.push_back (195.0 * AdtProAudioCodec::kUsPerCycle);
        halves.push_back (250.0 * AdtProAudioCodec::kUsPerCycle);

        for (Byte value : burst)
        {
            for (int bit = 7; bit >= 0; bit--)
            {
                double  half = (((value >> bit) & 1) != 0 ? 502.0 : 253.0) * AdtProAudioCodec::kUsPerCycle;

                halves.push_back (half);
                halves.push_back (half);
            }
        }

        CassetteCodec::Render (halves, 44100, kAmplitude, samples);
        bursts = Decode (samples, 44100);

        Assert::AreEqual<size_t> (1, bursts.size());
        Assert::IsTrue (std::ranges::equal (bursts[0], burst));

        found = AdtProAudioCodec::GetPacket (bursts[0]);
        Assert::IsTrue (std::ranges::equal (found, packet));
    }


    //  The byte before the check varies; the check alone marks the trailer.
    TEST_METHOD (ATrailer_IsRemovedWhenItsCheckMatches)
    {
        std::vector<Byte>      burst = { 0xC1, 0x00, 0x00, 0xD8, 0x19, 0x5A };
        Byte                   check = 0xFF;
        std::span<const Byte>  found;



        for (Byte value : burst)
        {
            check ^= value;
        }

        burst.push_back (check);
        found = AdtProAudioCodec::GetPacket (burst);
        Assert::AreEqual<size_t> (burst.size() - AdtProAudioCodec::kTrailerBytes, found.size());

        burst.back() ^= 0x01;
        found = AdtProAudioCodec::GetPacket (burst);
        Assert::AreEqual<size_t> (burst.size(), found.size(), L"a check that doesn't match leaves the burst whole");
    }
};
