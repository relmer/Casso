#include "Pch.h"

#include "CassoLink/Audio/CassetteCodec.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassetteCodecTests
//
//  Casso's audio encoding through the faults of a real cassette path:
//  resampling, an inverted cable, a comparator whose threshold isn't
//  centered, a weak and noisy signal, a low-pass and timing jitter.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassetteCodecTests)
{
public:

    static std::vector<Byte>  MakeBytes (size_t count, int seed)
    {
        std::vector<Byte>  bytes (count);



        for (size_t at = 0; at < count; at++)
        {
            bytes[at] = (Byte) (at * 37 + seed * 11 + (at >> 3));
        }

        return bytes;
    }

    //  The samples with silence before and after them.
    static std::vector<float>  Pad (const std::vector<float> & samples, uint32_t rate)
    {
        std::vector<float>  padded ((size_t) rate / 20, 0.0f);



        padded.insert (padded.end(), samples.begin(), samples.end());
        padded.insert (padded.end(), (size_t) rate / 20, 0.0f);

        return padded;
    }

    //  The bursts the decoder finds in the samples, pushed a block at a time.
    static std::vector<std::vector<Byte>>  Decode (const std::vector<float> & samples, uint32_t rate)
    {
        static constexpr size_t  kBlock = 441;

        CassetteDecoder                 decoder (rate);
        std::vector<std::vector<Byte>>  bursts;
        std::vector<Byte>               burst;



        for (size_t at = 0; at < samples.size(); at += kBlock)
        {
            decoder.Push (std::span<const float> (samples).subspan (at, std::min (kBlock, samples.size() - at)));
        }

        while (decoder.PopBurst (burst))
        {
            bursts.push_back (burst);
        }

        return bursts;
    }


    TEST_METHOD (ABurst_DecodesFromItsHalfCycles)
    {
        std::vector<Byte>    bytes  = MakeBytes (300, 1);
        std::vector<double>  halves;
        std::vector<Byte>    burst;
        CassetteDecoder      decoder (44100);



        CassetteCodec::AppendBurst (bytes, halves);

        for (double half : halves)
        {
            decoder.PushHalf (half);
        }

        Assert::IsTrue (decoder.PopBurst (burst));
        Assert::IsTrue (burst == bytes);
        Assert::IsFalse (decoder.PopBurst (burst));
    }


    TEST_METHOD (Bursts_DecodeAt44100And48000)
    {
        for (uint32_t rate : { 44100u, 48000u })
        {
            std::vector<Byte>               first  = MakeBytes (1, 2);
            std::vector<Byte>               second = MakeBytes (CassetteCodec::kMaxBurst, 3);
            std::vector<double>             halves;
            std::vector<float>              samples;
            std::vector<float>              more;
            std::vector<std::vector<Byte>>  bursts;



            CassetteCodec::AppendBurst (first, halves);
            CassetteCodec::Render (halves, rate, 0.8f, samples);

            samples = Pad (samples, rate);
            halves.clear();

            CassetteCodec::AppendBurst (second, halves);
            CassetteCodec::Render (halves, rate, 0.8f, more);

            samples.insert (samples.end(), more.begin(), more.end());
            bursts = Decode (Pad (samples, rate), rate);

            Assert::AreEqual ((size_t) 2, bursts.size(), std::format (L"{} Hz", rate).c_str());
            Assert::IsTrue (bursts[0] == first);
            Assert::IsTrue (bursts[1] == second);
        }
    }


    TEST_METHOD (AnInvertedSignal_Decodes)
    {
        std::vector<Byte>               bytes = MakeBytes (200, 4);
        std::vector<double>             halves;
        std::vector<float>              samples;
        std::vector<std::vector<Byte>>  bursts;



        CassetteCodec::AppendBurst (bytes, halves);
        CassetteCodec::Render (halves, 48000, 0.8f, samples);

        for (float & sample : samples)
        {
            sample = -sample;
        }

        bursts = Decode (Pad (samples, 48000), 48000);

        Assert::AreEqual ((size_t) 1, bursts.size());
        Assert::IsTrue (bursts[0] == bytes);
    }


    //  A comparator whose threshold is off center lengthens one half of each
    //  cycle and shortens the other by the same time.
    TEST_METHOD (UnequalHalves_Decode)
    {
        static constexpr double  kSkewUs = 100.0;

        std::vector<Byte>               bytes = MakeBytes (200, 5);
        std::vector<double>             halves;
        std::vector<float>              samples;
        std::vector<std::vector<Byte>>  bursts;



        CassetteCodec::AppendBurst (bytes, halves);

        for (size_t at = 0; at < halves.size(); at++)
        {
            halves[at] += (at % 2 == 0) ? kSkewUs : -kSkewUs;
        }

        CassetteCodec::Render (halves, 44100, 0.8f, samples);
        bursts = Decode (Pad (samples, 44100), 44100);

        Assert::AreEqual ((size_t) 1, bursts.size());
        Assert::IsTrue (bursts[0] == bytes);
    }


    //  A microphone-level signal through a 5 kHz low-pass, with hiss and
    //  each half cycle up to 8% off.
    TEST_METHOD (AWeakNoisyJitteredSignal_Decodes)
    {
        static constexpr uint32_t  kRate      = 44100;
        static constexpr float     kAmplitude = 0.03f;
        static constexpr float     kNoise     = 0.004f;
        static constexpr double    kJitter    = 0.08;

        std::vector<Byte>                       bytes = MakeBytes (1000, 6);
        std::vector<double>                     halves;
        std::vector<float>                      samples;
        std::vector<std::vector<Byte>>          bursts;
        std::mt19937                            random (1234);
        std::uniform_real_distribution<double>  jitter (-kJitter, kJitter);
        std::uniform_real_distribution<float>   noise (-kNoise, kNoise);
        double                                  smoothing = 1.0 - exp (-2.0 * std::numbers::pi * 5000.0 / kRate);
        double                                  level     = 0.0;



        CassetteCodec::AppendBurst (bytes, halves);

        for (double & half : halves)
        {
            half *= 1.0 + jitter (random);
        }

        CassetteCodec::Render (halves, kRate, kAmplitude, samples);
        samples = Pad (samples, kRate);

        for (float & sample : samples)
        {
            level  += smoothing * ((double) sample - level);
            sample  = (float) level + noise (random);
        }

        bursts = Decode (samples, kRate);

        Assert::AreEqual ((size_t) 1, bursts.size());
        Assert::IsTrue (bursts[0] == bytes);
    }


    //  A count that doesn't match its complement drops the burst, and the
    //  next burst decodes.
    TEST_METHOD (ADamagedCount_DropsTheBurst)
    {
        std::vector<Byte>               bytes = MakeBytes (100, 7);
        std::vector<double>             halves;
        std::vector<double>             good;
        std::vector<float>              samples;
        std::vector<float>              more;
        std::vector<std::vector<Byte>>  bursts;
        size_t                          bit   = (size_t) CassetteCodec::kToneHalves + 2;



        CassetteCodec::AppendBurst (bytes, halves);

        //  The top bit of the count's low byte, a 0 for 100 bytes, made a 1.
        halves[bit]     = CassetteCodec::kOneHalfUs;
        halves[bit + 1] = CassetteCodec::kOneHalfUs;

        CassetteCodec::AppendBurst (bytes, good);
        CassetteCodec::Render (halves, 44100, 0.8f, samples);
        CassetteCodec::Render (good, 44100, 0.8f, more);

        samples = Pad (samples, 44100);
        samples.insert (samples.end(), more.begin(), more.end());
        bursts = Decode (Pad (samples, 44100), 44100);

        Assert::AreEqual ((size_t) 1, bursts.size());
        Assert::IsTrue (bursts[0] == bytes);
    }


    TEST_METHOD (ABurst_IsNoLongerThanGetBurstUs)
    {
        std::vector<Byte>    bytes (CassetteCodec::kMaxBurst, 0xFF);
        std::vector<double>  halves;
        double               total = 0.0;



        CassetteCodec::AppendBurst (bytes, halves);

        for (double half : halves)
        {
            total += half;
        }

        Assert::IsTrue (total <= CassetteCodec::GetBurstUs (bytes.size()));
    }
};
