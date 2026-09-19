#include "Pch.h"

#include "Assembler.h"
#include "Debugger/DebugFileWriter.h"
#include "Debugger/Handlers/ExecutionHandlers.h"
#include "Debugger/StepFilter.h"
#include "HandlerTestRig.h"
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  StepFilterTests
//
//  The step filter (FR-070): a step into a JSR whose target is in the filter
//  runs the call as a step over (R-033), by instruction and by source line.
//  SKIP sets, lists, removes and clears it, in AppleWin mode and through `/`
//  in Monitor mode.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    static const char * const  s_kFilterProgram =
        "        .org $0300\n"     //  1
        "start   ldx #0\n"         //  2
        "        jsr inc2\n"       //  3
        "after   nop\n"            //  4
        "        jsr spin\n"       //  5
        "        nop\n"            //  6
        "done    jmp done\n"       //  7
        "inc2    inx\n"            //  8
        "        inx\n"            //  9
        "        rts\n"            // 10
        "spin    jmp spin\n";      // 11



    TEST_CLASS (StepFilterTests)
    {
    public:

        using Rig = MachineHandlerRig<ExecutionHandlers>;

        struct Program
        {
            AssemblyResult  result;
            Word            Symbol (const char * name) const { return result.symbols.at (name); }
        };



        //  Loads the program with its debug file, the PC at the JSR to inc2,
        //  and INC2 and SPIN in the user symbol table.
        static Program Load (Rig & rig)
        {
            TestCpu           cpu;
            AssemblerOptions  opts;
            Program           program;
            Word              at      = 0x0300;



            program.result = Assembler (cpu.GetInstructionSet(), opts).Assemble (s_kFilterProgram);
            Assert::IsTrue (program.result.success);

            for (Byte b : program.result.bytes)
            {
                rig.target.TryPoke (at++, b);
            }

            rig.Load (0x0300, {}, 0x0302);
            rig.session.SetDebugFile (DebugFileWriter::Build (program.result, { { "", { "prog.a65", 0 } } }), L"C:\\Work\\prog.dbg");
            rig.session.GetSymbols().Add (SymbolTableId::User, "INC2", program.Symbol ("inc2"));
            rig.session.GetSymbols().Add (SymbolTableId::User, "SPIN", program.Symbol ("spin"));
            return program;
        }



        static Word Step (Rig & rig, const std::string & command)
        {
            rig.RunOk (command);
            return rig.LastStop().pc;
        }



        static const std::vector<StepFilterEntry> & Entries (Rig & rig)
        {
            return rig.session.GetStepFilter().GetEntries();
        }



        //  The model.

        TEST_METHOD (ContainsMatchesAnAddressAndARange)
        {
            StepFilter  filter;



            filter.Add ("COUT", 0xFDED, 0xFDED);
            filter.Add ("",     0xC300, 0xC3FF);

            Assert::IsTrue  (filter.Contains (0xFDED));
            Assert::IsFalse (filter.Contains (0xFDEE));
            Assert::IsTrue  (filter.Contains (0xC300));
            Assert::IsTrue  (filter.Contains (0xC3FF));
            Assert::IsFalse (filter.Contains (0xC400));
        }


        TEST_METHOD (AddingTheSameRangeTwiceKeepsOneEntry)
        {
            StepFilter  filter;



            filter.Add ("",     0xFDED, 0xFDED);
            filter.Add ("COUT", 0xFDED, 0xFDED);

            Assert::AreEqual ((size_t) 1,           filter.GetEntries().size());
            Assert::AreEqual (std::string ("COUT"), filter.GetEntries()[0].name);
        }


        TEST_METHOD (RemoveByNameOrByRange)
        {
            StepFilter  filter;



            filter.Add ("COUT", 0xFDED, 0xFDED);
            filter.Add ("",     0xF800, 0xFFFF);

            Assert::IsTrue  (filter.TryRemove ("cout", std::nullopt), L"names match without regard to case");
            Assert::IsFalse (filter.TryRemove ("RDKEY", std::nullopt));
            Assert::IsTrue  (filter.TryRemove ("F800.FFFF", std::pair<Word, Word> (0xF800, 0xFFFF)));
            Assert::IsTrue  (filter.IsEmpty());
        }


        //  Instruction steps.

        TEST_METHOD (StepIntoAFilteredNameStepsOver)
        {
            Rig      rig;
            Program  program = Load (rig);



            rig.RunOk ("SKIP INC2");

            Assert::AreEqual (program.Symbol ("after"), Step (rig, "T"), L"the call runs to its return");
            Assert::AreEqual ((Byte) 2,                 rig.LastStop().registers.x);
            Assert::AreEqual ((Byte) 0xFF,              rig.LastStop().registers.sp);
        }


        TEST_METHOD (StepIntoAFilteredAddressStepsOver)
        {
            Rig      rig;
            Program  program = Load (rig);



            rig.RunOk (std::format ("SKIP {:04X}", program.Symbol ("inc2")));

            Assert::AreEqual (program.Symbol ("after"), Step (rig, "T"));
            Assert::IsTrue   (Entries (rig)[0].name.empty(), L"an address has no name");
        }


        TEST_METHOD (StepIntoAFilteredRangeStepsOver)
        {
            Rig      rig;
            Program  program = Load (rig);
            Word     inc2    = program.Symbol ("inc2");



            rig.RunOk (std::format ("SKIP ${:04X}.${:04X}", inc2, inc2 + 2));

            Assert::AreEqual ((Word) inc2,      Entries (rig)[0].first);
            Assert::AreEqual ((Word) (inc2 + 2), Entries (rig)[0].last);
            Assert::AreEqual (program.Symbol ("after"), Step (rig, "T"));
        }


        TEST_METHOD (StepIntoAnUnfilteredRoutineStopsInside)
        {
            Rig      rig;
            Program  program = Load (rig);



            rig.RunOk ("SKIP SPIN");

            Assert::AreEqual (program.Symbol ("inc2"), Step (rig, "T"), L"only the filtered routine is stepped over");
        }


        TEST_METHOD (StepIntoAfterClearStopsInside)
        {
            Rig      rig;
            Program  program = Load (rig);



            rig.RunOk ("SKIP INC2");
            rig.RunOk ("SKIP CLEAR");

            Assert::IsTrue   (Entries (rig).empty());
            Assert::AreEqual (program.Symbol ("inc2"), Step (rig, "T"));
        }


        TEST_METHOD (StepIntoAfterRemoveStopsInside)
        {
            Rig      rig;
            Program  program = Load (rig);



            rig.RunOk ("SKIP INC2");
            rig.RunOk ("SKIP - inc2");
            Assert::IsTrue (Entries (rig).empty());

            rig.RunOk ("SKIP INC2");
            rig.RunOk ("SKIP -INC2");
            Assert::IsTrue (Entries (rig).empty());

            Assert::AreEqual (program.Symbol ("inc2"), Step (rig, "T"));
        }


        TEST_METHOD (AFilteredRoutineThatNeverReturnsLeavesTheRunGoing)
        {
            Rig               rig;
            Program           program   = Load (rig);
            Cpu6502Registers  registers = rig.target.GetRegisters();



            registers.pc = program.Symbol ("after") + 1;
            rig.target.SetRegisters (registers);

            rig.RunOk ("SKIP SPIN");
            rig.RunOk ("BUDGET 5000");

            Assert::AreEqual (program.Symbol ("spin"), Step (rig, "T"), L"the step does not end inside the loop");
            Assert::AreEqual ((int) StopReason::Budget, (int) rig.LastStop().reason);
        }


        TEST_METHOD (StepOverIsUnchangedByTheFilter)
        {
            Rig      rig;
            Program  program = Load (rig);



            rig.RunOk ("SKIP INC2");

            Assert::AreEqual (program.Symbol ("after"), Step (rig, "P"));
        }


        //  Source steps.

        TEST_METHOD (SourceStepIntoAFilteredRoutineStopsAtTheNextLine)
        {
            Rig      rig;
            Program  program = Load (rig);



            rig.RunOk ("SRC ON");
            rig.RunOk ("SKIP INC2");

            Assert::AreEqual (program.Symbol ("after"), Step (rig, "T"));
            Assert::AreEqual (4,                        rig.LastStop().sourceLine);
        }


        TEST_METHOD (SourceStepIntoAnUnfilteredRoutineStopsInside)
        {
            Rig      rig;
            Program  program = Load (rig);



            rig.RunOk ("SRC ON");

            Assert::AreEqual (program.Symbol ("inc2"), Step (rig, "T"));
            Assert::AreEqual (8,                       rig.LastStop().sourceLine);
        }


        //  Commands and replies.

        TEST_METHOD (SkipListsTheFilter)
        {
            Rig      rig;
            Program  program = Load (rig);
            Reply    reply;



            reply = rig.RunOk ("SKIP");
            Assert::IsTrue   (std::holds_alternative<StepFilterData> (reply.data));
            Assert::AreEqual (std::string ("The step filter is empty."), reply.text.at (0));

            rig.RunOk ("SKIP INC2");
            rig.RunOk ("SKIP F800.FFFF");
            rig.RunOk ("SKIP FDED");

            reply = rig.RunOk ("SKIP");
            Assert::AreEqual ((size_t) 3,                                                            reply.text.size());
            Assert::AreEqual (std::format ("INC2         ${:04X}", program.Symbol ("inc2")),        reply.text[0]);
            Assert::AreEqual (std::string ("$F800-$FFFF"),                                           reply.text[1]);
            Assert::AreEqual (std::string ("$FDED"),                                                 reply.text[2]);
        }


        TEST_METHOD (SkipAnUnknownNameIsAnError)
        {
            Rig  rig;



            Load (rig);

            rig.RunFails ("SKIP NOSUCH", "invalid arguments");
            Assert::IsTrue (Entries (rig).empty());
        }


        TEST_METHOD (SkipABackwardRangeIsAnError)
        {
            Rig  rig;



            Load (rig);

            rig.RunFails ("SKIP FFFF.F800", "invalid arguments");
        }


        TEST_METHOD (SkipRemovingWhatIsNotThereIsAnError)
        {
            Rig  rig;



            Load (rig);

            rig.RunFails ("SKIP - INC2", "not in the step filter");
        }


        TEST_METHOD (SkipThroughTheMonitorSlash)
        {
            Rig      rig;
            Program  program = Load (rig);
            Reply    reply;



            rig.RunOk ("MODE MONITOR");
            rig.RunOk ("/skip inc2");

            reply = rig.RunOk ("/skip");
            Assert::IsTrue   (std::holds_alternative<StepFilterData> (reply.data));
            Assert::AreEqual (std::format ("inc2         ${:04X}", program.Symbol ("inc2")), reply.text.at (0));

            rig.RunOk ("/skip clear");
            Assert::IsTrue (Entries (rig).empty());
        }
    };
}
