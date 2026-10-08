#include "Pch.h"

#include "Debugger/CapturedDebugTarget.h"
#include "Debugger/DebugViewCapture.h"
#include "Debugger/MachineDebugTarget.h"
#include "EmuTests/TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace CapturedDebugTargetTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CapturedDebugTargetTests
    //
    //  A capture answers as the machine did when it was taken, the whole 64K
    //  included, and nothing written through it reaches anything.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CapturedDebugTargetTests)
    {
    public:

        //  The page copy gives exactly what peeking each byte gives, with the
        //  language card's RAM read in and with aux RAM read, as well as at
        //  power-on.
        TEST_METHOD (ThePageCopyMatchesPeekingEachByteUnderEveryBanking)
        {
            TestMachine         machine ("Apple2e");
            MachineDebugTarget  target  (machine);



            machine.PowerCycle();
            AssertCopyMatchesPeeks (target, L"at power-on");

            //  Two reads of $C08B: language card bank 1, RAM read and write.
            (void) machine.GetMemoryBus().ReadByte (0xC08B);
            (void) machine.GetMemoryBus().ReadByte (0xC08B);
            machine.GetMemoryBus().WriteByte (0xD000, 0x5A);
            AssertCopyMatchesPeeks (target, L"with the language card's RAM read");

            //  RAMRD on: $0200-$BFFF read from aux RAM.
            machine.GetMemoryBus().WriteByte (0xC003, 0x00);
            AssertCopyMatchesPeeks (target, L"with aux RAM read");
        }


        //  Every read the panes make answers from the capture, even after the
        //  machine has changed.
        TEST_METHOD (TheCaptureAnswersAsTheMachineDidWhenItWasTaken)
        {
            TestMachine          machine  ("Apple2e");
            MachineDebugTarget   target   (machine);
            auto                 capture  = std::make_shared<DebugViewCapture>();
            CapturedDebugTarget  captured (capture);
            Cpu6502Registers     registers;
            Byte                 value    = 0;



            machine.PowerCycle();
            machine.GetMemoryBus().WriteByte (0x0300, 0xA9);

            registers    = target.GetRegisters();
            registers.pc = 0x0300;
            target.SetRegisters (registers);

            DebugViewCapture::Take (target, CallRecord(), 0, 0, *capture);

            machine.GetMemoryBus().WriteByte (0x0300, 0xEA);
            registers.pc = 0x0400;
            target.SetRegisters (registers);

            Assert::AreEqual ((Word) 0x0300, captured.GetRegisters().pc, L"the registers as taken");
            Assert::IsTrue   (captured.TryPeek (0x0300, value));
            Assert::AreEqual ((Byte) 0xA9, value, L"memory as taken");
            Assert::IsFalse  (captured.TryPeek (0xC000, value), L"I/O is not read");
            Assert::AreEqual ((int) MemoryRegion::Io, (int) captured.GetRegion (0xC030));
            Assert::AreEqual (target.GetCycleCount(), captured.GetCycleCount());
            Assert::IsTrue   (captured.GetInstructionSet() == target.GetInstructionSet());
        }


        //  Writes through a capture change neither it nor the machine.
        TEST_METHOD (NothingWrittenThroughACaptureLands)
        {
            TestMachine          machine  ("Apple2e");
            MachineDebugTarget   target   (machine);
            auto                 capture  = std::make_shared<DebugViewCapture>();
            CapturedDebugTarget  captured (capture);
            Cpu6502Registers     registers;
            Byte                 value    = 0;
            HRESULT              hr       = S_OK;



            machine.PowerCycle();
            machine.GetMemoryBus().WriteByte (0x0300, 0x11);
            DebugViewCapture::Take (target, CallRecord(), 0, 0, *capture);

            registers    = captured.GetRegisters();
            registers.pc = 0x1234;
            captured.SetRegisters (registers);

            Assert::IsFalse  (captured.TryPoke  (0x0300, 0x22), L"a poke does not land");
            Assert::IsFalse  (captured.TryPatch (0x0300, 0x22), L"nor a patch");
            Assert::IsTrue   (captured.TryPeek (0x0300, value));
            Assert::AreEqual ((Byte) 0x11, value);
            Assert::AreNotEqual ((Word) 0x1234, captured.GetRegisters().pc, L"the registers do not change");
            Assert::AreEqual ((Byte) 0x11, machine.GetMemoryBus().ReadByte (0x0300), L"and the machine is untouched");
            hr = captured.StartRun (RunRequest());
            Assert::IsTrue   (FAILED (hr), L"and nothing runs");
        }


        //  The trace window holds the entries asked for when it was taken, and
        //  a request reaching outside them comes back short.
        TEST_METHOD (TheTraceWindowAnswersOnlyFromWhatWasTaken)
        {
            auto                      capture  = std::make_shared<DebugViewCapture>();
            CapturedDebugTarget       captured (capture);
            std::vector<TraceRecord>  entries;
            TraceRecord               record;



            capture->traceSize  = 100;
            capture->traceFirst = 40;

            for (Word pc = 0; pc < 10; pc++)
            {
                record.pc = (Word) (0x0300 + pc);
                capture->trace.push_back (record);
            }

            captured.GetTraceWindow (42, 3, entries);
            Assert::AreEqual ((size_t) 3, entries.size());
            Assert::AreEqual ((Word) 0x0302, entries[0].pc, L"entry 42 is the third taken");

            captured.GetTraceWindow (45, 20, entries);
            Assert::AreEqual ((size_t) 5, entries.size(), L"cut at the end of what was taken");

            captured.GetTraceWindow (0, 10, entries);
            Assert::AreEqual ((size_t) 0, entries.size(), L"nothing before it");
            Assert::AreEqual ((size_t) 100, captured.GetTraceSize(), L"the size is the whole trace's");
        }


        //  A call stack built from the record's copy is the one built from the
        //  recorder itself.
        TEST_METHOD (ACallStackFromTheRecordsCopyMatchesTheRecorders)
        {
            CallStackRecorder  recorder;
            CallStackData      fromRecorder;
            CallStackData      fromCopy;
            CallStackPeek      peek = [] (Word) { return (Byte) 0; };



            recorder.Begin (0x0300, 0x20);
            recorder.OnInstruction (0x0300, 0xFF, 0x20);
            recorder.OnInstruction (0x1000, 0xFD, 0xEA);

            fromRecorder = CallStack::Build (CallStackMechanism::Recorded, recorder,             0xFD, peek, nullptr);
            fromCopy     = CallStack::Build (CallStackMechanism::Recorded, recorder.GetRecord(), 0xFD, peek, nullptr);

            Assert::AreEqual (fromRecorder.rows.size(), fromCopy.rows.size());
            Assert::IsTrue   (fromRecorder.rows.size() > 0, L"the record has a frame");
        }

    private:

        static void AssertCopyMatchesPeeks (const MachineDebugTarget & target, const wchar_t * when)
        {
            auto  fast = std::make_unique<DebugMemoryImage>();
            auto  slow = std::make_unique<DebugMemoryImage>();



            target.TakeMemoryImage (*fast);
            target.IDebugTarget::TakeMemoryImage (*slow);

            for (size_t address = 0; address < DebugMemoryImage::kBytes; address++)
            {
                if (fast->readable[address] != slow->readable[address] || fast->bytes[address] != slow->bytes[address])
                {
                    Assert::Fail (std::format (L"${:04X} differs {}", address, when).c_str());
                }
            }

            for (size_t page = 0; page < DebugMemoryImage::kPages; page++)
            {
                Assert::AreEqual ((int) slow->regions[page], (int) fast->regions[page], when);
            }
        }
    };
}
