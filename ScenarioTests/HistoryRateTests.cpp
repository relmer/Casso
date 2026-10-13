#include "Pch.h"

#include "GuestSession.h"
#include "HResultAssert.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "Debugger/Reverse/ReverseController.h"
#include "Ui/Debugger/ReverseBudget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryRateTests
//
//  How fast history grows for three programs on a //e booted from the DOS 3.3
//  master, measured over emulated minutes after the first whole snapshot:
//  the DOS prompt waiting for a key, an Applesoft program plotting random
//  hi-res points, and a machine-language loop that rewrites the whole hi-res
//  page with varied bytes. ReverseBudget's estimate before a recording has
//  run long enough to measure its own rate uses the busiest and the idle
//  rates, so each is checked against the constant it came from.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HistoryRateTests)
{
public:

    static constexpr uint64_t  kFramesPerSecond = 60;
    static constexpr uint64_t  kWarmSeconds     = 10;
    static constexpr uint64_t  kSeconds         = 120;


    //  The growth of history over kSeconds of emulated time after
    //  kWarmSeconds, in bytes per emulated second.
    static double MeasureBytesPerSecond (TestMachine & machine)
    {
        ReverseController  controller (machine);
        HRESULT            hr         = S_OK;
        size_t             startBytes = 0;
        uint64_t           frame      = 0;



        hr = controller.Start (ReverseSettings());
        AssertSucceeded (hr, L"Start");

        for (frame = 0; frame < kWarmSeconds * kFramesPerSecond; frame++)
        {
            machine.SampleHostInputs();
            machine.RunCycles (KeyframeSettings::kFrameCycles);
        }

        hr = controller.GetKeyframes().WaitForPending();
        AssertSucceeded (hr, L"WaitForPending after the warm-up");

        startBytes = controller.GetKeyframes().GetByteCount();

        for (frame = 0; frame < kSeconds * kFramesPerSecond; frame++)
        {
            machine.SampleHostInputs();
            machine.RunCycles (KeyframeSettings::kFrameCycles);
        }

        hr = controller.GetKeyframes().WaitForPending();
        AssertSucceeded (hr, L"WaitForPending");

        return (double) (controller.GetKeyframes().GetByteCount() - startBytes) / (double) kSeconds;
    }


    static void BootToPrompt (TestMachine & machine)
    {
        machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });

        GuestSession::BootToPrompt (machine, GuestSession::RequireDos33Master());
    }


    TEST_METHOD (TheDosPromptWaitingForAKey)
    {
        TestMachine  machine ("Apple2eEnhanced");
        double       rate    = 0.0;



        BootToPrompt (machine);
        rate = MeasureBytesPerSecond (machine);

        Logger::WriteMessage (std::format ("idle at the prompt: {:.0f} bytes per second\n", rate).c_str());
        Assert::IsTrue (rate < ReverseBudget::kIdleBytesPerSecond * 2, std::format (L"{:.0f} bytes per second", rate).c_str());
    }


    TEST_METHOD (AnApplesoftProgramPlottingHiRes)
    {
        TestMachine  machine ("Apple2eEnhanced");
        double       rate    = 0.0;



        BootToPrompt (machine);

        KeystrokeInjector::InjectLine (machine, "10 HGR : HCOLOR= 3");
        KeystrokeInjector::InjectLine (machine, "20 HPLOT RND (1) * 279, RND (1) * 159 : GOTO 20");
        KeystrokeInjector::InjectLine (machine, "RUN");

        rate = MeasureBytesPerSecond (machine);

        Logger::WriteMessage (std::format ("Applesoft plotting: {:.0f} bytes per second\n", rate).c_str());
        Assert::IsTrue (rate > ReverseBudget::kIdleBytesPerSecond, std::format (L"{:.0f} bytes per second", rate).c_str());
        Assert::IsTrue (rate < ReverseBudget::kBusyBytesPerSecond, std::format (L"{:.0f} bytes per second", rate).c_str());
    }


    //  $300: point ($06) at $2000, then add the low byte of the address and
    //  the page to every byte from $2000 to $3FFF, over and over.
    TEST_METHOD (AMachineLanguageLoopRewritingTheHiResPage)
    {
        static constexpr Byte  kLoop[] =
        {
            0xA0, 0x00, 0x84, 0x06, 0xA2, 0x20, 0x86, 0x07, 0xEA, 0xEA, 0xEA, 0xEA,
            0x98, 0x71, 0x06, 0x65, 0x07, 0x91, 0x06, 0xC8, 0xD0, 0xF6,
            0xE6, 0x07, 0xA6, 0x07, 0xE0, 0x40, 0xD0, 0xEE, 0x4C, 0x00, 0x03,
        };
        TestMachine  machine ("Apple2eEnhanced");
        std::string  pokes;
        double       rate    = 0.0;
        size_t       i       = 0;



        BootToPrompt (machine);

        //  POKEs about a hundred characters to a line; a line twice that long
        //  did not run.
        for (i = 0; i < std::size (kLoop); i++)
        {
            pokes += std::format ("{}POKE {},{}", pokes.empty() ? "" : ":", 0x300 + i, kLoop[i]);

            if (pokes.size() > 100 || i + 1 == std::size (kLoop))
            {
                KeystrokeInjector::InjectLine (machine, pokes);
                pokes.clear();
            }
        }

        KeystrokeInjector::InjectLine (machine, "HGR : CALL 768");

        rate = MeasureBytesPerSecond (machine);

        Logger::WriteMessage (std::format ("hi-res page rewritten: {:.0f} bytes per second\n", rate).c_str());
        Assert::IsTrue (rate < ReverseBudget::kBusyBytesPerSecond * 2, std::format (L"{:.0f} bytes per second", rate).c_str());
        Assert::IsTrue (rate > ReverseBudget::kBusyBytesPerSecond / 2, std::format (L"{:.0f} bytes per second", rate).c_str());
    }
};
