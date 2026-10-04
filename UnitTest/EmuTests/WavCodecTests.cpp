#include "Pch.h"

#include "Devices/Tape/WavCodec.h"
#include "TapeTestEncoder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WavCodecTests
//
//  Pure byte-buffer tests: every WAV is built in memory.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WavCodecTests)
{
public:

    static constexpr uint32_t  kRate       = 44100;
    static constexpr size_t    kCount      = 64;
    static constexpr double    kTolerance8 = 1.0 / 64.0;
    static constexpr double    kTolerance  = 1.0 / 16384.0;


    static void MakeRamp (TapeAudio & audio, uint32_t rate)
    {
        audio.sampleRate = rate;
        audio.samples.resize (kCount);

        for (size_t i = 0; i < kCount; i++)
        {
            audio.samples[i] = (float) ((double) i / kCount * 1.6 - 0.8);
        }
    }


    static void CheckRoundTrip (Word bits, bool isFloat, Word channels, uint32_t rate, double tolerance)
    {
        TapeAudio          source;
        TapeAudio          decoded;
        TapeEncodeOptions  options;
        std::vector<Byte>  bytes;
        std::string        error;
        HRESULT            hr = S_OK;



        MakeRamp (source, rate);
        options.bitsPerSample = bits;
        options.isFloat       = isFloat;
        options.channels      = channels;

        TapeTestEncoder::EncodeWav (source, options, bytes);
        hr = WavCodec::Decode (bytes, decoded, error);

        Assert::AreEqual (S_OK, hr, ToWide (error).c_str());
        Assert::AreEqual (rate,   decoded.sampleRate);
        Assert::AreEqual (kCount, decoded.samples.size());

        for (size_t i = 0; i < kCount; i++)
        {
            Assert::AreEqual ((double) source.samples[i], (double) decoded.samples[i], tolerance);
        }
    }


    static std::wstring ToWide (const std::string & text)
    {
        return std::wstring (text.begin(), text.end());
    }


    TEST_METHOD (Reads8BitPcm)      { CheckRoundTrip (8,  false, 1, kRate, kTolerance8); }
    TEST_METHOD (Reads16BitPcm)     { CheckRoundTrip (16, false, 1, kRate, kTolerance); }
    TEST_METHOD (Reads24BitPcm)     { CheckRoundTrip (24, false, 1, kRate, kTolerance); }
    TEST_METHOD (Reads32BitPcm)     { CheckRoundTrip (32, false, 1, kRate, kTolerance); }
    TEST_METHOD (Reads32BitFloat)   { CheckRoundTrip (32, true,  1, kRate, kTolerance); }
    TEST_METHOD (Reads64BitFloat)   { CheckRoundTrip (64, true,  1, kRate, kTolerance); }
    TEST_METHOD (ReadsStereoAsMono) { CheckRoundTrip (16, false, 2, kRate, kTolerance); }
    TEST_METHOD (Reads8kHz)         { CheckRoundTrip (16, false, 1, 8000,  kTolerance); }
    TEST_METHOD (Reads96kHz)        { CheckRoundTrip (16, false, 1, 96000, kTolerance); }


    //  Casso writes 16-bit unless the tape is 8-bit, and a tape's depth is
    //  what it was read with, so recording onto an 8-bit tape keeps it
    //  8-bit -- the format some tools, CiderPress II among them, insist on.
    TEST_METHOD (WritesTheDepthTheTapeWasReadWith)
    {
        for (Word bits : { (Word) 8, (Word) 16 })
        {
            TapeAudio          source;
            TapeAudio          decoded;
            TapeAudio          rewritten;
            std::vector<Byte>  bytes;
            std::string        error;
            HRESULT            hr = S_OK;



            MakeRamp (source, kRate);
            source.bitsPerSample = bits;

            WavCodec::Encode (source, bytes);
            Assert::AreEqual (bits, (Word) (bytes[34] | (bytes[35] << 8)), L"bits per sample in the header");

            hr = WavCodec::Decode (bytes, decoded, error);
            Assert::AreEqual (S_OK, hr, ToWide (error).c_str());
            Assert::AreEqual ((uint16_t) bits, decoded.bitsPerSample);

            for (size_t i = 0; i < kCount; i++)
            {
                Assert::AreEqual ((double) source.samples[i], (double) decoded.samples[i],
                                  bits == 8 ? kTolerance8 : kTolerance);
            }

            WavCodec::Encode (decoded, bytes);
            hr = WavCodec::Decode (bytes, rewritten, error);
            Assert::AreEqual (S_OK, hr);
            Assert::AreEqual ((uint16_t) bits, rewritten.bitsPerSample, L"a rewrite keeps the depth");
        }
    }


    TEST_METHOD (ReadsExtensibleFormat)
    {
        // A 40-byte extensible fmt chunk whose sub-format GUID starts with
        // the PCM tag.
        std::vector<Byte>  bytes   = {
            'R','I','F','F', 56,0,0,0, 'W','A','V','E',
            'f','m','t',' ', 40,0,0,0,
            0xFE,0xFF, 1,0, 0x44,0xAC,0,0, 0x88,0x58,1,0, 2,0, 16,0,
            22,0, 16,0, 4,0,0,0, 1,0, 0,0, 0,0,0x10,0, 0x80,0,0,0xAA,0,0x38,0x9B,0x71,
            'd','a','t','a', 4,0,0,0, 0x00,0x40, 0x00,0xC0 };
        TapeAudio          audio;
        std::string        error;
        HRESULT            hr      = WavCodec::Decode (bytes, audio, error);



        Assert::AreEqual (S_OK, hr, ToWide (error).c_str());
        Assert::AreEqual (size_t (2), audio.samples.size());
        Assert::AreEqual ( 0.5, (double) audio.samples[0], kTolerance);
        Assert::AreEqual (-0.5, (double) audio.samples[1], kTolerance);
    }


    TEST_METHOD (StereoWithInvertedChannelUsesLouderChannel)
    {
        // Left carries the signal, right carries it inverted at a slightly
        // lower level; the plain average would nearly cancel.
        std::vector<Byte>  bytes   = {
            'R','I','F','F', 44,0,0,0, 'W','A','V','E',
            'f','m','t',' ', 16,0,0,0, 1,0, 2,0, 0x44,0xAC,0,0, 0x10,0xB1,2,0, 4,0, 16,0,
            'd','a','t','a', 8,0,0,0, 0x00,0x40, 0x00,0xC8, 0x00,0xC0, 0x00,0x38 };
        TapeAudio          audio;
        std::string        error;
        HRESULT            hr      = WavCodec::Decode (bytes, audio, error);



        Assert::AreEqual (S_OK, hr, ToWide (error).c_str());
        Assert::AreEqual (size_t (2), audio.samples.size());
        Assert::AreEqual ( 0.5, (double) audio.samples[0], kTolerance);
        Assert::AreEqual (-0.5, (double) audio.samples[1], kTolerance);
    }


    TEST_METHOD (ZeroLengthDataIsValid)
    {
        TapeAudio          empty;
        TapeAudio          decoded;
        std::vector<Byte>  bytes;
        std::string        error;
        HRESULT            hr = S_OK;



        empty.sampleRate = kRate;
        WavCodec::Encode (empty, bytes);
        hr = WavCodec::Decode (bytes, decoded, error);

        Assert::AreEqual (S_OK, hr, ToWide (error).c_str());
        Assert::AreEqual (size_t (44), bytes.size());
        Assert::AreEqual (kRate, decoded.sampleRate);
        Assert::IsTrue   (decoded.samples.empty());
    }


    TEST_METHOD (EncodeThenDecodeIsIdentityAt16Bits)
    {
        TapeAudio          source;
        TapeAudio          decoded;
        std::vector<Byte>  bytes;
        std::vector<Byte>  again;
        std::string        error;
        HRESULT            hr = S_OK;



        MakeRamp (source, kRate);
        WavCodec::Encode (source, bytes);
        hr = WavCodec::Decode (bytes, decoded, error);
        Assert::AreEqual (S_OK, hr, ToWide (error).c_str());

        WavCodec::Encode (decoded, again);
        Assert::IsTrue (bytes == again);
    }


    TEST_METHOD (RejectsGarbage)
    {
        std::vector<Byte>  bytes = { 'N','O','P','E', 0,0,0,0, 'W','A','V','E' };
        TapeAudio          audio;
        std::string        error;
        HRESULT            hr    = WavCodec::Decode (bytes, audio, error);



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
        Assert::IsFalse  (error.empty());
    }


    TEST_METHOD (RejectsTruncatedFormatChunk)
    {
        std::vector<Byte>  bytes = { 'R','I','F','F', 20,0,0,0, 'W','A','V','E', 'f','m','t',' ', 16,0,0,0, 1,0,1,0 };
        TapeAudio          audio;
        std::string        error;
        HRESULT            hr    = WavCodec::Decode (bytes, audio, error);



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
    }


    TEST_METHOD (RejectsMissingDataChunk)
    {
        std::vector<Byte>  bytes = {
            'R','I','F','F', 28,0,0,0, 'W','A','V','E',
            'f','m','t',' ', 16,0,0,0, 1,0, 1,0, 0x44,0xAC,0,0, 0x88,0x58,1,0, 2,0, 16,0 };
        TapeAudio          audio;
        std::string        error;
        HRESULT            hr    = WavCodec::Decode (bytes, audio, error);



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
    }


    TEST_METHOD (RejectsOutOfRangeSampleRate)
    {
        TapeAudio          source;
        TapeAudio          decoded;
        TapeEncodeOptions  options;
        std::vector<Byte>  bytes;
        std::string        error;
        HRESULT            hr = S_OK;



        MakeRamp (source, 4000);
        TapeTestEncoder::EncodeWav (source, options, bytes);
        hr = WavCodec::Decode (bytes, decoded, error);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
    }


    TEST_METHOD (ReadsDataChunkThatClaimsMoreThanTheFile)
    {
        TapeAudio          source;
        TapeAudio          decoded;
        TapeEncodeOptions  options;
        std::vector<Byte>  bytes;
        std::string        error;
        HRESULT            hr = S_OK;



        MakeRamp (source, kRate);
        TapeTestEncoder::EncodeWav (source, options, bytes);
        bytes[40] = 0xFF;
        bytes[41] = 0xFF;
        bytes[42] = 0xFF;
        bytes[43] = 0x7F;
        hr = WavCodec::Decode (bytes, decoded, error);

        Assert::AreEqual (S_OK, hr, ToWide (error).c_str());
        Assert::AreEqual (kCount, decoded.samples.size());
    }
};
