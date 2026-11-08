#include "Pch.h"

#include "Debugger/Handlers/BreakpointHandlers.h"
#include "HandlerTestRig.h"
#include "RunControlRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FreeRunningStopTests
    //
    //  A breakpoint or watchpoint armed while the emulator runs on its own, with
    //  no debugger run in progress: set from the window or a pipe client while a
    //  game plays. The machine must stop where the condition fired and every
    //  client must hear it, as a stop that ends a run is heard.
    //
    //  The program is INX / INX / JMP $0300 at $0300, with X at zero.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (FreeRunningStopTests)
    {
    public:

        static void LoadLoop (EmulatorRunRig & rig)
        {
            rig.Load (0x0300, { 0xE8, 0xE8, 0x4C, 0x00, 0x03 }, 0x0300);
        }



        TEST_METHOD (ABreakpointHitWhileRunningFreelyStopsTheMachineAndIsHeard)
        {
            EmulatorRunRig  rig (true);
            JsonValue       stop;



            LoadLoop (rig);
            rig.ConsoleOk ("BP 301");
            Assert::IsTrue (rig.Session().GetRunState() == RunState::FreeRunning, L"setting a breakpoint does not stop the machine");

            Assert::IsTrue   (rig.RunUntilPaused(), L"the machine stopped at the breakpoint");
            stop = rig.OnlyStop();
            Assert::AreEqual (std::string ("breakpoint"), RunText::GetString (stop, "reason"));
            Assert::AreEqual (0x0301, RunText::GetInt (stop, "pc"));
            Assert::AreEqual (0,      RunText::GetInt (stop, "breakpointId"));
            Assert::AreEqual (0,      RunText::GetInt (stop, "cycles"), L"no debugger run was going, so none spent cycles");
            Assert::IsFalse  (RunText::Has (stop, "causeId"), L"no command caused it");
            Assert::AreEqual ((Word) 0x0301, rig.Registers().pc, L"stopped before the instruction ran");
            Assert::AreEqual ((Byte) 1,      rig.Registers().x,  L"one INX ran");
            Assert::IsTrue   (rig.Session().GetRunState() == RunState::Paused);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Console ("= 300").status, L"and the session knows the machine is paused");
        }



        //  A breakpoint a pipe client sets while a game plays stops it for
        //  everyone, and names no cause: the BP command started no run.
        TEST_METHOD (ABreakpointAPipeClientSetsStopsTheGameForEveryClient)
        {
            EmulatorRunRig       rig (true);
            ChannelConnectionId  other = 0;
            JsonValue            reply;



            LoadLoop (rig);
            other = rig.transport.Connect();
            reply = rig.Command ("BP 302", 5);
            Assert::AreEqual (std::string ("ok"), RunText::GetString (reply, "status"));
            Assert::IsFalse  (RunText::Has (reply, "running"), L"setting a breakpoint starts no run");

            Assert::IsTrue   (rig.RunUntilPaused());
            Assert::AreEqual (0x0302, RunText::GetInt (rig.OnlyStop(), "pc"));
            Assert::IsFalse  (RunText::Has (rig.OnlyStop(), "causeId"));
            Assert::AreEqual ((size_t) 1, rig.RecordsOfType ("stopped", other).size(), L"the other client heard it too");
        }



        //  Resuming at a breakpoint runs the instruction there, as the first
        //  instruction of a debugger run does, and stops only when the
        //  breakpoint is reached again.
        TEST_METHOD (ResumingAtABreakpointRunsOnUntilItIsReachedAgain)
        {
            EmulatorRunRig          rig (true);
            std::vector<JsonValue>  stops;



            LoadLoop (rig);
            rig.ConsoleOk ("BP 301");
            Assert::IsTrue (rig.RunUntilPaused());

            rig.UserResume();
            Assert::IsTrue (rig.Session().GetRunState() == RunState::FreeRunning);
            Assert::IsTrue (rig.RunUntilPaused(), L"the loop came round to the breakpoint again");

            stops = rig.RecordsOfType ("stopped");
            Assert::AreEqual ((size_t) 2,    stops.size());
            Assert::AreEqual (0x0301,        RunText::GetInt (stops[1], "pc"));
            Assert::AreEqual ((Byte) 3,      rig.Registers().x, L"the loop ran once more: X went from 1 to 3");
            Assert::AreEqual ((size_t) 1,    rig.RecordsOfType ("resumed").size());
        }



        //  An after-mode watchpoint hit while the machine runs freely stops it
        //  after the access, and leaves nothing behind for the next run.
        TEST_METHOD (AWatchpointHitWhileRunningFreelyIsHeardAndLeavesNothingBehind)
        {
            EmulatorRunRig  rig (true);
            JsonValue       stop;
            JsonValue       watch;



            // $0300: LDA $0400 / JMP $0300
            rig.Load (0x0300, { 0xAD, 0x00, 0x04, 0x4C, 0x00, 0x03 }, 0x0300);
            rig.ConsoleOk ("BPMR 400");

            Assert::IsTrue   (rig.RunUntilPaused(), L"the read stopped the machine");
            stop  = rig.OnlyStop();
            watch = RunText::GetObject (stop, "watch");
            Assert::AreEqual (std::string ("watchpoint"), RunText::GetString (stop, "reason"));
            Assert::AreEqual (0x0303, RunText::GetInt (stop,  "pc"), L"after the instruction that read");
            Assert::AreEqual (0,      RunText::GetInt (watch, "id"));
            Assert::AreEqual (0x0400, RunText::GetInt (watch, "address"));
            Assert::AreEqual (0x0300, RunText::GetInt (watch, "accessPc"));
            Assert::AreEqual (std::string ("read"),  RunText::GetString (watch, "access"));
            Assert::AreEqual (std::string ("after"), RunText::GetString (watch, "mode"));

            //  The next run ends where its own breakpoint says, not on the hit
            //  already reported.
            rig.ConsoleOk ("BPC 0");
            rig.ConsoleOk ("BP 300");
            rig.ConsoleOk ("G");
            Assert::IsTrue   (rig.RunUntilPaused());
            stop = rig.LastStop();
            Assert::AreEqual (std::string ("breakpoint"), RunText::GetString (stop, "reason"));
            Assert::AreEqual (1,      RunText::GetInt (stop, "breakpointId"));
            Assert::AreEqual (0x0300, RunText::GetInt (stop, "pc"));
        }



        //  A before-mode hit while running freely is reported as itself, and a
        //  later breakpoint stop is reported as the breakpoint.
        TEST_METHOD (ABeforeWatchpointHitWhileRunningFreelyIsNotReportedAgainLater)
        {
            EmulatorRunRig  rig (true);
            JsonValue       stop;



            rig.Load (0x0300, { 0xAD, 0x00, 0x04, 0x4C, 0x00, 0x03 }, 0x0300);
            rig.ConsoleOk ("BPMR 400 BEFORE");

            Assert::IsTrue   (rig.RunUntilPaused());
            stop = rig.OnlyStop();
            Assert::AreEqual (std::string ("watchpoint"), RunText::GetString (stop, "reason"));
            Assert::AreEqual (0x0300, RunText::GetInt (stop, "pc"), L"before the read");
            Assert::AreEqual (std::string ("before"), RunText::GetString (RunText::GetObject (stop, "watch"), "mode"));

            rig.ConsoleOk ("BPC 0");
            rig.ConsoleOk ("BP 303");
            rig.ConsoleOk ("G");
            Assert::IsTrue   (rig.RunUntilPaused());
            stop = rig.LastStop();
            Assert::AreEqual (std::string ("breakpoint"), RunText::GetString (stop, "reason"));
            Assert::AreEqual (1, RunText::GetInt (stop, "breakpointId"));
            Assert::IsFalse  (RunText::Has (stop, "watch"));
        }



        //  An opcode breakpoint must not stop the address breakpoints from
        //  being asked about while the machine runs freely.
        TEST_METHOD (AnAddressBreakpointStopsBesideAnOpcodeBreakpoint)
        {
            EmulatorRunRig  rig (true);
            JsonValue       stop;



            LoadLoop (rig);
            rig.ConsoleOk ("BRK ON");
            rig.ConsoleOk ("BP 302");

            Assert::IsTrue   (rig.RunUntilPaused(), L"the address breakpoint stopped the machine");
            stop = rig.OnlyStop();
            Assert::AreEqual (0x0302, RunText::GetInt (stop, "pc"));
            Assert::AreEqual (1,      RunText::GetInt (stop, "breakpointId"));
        }



        TEST_METHOD (ADisabledBreakpointDoesNotStopARunningMachine)
        {
            EmulatorRunRig  rig (true);



            LoadLoop (rig);
            rig.ConsoleOk ("BP 301");
            rig.ConsoleOk ("BPD 0");

            Assert::IsFalse  (rig.RunUntilPaused (3), L"three frames ran through it");
            Assert::IsTrue   (rig.RecordsOfType ("stopped").empty());
            Assert::IsNull   (rig.machine.GetDebugHook(), L"nothing is asked of a machine with no enabled condition");

            rig.ConsoleOk ("BPE 0");
            Assert::IsTrue   (rig.RunUntilPaused(), L"enabled again, it stops");
            Assert::AreEqual (0x0301, RunText::GetInt (rig.OnlyStop(), "pc"));
        }



        TEST_METHOD (ClearingOneBreakpointLeavesTheOthersArmed)
        {
            EmulatorRunRig  rig (true);



            LoadLoop (rig);
            rig.ConsoleOk ("BP 301");
            rig.ConsoleOk ("BP 302");
            rig.ConsoleOk ("BPC 0");

            Assert::IsTrue   (rig.RunUntilPaused());
            Assert::AreEqual (0x0302, RunText::GetInt (rig.OnlyStop(), "pc"));
            Assert::AreEqual (1,      RunText::GetInt (rig.OnlyStop(), "breakpointId"));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  IdleHookFilterTests
    //
    //  Which instructions MachineHost asks the installed hook about, read from
    //  the hook MachineHost holds: nothing without a condition, the pages of an
    //  address breakpoint, the opcodes of an opcode breakpoint, and every
    //  instruction for the conditions that must see them all and during a run.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (IdleHookFilterTests)
    {
    public:

        static size_t CountMarked (const DebugHookFilter::Table & table)
        {
            return (size_t) std::count (table.begin(), table.end(), true);
        }



        TEST_METHOD (NoConditionInstallsNoHook)
        {
            EmulatorRunRig  rig (true);



            Assert::IsNull (rig.machine.GetDebugHook());

            rig.ConsoleOk ("BP 1234");
            rig.ConsoleOk ("BPC 0");
            Assert::IsNull (rig.machine.GetDebugHook(), L"clearing the last one takes the hook away");
        }



        TEST_METHOD (AnAddressBreakpointIsAskedAboutOnlyOnItsPages)
        {
            EmulatorRunRig  rig (true);
            DebugHook     * hook = nullptr;



            rig.ConsoleOk ("BP 1234");
            hook = rig.machine.GetDebugHook();
            Assert::IsNotNull (hook);
            Assert::IsFalse   (hook->GetFilter().everyInstruction);
            Assert::IsFalse   (hook->GetFilter().opcodesStop);
            Assert::IsTrue    (hook->GetFilter().pages[0x12]);
            Assert::AreEqual  ((size_t) 1, CountMarked (hook->GetFilter().pages));

            rig.ConsoleOk ("BP 12F0:1310");
            Assert::IsTrue    (rig.machine.GetDebugHook()->GetFilter().pages[0x13], L"a range marks every page it covers");
            Assert::AreEqual  ((size_t) 2, CountMarked (rig.machine.GetDebugHook()->GetFilter().pages));
        }



        TEST_METHOD (AnOpcodeBreakpointIsAskedAboutOnlyForItsOpcode)
        {
            EmulatorRunRig  rig (true);
            DebugHook     * hook = nullptr;



            rig.ConsoleOk ("BRKOP A9");
            hook = rig.machine.GetDebugHook();
            Assert::IsNotNull (hook);
            Assert::IsFalse   (hook->GetFilter().everyInstruction);
            Assert::IsTrue    (hook->GetFilter().opcodesStop);
            Assert::IsTrue    (hook->GetFilter().opcodes[0xA9]);
            Assert::AreEqual  ((size_t) 1, CountMarked (hook->GetFilter().opcodes));
        }



        TEST_METHOD (ConditionsTestedOnEveryInstructionAreAskedAboutEveryOne)
        {
            for (const char * line : { "BPMR 400", "BPMW 400 BEFORE", "BPR A=0", "BPIO C030", "BPMV 6 7" })
            {
                EmulatorRunRig  rig (true);



                rig.ConsoleOk (line);
                Assert::IsNotNull (rig.machine.GetDebugHook(), RunText::Widen (line).c_str());
                Assert::IsTrue    (rig.machine.GetDebugHook()->GetFilter().everyInstruction, RunText::Widen (line).c_str());
            }
        }



        //  A run asks about every instruction; its stop puts back what the
        //  conditions ask, and a run with none leaves no hook at all.
        TEST_METHOD (ARunAsksAboutEveryInstructionUntilItStops)
        {
            EmulatorRunRig  rig;



            rig.Load (0x0300, { 0xE8, 0xE8, 0x4C, 0x00, 0x03 }, 0x0300);
            rig.ConsoleOk ("G");
            Assert::IsNotNull (rig.machine.GetDebugHook());
            Assert::IsTrue    (rig.machine.GetDebugHook()->GetFilter().everyInstruction);

            rig.ConsoleOk ("PAUSE");
            Assert::IsTrue    (rig.RunUntilPaused());
            Assert::IsNull    (rig.machine.GetDebugHook(), L"no condition, so no hook once the run is over");

            rig.ConsoleOk ("BP 1234");
            rig.ConsoleOk ("G");
            Assert::IsTrue    (rig.machine.GetDebugHook()->GetFilter().everyInstruction);
            rig.ConsoleOk ("PAUSE");
            Assert::IsTrue    (rig.RunUntilPaused());
            Assert::IsFalse   (rig.machine.GetDebugHook()->GetFilter().everyInstruction, L"back to the breakpoint's page");
            Assert::IsTrue    (rig.machine.GetDebugHook()->GetFilter().pages[0x12]);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  EmulatorRunEndingTests
    //
    //  How a run the console started ends in the emulator: which reason wins
    //  when two arrive in one slice, what the machine and the session are left
    //  holding, and that the instruction observer hears the end.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (EmulatorRunEndingTests)
    {
    public:

        //  The hook is asked first: a breakpoint hit in the slice that also
        //  spent the budget is reported as the breakpoint.
        TEST_METHOD (ABreakpointAndABudgetInOneSliceStopAsTheBreakpoint)
        {
            EmulatorRunRig  rig;
            JsonValue       stop;



            // Six INX, 12 cycles, then a NOP.
            rig.Load (0x0300, { 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xEA }, 0x0300);
            rig.ConsoleOk ("BUDGET 10");
            rig.ConsoleOk ("BP 306");
            rig.ConsoleOk ("G");

            Assert::IsTrue   (rig.RunUntilPaused());
            stop = rig.OnlyStop();
            Assert::AreEqual (std::string ("breakpoint"), RunText::GetString (stop, "reason"));
            Assert::AreEqual (0x0306, RunText::GetInt (stop, "pc"));
            Assert::AreEqual (12,     RunText::GetInt (stop, "cycles"));
        }



        //  A pause and a spent budget in one slice stop as the pause.
        TEST_METHOD (APauseAndABudgetInOneSliceStopAsThePause)
        {
            EmulatorRunRig  rig;
            JsonValue       stop;



            rig.Load (0x0300, { 0xE8, 0xE8, 0x4C, 0x00, 0x03 }, 0x0300);
            rig.ConsoleOk ("BUDGET 100");
            rig.ConsoleOk ("G");
            rig.ConsoleOk ("PAUSE");

            Assert::IsTrue   (rig.RunUntilPaused());
            stop = rig.OnlyStop();
            Assert::AreEqual (std::string ("pause"), RunText::GetString (stop, "reason"));
            Assert::IsTrue   (RunText::GetInt (stop, "cycles") >= (int) EmulatorRunRig::kSliceCycles, L"one slice ran");
        }



        //  The CPU manager is paused before the registers are read, so what
        //  is reported is what the machine holds.
        TEST_METHOD (TheStopReportsTheRegistersTheMachineHolds)
        {
            EmulatorRunRig    rig;
            JsonValue         stop;
            JsonValue         registers;
            Cpu6502Registers  held;



            rig.Load (0x0300, { 0xE8, 0xE8, 0x4C, 0x00, 0x03 }, 0x0300);
            rig.ConsoleOk ("BUDGET 5000");
            rig.ConsoleOk ("G");
            Assert::IsTrue   (rig.RunUntilPaused());

            stop      = rig.OnlyStop();
            registers = RunText::GetObject (stop, "registers");
            held      = rig.Registers();
            Assert::AreEqual ((int) held.pc, RunText::GetInt (stop, "pc"));
            Assert::AreEqual ((int) held.pc, RunText::GetInt (registers, "pc"));
            Assert::AreEqual ((int) held.x,  RunText::GetInt (registers, "x"));
            Assert::AreEqual ((int) held.sp, RunText::GetInt (registers, "s"));
            Assert::AreEqual ((int) held.p,  RunText::GetInt (registers, "p"));
            Assert::IsTrue   (rig.cpuManager.IsPaused());
            Assert::IsTrue   (rig.Session().GetRunState() == RunState::Paused);
        }



        TEST_METHOD (AfterAStopTheHookIsWhatTheConditionsNeed)
        {
            EmulatorRunRig  rig;



            rig.Load (0x0300, { 0xE8, 0xE8, 0x4C, 0x00, 0x03 }, 0x0300);
            rig.ConsoleOk ("BP 302");
            rig.ConsoleOk ("G");
            Assert::IsTrue    (rig.RunUntilPaused());
            Assert::IsNotNull (rig.machine.GetDebugHook(), L"the breakpoint is still armed");
            Assert::IsFalse   (rig.machine.GetDebugHook()->GetFilter().everyInstruction);

            rig.ConsoleOk ("BPC 0");
            rig.ConsoleOk ("T");
            Assert::IsTrue    (rig.RunUntilPaused());
            Assert::IsNull    (rig.machine.GetDebugHook(), L"nothing is armed, so nothing is asked");
        }



        //  The handler set is the session's instruction observer on the window's
        //  path too: PROFILE counts a console run, and the last instruction is
        //  billed when the stop is heard.
        TEST_METHOD (TheInstructionObserverCountsAConsoleRunAndHearsItsStop)
        {
            EmulatorRunRig  rig;



            rig.Load (0x0300, { 0xE8, 0xE8, 0xE8, 0xEA }, 0x0300);
            rig.ConsoleOk ("PROFILE ON");
            rig.ConsoleOk ("BP 303");
            rig.ConsoleOk ("G");
            Assert::IsTrue   (rig.RunUntilPaused());
            Assert::AreEqual (std::string ("Instructions: 3, cycles: 6"), rig.ConsoleOk ("PROFILE").text.at (0));
        }



        //  TF writes through the file system the controller was given.
        TEST_METHOD (TheTraceFileRecordsAConsoleRun)
        {
            EmulatorRunRig  rig;
            std::string     trace;



            rig.Load (0x0300, { 0xE8, 0xE8, 0xE8, 0xEA }, 0x0300);
            rig.ConsoleOk ("TF trace.txt");
            rig.ConsoleOk ("BP 303");
            rig.ConsoleOk ("G");
            Assert::IsTrue   (rig.RunUntilPaused());

            trace = rig.files.PeekContent (L"trace.txt");
            Assert::AreEqual ((size_t) 3, (size_t) std::count (trace.begin(), trace.end(), '\n'), RunText::Widen (trace).c_str());
            Assert::IsTrue   (trace.find ("0302: INX") != std::string::npos, RunText::Widen (trace).c_str());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SynchronousRunDriverTests
    //
    //  The batch driver puts back the hook the host had before the run, so a
    //  condition stays armed after it and a machine with none pays nothing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SynchronousRunDriverTests)
    {
    public:

        TEST_METHOD (TheHookBeforeARunIsTheHookAfterIt)
        {
            MachineHandlerRig<BreakpointHandlers>  rig;
            DebugHook                            * armed = nullptr;



            rig.Load (0x0300, { 0xE8, 0xE8, 0xE8, 0xEA }, 0x0300);
            Assert::IsNull (rig.machine.GetDebugHook());

            rig.RunOk ("T");
            Assert::AreEqual ((Word) 0x0301, rig.LastStop().pc);
            Assert::IsNull   (rig.machine.GetDebugHook(), L"a run with no condition leaves none");

            rig.RunOk ("BP 303");
            armed = rig.machine.GetDebugHook();
            Assert::IsNotNull (armed);

            rig.RunOk ("G");
            Assert::AreEqual ((Word) 0x0303, rig.LastStop().pc);
            Assert::IsTrue   (rig.machine.GetDebugHook() == armed, L"the breakpoint's hook is back");
            Assert::IsFalse  (armed->GetFilter().everyInstruction, L"asking about its page only");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MachineChangeTests
    //
    //  EmulatorShell tells the session of a machine switch with
    //  OnMachineChanged; every address the debugger held belonged to the old
    //  machine.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MachineChangeTests)
    {
    public:

        TEST_METHOD (AMachineChangeClearsBreakpointsAndIsHeard)
        {
            EmulatorRunRig  rig;
            JsonValue       change;



            rig.ConsoleOk ("BP 300");
            rig.ConsoleOk ("BPMR 400");
            Assert::IsNotNull (rig.machine.GetDebugHook());

            rig.Session().OnMachineChanged ("Apple2eEnhanced");
            rig.controller.Pump();

            Assert::IsTrue    (rig.Session().GetBreakpoints().GetAll().empty());
            Assert::IsTrue    (rig.Session().GetWatchpoints().GetAll().empty());
            Assert::IsNull    (rig.machine.GetDebugHook(), L"nothing is left to ask about");
            Assert::IsTrue    (rig.Session().IsCallRecording(), L"the call record starts over");

            change = rig.RecordsOfType ("machineChanged").at (0);
            Assert::AreEqual  (std::string ("Apple2eEnhanced"), RunText::GetString (change, "machine"));
        }



        //  Line assembly was for the old machine's CPU, so a switch ends it.
        TEST_METHOD (AMachineChangeEndsLineAssembly)
        {
            EmulatorRunRig  rig;
            Reply           reply;



            rig.ConsoleOk ("A 300");
            Assert::IsTrue   (rig.Session().IsAssembling());

            rig.Session().OnMachineChanged ("Apple2eEnhanced");
            Assert::IsFalse  (rig.Session().IsAssembling());

            reply = rig.ConsoleOk ("R");
            Assert::IsTrue   (std::holds_alternative<RegistersData> (reply.data), L"R is a command again");
        }
    };
}
