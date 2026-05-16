#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Audio/DiskIIAudioSource.h"
#include "Audio/DriveAudioMixer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveAudioMixerMechanismTests
//
//  Spec 005-disk-ii-audio Phase 14 (T143). Validates the runtime
//  mechanism dropdown's underlying state machine: SetMechanism must
//  reload every registered Disk II source through the cached asset
//  context, and bad input must not mutate state (SC-010 "no
//  state-change on validation failure").
//
//  The tests write tiny known-amplitude WAVs into a temp
//  Devices/DiskII/<Mechanism>/ tree (constitution \xA7II allows
//  tempdir use), then call SetMechanism and verify the active
//  motor-loop buffer reflects the new mechanism via the source's
//  GeneratePCM peak amplitude.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr uint32_t  s_kTestSampleRate = 44100;


    static void WriteSquareWav (const fs::path & path, float amplitude, size_t frames)
    {
        std::vector<float>  pcm (frames);

        for (size_t i = 0; i < frames; i++)
        {
            pcm[i] = ((i & 1) == 0) ? amplitude : -amplitude;
        }

        uint32_t   sampleRate    = s_kTestSampleRate;
        uint32_t   dataBytes     = static_cast<uint32_t> (pcm.size () * sizeof (int16_t));
        uint32_t   fileSize      = 36 + dataBytes;
        uint16_t   numChannels   = 1;
        uint16_t   bitsPerSample = 16;
        uint32_t   byteRate      = sampleRate * numChannels * (bitsPerSample / 8);
        uint16_t   blockAlign    = numChannels * (bitsPerSample / 8);
        uint16_t   formatPcm     = 1;
        uint32_t   fmtSize       = 16;
        std::error_code ec;

        fs::create_directories (path.parent_path (), ec);

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
            float  scaled = s * 32767.0f;
            int16_t  asI16 = static_cast<int16_t> (scaled);
            out.write (reinterpret_cast<const char *> (&asI16), sizeof (asI16));
        }
    }


    static fs::path MakeDevicesDir (const wchar_t * suffix)
    {
        fs::path  base = fs::temp_directory_path () / L"casso_mech_tests";
        fs::path  dir  = base / suffix;
        std::error_code  ec;

        fs::remove_all  (dir, ec);
        fs::create_directories (dir, ec);

        return dir;
    }


    static float SamplePeak (DiskIIAudioSource & src)
    {
        src.OnMotorStart ();

        std::vector<float>  frame (128);
        src.GeneratePCM (frame.data (), static_cast<uint32_t> (frame.size ()));

        float  peak = 0.0f;
        for (float s : frame)
        {
            float  a = (s < 0) ? -s : s;
            if (a > peak) { peak = a; }
        }

        src.OnMotorStop ();
        return peak;
    }
}





TEST_CLASS (DriveAudioMixerMechanismTests)
{
public:

    TEST_METHOD (SetMechanism_invalidName_returnsFailureNoStateChange)
    {
        DriveAudioMixer  mixer;

        Assert::AreEqual (std::wstring (L"Shugart"), std::wstring (mixer.GetMechanism ()),
            L"Default mechanism must be Shugart (SA400)");

        HRESULT  hr = mixer.SetMechanism (L"Sony");

        Assert::AreEqual (HRESULT (E_INVALIDARG), hr,
            L"Bogus mechanism name must return E_INVALIDARG");
        Assert::AreEqual (std::wstring (L"Shugart"), std::wstring (mixer.GetMechanism ()),
            L"Mechanism must not change on invalid input (SC-010)");
    }


    TEST_METHOD (SetMechanism_callsLoadSamplesOnAllRegisteredSources)
    {
        fs::path  devicesDir = MakeDevicesDir (L"AllSources");

        // Two distinct amplitudes per mechanism so the source's
        // active buffer is uniquely identifiable post-load.
        WriteSquareWav (devicesDir / L"Shugart" / L"MotorLoop.wav", 0.10f, 256);
        WriteSquareWav (devicesDir / L"Alps"    / L"MotorLoop.wav", 0.80f, 256);

        DiskIIAudioSource  srcA;
        DiskIIAudioSource  srcB;
        DriveAudioMixer    mixer;

        mixer.RegisterSource (&srcA);
        mixer.RegisterSource (&srcB);

        mixer.SetSampleLoadContext (devicesDir.wstring (), s_kTestSampleRate);

        HRESULT  hr = mixer.SetMechanism (L"Alps");
        Assert::IsTrue (SUCCEEDED (hr), L"SetMechanism(Alps) must succeed with valid context");

        float  peakA = SamplePeak (srcA);
        float  peakB = SamplePeak (srcB);

        // 0.80 amplitude * kMotorVolume (0.25) = 0.20 nominal; allow
        // generous slack for the float<->int16 round trip.
        Assert::IsTrue (peakA > 0.10f,
            std::format (L"srcA must be at Alps amplitude post-reload (peak={})", peakA).c_str ());
        Assert::IsTrue (peakB > 0.10f,
            std::format (L"srcB must be at Alps amplitude post-reload (peak={})", peakB).c_str ());

        std::error_code  ec;
        fs::remove_all (devicesDir.parent_path (), ec);
    }


    TEST_METHOD (SetMechanism_alpsToShugart_changesActiveBufferSet)
    {
        fs::path  devicesDir = MakeDevicesDir (L"SwapBack");

        WriteSquareWav (devicesDir / L"Shugart" / L"MotorLoop.wav", 0.80f, 256);
        WriteSquareWav (devicesDir / L"Alps"    / L"MotorLoop.wav", 0.10f, 256);

        DiskIIAudioSource  src;
        DriveAudioMixer    mixer;

        mixer.RegisterSource (&src);
        mixer.SetSampleLoadContext (devicesDir.wstring (), s_kTestSampleRate);

        HRESULT  hr = mixer.SetMechanism (L"Alps");
        Assert::IsTrue (SUCCEEDED (hr), L"Initial SetMechanism(Alps) must succeed");

        float  alpsPeak = SamplePeak (src);

        hr = mixer.SetMechanism (L"Shugart");
        Assert::IsTrue (SUCCEEDED (hr), L"Follow-up SetMechanism(Shugart) must succeed");

        float  shugartPeak = SamplePeak (src);

        // Shugart was written at 8x Alps' amplitude; verify the
        // active buffer actually swapped (the loop position resets
        // on Re-LoadSamples through the move-assignment of m_motorBuf
        // so the comparison is fair).
        Assert::IsTrue (shugartPeak > alpsPeak * 2.0f,
            std::format (L"Active mechanism's louder buffer must dominate (alps={}, shugart={})",
                         alpsPeak, shugartPeak).c_str ());

        std::error_code  ec;
        fs::remove_all (devicesDir.parent_path (), ec);
    }


    TEST_METHOD (SetMechanism_beforeSetSampleLoadContext_recordsButDoesNotLoad)
    {
        // The shell calls SetMechanism on the CPU thread before
        // SetSampleLoadContext when WASAPI is uninitialised. That
        // path must not crash and must record the chosen mechanism
        // so the eventual first load uses it.
        DriveAudioMixer  mixer;

        HRESULT  hr = mixer.SetMechanism (L"Alps");

        Assert::AreEqual (HRESULT (S_OK), hr,
            L"Pre-context SetMechanism must return S_OK without loading");
        Assert::AreEqual (std::wstring (L"Alps"), std::wstring (mixer.GetMechanism ()),
            L"Mechanism state must persist for the eventual first load");
    }
};
