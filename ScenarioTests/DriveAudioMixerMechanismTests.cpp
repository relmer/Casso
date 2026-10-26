#include "Pch.h"
#include "Machines/Apple2/Common/Disk2AudioSource.h"
#include "Audio/DriveAudioMixer.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveAudioMixerMechanismTests
//
//  SetMechanism against real WAVs: the tests write tiny known-amplitude
//  WAVs into a temp Devices/DiskII/<Mechanism>/ tree, then call
//  SetMechanism and verify the active motor-loop buffer reflects the new
//  mechanism through the source's GeneratePCM peak amplitude.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveAudioMixerMechanismTests)
{
public:

    static constexpr uint32_t  s_kTestSampleRate = 44100;


    static void WriteSquareWav (const fs::path & path, float amplitude, size_t frames)
    {
        uint32_t         sampleRate    = 0;
        uint32_t         dataBytes     = 0;
        uint32_t         fileSize      = 0;
        uint16_t         numChannels   = 0;
        uint16_t         bitsPerSample = 0;
        uint32_t         byteRate      = 0;
        uint16_t         blockAlign    = 0;
        uint16_t         formatPcm     = 0;
        uint32_t         fmtSize       = 0;
        std::error_code  ec;



        std::vector<float>  pcm (frames);

        for (size_t i = 0; i < frames; i++)
        {
            pcm[i] = ((i & 1) == 0) ? amplitude : -amplitude;
        }

        sampleRate = s_kTestSampleRate;
        dataBytes = static_cast<uint32_t> (pcm.size() * sizeof (int16_t));
        fileSize = 36 + dataBytes;
        numChannels = 1;
        bitsPerSample = 16;
        byteRate = sampleRate * numChannels * (bitsPerSample / 8);
        blockAlign = numChannels * (bitsPerSample / 8);
        formatPcm = 1;
        fmtSize = 16;

        fs::create_directories (path.parent_path(), ec);

        std::ofstream  out (path, std::ios::binary | std::ios::trunc);

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
            int16_t  asI16  = static_cast<int16_t> (scaled);
            out.write (reinterpret_cast<const char *> (&asI16), sizeof (asI16));
        }
    }


    static fs::path MakeDevicesDir (const wchar_t * suffix)
    {
        fs::path         dir;
        std::error_code  ec;



        fs::path  base = fs::temp_directory_path() / L"casso_mech_tests";
        dir = base / suffix;

        fs::remove_all  (dir, ec);
        fs::create_directories (dir, ec);

        return dir;
    }


    static float SamplePeak (Disk2AudioSource & src)
    {
        float  peak = 0.0f;



        // Spec-006 bug 14a: motor loop is gated on disk presence, so
        // tests that just want to measure the loaded motor buffer's
        // amplitude need to insert a (synthetic) disk first.
        src.OnDiskInserted();
        src.OnMotorEngaged();

        std::vector<float>  frame (128);
        src.GeneratePCM (frame.data(), static_cast<uint32_t> (frame.size()));

        for (float s : frame)
        {
            float  a = (s < 0) ? -s : s;
            if (a > peak) { peak = a; }
        }

        src.OnMotorDisengaged();
        src.OnDiskEjected();
        return peak;
    }


    TEST_METHOD (SetMechanism_callsLoadSamplesOnAllRegisteredSources)
    {
        Disk2AudioSource  srcA;
        Disk2AudioSource  srcB;
        DriveAudioMixer   mixer;
        float             peakA = 0.0f;
        float             peakB = 0.0f;
        std::error_code   ec;



        fs::path  devicesDir = MakeDevicesDir (L"AllSources");

        // Two distinct amplitudes per mechanism so the source's
        // active buffer is uniquely identifiable post-load.
        WriteSquareWav (devicesDir / L"Shugart" / L"MotorLoop.wav", 0.10f, 256);
        WriteSquareWav (devicesDir / L"Alps"    / L"MotorLoop.wav", 0.80f, 256);


        mixer.RegisterSource (&srcA);
        mixer.RegisterSource (&srcB);

        mixer.SetSampleLoadContext (devicesDir.wstring(), s_kTestSampleRate);

        HRESULT  hr = mixer.SetMechanism (L"Alps");
        AssertSucceeded (hr, L"SetMechanism(Alps) must succeed with valid context");

        peakA = SamplePeak (srcA);
        peakB = SamplePeak (srcB);

        // 0.80 amplitude * kMotorVolume (0.90) = 0.72 nominal; allow
        // generous slack for the float<->int16 round trip.
        Assert::IsTrue (peakA > 0.10f,
            std::format (L"srcA must be at Alps amplitude post-reload (peak={})", peakA).c_str());
        Assert::IsTrue (peakB > 0.10f,
            std::format (L"srcB must be at Alps amplitude post-reload (peak={})", peakB).c_str());

        fs::remove_all (devicesDir.parent_path(), ec);
    }


    TEST_METHOD (SetMechanism_alpsToShugart_changesActiveBufferSet)
    {
        Disk2AudioSource  src;
        DriveAudioMixer   mixer;
        float             alpsPeak    = 0.0f;
        float             shugartPeak = 0.0f;
        std::error_code   ec;



        fs::path  devicesDir = MakeDevicesDir (L"SwapBack");

        WriteSquareWav (devicesDir / L"Shugart" / L"MotorLoop.wav", 0.80f, 256);
        WriteSquareWav (devicesDir / L"Alps"    / L"MotorLoop.wav", 0.10f, 256);


        mixer.RegisterSource (&src);
        mixer.SetSampleLoadContext (devicesDir.wstring(), s_kTestSampleRate);

        HRESULT  hr = mixer.SetMechanism (L"Alps");
        AssertSucceeded (hr, L"Initial SetMechanism(Alps) must succeed");

        alpsPeak = SamplePeak (src);

        hr = mixer.SetMechanism (L"Shugart");
        AssertSucceeded (hr, L"Follow-up SetMechanism(Shugart) must succeed");

        shugartPeak = SamplePeak (src);

        // Shugart was written at 8x Alps' amplitude; verify the
        // active buffer actually swapped (the loop position resets
        // on Re-LoadSamples through the move-assignment of m_motorBuf
        // so the comparison is fair).
        Assert::IsTrue (shugartPeak > alpsPeak * 2.0f,
            std::format (L"Active mechanism's louder buffer must dominate (alps={}, shugart={})",
                         alpsPeak, shugartPeak).c_str());

        fs::remove_all (devicesDir.parent_path(), ec);
    }

};
