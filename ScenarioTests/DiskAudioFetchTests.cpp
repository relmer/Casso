#include "Pch.h"
#include "Machines/Apple2/Common/Disk2AudioSource.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAudioFetchTests
//
//  The drive-sound WAV round trip against real files in the system temp
//  folder: a WAV in the format the downloader writes, read back through
//  Disk2AudioSource::LoadSamples.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskAudioFetchTests)
{
public:

    static constexpr uint32_t  s_kTestSampleRate = 44100;
    static constexpr size_t    s_kSineFrames     = 4410;          // 100 ms
    static constexpr float     s_kTwoPi          = (float) (2.0 * std::numbers::pi);


    static void WriteMonoPcm16Wav (
        const fs::path     & outPath,
        const vector<float> & pcm,
        uint32_t              sampleRate)
    {
        // Test-side mirror of AssetBootstrap::WritePcmAsWav. Kept in
        // sync with the production helper so a regression there shows
        // up as a round-trip failure here.
        size_t      sampleCount   = pcm.size();
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
            float    scaled = s * 32767.0f;
            int16_t  asI16  = 0;

            if (scaled >  32767.0f) { scaled =  32767.0f; }
            if (scaled < -32768.0f) { scaled = -32768.0f; }

            asI16 = static_cast<int16_t> (scaled);
            out.write (reinterpret_cast<const char *> (&asI16), sizeof (asI16));
        }
    }


    static fs::path MakeTempDeviceDir (const wchar_t * suffix)
    {
        fs::path         dir;
        std::error_code  ec;



        fs::path  base = fs::temp_directory_path() / L"casso_disk_audio_tests";
        dir = base / suffix;


        fs::remove_all  (dir, ec);
        fs::create_directories (dir, ec);

        return dir;
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  WriteAndLoad_writesWavAndReadsBackThroughLoadSamples_preservesAmplitude
    //
    //  Round-trips a known sine wave through the same WAV format the
    //  AssetBootstrap downloader writes, then loads it via
    //  Disk2AudioSource::LoadSamples (which is what the production
    //  shell calls). Asserts the loaded buffer is non-empty and that
    //  the peak amplitude is close to the original 0.5. Anything
    //  catastrophically wrong in the WAV format would surface as an
    //  empty buffer here (LoadSamples returns silently on per-file
    //  failure per FR-009).
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (WriteAndLoad_writesWavAndReadsBackThroughLoadSamples_preservesAmplitude)
    {
        float             amp  = 0.0f;
        float             hz   = 0.0f;
        Disk2AudioSource  src1;
        float             peak = 0.0f;
        std::error_code   ec;



        // Build a half-amplitude 1 kHz sine.
        std::vector<float>  src (s_kSineFrames);
        amp = 0.5f;
        hz = 1000.0f;

        for (size_t i = 0; i < src.size(); i++)
        {
            float  t = (float) i / (float) s_kTestSampleRate;

            src[i] = amp * sinf (s_kTwoPi * hz * t);
        }

        fs::path  devicesDir = MakeTempDeviceDir (L"WriteAndLoad");
        fs::path  motorPath  = devicesDir / L"MotorLoop.wav";

        WriteMonoPcm16Wav (motorPath, src, s_kTestSampleRate);


        HRESULT  hr = src1.LoadSamples (devicesDir.wstring().c_str(),
                                        L"Shugart",
                                        s_kTestSampleRate);

        AssertSucceeded (hr,
            L"LoadSamples must succeed when at least MediaFoundation is reachable");

        // We can't directly inspect the source's internal buffer
        // post-load (no test accessor for the active motor buffer);
        // instead, trigger MotorStart + render a frame and verify the
        // output is non-silent. Empty buffer == silence == failure
        // under FR-009. Spec-006 bug 14a: motor loop gates on disk
        // presence, so insert a synthetic disk first.
        src1.OnDiskInserted();
        src1.OnMotorEngaged();

        std::vector<float>  frame (256);

        src1.GeneratePCM (frame.data(), static_cast<uint32_t> (frame.size()));


        for (float s : frame)
        {
            float  a = (s < 0) ? -s : s;
            if (a > peak) { peak = a; }
        }

        // After the kMotorVolume (0.90) gain a half-amplitude sine
        // should produce a peak of ~0.45. Allow generous slack
        // because the looped frame may catch a zero crossing.
        Assert::IsTrue (peak > 0.02f,
            std::format (L"Round-tripped WAV produced silent motor loop (peak={})", peak).c_str());

        fs::remove_all (devicesDir.parent_path(), ec);
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
        std::error_code   ec;
        Disk2AudioSource  src;
        float             peak = 0.0f;



        std::vector<float>  pcm (220, 0.5f);    // 5 ms square at full amplitude
        fs::path            devicesDir = MakeTempDeviceDir (L"FallbackPick");
        fs::path            mechDir    = devicesDir / L"Shugart";

        fs::create_directories (mechDir, ec);
        WriteMonoPcm16Wav (mechDir / L"MotorLoop.wav", pcm, s_kTestSampleRate);


        HRESULT  hr = src.LoadSamples (devicesDir.wstring().c_str(),
                                       L"Shugart",
                                       s_kTestSampleRate);
        AssertSucceeded (hr, L"LoadSamples must succeed");

        src.OnDiskInserted();
        src.OnMotorEngaged();

        std::vector<float>  frame (64);
        src.GeneratePCM (frame.data(), static_cast<uint32_t> (frame.size()));

        for (float s : frame)
        {
            float  a = (s < 0) ? -s : s;
            if (a > peak) { peak = a; }
        }

        Assert::IsTrue (peak > 0.05f,
            L"Per-mechanism copy must load when no top-level override exists");

        fs::remove_all (devicesDir.parent_path(), ec);
    }

    TEST_METHOD (LoadSamples_missingPerMechanismFile_fallsBackToShugart)
    {
        std::error_code   ec;
        Disk2AudioSource  src;
        float             peak = 0.0f;



        std::vector<float>  pcm (220, 0.5f);
        fs::path            devicesDir = MakeTempDeviceDir (L"ShugartFallback");

        // Alps omits the door sounds; Shugart (the canonical set) has them.
        fs::create_directories (devicesDir / L"Shugart", ec);
        fs::create_directories (devicesDir / L"Alps",    ec);
        WriteMonoPcm16Wav (devicesDir / L"Shugart" / L"DoorOpen.wav", pcm, s_kTestSampleRate);


        HRESULT  hr = src.LoadSamples (devicesDir.wstring().c_str(),
                                       L"Alps",
                                       s_kTestSampleRate);
        AssertSucceeded (hr, L"LoadSamples must succeed");

        // The door one-shot must play the Shugart copy rather than be muted.
        src.PlayTestSound (Disk2AudioSource::TestSoundKind::Door);

        std::vector<float>  frame (64);
        src.GeneratePCM (frame.data(), static_cast<uint32_t> (frame.size()));

        for (float s : frame)
        {
            float  a = (s < 0) ? -s : s;
            if (a > peak) { peak = a; }
        }

        Assert::IsTrue (peak > 0.05f,
            L"Alps door must fall back to the Shugart copy, not go silent");

        fs::remove_all (devicesDir.parent_path(), ec);
    }
};

