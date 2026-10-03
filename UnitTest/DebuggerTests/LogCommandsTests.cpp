#include "Pch.h"

#include "Debugger/Handlers/ExecutionHandlers.h"
#include "Debugger/Handlers/LogHandlers.h"
#include "HandlerTestRig.h"
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

        static constexpr int  kIoPage    = 0xC0;
        static constexpr int  kSlot4Page = 0xC4;



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



        //  $0300: STA $C050 / STA $C057 / STA $C054 / LDA $C051 / NOP
        TEST_METHOD (VIDEOLOG_RecordsEachModeChangeWithItsBeamPosition)
        {
            LogRig                    rig;
            MemoryBus               & bus     = rig.machine.GetMemoryBus();
            std::vector<std::string>  lines;



            rig.Load (0x0300, { 0x8D, 0x50, 0xC0, 0x8D, 0x57, 0xC0, 0x8D, 0x54, 0xC0, 0xAD, 0x51, 0xC0, 0xEA }, 0x0300);

            Assert::IsFalse  (bus.IsPageWatched (kIoPage), L"off, the I/O page keeps its usual path");
            Assert::AreEqual (std::string ("Video log on."), rig.RunOk ("VIDEOLOG ON").text.at (0));
            Assert::IsTrue   (bus.IsPageWatched (kIoPage));

            RunTo (rig, 0x030C);

            const std::deque<IoEvent>  & entries = rig.handlers.GetVideoLog().GetEntries();

            Assert::AreEqual ((size_t) 3, entries.size(), L"PAGE1 was already on, so $C054 changed nothing");
            Assert::AreEqual ((Word) 0xC050, entries[0].address);
            Assert::AreEqual ((int) IoEventLog::kText, (int) entries[0].turnedOff);
            Assert::AreEqual ((int) IoEventLog::kHires, (int) entries[1].turnedOn);
            Assert::AreEqual ((Word) 0xC051, entries[2].address);
            Assert::AreEqual ((int) IoEventLog::kText, (int) entries[2].turnedOn);
            Assert::AreEqual ((uint64_t) 4, entries[1].cycle - entries[0].cycle, L"one STA apart");

            for (const IoEvent & entry : entries)
            {
                Assert::AreEqual (entry.scanline * VideoTiming::kCyclesPerScanline + entry.cycleInLine, entry.cycleInFrame);
            }

            lines = rig.RunOk ("VIDEOLOG").text;
            Assert::AreEqual (std::string ("3 entries since the log was cleared; the last 3:"), lines.at (0));
            Assert::IsTrue   (lines.at (2).ends_with ("$C050  TEXT off"));

            Assert::AreEqual (std::string ("Saved the log to C:\\Work\\VideoLog.txt."), rig.RunOk ("VIDEOLOG SAVE").text.at (0));
            Assert::IsTrue   (rig.files.PeekContent (L"C:\\Work\\VideoLog.txt").find (lines.at (2) + "\n") != std::string::npos);

            Assert::AreEqual (std::string ("Video log off."), rig.RunOk ("VIDEOLOG OFF").text.at (0));
            Assert::IsFalse  (bus.IsPageWatched (kIoPage), L"off again, the logs cost nothing");

            Assert::AreEqual (std::string ("Video log cleared."), rig.RunOk ("VIDEOLOG CLEAR").text.at (0));
            Assert::AreEqual (std::string ("No entries."),        rig.RunOk ("VIDEOLOG LIST").text.at (0));
            rig.RunFails ("VIDEOLOG SIDEWAYS", "invalid arguments");
            rig.RunFails ("VIDEOLOG LIST 1 2", "invalid arguments");
        }



        //  $0300: LDA $C030 / LDA $C030 / LDA #$07 / STA $C401 / STA $C400
        //  $030E: LDA #$38 / STA $C401 / LDA #$06 / STA $C400 / NOP
        TEST_METHOD (SOUNDLOG_RecordsSpeakerTogglesAndMockingboardWrites)
        {
            LogRig                    rig ("Apple2e", TestMachine::Slots::AsShipped);
            MemoryBus               & bus = rig.machine.GetMemoryBus();
            std::vector<std::string>  lines;



            rig.Load (0x0300, { 0xAD, 0x30, 0xC0, 0xAD, 0x30, 0xC0, 0xA9, 0x07, 0x8D, 0x01, 0xC4, 0x8D, 0x00, 0xC4,
                                0xA9, 0x38, 0x8D, 0x01, 0xC4, 0xA9, 0x06, 0x8D, 0x00, 0xC4, 0xEA }, 0x0300);

            Assert::AreEqual (std::string ("Sound log on."), rig.RunOk ("SOUNDLOG ON").text.at (0));
            Assert::IsTrue   (bus.IsPageWatched (kSlot4Page), L"the Mockingboard's page");

            RunTo (rig, 0x0318);

            const std::deque<IoEvent>  & entries = rig.handlers.GetSoundLog().GetEntries();

            Assert::AreEqual ((size_t) 7, entries.size());
            Assert::IsTrue   (entries[0].kind == IoEventKind::Speaker);
            Assert::AreEqual ((uint64_t) 4, entries[1].sinceLast, L"one LDA apart");
            Assert::IsTrue   (entries[6].kind == IoEventKind::AyWrite);
            Assert::AreEqual ((int) 7,    (int) entries[6].reg);
            Assert::AreEqual ((int) 0x38, (int) entries[6].value);

            lines = rig.RunOk ("SOUNDLOG LIST 1").text;
            Assert::AreEqual (std::string ("7 entries since the log was cleared; the last 1:"), lines.at (0));
            Assert::IsTrue   (lines.at (1).ends_with ("Mockingboard chip 1 AY R07 = $38"));

            rig.RunOk ("SOUNDLOG OFF");
            Assert::IsFalse  (bus.IsPageWatched (kIoPage));
            Assert::IsFalse  (bus.IsPageWatched (kSlot4Page));
        }
    };
}
