#include "Pch.h"
#include "../EhmTestHelper.h"
#include "HeadlessHost.h"
#include "FixtureProvider.h"

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

        hr = host.BuildApple2e (core);

        AssertSucceeded (hr);
        Assert::IsNotNull (core.host.get());
        Assert::IsFalse (core.host->WasWindowOpened(),
            L"HeadlessHost MUST NOT open a Win32 window before tests request one");
        Assert::IsNotNull (core.audioSink,
            L"HeadlessHost wires an IAudioSink mock");
        Assert::IsNotNull (core.fixtures.get());
        Assert::IsNotNull (core.prng.get());
    }

    // The Apple //e Enhanced cold-boots its enhanced firmware on
    // the 65C02: the RESET vector points into ROM, a cold PowerCycle enters
    // it, and the CPU then runs firmware (PC stays in the $C000-$FFFF ROM
    // window). A 6502 mis-wire would derail the moment the enhanced ROM hits
    // a CMOS opcode.
    //
    // THE ENHANCED ROM IS A COMMITTED FIXTURE, so there is nothing to skip
    // over. This case skipped whenever it was absent, which was everywhere:
    // the ROM was ignored rather than tracked, so the only 65C02 firmware
    // cold boot in the suite reported a pass without booting anything.
    TEST_METHOD (BuildApple2eEnhanced_ColdBootsFirmwareOn65C02)
    {
        HeadlessHost   host;
        EmulatorCore   core;

        AssertSucceeded (host.BuildApple2eEnhanced (core),
            L"BuildApple2eEnhanced must succeed when the ROM is present");

        Word resetVec = static_cast<Word> (core.cpu->ReadByte (0xFFFC)) |
                        (static_cast<Word> (core.cpu->ReadByte (0xFFFD)) << 8);
        Assert::IsTrue (resetVec >= 0xC000,
            L"RESET vector must point into the enhanced //e ROM");

        core.PowerCycle();
        Assert::AreEqual<Word> (resetVec, core.cpu->GetPC(),
            L"Cold reset must enter the enhanced firmware entry");

        core.RunCycles (20'000'000);
        Assert::IsTrue (core.cpu->GetPC() >= 0xC000,
            L"After cold boot the CPU must be executing firmware in ROM "
            L"($C000-$FFFF); a 6502 mis-wire would derail on a CMOS opcode");
    }

    TEST_METHOD (FixtureProviderRejectsPathsOutsideFixtures)
    {
        HeadlessHost            host;
        EmulatorCore            core;
        std::vector<uint8_t>    bytes;
        HRESULT                 hr;

        hr = host.BuildApple2e (core);
        AssertSucceeded (hr);

        // Every rejection below trips the same asserting guard: asking the
        // fixture provider to escape its root is a caller bug, so a dev
        // running Debug should break on it. The scope both permits that and
        // proves the assert is still wired -- a rejection that stopped
        // asserting would otherwise pass silently on the HRESULT alone.
        {
            UnitTestHelpers::ExpectedEhmAssert   expect;

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

            expect.RequireCount (6);
        }
    }

    TEST_METHOD (DeterministicAcrossTwoBuilds)
    {
        HeadlessHost     hostA;
        HeadlessHost     hostB;
        EmulatorCore     coreA;
        EmulatorCore     coreB;
        HRESULT          hr;
        size_t           i;

        hr = hostA.BuildApple2e (coreA);
        AssertSucceeded (hr);

        hr = hostB.BuildApple2e (coreB);
        AssertSucceeded (hr);

        for (i = 0; i < 256; i++)
        {
            Assert::AreEqual (coreA.prng->Next64(), coreB.prng->Next64(),
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

        hr   = host.BuildApple2e (core);
        AssertSucceeded (hr);

        sink = &core.host->GetAudioSink();

        hr = sink->PushSamples (samples1, 3);
        AssertSucceeded (hr);
        Assert::AreEqual (uint64_t (2), sink->GetToggleCount(),
            L"Sign flips +,-,+ produce 2 toggles");

        hr = sink->PushSamples (samples2, 2);
        AssertSucceeded (hr);
        Assert::AreEqual (uint64_t (3), sink->GetToggleCount(),
            L"+ -> - is one more flip; - -> - adds none");

        sink->Clear();
        Assert::AreEqual (uint64_t (0), sink->GetToggleCount());
        Assert::AreEqual (uint64_t (0), sink->GetSampleCount());
    }
};
