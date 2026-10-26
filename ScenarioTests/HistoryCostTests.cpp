#include "Pch.h"

#include "GuestSession.h"
#include "HResultAssert.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "Debugger/Reverse/ReverseController.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryCostTests
//
//  What recording history costs on ordinary software, as a share of one core
//  at 1x: two //e machines booted the same way from the DOS 3.3 master run
//  the same program, one with history on and one with it off, in
//  alternating rounds, and the difference in host time per emulated second
//  is the cost. Run pinned to one core, so the packing work on the thread
//  pool runs on the same core and is counted. Logged rather than asserted,
//  and measured in Release.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HistoryCostTests)
{
public:

    static constexpr uint64_t  kFramesPerSecond = 60;
    static constexpr uint64_t  kWarmSeconds     = 10;
    static constexpr uint64_t  kRoundSeconds    = 10;
    static constexpr uint64_t  kRounds          = 6;
    static constexpr double    kUsPerSecond     = 1e6;
    static constexpr double    kPercent         = 100.0;


    static void BootToPrompt (TestMachine & machine)
    {
        machine.GetDiskStore().SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });

        GuestSession::BootToPrompt (machine, GuestSession::RequireDos33Master());
    }


    static void RunSeconds (TestMachine & machine, uint64_t seconds)
    {
        uint64_t  frame = 0;



        for (frame = 0; frame < seconds * kFramesPerSecond; frame++)
        {
            machine.SampleHostInputs();
            machine.RunCycles (KeyframeSettings::kFrameCycles);
        }
    }


    static double TimeSeconds (TestMachine & machine, ReverseController * controller, uint64_t seconds)
    {
        LARGE_INTEGER  frequency = {};
        LARGE_INTEGER  start     = {};
        LARGE_INTEGER  stop      = {};
        HRESULT        hr        = S_OK;



        QueryPerformanceFrequency (&frequency);
        QueryPerformanceCounter   (&start);

        RunSeconds (machine, seconds);

        if (controller != nullptr)
        {
            hr = controller->GetKeyframes().WaitForPending();
            AssertSucceeded (hr, L"WaitForPending");
        }

        QueryPerformanceCounter (&stop);

        return static_cast<double> (stop.QuadPart - start.QuadPart) * kUsPerSecond / static_cast<double> (frequency.QuadPart);
    }


    //  Boots both machines, types the same program into each, warms them up,
    //  then times them in alternating rounds and logs the cost.
    static void Measure (const char * label, const std::vector<std::string> & lines)
    {
        TestMachine        plain      ("Apple2eEnhanced");
        TestMachine        recorded   ("Apple2eEnhanced");
        ReverseController  controller (recorded);
        HRESULT            hr         = S_OK;
        double             plainUs    = 0;
        double             recordedUs = 0;
        double             seconds    = static_cast<double> (kRounds * kRoundSeconds);
        double             plainPer   = 0;
        double             recordPer  = 0;
        uint64_t           round      = 0;



        BootToPrompt (plain);
        BootToPrompt (recorded);

        for (const std::string & line : lines)
        {
            KeystrokeInjector::InjectLine (plain,    line);
            KeystrokeInjector::InjectLine (recorded, line);
        }

        RunSeconds (plain,    kWarmSeconds);
        RunSeconds (recorded, kWarmSeconds);

        hr = controller.Start (ReverseSettings());
        AssertSucceeded (hr, L"Start");

        hr = controller.GetKeyframes().WaitForPending();
        AssertSucceeded (hr, L"WaitForPending after the first keyframe");

        for (round = 0; round < kRounds; round++)
        {
            plainUs    += TimeSeconds (plain,    nullptr,     kRoundSeconds);
            recordedUs += TimeSeconds (recorded, &controller, kRoundSeconds);
        }

        Assert::AreEqual<uint64_t> (plain.GetPosition(), recorded.GetPosition(), L"both ran the same instructions");

        plainPer  = plainUs    / seconds;
        recordPer = recordedUs / seconds;

        Logger::WriteMessage (std::format ("{}: without history {:.0f} us per emulated second ({:.1f}x real time, {:.2f}% of a core at 1x); recording {:.0f} us ({:.1f}x, {:.2f}%); cost {:.3f}% of a core at 1x; {} keyframes in {} bytes\n",
                                           label,
                                           plainPer,
                                           kUsPerSecond / plainPer,
                                           plainPer * kPercent / kUsPerSecond,
                                           recordPer,
                                           kUsPerSecond / recordPer,
                                           recordPer * kPercent / kUsPerSecond,
                                           (recordPer - plainPer) * kPercent / kUsPerSecond,
                                           controller.GetKeyframes().GetCount(),
                                           controller.GetKeyframes().GetByteCount()).c_str());
    }


    TEST_METHOD (TheDosPromptWaitingForAKey)
    {
        Measure ("DOS prompt", {});
    }


    TEST_METHOD (AnApplesoftLoResGameLoop)
    {
        Measure ("Applesoft lo-res game loop",
                 {
                     "10 GR : X = 5 : Y = 9 : DX = 1 : DY = 1",
                     "20 COLOR= 0 : PLOT X,Y : X = X + DX : Y = Y + DY",
                     "30 IF X < 1 OR X > 38 THEN DX = - DX",
                     "40 IF Y < 1 OR Y > 38 THEN DY = - DY",
                     "50 COLOR= 15 : PLOT X,Y : K = PEEK (- 16384) : GOTO 20",
                     "RUN",
                 });
    }


    TEST_METHOD (AnApplesoftProgramPlottingHiRes)
    {
        Measure ("Applesoft hi-res plotting",
                 {
                     "10 HGR : HCOLOR= 3",
                     "20 HPLOT RND (1) * 279, RND (1) * 159 : GOTO 20",
                     "RUN",
                 });
    }


    //  VERIFY reads the whole file on every pass. CATALOG is not used: it
    //  pauses at the first full screen until a key is pressed.
    TEST_METHOD (AnApplesoftLoopReadingTheDisk)
    {
        Measure ("Applesoft VERIFY loop",
                 {
                     "10 PRINT CHR$ (4);\"VERIFY HELLO\" : GOTO 10",
                     "RUN",
                 });
    }
};
