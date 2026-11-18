#include "Pch.h"

#include "CassoLink/Audio/AdtProAudioCodec.h"
#include "CassoLink/Audio/CassetteCodec.h"
#include "CassoLink/Audio/DualCassetteByteStream.h"
#include "ScriptedCassetteAudio.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DualCassetteByteStreamTests
//
//  The cassette link that serves both clients: a burst in either encoding
//  is read, and a reply goes out in the encoding of the last burst read.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DualCassetteByteStreamTests)
{
public:

    static constexpr DWORD  kTimeoutMs = 2000;

    //  ADTPro's Home packet as its client sends it, with a trailer.
    static std::vector<Byte>  MakeAdtProBurst()
    {
        std::vector<Byte>  burst = { 0xC1, 0x00, 0x00, 0xD8, 0x19, 0x5A };
        Byte               check = 0xFF;



        for (Byte value : burst)
        {
            check ^= value;
        }

        burst.push_back (check);
        return burst;
    }

    static std::vector<Byte>  ReadAll (DualCassetteByteStream & stream)
    {
        std::vector<Byte>  bytes (256);
        size_t             read  = 0;



        AssertSucceeded (stream.Read (bytes.data(), bytes.size(), kTimeoutMs, read));
        bytes.resize (read);
        return bytes;
    }

    //  The bursts each decoder finds in the output samples.
    static void  DecodePlayed (const std::vector<float> & played, std::vector<std::vector<Byte>> & outCasso, std::vector<std::vector<Byte>> & outAdtPro)
    {
        CassetteDecoder     casso  (ScriptedCassetteAudio::kSampleRate);
        AdtProAudioDecoder  adtPro (ScriptedCassetteAudio::kSampleRate);
        std::vector<float>  padded (played);
        std::vector<Byte>   burst;



        padded.insert (padded.end(), ScriptedCassetteAudio::kSampleRate / 10, 0.0f);
        casso.Push (padded);
        adtPro.Push (padded);
        outCasso.clear();
        outAdtPro.clear();

        while (casso.PopBurst (burst))
        {
            outCasso.push_back (burst);
        }

        while (adtPro.PopBurst (burst))
        {
            outAdtPro.push_back (burst);
        }
    }

    //  Writes a reply and reads, which sends it, and returns the output samples.
    static std::vector<float>  Reply (DualCassetteByteStream & stream, ScriptedCassetteAudio & audio, std::span<const Byte> reply)
    {
        Byte    buffer[1];
        size_t  read = 0;



        audio.played.clear();
        AssertSucceeded (stream.Write (reply.data(), reply.size()));
        AssertSucceeded (stream.Read (buffer, sizeof (buffer), 1, read));
        return audio.played;
    }


    TEST_METHOD (AnAdtProBurst_IsReadAsItsPacket_AndAnsweredInItsEncoding)
    {
        auto                            audio  = std::make_shared<ScriptedCassetteAudio>();
        DualCassetteByteStream          stream (audio);
        std::vector<Byte>               burst   = MakeAdtProBurst();
        std::vector<double>             halves;
        const Byte                      reply[] = { 0xC1, 0x01, 0x00, 0xD3 };
        std::vector<std::vector<Byte>>  casso;
        std::vector<std::vector<Byte>>  adtPro;



        AdtProAudioCodec::AppendBurst (burst, halves);
        audio->Queue (halves);

        Assert::IsTrue (ReadAll (stream) == std::vector<Byte> (burst.begin(), burst.end() - AdtProAudioCodec::kTrailerBytes), L"the packet, without the trailer");
        Assert::IsTrue (stream.IsAdtPro());

        DecodePlayed (Reply (stream, *audio, reply), casso, adtPro);
        Assert::AreEqual<size_t> (0, casso.size(), L"no burst in Casso's encoding");
        Assert::AreEqual<size_t> (1, adtPro.size());
        Assert::IsTrue (std::ranges::equal (adtPro[0], reply));
    }


    //  A run of $FF in Casso's encoding is a long run of 500-microsecond
    //  halves, which isn't ADTPro's tone, so the bytes after it aren't read as
    //  an ADTPro burst.
    TEST_METHOD (ACassoBurst_OfOnes_IsAnsweredInCassosEncoding)
    {
        auto                            audio  = std::make_shared<ScriptedCassetteAudio>();
        DualCassetteByteStream          stream (audio);
        std::vector<Byte>               ones (60, 0xFF);
        std::vector<double>             halves;
        const Byte                      reply[] = { 0x01, 0x02, 0x03 };
        std::vector<std::vector<Byte>>  casso;
        std::vector<std::vector<Byte>>  adtPro;



        for (int at = 0; at < 16; at++)
        {
            ones.push_back ((Byte) (at * 37));
        }

        CassetteCodec::AppendBurst (ones, halves);
        audio->Queue (halves);

        Assert::IsTrue (ReadAll (stream) == ones);
        Assert::IsFalse (stream.IsAdtPro());

        DecodePlayed (Reply (stream, *audio, reply), casso, adtPro);
        Assert::AreEqual<size_t> (1, casso.size());
        Assert::IsTrue (std::ranges::equal (casso[0], reply));
        Assert::AreEqual<size_t> (0, adtPro.size(), L"no burst in ADTPro's encoding");
    }


    //  A Casso burst after an ADTPro one goes back to Casso's encoding.
    TEST_METHOD (TheReplyEncoding_FollowsTheLastBurst)
    {
        auto                    audio  = std::make_shared<ScriptedCassetteAudio>();
        DualCassetteByteStream  stream (audio);
        std::vector<Byte>       burst  = MakeAdtProBurst();
        std::vector<Byte>       bytes  = { 0x10, 0x20 };
        std::vector<double>     halves;



        AdtProAudioCodec::AppendBurst (burst, halves);
        audio->Queue (halves);
        ReadAll (stream);
        Assert::IsTrue (stream.IsAdtPro());

        halves.clear();
        CassetteCodec::AppendBurst (bytes, halves);
        audio->Queue (halves);
        Assert::IsTrue (ReadAll (stream) == bytes);
        Assert::IsFalse (stream.IsAdtPro());
    }
};
