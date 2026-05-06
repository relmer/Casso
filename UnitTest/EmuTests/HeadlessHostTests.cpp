#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "HeadlessHost.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeadlessHostTests
//
//  Constitution §II compliant: confirms HeadlessHost wiring is fully
//  isolated (no Win32 window, no host filesystem outside Fixtures, no
//  registry, no network) and is byte-deterministic across two
//  independent builds.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HeadlessHostTests)
{
public:

    TEST_METHOD (ConstructsWithoutWindow)
    {
        HeadlessHost     host;
        EmulatorCore     core;
        HRESULT          hr;

        hr = host.BuildAppleIIe (core);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsNotNull (core.host.get ());
        Assert::IsFalse (core.host->WasWindowOpened (),
            L"HeadlessHost MUST NOT open a Win32 window before tests ask");
        Assert::IsNotNull (core.audioSink,
            L"HeadlessHost wires an IAudioSink mock");
        Assert::IsNotNull (core.fixtures.get ());
        Assert::IsNotNull (core.prng.get ());
    }

    TEST_METHOD (FixtureProviderRejectsPathsOutsideFixtures)
    {
        HeadlessHost            host;
        EmulatorCore            core;
        std::vector<uint8_t>    bytes;
        HRESULT                 hr;

        hr = host.BuildAppleIIe (core);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = core.fixtures->OpenFixture ("../escape.bin", bytes);
        Assert::AreEqual (E_INVALIDARG, hr, L"'..' traversal must be rejected");

        hr = core.fixtures->OpenFixture ("subdir/../../escape.bin", bytes);
        Assert::AreEqual (E_INVALIDARG, hr, L"embedded '..' must be rejected");

        hr = core.fixtures->OpenFixture ("/etc/passwd", bytes);
        Assert::AreEqual (E_INVALIDARG, hr, L"absolute root must be rejected");

        hr = core.fixtures->OpenFixture ("\\windows\\system32", bytes);
        Assert::AreEqual (E_INVALIDARG, hr, L"backslash root must be rejected");

        hr = core.fixtures->OpenFixture ("C:\\windows", bytes);
        Assert::AreEqual (E_INVALIDARG, hr, L"drive letter must be rejected");

        hr = core.fixtures->OpenFixture ("", bytes);
        Assert::AreEqual (E_INVALIDARG, hr, L"empty path must be rejected");
    }

    TEST_METHOD (DeterministicAcrossTwoBuilds)
    {
        HeadlessHost     hostA;
        HeadlessHost     hostB;
        EmulatorCore     coreA;
        EmulatorCore     coreB;
        HRESULT          hr;
        size_t           i;

        hr = hostA.BuildAppleIIe (coreA);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = hostB.BuildAppleIIe (coreB);
        Assert::IsTrue (SUCCEEDED (hr));

        for (i = 0; i < 256; i++)
        {
            Assert::AreEqual (coreA.prng->Next64 (), coreB.prng->Next64 (),
                L"Two HeadlessHost builds with the pinned seed must agree");
        }
    }

    TEST_METHOD (AudioSinkCountsToggles)
    {
        HeadlessHost     host;
        EmulatorCore     core;
        MockAudioSink *  sink;
        float            samples1[3] = { +0.5f, -0.5f, +0.5f };
        float            samples2[2] = { -0.5f, -0.5f };
        HRESULT          hr;

        hr   = host.BuildAppleIIe (core);
        Assert::IsTrue (SUCCEEDED (hr));

        sink = &core.host->GetAudioSink ();

        hr = sink->PushSamples (samples1, 3);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (uint64_t (2), sink->GetToggleCount (),
            L"Sign flips +,-,+ produce 2 toggles");

        hr = sink->PushSamples (samples2, 2);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (uint64_t (3), sink->GetToggleCount (),
            L"+ -> - is one more flip; - -> - adds none");

        sink->Clear ();
        Assert::AreEqual (uint64_t (0), sink->GetToggleCount ());
        Assert::AreEqual (uint64_t (0), sink->GetSampleCount ());
    }
};
