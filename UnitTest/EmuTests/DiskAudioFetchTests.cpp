#include "Pch.h"
#include "Machines/Apple2/Common/Disk2AudioSource.h"
#include "External/StbVorbisWrapper.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAudioFetchTests
//
//  Unit-test coverage for the spec/005-disk-ii-audio Phase 13
//  bootstrap pipeline: the OGG decoder wrapper in CassoEmuCore. The WAV
//  round trip through Disk2AudioSource::LoadSamples writes real files, so it
//  is in the scenario suite's DiskAudioFetchTests. The Win32-side AssetBootstrap
//  glue (FetchAndDecodeOgg / WritePcmAsWav / CheckAndFetchDiskAudio)
//  ships inside Casso.exe rather than a static library, so its
//  network-touching paths are covered by the manual integration
//  test described in T138 instead of here -- the constitution
//  forbids tests that hit the network or system state.
//
////////////////////////////////////////////////////////////////////////////////






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
        Assert::IsTrue (pcm.empty(), L"PCM buffer must be cleared on failure");
        Assert::IsFalse (err.empty(), L"Error string must be set on failure");
    }


    TEST_METHOD (DecodeOggToInterleavedShort_garbageBytes_returnsFailureNoCrash)
    {
        std::vector<int16_t> pcm;
        uint32_t             rate     = 0;
        uint32_t             channels = 0;
        std::string          err;



        // 64 bytes of pseudo-random non-OGG junk. stb_vorbis must
        // reject this cleanly and report an error rather than crash
        // the test DLL (FR-009 graceful degradation: a corrupt
        // upstream OGG must never crash the emulator).
        std::vector<uint8_t> junk (64);

        for (size_t i = 0; i < junk.size(); i++)
        {
            junk[i] = static_cast<uint8_t> (i * 13 + 7);
        }


        HRESULT  hr = StbVorbisWrapper::DecodeOggToInterleavedShort (
            junk.data(), junk.size(), pcm, rate, channels, err);

        AssertFailed (hr,
            L"Garbage input must return a failure HRESULT");
        Assert::IsTrue (pcm.empty(), L"PCM buffer must be empty on failure");
        Assert::IsFalse (err.empty(), L"Error string must be set on failure");
    }
};
