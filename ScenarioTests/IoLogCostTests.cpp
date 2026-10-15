#include "Pch.h"

#include "TestMachine.h"
#include "Shell/DebugEventLogs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace IoLogCostTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  IoLogCostTests
    //
    //  What the always-on video and sound logs cost emulation: a //e runs a
    //  loop of I/O accesses for a fixed number of cycles with the event logs
    //  on its History slot and with none, alternating rounds, and the fastest
    //  round of each is compared, since the host's other work only ever adds
    //  time to a round.
    //
    //  The keyboard poll is what an idle prompt does, and the bus passes none
    //  of it to the logs. Each speaker toggle is an entry, about fourteen
    //  nanoseconds of host time. The tone loop toggles the speaker every
    //  hundred cycles or so, as a program playing a note does, and slows by
    //  one to three percent at maximum speed. The speaker loop toggles it
    //  every nine cycles, as fast as a 6502 can, and slows by about a sixth;
    //  it is bounded to catch that growing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (IoLogCostTests)
    {
    public:

        static constexpr uint64_t  kCycles = 20'000'000;
        static constexpr int       kRounds = 11;
        static constexpr Word      kLoop   = 0x0300;

        //  The slowdowns allowed with the logs on: the noise of a run for the
        //  poll and a little over it for the tone, and the worst case's own
        //  bound.
        static constexpr double  kQuietAllowed = 0.04;
        static constexpr double  kWorstAllowed = 0.25;


        //  Milliseconds to run kCycles from the loop's start.
        static double  TimeRun (TestMachine & machine)
        {
            std::chrono::steady_clock::time_point  began;
            uint64_t                               spent = 0;



            machine.GetCpu()->SetPC (kLoop);

            began = std::chrono::steady_clock::now();

            while (spent < kCycles)
            {
                spent += machine.StepOne();
            }

            return std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - began).count();
        }


        //  The slowdown with the logs on, as a fraction of the time with none.
        static double  MeasureSlowdown (std::initializer_list<Byte> loop, const char * name)
        {
            TestMachine          machine  ("Apple2e");
            DebugEventLogs       logs     (machine);
            std::vector<double>  on;
            std::vector<double>  off;
            Word                 at       = kLoop;
            int                  round    = 0;
            double               slowdown = 0.0;



            machine.PowerCycle();

            for (Byte b : loop)
            {
                machine.GetMemoryBus().WriteByte (at++, b);
            }

            (void) TimeRun (machine);

            for (round = 0; round < kRounds; round++)
            {
                logs.Start (nullptr);
                on.push_back (TimeRun (machine));
                logs.Stop();

                off.push_back (TimeRun (machine));
            }

            std::ranges::sort (on);
            std::ranges::sort (off);

            slowdown = on.front() / off.front() - 1.0;

            Logger::WriteMessage (std::format ("{}: {:.1f} ms with the logs, {:.1f} ms without, fastest of {}: {:+.1f}%\n",
                                               name, on.front(), off.front(), kRounds, slowdown * 100.0).c_str());

            return slowdown;
        }


        static void  AssertWithin (double slowdown, double allowed)
        {
#ifdef NDEBUG
            Assert::IsTrue (slowdown <= allowed, std::format (L"{:+.1f}%", slowdown * 100.0).c_str());
#else
            UNREFERENCED_PARAMETER (slowdown);
            UNREFERENCED_PARAMETER (allowed);
            Logger::WriteMessage ("Debug build: the bound is checked in Release only\n");
#endif
        }


        //  $0300: LDA $C000 / BPL $0300 / JMP $0300
        TEST_METHOD (TheKeyboardPollCostsNothingMeasurable)
        {
            AssertWithin (MeasureSlowdown ({ 0xAD, 0x00, 0xC0, 0x10, 0xFB, 0x4C, 0x00, 0x03 }, "keyboard poll"), kQuietAllowed);
        }


        //  $0300: LDA $C030 / LDY #$14 / DEY / BNE $0305 / JMP $0300
        TEST_METHOD (ANoteCostsNothingMeasurable)
        {
            AssertWithin (MeasureSlowdown ({ 0xAD, 0x30, 0xC0, 0xA0, 0x14, 0x88, 0xD0, 0xFD, 0x4C, 0x00, 0x03 }, "tone loop"), kQuietAllowed);
        }


        //  $0300: LDA $C030 / DEX / BNE $0300 / JMP $0300
        TEST_METHOD (TheFastestSpeakerLoopStaysWithinItsBound)
        {
            AssertWithin (MeasureSlowdown ({ 0xAD, 0x30, 0xC0, 0xCA, 0xD0, 0xFA, 0x4C, 0x00, 0x03 }, "speaker loop"), kWorstAllowed);
        }
    };
}
