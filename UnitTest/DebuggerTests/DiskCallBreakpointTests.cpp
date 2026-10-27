#include "Pch.h"

#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Handlers/MemoryHandlers.h"
#include "HandlerTestRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCallBreakpointTests
//
//  RWTS and driver breakpoints on a //e with a Disk II in slot 6 and a
//  made-up routine in RAM, an RTS at $2000, called through the vector or
//  by a direct JSR. The caller loads the request's pointer and ends in a
//  loop at $0807, where an address breakpoint ends a run that matched
//  nothing.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    static constexpr Word  kCaller    = 0x0800;
    static constexpr Word  kCallEnd   = 0x0807;
    static constexpr Word  kRoutine   = 0x2000;
    static constexpr Word  kIob       = 0x2100;
    static constexpr Word  kRwtsJmp   = 0x03D9;
    static constexpr Word  kDriveOne  = 0xBF1C;     // $BF10 plus twice slot 6
    static constexpr Word  kDriveTwo  = 0xBF2C;
    static constexpr Byte  kRts       = 0x60;
    static constexpr Byte  kJmp       = 0x4C;



    TEST_CLASS (DiskCallBreakpointTests)
    {
    public:

        using Rig = MachineHandlerRig<BreakpointHandlers>;

        struct CallRig
        {
            Rig             rig { "Apple2e", TestMachine::Slots::DiskOnly };
            MemoryHandlers  memory;

            CallRig()
            {
                rig.session.AddHandler (&memory);
                rig.session.SetBudget  (100'000);
            }
        };

        static void Poke (Rig & rig, Word address, std::initializer_list<Byte> bytes)
        {
            Word  at = address;



            for (Byte b : bytes)
            {
                rig.target.TryPoke (at++, b);
            }
        }

        //  The routine, an IOB with a request for track 17 sector 5 on drive 1, and a
        //  caller that points Y and A at it and calls target.
        static void LoadRwtsCall (Rig & rig, Word target, bool isVectorSet = true)
        {
            Poke (rig, kRoutine, { kRts });
            Poke (rig, kIob,     { 0x01, 0x60, 0x01, 0x00, 0x11, 0x05, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x01 });
            Poke (rig, kRwtsJmp, { (Byte) (isVectorSet ? kJmp : 0x00), 0x00, 0x20 });
            rig.Load (kCaller, { 0xA9, 0x21, 0xA0, 0x00, 0x20, (Byte) target, (Byte) (target >> 8), kJmp, 0x07, 0x08 }, kCaller);
        }

        //  ProDOS's global JMP, both of slot 6's vectors at the routine, and a
        //  READ of block 2 on the unit given.
        static void LoadDriverCall (Rig & rig, Byte unit)
        {
            Poke (rig, kRoutine,  { kRts });
            Poke (rig, 0xBF00,    { kJmp });
            Poke (rig, kDriveOne, { 0x00, 0x20 });
            Poke (rig, kDriveTwo, { 0x00, 0x20 });
            Poke (rig, 0x0042,    { 0x01, unit, 0x00, 0x30, 0x02, 0x00 });
            rig.Load (kCaller, { 0xEA, 0xEA, 0xEA, 0xEA, 0x20, 0x00, 0x20, kJmp, 0x07, 0x08 }, kCaller);
        }

        static const StopEvent & Go (Rig & rig)
        {
            rig.RunOk ("G");
            return rig.LastStop();
        }

        static const Breakpoint & Entry (Rig & rig)
        {
            return rig.session.GetBreakpoints().GetAll().at (0);
        }


        TEST_METHOD (AnRwtsCallThroughTheVectorStopsBeforeTheRoutine)
        {
            CallRig    call;
            StopEvent  stop;



            LoadRwtsCall (call.rig, kRwtsJmp);
            call.rig.RunOk ("BPDISK RWTS 11 5 READ");
            stop = Go (call.rig);

            Assert::IsTrue   (stop.reason == StopReason::Disk, L"a disk stop");
            Assert::AreEqual (kRoutine, stop.pc);
            Assert::IsTrue   (stop.disk.has_value() && stop.disk->request.has_value());
            Assert::AreEqual (0,           stop.disk->breakpointId);
            Assert::AreEqual (kRwtsJmp,    stop.disk->vector);
            Assert::AreEqual ((Byte) 0x11, stop.disk->request->track);
            Assert::AreEqual ((Byte) 5,    stop.disk->request->sector);
            Assert::AreEqual ((Byte) 1,    stop.disk->request->command);
            Assert::AreEqual (1,           stop.disk->request->drive);
            Assert::AreEqual (6,           stop.disk->request->slot);
            Assert::AreEqual (1u,          Entry (call.rig).hits);
        }

        TEST_METHOD (ADirectCallToTheRoutineStops)
        {
            CallRig  call;



            LoadRwtsCall (call.rig, kRoutine);
            call.rig.RunOk ("BPDISK RWTS 11 5");

            Assert::AreEqual (kRoutine, Go (call.rig).pc);
        }

        TEST_METHOD (AnotherRequestRunsOn)
        {
            CallRig    call;
            StopEvent  stop;



            LoadRwtsCall (call.rig, kRwtsJmp);
            call.rig.RunOk ("BPDISK RWTS 11 6");
            call.rig.RunOk ("BPDISK D2 RWTS");
            call.rig.RunOk ("BPDISK RWTS WRITE");
            call.rig.RunOk ("BP 807");
            stop = Go (call.rig);

            Assert::IsTrue   (stop.reason == StopReason::Breakpoint);
            Assert::AreEqual (kCallEnd, stop.pc);
            Assert::AreEqual (0u, Entry (call.rig).hits);
        }

        TEST_METHOD (TheUsersConditionIsTestedAfterTheRequest)
        {
            CallRig  call;



            LoadRwtsCall (call.rig, kRwtsJmp);
            call.rig.RunOk ("BPDISK RWTS 11 IF A == 22");
            call.rig.RunOk ("BP 807");

            Assert::AreEqual (kCallEnd, Go (call.rig).pc);
            Assert::AreEqual (0u, Entry (call.rig).hits);
        }

        //  The program sets the JMP after the breakpoint: the write to the
        //  vector resolves it during the run.
        TEST_METHOD (AVectorTheProgramSetsResolvesTheBreakpoint)
        {
            CallRig  call;



            LoadRwtsCall (call.rig, kRwtsJmp, false);
            call.rig.Load (0x0900, { 0xA9, kJmp, 0x8D, 0xD9, 0x03, 0x4C, 0x00, 0x08 }, 0x0900);
            call.rig.RunOk ("BPDISK RWTS");

            Assert::IsFalse  (Entry (call.rig).resolved.has_value(), L"no JMP yet");
            Assert::AreEqual (kRoutine, Go (call.rig).pc);
            Assert::AreEqual (kRoutine, Entry (call.rig).resolved.value_or (0));
        }

        TEST_METHOD (ADebuggerWriteResolvesTheBreakpoint)
        {
            CallRig  call;



            LoadRwtsCall (call.rig, kRwtsJmp, false);
            call.rig.RunOk ("BPDISK RWTS");
            call.rig.RunOk ("MEB 3D9 4C 00 30");

            Assert::AreEqual ((Word) 0x3000, Entry (call.rig).resolved.value_or (0));
        }

        TEST_METHOD (AResetResolvesTheBreakpointAgain)
        {
            CallRig  call;



            LoadRwtsCall (call.rig, kRwtsJmp);
            call.rig.RunOk ("BPDISK RWTS");
            Assert::AreEqual (kRoutine, Entry (call.rig).resolved.value_or (0));

            Poke (call.rig, kRwtsJmp, { 0x00 });
            call.rig.session.OnReset (true);

            Assert::IsFalse (Entry (call.rig).resolved.has_value());
        }

        TEST_METHOD (ADriverCallStopsOnlyForItsDrive)
        {
            auto       driveOne = std::make_unique<CallRig>();   // heap: two rigs overflow the frame (C6262)
            auto       driveTwo = std::make_unique<CallRig>();
            StopEvent  stop;



            LoadDriverCall (driveOne->rig, 0xE0);
            driveOne->rig.RunOk ("BPDISK D1 DRIVER READ");
            driveOne->rig.RunOk ("BP 807");
            Assert::AreEqual (kCallEnd, Go (driveOne->rig).pc, L"unit $E0 is drive 2");

            LoadDriverCall (driveTwo->rig, 0xE0);
            driveTwo->rig.RunOk ("BPDISK D2 DRIVER BLOCK 2 READ");
            stop = Go (driveTwo->rig);

            Assert::IsTrue   (stop.reason == StopReason::Disk);
            Assert::AreEqual (kRoutine,  stop.pc);
            Assert::AreEqual (kDriveTwo, stop.disk->vector);
            Assert::IsTrue   (stop.disk->request->isDriver);
            Assert::AreEqual (2,         stop.disk->request->drive);
            Assert::AreEqual ((Word) 2,  stop.disk->request->block);
            Assert::AreEqual ((Byte) 0xE0, stop.disk->request->unit);
        }

        TEST_METHOD (ADriverBreakpointWaitsForProDosGlobalPage)
        {
            CallRig  call;



            LoadDriverCall (call.rig, 0x60);
            Poke (call.rig, 0xBF00, { 0x00 });
            call.rig.RunOk ("BPDISK DRIVER");

            Assert::IsFalse (Entry (call.rig).resolved.has_value());
        }

        //  An address breakpoint on the routine stops with its own reason and
        //  the call's report beside it.
        TEST_METHOD (AnAddressBreakpointOnTheRoutineHasTheReport)
        {
            CallRig    call;
            StopEvent  stop;



            LoadRwtsCall (call.rig, kRwtsJmp);
            call.rig.RunOk ("BP 2000");
            call.rig.RunOk ("BPDISK RWTS 11 5");
            stop = Go (call.rig);

            Assert::IsTrue   (stop.reason == StopReason::Breakpoint);
            Assert::AreEqual (0, stop.breakpointId.value_or (-1));
            Assert::IsTrue   (stop.disk.has_value());
            Assert::AreEqual (1, stop.disk->breakpointId);
        }

        TEST_METHOD (CtrlResetKeepsTheEntryAndItsRoutine)
        {
            CallRig  call;



            LoadRwtsCall (call.rig, kRwtsJmp);
            call.rig.RunOk ("BPDISK RWTS");
            call.rig.session.OnReset (false);

            Assert::AreEqual ((size_t) 1, call.rig.session.GetBreakpoints().GetAll().size());
            Assert::AreEqual (kRoutine, Entry (call.rig).resolved.value_or (0));
        }

        //  BPL lists the routine, or that there is none yet, and a user IF
        //  alone, without the request.
        TEST_METHOD (BplListsTheRoutineAndTheUsersConditionAlone)
        {
            CallRig  call;



            LoadRwtsCall (call.rig, kRwtsJmp, false);
            call.rig.RunOk ("BPDISK RWTS 11 IF A == 21");
            Assert::IsTrue (call.rig.RunOk ("BPL").text.at (0).find (", unresolved") != std::string::npos);

            call.rig.RunOk ("MEB 3D9 4C");
            Assert::IsTrue (call.rig.RunOk ("BPL").text.at (0).find ("at $2000 if A == 21,") != std::string::npos,
                            TextEncoding::NarrowToWide (call.rig.RunOk ("BPL").text.at (0)).c_str());
        }

        TEST_METHOD (TheVectorWatchIsNoWatchpoint)
        {
            CallRig  call;



            LoadRwtsCall (call.rig, kRwtsJmp);
            call.rig.RunOk ("BPDISK RWTS");

            Assert::IsTrue   (call.rig.session.GetWatchpoints().GetAll().empty());
            Assert::AreEqual ((size_t) 1, call.rig.session.GetBreakpoints().GetAll().size());
            Assert::IsTrue   (Entry (call.rig).condition.postfix.empty(), L"the request is not in the condition");
        }
    };
}
