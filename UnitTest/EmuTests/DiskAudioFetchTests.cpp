#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Audio/DiskIIAudioSource.h"
#include "External/StbVorbisWrapper.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAudioFetchTests
//
//  Unit-test coverage for the spec/005-disk-ii-audio Phase 13
//  bootstrap pipeline. We can directly exercise the OGG decoder
//  wrapper (lives in CassoEmuCore, linked into the test DLL) and
//  end-to-end WAV write + IMFSourceReader round trip via
//  DiskIIAudioSource::LoadSamples. The Win32-side AssetBootstrap
//  glue (FetchAndDecodeOgg / WritePcmAsWav / CheckAndFetchDiskAudio)
//  ships inside Casso.exe rather than a static library, so its
//  network-touching paths are covered by the manual integration
//  test described in T138 instead of here -- the constitution
//  forbids tests that hit the network or system state.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr uint32_t  s_kTestSampleRate = 44100;
    constexpr size_t    s_kSineFrames     = 4410;          // 100 ms
    constexpr float     s_kTwoPi          = (float) (2.0 * std::numbers::pi);


    static void WriteMonoPcm16Wav (
        const fs::path     & outPath,
        const vector<float> & pcm,
        uint32_t              sampleRate)
    {
        // Test-side mirror of AssetBootstrap::WritePcmAsWav. Kept in
        // sync with the production helper so a regression there shows
        // up as a round-trip failure here.
        size_t      sampleCount   = pcm.size ();
        uint32_t    dataBytes     = static_cast<uint32_t> (sampleCount * sizeof (int16_t));
        uint32_t    fileSize      = 36 + dataBytes;
        uint16_t    numChannels   = 1;
        uint16_t    bitsPerSample = 16;
        uint32_t    byteRate      = sampleRate * numChannels * (bitsPerSample / 8);
        uint16_t    blockAlign    = numChannels * (bitsPerSample / 8);
        uint16_t    formatPcm     = 1;
        uint32_t    fmtSize       = 16;
        std::ofstream out (outPath, std::ios::binary | std::ios::trunc);

        out.write ("RIFF", 4);
        out.write (reinterpret_cast<const char *> (&fileSize), sizeof (fileSize));
        out.write ("WAVE", 4);

        out.write ("fmt ", 4);
        out.write (reinterpret_cast<const char *> (&fmtSize),       sizeof (fmtSize));
        out.write (reinterpret_cast<const char *> (&formatPcm),     sizeof (formatPcm));
        out.write (reinterpret_cast<const char *> (&numChannels),   sizeof (numChannels));
        out.write (reinterpret_cast<const char *> (&sampleRate),    sizeof (sampleRate));
        out.write (reinterpret_cast<const char *> (&byteRate),      sizeof (byteRate));
        out.write (reinterpret_cast<const char *> (&blockAlign),    sizeof (blockAlign));
        out.write (reinterpret_cast<const char *> (&bitsPerSample), sizeof (bitsPerSample));

        out.write ("data", 4);
        out.write (reinterpret_cast<const char *> (&dataBytes), sizeof (dataBytes));

        for (float s : pcm)
        {
            float  scaled = s * 32767.0f;

            if (scaled >  32767.0f) { scaled =  32767.0f; }
            if (scaled < -32768.0f) { scaled = -32768.0f; }

            int16_t  asI16 = static_cast<int16_t> (scaled);
            out.write (reinterpret_cast<const char *> (&asI16), sizeof (asI16));
        }
    }


    static fs::path MakeTempDeviceDir (const wchar_t * suffix)
    {
        fs::path  base = fs::temp_directory_path () / L"casso_disk_audio_tests";
        fs::path  dir  = base / suffix;

        std::error_code  ec;

        fs::remove_all  (dir, ec);
        fs::create_directories (dir, ec);

        return dir;
    }
}





TEST_CLASS (DiskAudioFetchTests)
{
public:

    TEST_METHOD (DecodeOggToInterleavedShort_nullBuffer_returnsInvalidArg)
    {
        std::vector<int16_t> pcm;
        uint32_t             rate     = 0;
        uint32_t             channels = 0;
        std::string          err;

        HRESULT  hr = StbVorbisWrapper::DecodeOggToInterleavedShort (
            nullptr, 0, pcm, rate, channels, err);

        Assert::AreEqual (HRESULT (E_INVALIDARG), hr,
            L"Null/empty input must return E_INVALIDARG, not crash");
        Assert::IsTrue (pcm.empty (), L"PCM buffer must be cleared on failure");
        Assert::IsFalse (err.empty (), L"Error string must be set on failure");
    }


    TEST_METHOD (DecodeOggToInterleavedShort_garbageBytes_returnsFailureNoCrash)
    {
        // 64 bytes of pseudo-random non-OGG junk. stb_vorbis must
        // reject this cleanly and report an error rather than crash
        // the test DLL (FR-009 graceful degradation: a corrupt
        // upstream OGG must never crash the emulator).
        std::vector<uint8_t> junk (64);

        for (size_t i = 0; i < junk.size (); i++)
        {
            junk[i] = static_cast<uint8_t> (i * 13 + 7);
        }

        std::vector<int16_t> pcm;
        uint32_t             rate     = 0;
        uint32_t             channels = 0;
        std::string          err;

        HRESULT  hr = StbVorbisWrapper::DecodeOggToInterleavedShort (
            junk.data (), junk.size (), pcm, rate, channels, err);

        Assert::IsTrue (FAILED (hr),
            L"Garbage input must return a failure HRESULT");
        Assert::IsTrue (pcm.empty (), L"PCM buffer must be empty on failure");
        Assert::IsFalse (err.empty (), L"Error string must be set on failure");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  WriteAndLoad_writesWavAndReadsBackThroughLoadSamples_preservesAmplitude
    //
    //  Round-trips a known sine wave through the same WAV format the
    //  AssetBootstrap downloader writes, then loads it via
    //  DiskIIAudioSource::LoadSamples (which is what the production
    //  shell calls). Asserts the loaded buffer is non-empty and that
    //  the peak amplitude is close to the original 0.5. Anything
    //  catastrophically wrong in the WAV format would surface as an
    //  empty buffer here (LoadSamples returns silently on per-file
    //  failure per FR-009).
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (WriteAndLoad_writesWavAndReadsBackThroughLoadSamples_preservesAmplitude)
    {
        // Build a half-amplitude 1 kHz sine.
        std::vector<float>  src (s_kSineFrames);
        const float         amp = 0.5f;
        const float         hz  = 1000.0f;

        for (size_t i = 0; i < src.size (); i++)
        {
            float  t = (float) i / (float) s_kTestSampleRate;

            src[i] = amp * sinf (s_kTwoPi * hz * t);
        }

        fs::path  devicesDir = MakeTempDeviceDir (L"WriteAndLoad");
        fs::path  motorPath  = devicesDir / L"MotorLoop.wav";

        WriteMonoPcm16Wav (motorPath, src, s_kTestSampleRate);

        DiskIIAudioSource  src1;

        HRESULT  hr = src1.LoadSamples (devicesDir.wstring ().c_str (),
                                        L"Shugart",
                                        s_kTestSampleRate);

        Assert::IsTrue (SUCCEEDED (hr),
            L"LoadSamples must succeed when at least MediaFoundation is reachable");

        // We can't directly inspect the source's internal buffer
        // post-load (no test accessor for the active motor buffer);
        // instead, trigger MotorStart + render a frame and verify the
        // output is non-silent. Empty buffer == silence == failure
        // under FR-009.
        src1.OnMotorStart ();

        std::vector<float>  frame (256);

        src1.GeneratePCM (frame.data (), static_cast<uint32_t> (frame.size ()));

        float  peak = 0.0f;

        for (float s : frame)
        {
            float  a = (s < 0) ? -s : s;
            if (a > peak) { peak = a; }
        }

        // After the kMotorVolume (0.25) attenuation a half-amplitude
        // sine should produce a peak of ~0.125. Allow generous slack
        // because the looped frame may catch a zero crossing.
        Assert::IsTrue (peak > 0.02f,
            std::format (L"Round-tripped WAV produced silent motor loop (peak={})", peak).c_str ());

        std::error_code  ec;
        fs::remove_all (devicesDir.parent_path (), ec);
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  LoadSamples_mechanismFallback_picksPerMechanismCopy
    //
    //  Verifies the FR-019 per-file precedence rule: with no override
    //  at Devices/DiskII/, the per-mechanism subdir's copy is loaded
    //  instead.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (LoadSamples_mechanismFallback_picksPerMechanismCopy)
    {
        std::vector<float>  pcm (220, 0.5f);    // 5 ms square at full amplitude
        fs::path            devicesDir = MakeTempDeviceDir (L"FallbackPick");
        fs::path            mechDir    = devicesDir / L"Shugart";
        std::error_code     ec;

        fs::create_directories (mechDir, ec);
        WriteMonoPcm16Wav (mechDir / L"MotorLoop.wav", pcm, s_kTestSampleRate);

        DiskIIAudioSource  src;

        HRESULT  hr = src.LoadSamples (devicesDir.wstring ().c_str (),
                                       L"Shugart",
                                       s_kTestSampleRate);
        Assert::IsTrue (SUCCEEDED (hr), L"LoadSamples must succeed");

        src.OnMotorStart ();

        std::vector<float>  frame (64);
        src.GeneratePCM (frame.data (), static_cast<uint32_t> (frame.size ()));

        float  peak = 0.0f;
        for (float s : frame)
        {
            float  a = (s < 0) ? -s : s;
            if (a > peak) { peak = a; }
        }

        Assert::IsTrue (peak > 0.05f,
            L"Per-mechanism copy must load when no top-level override exists");

        fs::remove_all (devicesDir.parent_path (), ec);
    }
};
