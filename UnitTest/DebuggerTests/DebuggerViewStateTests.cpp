#include "Pch.h"

#include "Ui/Debugger/Panes/BreakpointsPane.h"
#include "Ui/Debugger/RunActions.h"

#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugHandlerSet.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/MonitorParser.h"
#include "Debugger/Source/SourcePathList.h"
#include "EmuTests/TestMachine.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "FakeDiagnosticsProvider.h"
#include "ControllerRig.h"
#include "DebuggerViewStateTestRig.h"
#include "HandlerTestRig.h"
#include "InMemoryPipeTransport.h"
#include "TestHelpers.h"
#include "MockDebugTarget.h"
#include "MockExpressionContext.h"
#include "Shell/CpuManager.h"
#include "Ui/Debugger/BreakpointDialog.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Ui/Debugger/DebuggerCommandLines.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/Panes/CallStackPane.h"
#include "Ui/Debugger/Panes/DiagnosticsPane.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/SourceLineLookup.h"
#include "Ui/Debugger/SourceRows.h"
#include "Ui/Debugger/SourceBannerText.h"
#include "Ui/Debugger/SourceMacroPlaces.h"
#include "Ui/Debugger/Panes/TracePane.h"
#include "Ui/Debugger/InstructionTouches.h"
#include "Ui/Debugger/InstructionEffect.h"
#include "Ui/Debugger/StopChanges.h"
#include "Core/UnicodeSymbols.h"
#include "UiTests/InMemoryFileSystem.h"
#include "UnreadableStackTarget.h"
#include "Ui/Debugger/Panes/MemoryMapBar.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/MeterBar.h"
#include "Ui/Debugger/Panes/SoundBoardView.h"
#include "Core/TextEncoding.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerViewStateTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PaneTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (PaneTests)
    {
    public:

        //  The registers pane ends with the machine's CPU cycle count since
        //  power-on, which the registers views of other emulators show too.
        TEST_METHOD (TheRegisterPaneShowsTheCycleCount)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;
            uint64_t              cycles = 0;



            rig.machine.GetRunner().RunCycles (1234);

            cycles   = rig.machine.GetCpu()->GetTotalCycles();
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((size_t) 9, snapshot.registers.size(), L"the register pane is filled");
            Assert::AreEqual (std::string ("Cycles"),      snapshot.registers[6].name);
            Assert::AreEqual (std::format ("{}", cycles),  snapshot.registers[6].value);
            Assert::AreEqual (std::string ("Trip"),        snapshot.registers[7].name);
            Assert::AreEqual (std::format ("{}", cycles),  snapshot.registers[7].value, L"from power-on until RCC");
            Assert::AreEqual (std::string ("Last branch"), snapshot.registers[8].name);
            Assert::IsTrue   (snapshot.lastBranchFrom.has_value() && snapshot.lastBranchTo.has_value(), L"the ROM has branched by now");
        }



        //  RCC starts the trip count again; the total goes on.
        TEST_METHOD (RccStartsTheTripCountAgain)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;
            uint64_t              trip = 0;



            rig.machine.GetRunner().RunCycles (5000);
            rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(), RunActions::GetResetCycles (CommandMode::AppleWin));
            rig.machine.GetRunner().RunCycles (100);

            snapshot = rig.view.Build (rig.controller.GetSession());
            trip     = std::stoull (snapshot.registers[7].value);

            Assert::IsTrue (trip >= 100 && trip < 200, L"counted from RCC");
            Assert::IsTrue (std::stoull (snapshot.registers[6].value) > 5000, L"the total is not reset");
        }

        TEST_METHOD (TheCodePaneShowsThePcWithWhatLedToItAbove)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot = rig.view.Build (rig.controller.GetSession());
            int                   current  = -1;



            for (int i = 0; i < (int) snapshot.code.size(); i++)
            {
                current = snapshot.code[(size_t) i].isCurrent ? i : current;
            }

            Assert::IsTrue   (current > 0, L"the PC is shown, and not jammed against the top edge");
            Assert::AreEqual ((Word) 0x0300, snapshot.code[(size_t) current].address);
            Assert::AreEqual (std::string ("A9 41"), snapshot.code[(size_t) current].bytes);
            Assert::IsTrue   (snapshot.code[(size_t) current].instruction.find ("LDA") == 0);
        }


        //  A bare D or U continues from the user's last one, not from what
        //  the panes read in between.
        TEST_METHOD (APaneReadLeavesBareDAndUWhereTheUserLeftThem)
        {
            MachineRig  rig;
            Reply       dump;
            Reply       list;



            rig.Run ("D 2000");
            rig.Run ("U 1000");
            (void) rig.view.Build (rig.controller.GetSession());

            dump = rig.Run ("D");
            list = rig.Run ("U");

            Assert::IsTrue   (std::holds_alternative<MemoryData> (dump.data));
            Assert::AreEqual ((Word) 0x2040, std::get<MemoryData> (dump.data).rows.front().address);
            Assert::IsTrue   (std::holds_alternative<DisassemblyData> (list.data));
            Assert::IsTrue   (std::get<DisassemblyData> (list.data).lines.front().instruction.address > 0x1000);
            Assert::IsTrue   (std::get<DisassemblyData> (list.data).lines.front().instruction.address < 0x1100);
        }


        //  An open A block takes the user's lines, not the panes' reads.
        TEST_METHOD (APaneReadDuringAssemblyIsNotAssembled)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;
            Byte                  before   = rig.machine.GetMemoryBus().ReadByte (0x0800);



            rig.Run ("A 800");
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsTrue   (rig.controller.GetSession().GetInterpreter().IsAssembling());
            Assert::IsFalse  (snapshot.registers.empty(), L"the register pane is filled");
            Assert::AreEqual ((Word) 0x0300, snapshot.pc);
            Assert::AreEqual (before,        rig.machine.GetMemoryBus().ReadByte (0x0800), L"nothing was assembled");
        }


        //  A pane's read is not the user's listing: U and D with no address
        //  go on from the user's own last U and D.
        TEST_METHOD (APaneReadLeavesTheConsolesContinuationAlone)
        {
            MachineRig            rig;
            Reply                 list;
            Reply                 dump;



            for (Word address = 0x1000; address < 0x1004; address++)
            {
                rig.machine.GetMemoryBus().WriteByte (address, 0xEA);    // NOP
            }

            rig.Run ("U 1000:1002");
            rig.Run ("D 2000:200F");
            (void) rig.view.Build (rig.controller.GetSession());

            list = rig.Run ("U");
            dump = rig.Run ("D");

            Assert::AreEqual ((Word) 0x1003, std::get<DisassemblyData> (list.data).lines.front().instruction.address, L"U goes on from the user's U");
            Assert::AreEqual ((Word) 0x2010, std::get<MemoryData>      (dump.data).rows.front().address,             L"D goes on from the user's D");
        }


        //  While the line assembler takes the console's lines, the panes still
        //  read the machine.
        TEST_METHOD (ThePanesReadTheMachineWhileTheAssemblerIsActive)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            rig.Run ("!", CommandMode::Monitor);
            Assert::IsTrue (rig.controller.GetSession().GetInterpreter().IsAssembling());

            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsFalse  (snapshot.registers.empty(), L"registers");
            Assert::IsFalse  (snapshot.code.empty(),      L"code");
            Assert::IsFalse  (snapshot.memory.empty(),    L"memory");
            Assert::IsTrue   (rig.controller.GetSession().GetInterpreter().IsAssembling(), L"and the assembler is still active");
        }


        //  The panes' own reads are not lines for the mini-assembler, so an
        //  A block in progress leaves them filled.
        TEST_METHOD (ThePanesStayFilledWhileTheAssemblerIsActive)
        {
            MachineRig            rig;
            DebugSession        & session  = rig.controller.GetSession();
            DebuggerViewSnapshot  snapshot;



            Assert::AreEqual ((int) CommandStatus::Ok, (int) session.ExecuteLine ("WA 400", CommandMode::AppleWin).status);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) session.ExecuteLine ("A 310",  CommandMode::AppleWin).status);

            snapshot = rig.view.Build (session);

            Assert::AreEqual ((size_t) 1,    snapshot.watches.size(), L"the watch is still listed");
            Assert::AreEqual ((Word) 0x0300, LineAt (snapshot, 0x0300).address, L"and the code pane still shows the PC");
            Assert::AreEqual ((int) CommandStatus::Ok, (int) session.ExecuteLine ("NOP", CommandMode::AppleWin).status, L"the block is still open");
            Assert::AreEqual ((Byte) 0xEA,   rig.machine.GetMemoryBus().ReadByte (0x0310), L"and assembles where it was");
        }


        TEST_METHOD (ChangesAreMarkedAgainstThePreviousStopAndHeld)
        {
            StopChanges  changes;



            changes.Update (true, 100, { { "R:A", "00" }, { "R:X", "01" } });
            Assert::IsFalse (changes.IsChanged ("R:A"), L"the first stop has nothing to compare with");

            changes.Update (true, 102, { { "R:A", "05" }, { "R:X", "01" } });
            Assert::IsTrue  (changes.IsChanged ("R:A"), L"a step that changed A");
            Assert::IsFalse (changes.IsChanged ("R:X"));

            changes.Update (true, 102, { { "R:A", "05" }, { "R:X", "01" } });
            Assert::IsTrue  (changes.IsChanged ("R:A"), L"a repeated snapshot of the same stop keeps the mark");

            changes.Update (false, 5000, { { "R:A", "77" }, { "R:X", "02" } });
            Assert::IsFalse (changes.IsChanged ("R:A"), L"nothing is marked while running");

            changes.Update (true, 9000, { { "R:A", "05" }, { "R:X", "09" } });
            Assert::IsFalse (changes.IsChanged ("R:A"), L"against the previous stop, not the running values");
            Assert::IsTrue  (changes.IsChanged ("R:X"));
            Assert::IsFalse (changes.IsChanged ("W:1"), L"a key never shown is not changed");
        }



        //  An edit at a stop doesn't move the CPU's cycles, so it isn't a
        //  stop: the values are compared with the stop before, and the marks
        //  of the stop stay.
        TEST_METHOD (AnEditAtAStopKeepsTheStopsMarks)
        {
            StopChanges  changes;



            changes.Update (true, 100, { { "R:PC", "0300" }, { "R:Y", "00" } });
            changes.Update (true, 102, { { "R:PC", "0302" }, { "R:Y", "00" } });
            Assert::IsTrue  (changes.IsChanged ("R:PC"), L"the step moved PC");
            Assert::IsFalse (changes.IsChanged ("R:Y"));

            changes.Update (true, 102, { { "R:PC", "0302" }, { "R:Y", "07" } });
            Assert::IsTrue  (changes.IsChanged ("R:PC"), L"PC's mark stays after Y is edited");
            Assert::IsTrue  (changes.IsChanged ("R:Y"),  L"the edited Y differs from the stop before");

            changes.Update (true, 102, { { "R:PC", "0302" }, { "R:Y", "00" } });
            Assert::IsFalse (changes.IsChanged ("R:Y"),  L"Y edited back to its value at the stop before");
            Assert::IsTrue  (changes.IsChanged ("R:PC"));

            changes.Update (true, 104, { { "R:PC", "0304" }, { "R:Y", "00" } });
            Assert::IsTrue  (changes.IsChanged ("R:PC"), L"the next step compares with the edited stop");
            Assert::IsFalse (changes.IsChanged ("R:Y"));
        }


        //  The registers pane shows how far the cycle count moved since the
        //  stop before: forward for a step or a run, back for a step back or
        //  a seek into the past.
        TEST_METHOD (TheCycleDeltaIsSignedAgainstThePreviousStop)
        {
            StopChanges  changes;



            changes.Update (true, 1000, { { "R:Cycles", "1000" } });
            Assert::AreEqual (std::string(), changes.GetDelta ("R:Cycles"), L"the first stop has nothing to compare with");

            changes.Update (true, 1006, { { "R:Cycles", "1006" } });
            Assert::AreEqual (std::string ("+6"), changes.GetDelta ("R:Cycles"), L"a step");

            changes.Update (false, 90000, { { "R:Cycles", "90000" } });
            Assert::AreEqual (std::string(), changes.GetDelta ("R:Cycles"), L"nothing while running");

            changes.Update (true, 123456, { { "R:Cycles", "123456" } });
            Assert::AreEqual (std::string ("+122450"), changes.GetDelta ("R:Cycles"), L"a run stopped at a breakpoint");

            changes.Update (true, 123450, { { "R:Cycles", "123450" } });
            Assert::AreEqual (std::string ("-6"), changes.GetDelta ("R:Cycles"), L"a step back");

            Assert::AreEqual (std::string(), changes.GetDelta ("R:A"), L"a key never shown");
        }


        //  P's flags are colored one by one, so a step that sets Z marks Z
        //  alone.
        TEST_METHOD (TheChangedBitsOfAHexValueAreThoseThatDiffer)
        {
            StopChanges  changes;



            changes.Update (true, 100, { { "R:P", "24" } });
            Assert::AreEqual (0u, changes.GetChangedBits ("R:P"), L"the first stop has nothing to compare with");

            changes.Update (true, 102, { { "R:P", "26" } });
            Assert::AreEqual (0x02u, changes.GetChangedBits ("R:P"), L"DEX to zero sets Z alone");

            changes.Update (true, 104, { { "R:P", "A5" } });
            Assert::AreEqual (0x83u, changes.GetChangedBits ("R:P"), L"N and C set, Z clear");

            changes.Update (false, 900, { { "R:P", "00" } });
            Assert::AreEqual (0u, changes.GetChangedBits ("R:P"), L"nothing while running");
        }

        //  A source line's breakpoint is the one BP file:line sets, and the New
        //  drop-down's row for it has no file to offer without a debug file.
        TEST_METHOD (TheDialogsSourceLineIsBpFileLine)
        {
            BreakpointDialog::Fields         fields;
            DebuggerViewSnapshot             none;
            DebuggerViewSnapshot             loaded;
            DebugSourceFile                  file;



            fields.type      = BreakpointDialog::Type::SourceLine;
            fields.file      = "main.a65";
            fields.address   = "12";
            fields.condition = "A == 1";

            Assert::AreEqual (std::string ("BP main.a65:12 IF A == 1"), BreakpointDialog::MakeDefinition (fields));
            Assert::IsTrue   (BreakpointDialog::GetVisibleFields (BreakpointDialog::Type::SourceLine).file, L"the file drop-down");

            file.name     = "main.a65";
            loaded.source = DebuggerViewSnapshot::SourceState();
            loaded.source->files.push_back (file);

            Assert::IsTrue   (BreakpointsPane::GetSourceFiles (&none).empty(), L"no debug file, no row to choose");
            Assert::AreEqual ((size_t) 1, BreakpointsPane::GetSourceFiles (&loaded).size());
        }


        TEST_METHOD (TheBreakpointDialogWritesADefinitionBpeditTakes)
        {
            using Type = BreakpointDialog::Type;

            const std::tuple<Type, const char *, const char *, const char *, const char *>  cases[] =
            {
                { Type::Execute,   "0300",      "",     "A == 1", "BP 0300 IF A == 1" },
                { Type::Read,      "0400:040F", "",     "",    "BPMR 0400:040F"      },
                { Type::Write,     "0400",      "",     "",    "BPMW 0400"           },
                { Type::ReadWrite, "0400",      "",     "",    "BPM 0400"            },
                { Type::Value,     "0400",      "41",   "",    "BPMV 0400 41"        },
                { Type::Condition, "",          "",     "A == 41 && X > 3", "BPR IF A == 41 && X > 3" },
                { Type::Opcode,    "",          "EA",   "",    "BRKOP EA"            },
            };

            MachineRig  rig;
            Reply       set = rig.Run ("BP 0300");
            int         id  = std::get<BreakpointSetData> (set.data).breakpoint.id;




            for (const auto & [type, address, value, condition, expected] : cases)
            {
                std::string  definition = BreakpointDialog::MakeDefinition ({ type, WatchMode::After, address, value, condition });
                Reply        reply      = rig.Run (std::format ("BPEDIT {} {}", id, definition));

                Assert::AreEqual (std::string (expected), definition);
                std::string  why = definition + ": " + reply.error.detail;

                Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, TextEncoding::NarrowToWide (why).c_str());
            }
        }


        //  Each entry is set as a person would set it, filled into the dialog's
        //  fields and written straight back with OK. The entry BPEDIT leaves
        //  must be the one it started from.
        TEST_METHOD (EveryBreakpointKindRoundTripsThroughTheEditDialog)
        {
            const std::pair<BreakpointKind, const char *>  setters[] =
            {
                { BreakpointKind::Address,     "BP 0300:0302 IF A == 1"           },
                { BreakpointKind::Opcode,      "BRKOP EA"                         },
                { BreakpointKind::Register,    "BPR A = 41"                       },
                { BreakpointKind::Memory,      "BPMR 0400:040F BEFORE IF A == 1"  },
                { BreakpointKind::Memory,      "BPMW 0400 BEFORE"                 },
                { BreakpointKind::Memory,      "BPM 0400 IF A == 1"               },
                { BreakpointKind::Brk,         "BRK ON"                           },
                { BreakpointKind::Interrupt,   "BRKINT ON"                        },
                { BreakpointKind::MemoryValue, "BPMV 0400 41 IF A == 1"           },
            };

            for (int kind = 0; kind <= (int) BreakpointKind::MemoryValue; ++kind)
            {
                bool  isCovered = std::any_of (std::begin (setters), std::end (setters), [kind] (const auto & each) { return (int) each.first == kind; });

                Assert::IsTrue (isCovered, std::format (L"kind {} has no case", kind).c_str());
            }

            for (const auto & [kind, setter] : setters)
            {
                MachineRig      rig;
                Reply           set    = rig.Run (setter);
                BreakpointInfo  before = GetOnlyBreakpoint (rig);
                std::string     line   = std::format ("BPEDIT {} {}", before.id, BreakpointDialog::MakeDefinition (BreakpointDialog::GetFields (before)));
                Reply           edit   = rig.Run (line);
                BreakpointInfo  after  = GetOnlyBreakpoint (rig);
                std::wstring    why    = SourcePathList::Utf8ToWide (std::string (setter) + " -> " + line + ": " + edit.error.detail);



                Assert::AreEqual ((int) CommandStatus::Ok, (int) set.status,  SourcePathList::Utf8ToWide (setter).c_str());
                Assert::AreEqual ((int) kind,              (int) before.kind, SourcePathList::Utf8ToWide (setter).c_str());
                Assert::AreEqual ((int) CommandStatus::Ok, (int) edit.status, why.c_str());
                Assert::AreEqual (before.id,               after.id,          why.c_str());
                Assert::AreEqual ((int) before.kind,       (int) after.kind,  why.c_str());
                Assert::AreEqual (before.address,          after.address,     why.c_str());
                Assert::AreEqual (before.last,             after.last,        why.c_str());
                Assert::AreEqual (before.opcode,           after.opcode,      why.c_str());
                Assert::AreEqual (before.condition,        after.condition,   why.c_str());
                Assert::AreEqual ((int) before.access,     (int) after.access, why.c_str());
                Assert::AreEqual ((int) before.mode,       (int) after.mode,  why.c_str());
                Assert::IsTrue   (before.value == after.value,                why.c_str());
            }
        }


        static BreakpointInfo GetOnlyBreakpoint (MachineRig & rig)
        {
            Reply  list = rig.Run ("BPL");



            Assert::AreEqual ((size_t) 1, std::get<BreakpointListData> (list.data).breakpoints.size(), L"one entry in the table");
            return std::get<BreakpointListData> (list.data).breakpoints[0];
        }


        TEST_METHOD (ABreakpointRowSaysWhetherItIsEnabled)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            rig.Run ("BP 0302");
            rig.Run ("BPD *");
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsTrue  (LineAt (snapshot, 0x0302).hasBreakpoint);
            Assert::IsFalse (LineAt (snapshot, 0x0302).isEnabled);
        }



        //  FR-067: the call-stack pane is CALLS, recorded while the debugger
        //  is attached; a row's activation moves the disassembly to the call
        //  site, and the button moves to the next mechanism.
        TEST_METHOD (TheCallStackPaneShowsTheRecordedChain)
        {
            MachineRig                          rig;
            DebuggerViewSnapshot                snapshot;
            std::vector<CallStackPane::Row>     rows;
            Cpu6502Registers                    r;
            HRESULT                             hr = S_OK;



            //  JSR $0320 at $0310, and a NOP there.
            rig.machine.GetMemoryBus().WriteByte (0x0310, 0x20);
            rig.machine.GetMemoryBus().WriteByte (0x0311, 0x20);
            rig.machine.GetMemoryBus().WriteByte (0x0312, 0x03);
            rig.machine.GetMemoryBus().WriteByte (0x0320, 0xEA);

            r    = rig.controller.GetSession().GetTarget().GetRegisters();
            r.pc = 0x0310;
            r.sp = 0xFF;
            rig.controller.GetSession().GetTarget().SetRegisters (r);

            hr = rig.controller.Open();
            Assert::IsTrue (SUCCEEDED (hr));
            Assert::IsTrue (rig.controller.GetSession().GetCallRecords().IsRecording(), L"attached: the record is kept");

            rig.machine.GetRunner().StepOne();
            snapshot = rig.view.Build (rig.controller.GetSession());
            rows     = CallStackPane::GetRows (snapshot.callStack);

            Assert::IsTrue   (snapshot.callStack.rows[0].frame.has_value());
            Assert::AreEqual ((Word) 0x0310, snapshot.callStack.rows[0].frame->callSite);
            Assert::AreEqual ((Word) 0x0320, snapshot.callStack.rows[0].frame->target);
            Assert::AreEqual (std::wstring (L"$0310"),    rows[0].site);
            Assert::AreEqual (std::wstring (L"found during execution"), rows[0].foundBy);
            Assert::AreEqual ((Word) 0x0310,              rows[0].address, L"activating the row shows the call site");
            Assert::IsTrue   (rows[1].isBreak,            L"where recording began is a separator row");
            Assert::AreEqual (std::string ("CALLS MODE RECORDED"), CallStackPane::GetNextModeLine (snapshot.callStack.mechanism));

            rig.controller.Close();
            Assert::IsNull (rig.machine.GetRunner().GetDebugHook(), L"detached: no hook (FR-064)");
        }



        TEST_METHOD (RegistersAndFlagsAreRows)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;
            Cpu6502Registers      r        = rig.controller.GetSession().GetTarget().GetRegisters();



            r.a = 0x41;
            r.p = 0x81;     // N and C
            rig.controller.GetSession().GetTarget().SetRegisters (r);

            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((size_t) 9, snapshot.registers.size(), L"six registers, the cycle and trip counts and the last branch");
            Assert::AreEqual (std::string ("A"),  snapshot.registers[0].name);
            Assert::AreEqual (std::string ("41"), snapshot.registers[0].value);
            Assert::AreEqual (std::string ("PC"), snapshot.registers[5].name);
            Assert::AreEqual (std::string ("0300"), snapshot.registers[5].value);
            Assert::AreEqual (std::string ("N......C"), snapshot.flags);
        }



        TEST_METHOD (MemoryRowsCarryTheirRegion)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            rig.view.GetMemoryWindows().SetMemoryAddress (0x0300);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsFalse  (snapshot.memory.empty());
            Assert::AreEqual ((Word) 0x0300, snapshot.memory[0].address);
            Assert::IsTrue   (snapshot.memory[0].bytes.find ("A9 41 8D") == 0);
            Assert::AreEqual (std::string ("RAM"), snapshot.memory[0].region);

            rig.view.GetMemoryWindows().SetMemoryAddress (0xF800);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual (std::string ("ROM"), snapshot.memory[0].region);
        }



        TEST_METHOD (TheStackWatchesAndBreakpointsAreListed)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            rig.Run ("BP 0302");
            rig.Run ("W 0400");

            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((size_t) 1, snapshot.breakpoints.size());
            Assert::AreEqual ((Word) 0x0302, snapshot.breakpoints[0].address);
            Assert::IsTrue   (LineAt (snapshot, 0x0302).hasBreakpoint, L"the code pane marks the line");

            Assert::AreEqual ((size_t) 1, snapshot.watches.size());
            Assert::AreEqual ((Word) 0x0400, snapshot.watches[0].address);

            Assert::IsFalse  (snapshot.stack.empty(), L"the stack pane shows the page above SP");
        }



        //  WD shows in the Watches pane: the line carries whether it is on.
        TEST_METHOD (DisabledWatch_IsMarkedOff)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            rig.Run ("W 0400");
            rig.Run ("W 0402");
            rig.Run ("WD 1");

            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((size_t) 2, snapshot.watches.size());
            Assert::IsTrue   (snapshot.watches[0].enabled);
            Assert::IsFalse  (snapshot.watches[1].enabled);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TracePaneTests
    //
    //  The trace pane reads a window of entries through HISTORY: from where the
    //  pane is scrolled, or ending at the newest while it follows the end. The
    //  pane asks for a new window only when the rows on screen leave the last.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (TracePaneTests)
    {
    public:

        static constexpr int       kRows      = DebuggerViewState::kTraceRows;
        static constexpr uint64_t  kTotal     = 2 * kRows + 44;
        static constexpr uint64_t  kJmpCycles = 3;

        //  JMP $0300 at $0300, run with the trace on until it holds kTotal.
        static void FillTrace (MachineRig & rig)
        {
            rig.machine.GetMemoryBus().WriteByte (0x0300, 0x4C);
            rig.machine.GetMemoryBus().WriteByte (0x0301, 0x00);
            rig.machine.GetMemoryBus().WriteByte (0x0302, 0x03);

            rig.Run ("HISTORY ON");
            rig.machine.GetRunner().RunCycles (kTotal * kJmpCycles);
        }



        TEST_METHOD (TheWindowEndsAtTheNewestOrStartsWhereThePaneIs)
        {
            static constexpr uint64_t  kHeld = 4 * kRows;

            Assert::AreEqual ((uint64_t) 0,     DebuggerViewState::GetTraceWindowFirst (0,     std::nullopt, kRows));
            Assert::AreEqual ((uint64_t) 0,     DebuggerViewState::GetTraceWindowFirst (100,   std::nullopt, kRows), L"fewer than a window");
            Assert::AreEqual (kHeld - kRows,    DebuggerViewState::GetTraceWindowFirst (kHeld, std::nullopt, kRows));
            Assert::AreEqual ((uint64_t) 500,   DebuggerViewState::GetTraceWindowFirst (kHeld, 500,          kRows));
            Assert::AreEqual (kHeld - kRows,    DebuggerViewState::GetTraceWindowFirst (kHeld, kHeld - 10,   kRows), L"never past the newest");
        }



        TEST_METHOD (TheSnapshotHoldsTheWindowForTheScrollPosition)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;
            uint64_t              newest   = 0;



            FillTrace (rig);

            snapshot = rig.view.Build (rig.controller.GetSession());
            Assert::IsTrue   (snapshot.trace.isOn);
            Assert::AreEqual (kTotal,                     snapshot.trace.total);
            Assert::AreEqual (kTotal - kRows,             snapshot.trace.first, L"following the end");
            Assert::AreEqual ((size_t) kRows,             snapshot.trace.entries.size());
            newest = snapshot.trace.entries.back().cycles;

            rig.view.SetTraceTop (100);
            snapshot = rig.view.Build (rig.controller.GetSession());
            Assert::AreEqual ((uint64_t) 100,             snapshot.trace.first);
            Assert::IsTrue   (snapshot.trace.entries.front().cycles < newest, L"older than the window at the end");
            Assert::AreEqual ((size_t) kRows,             snapshot.trace.entries.size(), L"a window, never the whole trace");
            Assert::AreEqual (std::string ("JMP $0300"),  snapshot.trace.entries.front().instruction);
        }



        TEST_METHOD (ThePaneAsksOnlyWhenItsRowsLeaveTheWindow)
        {
            static constexpr int  kVisible = 20;



            Assert::AreEqual (TracePane::kFollowEnd, TracePane::GetReadStartFor (872, 128, 980, kVisible, true).value(), L"the end follows the newest");
            Assert::IsFalse  (TracePane::GetReadStartFor (500, 128, 510, kVisible, false).has_value(),                  L"rows inside the window");
            Assert::AreEqual ((uint64_t) 384, TracePane::GetReadStartFor (500, 128, 400, kVisible, false).value(),      L"above it, with a lead");
            Assert::AreEqual ((uint64_t) 604, TracePane::GetReadStartFor (500, 128, 620, kVisible, false).value(),      L"below it");
            Assert::AreEqual ((uint64_t) 0,   TracePane::GetReadStartFor (500, 128, 4,   kVisible, false).value(),      L"never before the first entry");
        }



        //  Every row of a tall pane, as at 50% text zoom on a tall monitor,
        //  fits in one read; and the read a pane has is never requested
        //  again.
        TEST_METHOD (ATallTracePaneIsHeldByOneRead)
        {
            static constexpr int       kTall = 300;
            static constexpr uint64_t  kTop  = 1000;
            std::optional<uint64_t>    start = TracePane::GetReadStartFor (0, DebuggerViewState::kTraceRows, kTop, kTall, false);



            Assert::IsTrue  (start.has_value(), L"rows outside the read");
            Assert::IsFalse (TracePane::GetReadStartFor (*start, DebuggerViewState::kTraceRows, kTop, kTall, false).has_value(), L"the read that came holds them");
            Assert::IsFalse (TracePane::GetReadStartFor (*start, DebuggerViewState::kTraceRows, kTop, 10 * DebuggerViewState::kTraceRows, false).has_value(),
                             L"more rows than a read holds ask for the same read no more");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MemoryWindowTests
    //
    //  Up to four memory windows (FR-034), each read on the CPU thread from its
    //  own address into the snapshot, a region for every byte.
    //
    ////////////////////////////////////////////////////////////////////////////////





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ControlTests
    //
    ////////////////////////////////////////////////////////////////////////////////





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WindowCommandTests
    //
    //  The AppleWin names that need the window, carried out on its panes.
    //
    ////////////////////////////////////////////////////////////////////////////////





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ActionTests
    //
    //  What each keyboard-scheme action sends. A key and the button it stands
    //  for produce the same command line, so a key is only a faster click.
    //
    ////////////////////////////////////////////////////////////////////////////////





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CadenceTests
    //
    //  When the CPU thread rebuilds the snapshot. A running machine gets one
    //  every frame, so a device panel or the registers never lag the screen by
    //  more than that; a stopped one is rebuilt only when something changes it,
    //  which is an action from a way in or the machine stopping or starting.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CadenceTests)
    {
    public:

        static constexpr ULONGLONG  kFrame = DebuggerViewState::kBuildIntervalMs;



        TEST_METHOD (ARunningMachineIsRebuiltEveryFrame)
        {
            Assert::IsTrue  (DebuggerViewState::IsBuildDue (false, false, false, 1000 + kFrame,     1000));
            Assert::IsFalse (DebuggerViewState::IsBuildDue (false, false, false, 1000 + kFrame - 1, 1000));
        }


        TEST_METHOD (TheIntervalIsAtMostOneFrame)
        {
            //  FR-051: a panel updates at least once per frame, at 60 Hz.
            Assert::IsTrue (kFrame <= 1000 / 60);
        }


        TEST_METHOD (AStoppedMachineIsNotRebuiltForTimeAlone)
        {
            Assert::IsFalse (DebuggerViewState::IsBuildDue (false, true, true, 1000 + 60000, 1000));
        }


        TEST_METHOD (StoppingOrStartingIsDueAtOnce)
        {
            Assert::IsTrue (DebuggerViewState::IsBuildDue (false, true,  false, 1000, 1000),  L"a breakpoint just stopped it");
            Assert::IsTrue (DebuggerViewState::IsBuildDue (false, false, true,  1000, 1000),  L"it was just resumed");
        }


        TEST_METHOD (AnActionIsDueAtOnce)
        {
            Assert::IsTrue (DebuggerViewState::IsBuildDue (true, true,  true,  1000, 1000));
            Assert::IsTrue (DebuggerViewState::IsBuildDue (true, false, false, 1000, 1000));
        }


        TEST_METHOD (ARunningMachineWaitsForTheWindowToTakeTheLastSnapshot)
        {
            //  A snapshot the window has not taken yet would only be thrown
            //  away by the next, so time alone builds none until it is taken.
            Assert::IsFalse (DebuggerViewState::IsBuildDue (false, false, false, false, 1000 + 10 * kFrame, 1000), L"not taken");
            Assert::IsTrue  (DebuggerViewState::IsBuildDue (false, false, false, true,  1000 + kFrame,      1000), L"taken");

            Assert::IsTrue  (DebuggerViewState::IsBuildDue (true,  false, false, false, 1000, 1000), L"an action is due all the same");
            Assert::IsTrue  (DebuggerViewState::IsBuildDue (false, true,  false, false, 1000, 1000), L"so is a stop");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourcePaneTests
    //
    //  The source pane's share of the snapshot and the pane itself (FR-054 to
    //  FR-059). The debug file matches the rig's program: main.a65 line 2 is
    //  the LDA, line 3 a macro invocation whose body, macros.inc line 5, is
    //  the STA, and line 4 the RTS.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static const char * const  s_kMainText = "; main\n        lda #$41\n        store\n        rts\n";

    TEST_CLASS (SourcePaneTests)
    {
    public:

        static void LoadDebugFile (MachineRig & rig, Word start = 0x0300)
        {
            DebugFile  file;



            file.major = 2;
            file.files = { { 0, "main.a65", 45, 0, "", 0 }, { 1, "macros.inc", 30, 0, "", 0 } };
            file.segments.push_back ({ 0, "CODE", start, 6 });
            file.spans = { { 0, 0, 0, 2 }, { 1, 0, 2, 3 }, { 2, 0, 5, 1 } };
            file.lines = { { 0, 0, 2, DebugLineType::Asm,   0, { 0 } },
                           { 1, 0, 3, DebugLineType::Asm,   0, { 1 } },
                           { 2, 1, 5, DebugLineType::Macro, 1, { 1 } },
                           { 3, 0, 4, DebugLineType::Asm,   0, { 2 } } };

            rig.controller.GetSession().GetFiles().SetDebugFile (std::move (file), L"C:\\Work\\main.dbg", "key");
        }



        //  Focus moving between the source and the disassembly switches how
        //  steps go, in whatever mode the session is in, and runs nothing.
        //  The action's echo is a line that mode's console runs too.
        TEST_METHOD (TheSourceStepActionWorksInEveryMode)
        {
            for (CommandMode mode : { CommandMode::AppleWin, CommandMode::Monitor, CommandMode::GSSquared, CommandMode::WinDbg })
            {
                MachineRig    rig;
                Reply         reply;
                std::wstring  where = std::format (L"mode {}", (int) mode);



                LoadDebugFile (rig);

                rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(), RunActions::GetSourceStepping (true, mode));
                Assert::IsTrue   (rig.controller.GetSession().GetFiles().IsStepBySource(), where.c_str());

                rig.view.GetCommands().ExecuteAction (rig.controller.GetSession(), RunActions::GetSourceStepping (false, mode));
                Assert::IsFalse  (rig.controller.GetSession().GetFiles().IsStepBySource(), where.c_str());

                reply = rig.Run (RunActions::GetSourceStepping (true, mode).echo, mode);
                Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, where.c_str());
                Assert::IsTrue   (rig.controller.GetSession().GetFiles().IsStepBySource(), where.c_str());

                Assert::AreEqual ((int) 0x0300, (int) rig.controller.GetSession().GetTarget().GetRegisters().pc, where.c_str());
            }
        }



        static void SetPc (MachineRig & rig, Word pc)
        {
            Cpu6502Registers  r = rig.controller.GetSession().GetTarget().GetRegisters();



            r.pc = pc;
            rig.controller.GetSession().GetTarget().SetRegisters (r);
        }



        TEST_METHOD (NoDebugFileNoSourceState)
        {
            MachineRig  rig;



            Assert::IsFalse (rig.view.Build (rig.controller.GetSession()).source.has_value());
        }


        TEST_METHOD (TheLineAtPcAndEachCodeRowsLine)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            LoadDebugFile (rig);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsTrue   (snapshot.source.has_value());
            Assert::AreEqual (0, snapshot.source->fileId);
            Assert::AreEqual (2, snapshot.source->line);
            Assert::AreEqual (0, snapshot.source->depth);
            Assert::AreEqual (2, LineAt (snapshot, 0x0300).sourceLine);
            Assert::AreEqual (3, LineAt (snapshot, 0x0302).sourceLine, L"the outermost line: the invocation, not the body");
            Assert::AreEqual (std::wstring (L"C:\\Work\\main.dbg"), snapshot.source->debugFilePath);
        }


        TEST_METHOD (ThePaintedCodeViewsCarryTheirSourceLines)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;
            bool                  found = false;



            LoadDebugFile (rig);
            snapshot = rig.view.Build (rig.controller.GetSession());

            //  The window reads each view's rows, not the copy in `code`.
            for (const DebuggerViewSnapshot::CodeLine & row : snapshot.codeViews[0])
            {
                if (row.address == 0x0302)
                {
                    Assert::AreEqual (0, row.sourceFileId);
                    Assert::AreEqual (3, row.sourceLine);
                    found = true;
                }
            }

            Assert::IsTrue (found);
        }


        TEST_METHOD (InsideAMacroBothEndsAreGiven)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;
            std::vector<int>      lines;



            LoadDebugFile (rig);
            SetPc (rig, 0x0302);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual (3, snapshot.source->line);
            Assert::AreEqual (1, snapshot.source->bodyFileId);
            Assert::AreEqual (5, snapshot.source->bodyLine);
            Assert::AreEqual (1, snapshot.source->depth);
        }


        TEST_METHOD (LinesMapToTheirFirstAddress)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;
            DebuggerViewSnapshot  again;



            LoadDebugFile (rig);
            snapshot = rig.view.Build (rig.controller.GetSession());
            again    = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((Word) 0x0302, snapshot.source->lineAddresses->at ({ 0, 3 }));
            Assert::AreEqual ((Word) 0x0302, snapshot.source->lineAddresses->at ({ 1, 5 }));
            Assert::AreEqual ((Word) 0x0305, snapshot.source->lineAddresses->at ({ 0, 4 }));
            Assert::IsTrue   (snapshot.source->lineAddresses == again.source->lineAddresses, L"built once per load");
        }


        //  The same file loaded again at another offset moves every line.
        TEST_METHOD (ReloadingAtAnotherOffsetMovesTheLines)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            LoadDebugFile (rig);
            snapshot = rig.view.Build (rig.controller.GetSession());
            LoadDebugFile (rig, 0x1300);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::AreEqual ((Word) 0x1305, snapshot.source->lineAddresses->at ({ 0, 4 }));
        }


        TEST_METHOD (ABreakpointMarksEveryLineAtItsAddress)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;
            int                   id       = 0;



            LoadDebugFile (rig);
            rig.Run ("BP 302");
            snapshot = rig.view.Build (rig.controller.GetSession());
            id       = snapshot.breakpoints.at (0).id;

            Assert::AreEqual ((size_t) 2, snapshot.source->breakpointLines.size(), L"the invocation and the body line");
            Assert::AreEqual (std::format ("BPC {}", id), SourceLineLookup::GetToggleLine (*snapshot.source, 0, 3));
            Assert::AreEqual (std::format ("BPC {}", id), SourceLineLookup::GetToggleLine (*snapshot.source, 1, 5));
            Assert::AreEqual (std::string ("BP main.a65:4"), SourceLineLookup::GetToggleLine (*snapshot.source, 0, 4));
        }


        //  A dropped file with no record has no lines to set a breakpoint on.
        TEST_METHOD (AFileWithNoRecordTogglesNothing)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            LoadDebugFile (rig);
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsTrue (SourceLineLookup::GetToggleLine (*snapshot.source, -1, 4).empty());
        }


        TEST_METHOD (SplitLinesExpandsTabsAndEveryLineEnding)
        {
            std::vector<std::wstring>  lines = SourceRows::SplitLines ("a\tb\r\nc\rd\n\te");



            Assert::AreEqual ((size_t) 4,                   lines.size());
            Assert::AreEqual (std::wstring (L"a   b"),      lines[0], L"to the next multiple of four, not four spaces");
            Assert::AreEqual (std::wstring (L"c"),          lines[1]);
            Assert::AreEqual (std::wstring (L"    e"),      lines[3]);
        }


        TEST_METHOD (RowsCarryTheMarkersAndLineNumbers)
        {
            std::vector<std::wstring>        lines (12, L"x");
            std::vector<DxuiTextView::Row>   rows  = SourceRows::BuildRows (lines, 3, { 3, 10 });



            Assert::AreEqual ((size_t) 12, rows.size());
            Assert::AreEqual (std::wstring (L" 3"), rows[2].cells[1], L"numbers right-aligned to the widest");
            Assert::AreEqual (std::wstring (1, s_kchBullet) + s_kpszTriangleRight, rows[2].cells[0]);
            Assert::AreEqual (std::wstring (1, s_kchBullet) + L" ",                rows[9].cells[0]);
            Assert::AreEqual (std::wstring (L"  "),                                 rows[0].cells[0]);
        }


        TEST_METHOD (TheBannerSaysWhatTheFileNeedsSaid)
        {
            Assert::IsTrue (SourceBannerText::GetBannerText (SourceMatch::Exact, "a.s", true, 0, false, "", 0).empty());
            Assert::IsTrue (SourceBannerText::GetBannerText (SourceMatch::Mismatch, "a.s", true, 0, false, "", 0).find (L"may not match") != std::wstring::npos);
            Assert::IsTrue (SourceBannerText::GetBannerText (SourceMatch::NotFound, "a.s", false, 0, false, "", 0).find (L"Drop it") != std::wstring::npos);
            Assert::IsTrue (SourceBannerText::GetBannerText (SourceMatch::NotFound, "a.s", true, 0, false, "", 0).find (L"no line mapping") != std::wstring::npos);
            Assert::IsTrue (SourceBannerText::GetBannerText (SourceMatch::Exact, "a.s", true, 1, false, "m.inc", 5).find (L"m.inc line 5") != std::wstring::npos);

            //  Two things to say go on two lines, not run together.
            Assert::AreEqual (std::wstring (L"a.s is not the file that was assembled, so lines may not match.\n"
                                            L"Stopped inside a macro. Its body line is a.s line 5."),
                              SourceBannerText::GetBannerText (SourceMatch::Mismatch, "a.s", true, 1, false, "a.s", 5));
        }


        //  A body in the file that could not be found cannot be shown; one in a
        //  file that could, or in another file, can.
        TEST_METHOD (AMacroBodyInTheMissingFileIsNotOffered)
        {
            Assert::IsFalse (SourceMacroPlaces::CanShowBody (false, 1, 0, 0), L"the body is in the missing file");
            Assert::IsTrue  (SourceMacroPlaces::CanShowBody (false, 1, 1, 0), L"the body is in another file");
            Assert::IsTrue  (SourceMacroPlaces::CanShowBody (true,  1, 0, 0), L"the file is shown");
            Assert::IsFalse (SourceMacroPlaces::CanShowBody (true,  0, 0, 0), L"not in a macro");
        }


        TEST_METHOD (ThePaneLoadsTheFileAtPcAndMarksItsLine)
        {
            std::unique_ptr<MachineRig>  rig = std::make_unique<MachineRig>();

            DxuiTextView    view;
            DxuiInfoBanner  banner;
            int             finds   = 0;
            bool            toggled = false;
            std::string     ran;
            SourcePane            pane (&view, &banner,
                                        [&] (const DebugSourceFile & record, const std::wstring &, const std::string &)
                                        {
                                            SourceLookup  lookup;

                                            finds++;
                                            lookup.match = SourceMatch::Exact;
                                            lookup.text  = (record.id == 0) ? s_kMainText : "; macros\n\n\n\n        sta $0400\n";
                                            return lookup;
                                        },
                                        [&] (const DebuggerActionBuilder & build) { ran = build (CommandMode::AppleWin).echo; });
            DebuggerViewSnapshot  snapshot;
            std::vector<int>      lines;



            LoadDebugFile (*rig);
            SetPc (*rig, 0x0302);
            snapshot = rig->view.Build (rig->controller.GetSession());
            pane.SetOnToggleBody ([&toggled] { toggled = true; });
            pane.SetFile (snapshot.source->fileId);
            pane.Apply (snapshot);
            pane.Apply (snapshot);

            Assert::AreEqual (1,          finds,                      L"found once, not every snapshot");
            Assert::IsTrue   (pane.IsActive());
            lines = SourceRows::GetRowLines (view.GetRows());
            Assert::AreEqual ((ptrdiff_t) 4, std::count_if (lines.begin(), lines.end(), [] (int line) { return line > 0; }), L"a row for each line, and the instructions below them");
            Assert::AreEqual (std::wstring (L" ") + s_kpszTriangleRight, view.GetRows()[(size_t) (std::find (lines.begin(), lines.end(), 3) - lines.begin())].cells[0], L"the invocation line");
            Assert::IsTrue   (pane.HasBanner(),                           L"inside a macro");

            pane.ToggleBody();
            Assert::IsTrue (toggled, L"the choice is the window's, for every document");

            //  The body's file, as its own document shows it with the body chosen.
            pane.SetFile     (snapshot.source->bodyFileId);
            pane.SetShowBody (true);
            pane.Apply       (snapshot);
            Assert::AreEqual ((size_t) 5, view.GetRows().size(),         L"the body's file, with no instruction rows under its lines");
            Assert::AreEqual (std::wstring (L" ") + s_kpszTriangleRight, view.GetRows()[4].cells[0]);
        }


        TEST_METHOD (ADroppedFileThatMatchesNothingIsPlainText)
        {
            MachineRig      rig;
            DxuiTextView    view;
            DxuiInfoBanner  banner;
            SourceLookup    dropped;
            SourcePane            pane (&view, &banner,
                                        [] (const DebugSourceFile &, const std::wstring &, const std::string &) { return SourceLookup(); },
                                        [] (const DebuggerActionBuilder &) {});



            LoadDebugFile (rig);
            pane.SetFile (0);
            pane.Apply (rig.view.Build (rig.controller.GetSession()));
            Assert::IsTrue (banner.GetText().find (L"main.a65 was not found") != std::wstring::npos);

            dropped.text = "hello\nworld\n";
            pane.ShowDropped (dropped, -1);
            pane.Apply (rig.view.Build (rig.controller.GetSession()));

            Assert::AreEqual ((size_t) 2, view.GetRows().size(), L"the dropped text stays while the PC is in the same file");
            Assert::IsTrue   (banner.GetText().find (L"no line mapping") != std::wstring::npos);
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DiagnosticsPanelTests
    //
    //  Device panels in the window's snapshot, the PANEL command, and the rows
    //  a panel draws, all from a provider that stands for no real device.
    //
    ////////////////////////////////////////////////////////////////////////////////





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ViewQueryTests
    //
    //  The lines the panes run to fill themselves are the window's own, not
    //  the user's: they neither go to the line assembler nor move where a bare
    //  U or D continues.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ViewQueryTests)
    {
    public:

        static Word FirstListed (const Reply & reply)
        {
            HRESULT                  hr       = S_OK;
            const DisassemblyData  * data     = std::get_if<DisassemblyData> (&reply.data);
            bool                     hasLines = false;
            Word                     address  = 0;



            Assert::IsNotNull (data);
            CBR (data != nullptr);

            hasLines = !data->lines.empty();
            Assert::IsTrue (hasLines);
            CBR (hasLines);

            address = data->lines[0].instruction.address;

        Error:
            return address;
        }



        static Word FirstDumped (const Reply & reply)
        {
            HRESULT             hr      = S_OK;
            const MemoryData  * data    = std::get_if<MemoryData> (&reply.data);
            bool                hasRows = false;
            Word                address = 0;



            Assert::IsNotNull (data);
            CBR (data != nullptr);

            hasRows = !data->rows.empty();
            Assert::IsTrue (hasRows);
            CBR (hasRows);

            address = data->rows[0].address;

        Error:
            return address;
        }



        TEST_METHOD (APaneIsFilledWhileTheUserAssembles)
        {
            MachineRig            rig;
            DebuggerViewSnapshot  snapshot;



            rig.Run ("A 800");
            snapshot = rig.view.Build (rig.controller.GetSession());

            Assert::IsFalse  (snapshot.registers.empty(), L"the registers pane");
            Assert::IsFalse  (snapshot.code.empty(),      L"the code pane");
            Assert::IsFalse  (snapshot.memory.empty(),    L"the memory pane");
            Assert::IsTrue   (snapshot.isAssembling);
            Assert::IsTrue   (rig.controller.GetSession().GetInterpreter().IsAssembling(), L"the user's assembly goes on");

            rig.Run ("NOP");
            rig.Run ("");
            Assert::AreEqual ((int) 0xEA, (int) rig.machine.GetMemoryBus().ReadByte (0x0800));
            Assert::IsFalse  (rig.controller.GetSession().GetInterpreter().IsAssembling());
        }



        TEST_METHOD (ABareUOrDContinuesTheUsersListingAfterARebuild)
        {
            auto  plain   = std::make_unique<MachineRig>();   // heap: two rigs overflow the frame (C6262)
            auto  rebuilt = std::make_unique<MachineRig>();
            Word  listed  = 0;
            Word  dumped  = 0;



            plain->Run ("U 1000");
            plain->Run ("D 2000");
            listed = FirstListed (plain->Run ("U"));
            dumped = FirstDumped (plain->Run ("D"));

            rebuilt->Run ("U 1000");
            rebuilt->Run ("D 2000");
            (void) rebuilt->view.Build (rebuilt->controller.GetSession());

            Assert::AreEqual ((int) listed, (int) FirstListed (rebuilt->Run ("U")));
            Assert::AreEqual ((int) dumped, (int) FirstDumped (rebuilt->Run ("D")));
        }



        TEST_METHOD (ReturnAndSpaceEndAnAssemblyFromTheBox)
        {
            DebuggerViewSnapshot  snapshot;



            Assert::IsFalse (DebuggerCommandLines::DoesAssemblerKeepKey (&snapshot, VK_RETURN, false, false));

            snapshot.isAssembling = true;
            Assert::IsTrue  (DebuggerCommandLines::DoesAssemblerKeepKey (&snapshot, VK_RETURN, false, false));
            Assert::IsTrue  (DebuggerCommandLines::DoesAssemblerKeepKey (&snapshot, VK_SPACE,  false, false));
            Assert::IsFalse (DebuggerCommandLines::DoesAssemblerKeepKey (&snapshot, VK_F5,     false, false), L"a function key is still the scheme's");
            Assert::IsFalse (DebuggerCommandLines::DoesAssemblerKeepKey (&snapshot, VK_SPACE,  true,  false));
            Assert::IsFalse (DebuggerCommandLines::DoesAssemblerKeepKey (nullptr,   VK_RETURN, false, false));
        }
    };
}
