#include "Pch.h"

#include "Debugger/Handlers/ExecutionHandlers.h"
#include "Debugger/Handlers/LogHandlers.h"
#include "HandlerTestRig.h"
#include "Shell/DebugEventLogs.h"
#include "Machines/Apple2/Common/VideoTiming.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LogCommandsTests
//
//  STOPWATCH, VIDEOLOG and SOUNDLOG against a real machine driven
//  synchronously.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (LogCommandsTests)
    {
    public:

        using LogRig       = MachineHandlerRig<LogHandlers>;
        using ExecutionRig = MachineHandlerRig<ExecutionHandlers>;

        static constexpr int  kIoPage = 0xC0;



        static void RunTo (LogRig & rig, Word stop)
        {
            rig.session.GetBreakpoints().AddAddress (stop, stop);
            rig.session.OnStopConditionsChanged();
            rig.RunOk ("G");
            Assert::AreEqual (stop, rig.LastStop().pc);
        }



        //  $0300: JSR $0310 / JSR $0310 / NOP    $0310: INX / INX / RTS
        TEST_METHOD (STOPWATCH_TimesEachCallFromEntryToReturn)
        {
            ExecutionRig              rig;
            std::vector<std::string>  lines;



            rig.session.SetInstructionObserver (&rig.handlers);
            rig.Load (0x0300, { 0x20, 0x10, 0x03, 0x20, 0x10, 0x03, 0xEA }, 0x0300);
            rig.Load (0x0310, { 0xE8, 0xE8, 0x60 }, 0x0300);

            lines = rig.RunOk ("STOPWATCH").text;
            Assert::AreEqual (std::string ("Stopwatch off."), lines.at (0));
            Assert::AreEqual (std::string ("No laps."),       lines.at (1));

            lines = rig.RunOk ("STOPWATCH 310 312").text;
            Assert::AreEqual (std::string ("Stopwatch from $0310 to $0312, in runs the debugger starts."), lines.at (0));

            rig.session.GetBreakpoints().AddAddress (0x0306, 0x0306);
            rig.session.OnStopConditionsChanged();
            rig.RunOk ("G");

            lines = rig.RunOk ("STOPWATCH").text;
            Assert::AreEqual (std::string ("2 laps: last 4 cycles, shortest 4, longest 4, average 4."), lines.at (1), L"two INX each time");

            lines = rig.RunOk ("STOPWATCH RESET").text;
            Assert::AreEqual (std::string ("No laps."), lines.at (1));
            Assert::AreEqual (std::string ("Stopwatch off."), rig.RunOk ("STOPWATCH OFF").text.at (0));

            rig.RunFails ("STOPWATCH 310 312 314", "invalid arguments");
            rig.RunFails ("STOPWATCH 10000",       "invalid arguments");
        }



        //  The rig's session with the shell's event logs as its video and
        //  sound logs, recording from the start, with no history.
        struct LoggedRig
        {
            LogRig          rig;
            DebugEventLogs  logs { rig.machine };

            explicit LoggedRig (TestMachine::Slots slots = TestMachine::Slots::Empty) :
                rig ("Apple2e", slots)
            {
                logs.Start (nullptr);
                rig.session.SetIoEventSource (&logs);
            }

            ~LoggedRig()
            {
                logs.Stop();
            }
        };



        //  $0300: STA $C050 / STA $C057 / STA $C054 / LDA $C051 / NOP
        TEST_METHOD (VIDEOLOG_ListsEachModeChangeWithItsBeamPosition)
        {
            LoggedRig                  logged;
            LogRig                   & rig     = logged.rig;
            MemoryBus                & bus     = rig.machine.GetMemoryBus();
            std::vector<std::string>   lines;
            size_t                     i       = 0;



            rig.Load (0x0300, { 0x8D, 0x50, 0xC0, 0x8D, 0x57, 0xC0, 0x8D, 0x54, 0xC0, 0xAD, 0x51, 0xC0, 0xEA }, 0x0300);

            RunTo (rig, 0x030C);

            const DebugEventCache<IoEvent>  & cache = logged.logs.GetVideoCache();

            Assert::IsFalse  (bus.IsPageWatched (kIoPage), L"the log takes nothing from the watched path");
            Assert::AreEqual ((size_t) 3, cache.GetCount(), L"PAGE1 was already on, so $C054 changed nothing");
            Assert::AreEqual ((Word) 0xC050, cache.GetAt (0).event.address);
            Assert::AreEqual ((int) IoEventLog::kText,  (int) cache.GetAt (0).event.turnedOff);
            Assert::AreEqual ((int) IoEventLog::kHires, (int) cache.GetAt (1).event.turnedOn);
            Assert::AreEqual ((Word) 0xC051, cache.GetAt (2).event.address);
            Assert::AreEqual ((int) IoEventLog::kText,  (int) cache.GetAt (2).event.turnedOn);
            Assert::AreEqual ((uint64_t) 4, cache.GetAt (1).event.cycle - cache.GetAt (0).event.cycle, L"one STA apart");

            for (i = 0; i < cache.GetCount(); i++)
            {
                const IoEvent  & entry = cache.GetAt (i).event;

                Assert::AreEqual (entry.scanline * VideoTiming::kCyclesPerScanline + entry.cycleInLine, entry.cycleInFrame);
            }

            lines = rig.RunOk ("VIDEOLOG").text;
            Assert::AreEqual (std::string ("3 entries:"), lines.at (0));
            Assert::IsTrue   (lines.at (2).ends_with ("$C050  TEXT off"));

            Assert::AreEqual (std::string ("Saved the log to C:\\Work\\VideoLog.txt."), rig.RunOk ("VIDEOLOG SAVE").text.at (0));
            Assert::IsTrue   (rig.files.PeekContent (L"C:\\Work\\VideoLog.txt").find (lines.at (2) + "\n") != std::string::npos);

            lines = rig.RunOk ("VIDEOLOG LIST 1").text;
            Assert::AreEqual (std::string ("The last entry:"), lines.at (0));

            Assert::AreEqual (std::string ("Video log cleared."), rig.RunOk ("VIDEOLOG CLEAR").text.at (0));
            Assert::AreEqual (std::string ("No entries."),        rig.RunOk ("VIDEOLOG LIST").text.at (0));
            rig.RunFails ("VIDEOLOG ON",       "invalid arguments");
            rig.RunFails ("VIDEOLOG SIDEWAYS", "invalid arguments");
            rig.RunFails ("VIDEOLOG LIST 1 2", "invalid arguments");
        }



        //  $0300: LDA $C030 / LDA $C030 / DDRA = $FF / DDRB = $07
        //  $0310: ORA = $07 / ORB = $07 (latch) / ORA = $38 / ORB = $06 (write) / NOP
        TEST_METHOD (SOUNDLOG_ListsSpeakerTogglesAndMockingboardWrites)
        {
            LoggedRig                  logged (TestMachine::Slots::AsShipped);
            LogRig                   & rig    = logged.rig;
            std::vector<std::string>   lines;



            rig.Load (0x0300, { 0xAD, 0x30, 0xC0, 0xAD, 0x30, 0xC0, 0xA9, 0xFF, 0x8D, 0x03, 0xC4, 0xA9, 0x07, 0x8D, 0x02, 0xC4,
                                0x8D, 0x01, 0xC4, 0x8D, 0x00, 0xC4, 0xA9, 0x38, 0x8D, 0x01, 0xC4, 0xA9, 0x06, 0x8D, 0x00, 0xC4,
                                0xEA }, 0x0300);

            RunTo (rig, 0x0320);

            const DebugEventCache<IoEvent>  & cache = logged.logs.GetSoundCache();

            Assert::AreEqual ((size_t) 9, cache.GetCount(), L"two toggles, six 6522 writes and the AY write");
            Assert::IsTrue   (cache.GetAt (0).event.kind == IoEventKind::Speaker);
            Assert::IsTrue   (cache.GetAt (8).event.kind == IoEventKind::AyWrite);
            Assert::AreEqual ((int) 7,    (int) cache.GetAt (8).event.reg);
            Assert::AreEqual ((int) 0x38, (int) cache.GetAt (8).event.value);
            Assert::AreEqual ((size_t) 0, logged.logs.GetVideoCache().GetCount(), L"nothing for the video log");

            lines = rig.RunOk ("SOUNDLOG").text;
            Assert::AreEqual (std::string ("9 entries:"), lines.at (0));
            Assert::IsTrue   (lines.at (2).find ("+4") != std::string::npos, L"one LDA after the first toggle");
            Assert::IsTrue   (lines.at (9).ends_with ("Mockingboard chip 1 AY R07 = $38"));

            lines = rig.RunOk ("SOUNDLOG LIST 2").text;
            Assert::AreEqual (std::string ("The last 2 entries:"), lines.at (0));
        }
    };
}