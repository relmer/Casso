#include "Pch.h"

#include "Devices/Tape/AiffCodec.h"
#include "TapeTestEncoder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AiffCodecTests
//
//  Pure byte-buffer tests: every AIFF is built in memory.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AiffCodecTests)
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


    static void CheckRoundTrip (Word bits, Word channels, uint32_t rate, double tolerance)
    {
        TapeAudio          source;
        TapeAudio          decoded;
        TapeEncodeOptions  options;
        std::vector<Byte>  bytes;
        std::string        error;
        HRESULT            hr = S_OK;



        MakeRamp (source, rate);
        options.bitsPerSample = bits;
        options.channels      = channels;

        TapeTestEncoder::EncodeAiff (source, options, bytes);
        hr = AiffCodec::Decode (bytes, decoded, error);

        Assert::AreEqual (S_OK, hr);
        Assert::AreEqual (rate,   decoded.sampleRate);
        Assert::AreEqual (kCount, decoded.samples.size());

        for (size_t i = 0; i < kCount; i++)
        {
            Assert::AreEqual ((double) source.samples[i], (double) decoded.samples[i], tolerance);
        }
    }


    static void MakeAifc (const char * pszCompression, std::vector<Byte> & bytes)
    {
        // One 16-bit mono frame holding +0.5, in the byte order the
        // compression type calls for.
        bool  isSowt = strcmp (pszCompression, "sowt") == 0;



        bytes = {
            'F','O','R','M', 0,0,0,46, 'A','I','F','C',
            'C','O','M','M', 0,0,0,22, 0,1, 0,0,0,1, 0,16,
            0x40,0x0E, 0xAC,0x44,0,0,0,0,0,0,
            (Byte) pszCompression[0], (Byte) pszCompression[1], (Byte) pszCompression[2], (Byte) pszCompression[3],
            'S','S','N','D', 0,0,0,10, 0,0,0,0, 0,0,0,0,
            (Byte) (isSowt ? 0x00 : 0x40), (Byte) (isSowt ? 0x40 : 0x00) };
    }


    TEST_METHOD (Reads8BitPcm)      { CheckRoundTrip (8,  1, kRate, kTolerance8); }
    TEST_METHOD (Reads16BitPcm)     { CheckRoundTrip (16, 1, kRate, kTolerance); }
    TEST_METHOD (Reads24BitPcm)     { CheckRoundTrip (24, 1, kRate, kTolerance); }
    TEST_METHOD (Reads32BitPcm)     { CheckRoundTrip (32, 1, kRate, kTolerance); }
    TEST_METHOD (ReadsStereoAsMono) { CheckRoundTrip (16, 2, kRate, kTolerance); }
    TEST_METHOD (ReadsEightKilohertz) { CheckRoundTrip (16, 1, 8000,  kTolerance); }
    TEST_METHOD (ReadsOddSampleRate)  { CheckRoundTrip (16, 1, 22050, kTolerance); }


    TEST_METHOD (ReadsAifcNone)
    {
        std::vector<Byte>  bytes;
        TapeAudio          audio;
        std::string        error;
        HRESULT            hr = S_OK;



        MakeAifc ("NONE", bytes);
        hr = AiffCodec::Decode (bytes, audio, error);

        Assert::AreEqual (S_OK, hr);
        Assert::AreEqual (kRate, audio.sampleRate);
        Assert::AreEqual (size_t (1), audio.samples.size());
        Assert::AreEqual (0.5, (double) audio.samples[0], kTolerance);
    }


    TEST_METHOD (ReadsAifcSowtAsLittleEndian)
    {
        std::vector<Byte>  bytes;
        TapeAudio          audio;
        std::string        error;
        HRESULT            hr = S_OK;



        MakeAifc ("sowt", bytes);
        hr = AiffCodec::Decode (bytes, audio, error);

        Assert::AreEqual (S_OK, hr);
        Assert::AreEqual (size_t (1), audio.samples.size());
        Assert::AreEqual (0.5, (double) audio.samples[0], kTolerance);
    }


    TEST_METHOD (RejectsCompressedAifc)
    {
        std::vector<Byte>  bytes;
        TapeAudio          audio;
        std::string        error;
        HRESULT            hr = S_OK;



        MakeAifc ("ima4", bytes);
        hr = AiffCodec::Decode (bytes, audio, error);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
        Assert::IsFalse  (error.empty());
    }


    TEST_METHOD (RejectsMissingComm)
    {
        std::vector<Byte>  bytes = { 'F','O','R','M', 0,0,0,4, 'A','I','F','F' };
        TapeAudio          audio;
        std::string        error;
        HRESULT            hr    = AiffCodec::Decode (bytes, audio, error);



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
    }


    TEST_METHOD (RejectsShortComm)
    {
        std::vector<Byte>  bytes = { 'F','O','R','M', 0,0,0,18, 'A','I','F','F', 'C','O','M','M', 0,0,0,6, 0,1, 0,0,0,1 };
        TapeAudio          audio;
        std::string        error;
        HRESULT            hr    = AiffCodec::Decode (bytes, audio, error);



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
    }


    TEST_METHOD (RejectsNonAiff)
    {
        std::vector<Byte>  bytes = { 'R','I','F','F', 0,0,0,0, 'W','A','V','E' };
        TapeAudio          audio;
        std::string        error;
        HRESULT            hr    = AiffCodec::Decode (bytes, audio, error);



        Assert::IsFalse  (AiffCodec::IsAiff (bytes));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
    }
};
