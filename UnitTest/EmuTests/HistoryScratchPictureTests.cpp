#include "Pch.h"

#include "TestMachine.h"
#include "HResultAssert.h"
#include "EmuTests/ReverseSessionRig.h"
#include "Core/StateWriter.h"
#include "Shell/ScratchMachineRenderer.h"
#include "Video/MachineFrameRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kPictureWarmupCycles = 200000;
static constexpr Word      s_kTextPage            = 0x0400;
static constexpr int       s_kTimedRenders        = 20;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryScratchPictureTests
//
//  A snapshot's screen drawn on a second machine of its own: the picture is
//  the one the running machine draws, though the scratch machine holds none
//  of its disks, and a snapshot of another machine is refused rather than
//  drawn wrong.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HistoryScratchPictureTests)
{
public:

    TEST_METHOD (TheScratchMachineDrawsWhatTheMachineDrew)
    {
        TestMachine             machine ("Apple2e");
        ScratchMachineRenderer  scratch;
        std::vector<Byte>       state;
        std::vector<uint32_t>   expected;
        std::vector<uint32_t>   actual;
        int                     width   = 0;
        int                     height  = 0;
        HRESULT                 hr      = S_OK;



        Snapshot (machine, state);

        hr = MachineFrameRenderer::Render (machine, machine.GetBuilder(), true, expected);
        AssertSucceeded (hr, L"the machine's own picture");

        Assert::IsTrue (CountLit (expected) > 0, L"the picture has text on it, or the comparison proves nothing");

        scratch.SetMachine (machine.GetConfig(), machine.GetCurrentMachineName());

        hr = scratch.Render (state, actual, width, height);
        AssertSucceeded (hr, L"the scratch machine's picture");

        Assert::AreEqual (MachineFrameRenderer::kWidth,  width,  L"width");
        Assert::AreEqual (MachineFrameRenderer::kHeight, height, L"height");
        Assert::IsTrue (actual == expected, L"the same picture, pixel for pixel");
    }


    TEST_METHOD (ASnapshotOfAnotherMachineIsRefused)
    {
        TestMachine             machine ("Apple2e");
        TestMachine             other   ("Apple2Plus");
        ScratchMachineRenderer  scratch;
        std::vector<Byte>       state;
        std::vector<uint32_t>   actual;
        int                     width   = 0;
        int                     height  = 0;
        HRESULT                 hr      = S_OK;



        Snapshot (machine, state);

        scratch.SetMachine (other.GetConfig(), other.GetCurrentMachineName());

        hr = scratch.Render (state, actual, width, height);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, L"another machine's state is not drawn");
        Assert::AreEqual (0, width, L"and no picture is claimed");
    }


    //  The renderer's scratch machine belongs to the thread drawing with it,
    //  so the next picture on another thread takes its disks over rather than
    //  touching a store the last thread still holds.
    TEST_METHOD (ARendererReusedOnAnotherThreadTakesItsDisksWithIt)
    {
        TestMachine             machine  ("Apple2e");
        ScratchMachineRenderer  scratch;
        std::vector<Byte>       state;
        std::vector<uint32_t>   first;
        std::vector<uint32_t>   second;
        int                     width    = 0;
        int                     height   = 0;
        HRESULT                 hr       = S_OK;
        HRESULT                 hrWorker = E_FAIL;



        Snapshot (machine, state);

        scratch.SetMachine (machine.GetConfig(), machine.GetCurrentMachineName());

        {
            std::thread  worker ([&] { hrWorker = scratch.Render (state, first, width, height); });

            worker.join();
        }

        AssertSucceeded (hrWorker, L"Render on the worker thread");

        hr = scratch.Render (state, second, width, height);
        AssertSucceeded (hr, L"Render on the test thread");
    }


    //  Not a pass or fail: what one picture costs, and so what a strip of
    //  them costs, written to the test log.
    TEST_METHOD (OnePictureCostIsMeasured)
    {
        TestMachine             machine ("Apple2e");
        ScratchMachineRenderer  scratch;
        std::vector<Byte>       state;
        std::vector<uint32_t>   actual;
        int                     width   = 0;
        int                     height  = 0;
        HRESULT                 hr      = S_OK;
        LARGE_INTEGER           start   = {};
        LARGE_INTEGER           end     = {};
        LARGE_INTEGER           freq    = {};
        double                  firstMs = 0.0;
        double                  eachMs  = 0.0;
        int                     i       = 0;



        Snapshot (machine, state);
        scratch.SetMachine (machine.GetConfig(), machine.GetCurrentMachineName());

        QueryPerformanceFrequency (&freq);
        QueryPerformanceCounter   (&start);

        hr = scratch.Render (state, actual, width, height);
        AssertSucceeded (hr, L"the first picture, building the scratch machine");

        QueryPerformanceCounter (&end);
        firstMs = (double) (end.QuadPart - start.QuadPart) * 1000.0 / (double) freq.QuadPart;

        QueryPerformanceCounter (&start);

        for (i = 0; i < s_kTimedRenders; i++)
        {
            hr = scratch.Render (state, actual, width, height);
            AssertSucceeded (hr, L"a later picture");
        }

        QueryPerformanceCounter (&end);
        eachMs = (double) (end.QuadPart - start.QuadPart) * 1000.0 / (double) freq.QuadPart / s_kTimedRenders;

        Logger::WriteMessage (std::format ("history picture: first {:.2f} ms (builds the scratch machine), then {:.3f} ms each; "
                                           "a strip of 20 at 8 a second refreshes in 2.5 s\n", firstMs, eachMs).c_str());
    }

private:

    //  The rig's machine, run a while, with text on its 40-column screen.
    static void Snapshot (TestMachine & machine, std::vector<Byte> & outState)
    {
        constexpr Byte  kInverseA = 0x01;
        constexpr int   kLetters  = 26;

        StateWriter  writer;
        HRESULT      hr = S_OK;
        int          i  = 0;



        ReverseSessionRig::Prepare (machine);
        machine.RunCycles (s_kPictureWarmupCycles);

        for (i = 0; i < kLetters; i++)
        {
            machine.GetMemoryBus().WriteByte (static_cast<Word> (s_kTextPage + i), static_cast<Byte> (kInverseA + i));
        }

        hr = machine.SaveState (writer);
        AssertSucceeded (hr, L"SaveState");

        outState = writer.GetBytes();
    }


    static size_t CountLit (const std::vector<uint32_t> & pixels)
    {
        constexpr uint32_t  kRgb = 0x00FFFFFFu;

        return (size_t) std::count_if (pixels.begin(), pixels.end(), [] (uint32_t pixel) { return (pixel & kRgb) != 0; });
    }
};
