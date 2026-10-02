#include "Pch.h"

#include "Devices/Tape/TapeImageLoader.h"
#include "Devices/Tape/WavCodec.h"
#include "FakeTapeAudioDecoder.h"
#include "TapeTestEncoder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TapeImageLoaderTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TapeImageLoaderTests)
{
public:

    static constexpr size_t  kToneHalves = 200;
    static constexpr double  kToneHalfUs = 500.0;


    static void MakeToneAudio (TapeAudio & audio)
    {
        std::vector<double>  halves (kToneHalves, kToneHalfUs);
        TapeEncodeOptions    options;



        options.tailSeconds = 0.1;
        TapeTestEncoder::Render (halves, options, audio);
    }


    TEST_METHOD (WavLoadsAndIsWritable)
    {
        TapeAudio             audio;
        std::vector<Byte>     bytes;
        FakeTapeAudioDecoder  mp3;
        TapeImage             image;
        std::string           error;
        HRESULT               hr = S_OK;



        MakeToneAudio (audio);
        WavCodec::Encode (audio, bytes);
        hr = TapeImageLoader::Load (bytes, "tone.wav", false, mp3, image, error);

        Assert::AreEqual (S_OK, hr);
        Assert::IsTrue   (image.format == TapeFormat::Wav);
        Assert::IsTrue   (image.isWritable);
        Assert::AreEqual (std::string ("tone.wav"), image.path);
        Assert::AreEqual (kToneHalves, image.signal.transitions.size());
        Assert::AreEqual ((uint64_t) audio.samples.size(), image.signal.lengthSamples);
        Assert::AreEqual (0, mp3.callCount);
    }


    TEST_METHOD (ReadOnlyWavIsNotWritable)
    {
        TapeAudio             audio;
        std::vector<Byte>     bytes;
        FakeTapeAudioDecoder  mp3;
        TapeImage             image;
        std::string           error;
        HRESULT               hr = S_OK;



        MakeToneAudio (audio);
        WavCodec::Encode (audio, bytes);
        hr = TapeImageLoader::Load (bytes, "tone.wav", true, mp3, image, error);

        Assert::AreEqual (S_OK, hr);
        Assert::IsFalse  (image.isWritable);
    }


    TEST_METHOD (FormatComesFromContentNotExtension)
    {
        TapeAudio             audio;
        TapeEncodeOptions     options;
        std::vector<Byte>     bytes;
        FakeTapeAudioDecoder  mp3;
        TapeImage             image;
        std::string           error;
        HRESULT               hr = S_OK;



        MakeToneAudio (audio);
        TapeTestEncoder::EncodeAiff (audio, options, bytes);
        hr = TapeImageLoader::Load (bytes, "mislabeled.wav", false, mp3, image, error);

        Assert::AreEqual (S_OK, hr);
        Assert::IsTrue   (image.format == TapeFormat::Aiff);
        Assert::IsFalse  (image.isWritable);
        Assert::AreEqual (kToneHalves, image.signal.transitions.size());
    }


    TEST_METHOD (Mp3GoesThroughTheDecoderAndIsNotWritable)
    {
        std::vector<Byte>     bytes = { 'I','D','3', 4,0,0, 0,0,0,0 };
        FakeTapeAudioDecoder  mp3;
        TapeImage             image;
        std::string           error;
        HRESULT               hr    = S_OK;



        MakeToneAudio (mp3.audio);
        hr = TapeImageLoader::Load (bytes, "tape.mp3", false, mp3, image, error);

        Assert::AreEqual (S_OK, hr);
        Assert::AreEqual (1, mp3.callCount);
        Assert::IsTrue   (image.format == TapeFormat::Mp3);
        Assert::IsFalse  (image.isWritable);
        Assert::AreEqual (kToneHalves, image.signal.transitions.size());
    }


    TEST_METHOD (DetectsMp3FrameSyncWithoutId3)
    {
        std::vector<Byte>  bytes = { 0xFF, 0xFB, 0x90, 0x00 };



        Assert::IsTrue (TapeImageLoader::IsMp3 (bytes));
    }


    TEST_METHOD (UnknownContentFailsAndLeavesNoImage)
    {
        std::vector<Byte>     bytes = { 'H','e','l','l','o' };
        FakeTapeAudioDecoder  mp3;
        TapeImage             image;
        std::string           error;
        HRESULT               hr    = S_OK;



        image.path = "stale";
        hr = TapeImageLoader::Load (bytes, "hello.wav", false, mp3, image, error);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
        Assert::IsTrue   (image.path.empty());
        Assert::IsTrue   (image.signal.transitions.empty());
        Assert::IsFalse  (error.empty());
    }


    TEST_METHOD (DecoderFailureLeavesNoImage)
    {
        std::vector<Byte>     bytes = { 'I','D','3', 4,0,0, 0,0,0,0 };
        FakeTapeAudioDecoder  mp3;
        TapeImage             image;
        std::string           error;
        HRESULT               hr    = S_OK;



        mp3.result = E_FAIL;
        hr = TapeImageLoader::Load (bytes, "broken.mp3", false, mp3, image, error);

        Assert::AreEqual (E_FAIL, hr);
        Assert::IsTrue   (image.path.empty());
    }

    TEST_METHOD (TapeExtensionsAreRecognizedWhateverTheirCase)
    {
        Assert::IsTrue  (TapeImageLoader::IsTapeFileExtension (L"C:\\Tapes\\a.wav"));
        Assert::IsTrue  (TapeImageLoader::IsTapeFileExtension (L"C:\\Tapes\\b.AIFF"));
        Assert::IsTrue  (TapeImageLoader::IsTapeFileExtension (L"c.Mp3"));
        Assert::IsFalse (TapeImageLoader::IsTapeFileExtension (L"C:\\Disks\\dos.dsk"));
        Assert::IsFalse (TapeImageLoader::IsTapeFileExtension (L"noextension"));
    }
};
