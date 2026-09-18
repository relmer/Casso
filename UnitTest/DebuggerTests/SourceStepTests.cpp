#include "Pch.h"

#include "Assembler.h"
#include "Debugger/DebugFileWriter.h"
#include "Debugger/Handlers/BreakpointHandlers.h"
#include "Debugger/Handlers/ExecutionHandlers.h"
#include "HandlerTestRig.h"
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SourceStepTests
//
//  Step over and step out by the stack pointer (R-033, SC-011), and stepping
//  by source line (FR-056): the same T, P and RTS, with SRC ON.
//
//  The program has a call followed by inline parameters, a recursive call, a
//  routine that discards its return address, a macro whose two lines each
//  assemble an instruction, and an interrupt handler.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    static const char * const  s_kProgram =
        "        .org $0300\n"     //  1
        "twoinx  macro\n"          //  2
        "        inx\n"            //  3
        "        inx\n"            //  4
        "        endm\n"           //  5
        "start   ldx #0\n"         //  6
        "        jsr inline\n"     //  7
        "        .byte 1, 2\n"     //  8
        "after   twoinx\n"         //  9
        "        lda #3\n"         // 10
        "        jsr recurse\n"    // 11
        "next    nop\n"            // 12
        "        jsr discard\n"    // 13
        "back    nop\n"            // 14
        "done    jmp done\n"       // 15
        "inline  pla\n"            // 16
        "        clc\n"            // 17
        "        adc #2\n"         // 18
        "        tay\n"            // 19
        "        pla\n"            // 20
        "        adc #0\n"         // 21
        "        pha\n"            // 22
        "        tya\n"            // 23
        "        pha\n"            // 24
        "        rts\n"            // 25
        "recurse sec\n"            // 26
        "        sbc #1\n"         // 27
        "        beq rdone\n"      // 28
        "        jsr recurse\n"    // 29
        "rdone   rts\n"            // 30
        "discard pla\n"            // 31
        "        pla\n"            // 32
        "dtail   jmp back\n"       // 33
        "irqh    rti\n";           // 34



    TEST_CLASS (SourceStepTests)
    {
    public:

        using Rig = MachineHandlerRig<ExecutionHandlers>;

        struct Program
        {
            AssemblyResult  result;
            Word            Symbol (const char * name) const { return result.symbols.at (name); }
        };



        static Program Load (Rig & rig, Word start = 0x0300)
        {
            TestCpu           cpu;
            AssemblerOptions  opts;
            Program           program;
            Word              at      = 0x0300;



            program.result = Assembler (cpu.GetInstructionSet(), opts).Assemble (s_kProgram);
            Assert::IsTrue (program.result.success);

            for (Byte b : program.result.bytes)
            {
                rig.target.TryPoke (at++, b);
            }

            rig.Load (0x0300, {}, start);
            rig.session.SetDebugFile (DebugFileWriter::Build (program.result, { { "", { "prog.a65", 0 } } }), L"C:\\Work\\prog.dbg");
            return program;
        }



        static Word Step (Rig & rig, const char * command)
        {
            rig.RunOk (command);
            return rig.LastStop().pc;
        }



        //  Instruction stepping, the stack-pointer rule.

        TEST_METHOD (StepOverACallWithInlineParameters)
        {
            Rig      rig;
            Program  program = Load (rig, 0x0302);



            Assert::AreEqual (program.Symbol ("after"), Step (rig, "P"), L"the routine returns past its two parameter bytes");
            Assert::AreEqual ((Byte) 0xFF, rig.LastStop().registers.sp);
        }


        TEST_METHOD (StepOverARecursiveCall)
        {
            Rig               rig;
            Program           program   = Load (rig);
            Cpu6502Registers  registers = rig.target.GetRegisters();



            registers.pc = program.Symbol ("next") - 3;
            registers.a  = 3;
            rig.target.SetRegisters (registers);

            Assert::AreEqual (program.Symbol ("next"), Step (rig, "P"), L"three levels deep and back is one step");
            Assert::AreEqual ((Byte) 0,              rig.LastStop().registers.a);
        }


        TEST_METHOD (StepOverARoutineThatDiscardsItsReturn)
        {
            Rig               rig;
            Program           program   = Load (rig);
            Cpu6502Registers  registers = rig.target.GetRegisters();



            registers.pc = program.Symbol ("back") - 3;
            rig.target.SetRegisters (registers);

            Assert::AreEqual (program.Symbol ("back"), Step (rig, "P"),
                              L"the return address is pulled and the routine jumps away, which ends the call");
        }


        TEST_METHOD (StepOutReturnsOneLevel)
        {
            Rig               rig;
            Program           program   = Load (rig);
            Cpu6502Registers  registers = rig.target.GetRegisters();



            registers.pc = program.Symbol ("next") - 3;
            registers.a  = 3;
            rig.target.SetRegisters (registers);

            Assert::AreEqual (program.Symbol ("recurse"), Step (rig, "T"));

            //  sec, sbc, beq, then into the second level.
            Step (rig, "T");
            Step (rig, "T");
            Step (rig, "T");
            Assert::AreEqual (program.Symbol ("recurse"), Step (rig, "T"));

            Assert::AreEqual (program.Symbol ("rdone"), Step (rig, "RTS"), L"back in the first level, after its call");
            Assert::AreEqual (program.Symbol ("next"),  Step (rig, "RTS"), L"then out to the caller");
        }


        //  Source stepping.

        TEST_METHOD (SourceStepIntoStopsAtTheNextLine)
        {
            Rig      rig;
            Program  program = Load (rig);



            rig.RunOk ("SRC ON");

            Assert::AreEqual ((Word) 0x0302, Step (rig, "T"));
            Assert::AreEqual (7,             rig.LastStop().sourceLine);
            Assert::AreEqual (std::string ("prog.a65"), rig.LastStop().sourceFile);
            Assert::AreEqual (program.Symbol ("inline"), Step (rig, "T"), L"into the call");
        }


        TEST_METHOD (SourceStepIntoEntersAMacroBodyLineByLine)
        {
            Rig      rig;
            Program  program = Load (rig, 0x0302);



            rig.RunOk ("SRC ON");
            rig.RunOk ("P");

            Assert::AreEqual (program.Symbol ("after"),                  rig.LastStop().pc);
            Assert::AreEqual ((Word) (program.Symbol ("after") + 1),     Step (rig, "T"), L"the second body line");
            Assert::AreEqual (4,                                         rig.LastStop().sourceLine);
            Assert::AreEqual ((Word) (program.Symbol ("after") + 2),     Step (rig, "T"), L"then line 10");
            Assert::AreEqual (10,                                        rig.LastStop().sourceLine);
        }


        TEST_METHOD (SourceStepOverRunsAWholeMacroAndCall)
        {
            Rig      rig;
            Program  program = Load (rig, 0x0302);



            rig.RunOk ("SRC ON");

            Assert::AreEqual (program.Symbol ("after"),            Step (rig, "P"), L"the call and its parameters");
            Assert::AreEqual ((Word) (program.Symbol ("after") + 2), Step (rig, "P"), L"both of the macro's instructions");
            Step (rig, "P");
            Assert::AreEqual (program.Symbol ("next"),             Step (rig, "P"), L"the recursive call");
        }


        TEST_METHOD (SourceStepOverLeavesTheRoutineAtItsEnd)
        {
            Rig      rig;
            Program  program = Load (rig, 0x0302);



            rig.RunOk ("SRC ON");
            rig.RunOk ("T");
            Assert::AreEqual (program.Symbol ("inline"), rig.LastStop().pc);

            for (int i = 0; i < 9; i++)
            {
                rig.RunOk ("P");
            }

            Assert::AreEqual (program.Symbol ("after"), Step (rig, "P"), L"over the RTS, and on to the line after the parameters");
        }


        TEST_METHOD (SourceStepIntoDuringAnIrqLandsInTheHandler)
        {
            static constexpr Word  kIrqUserVector = 0x03FE;
            static constexpr Byte  kIrqEnabled    = 0x30;
            Rig                    rig;
            Program                program        = Load (rig);
            Cpu6502Registers       registers      = rig.target.GetRegisters();
            Word                   handler   = program.Symbol ("irqh");



            rig.target.TryPoke (kIrqUserVector,     (Byte) (handler & 0xFF));
            rig.target.TryPoke (kIrqUserVector + 1, (Byte) (handler >> 8));
            registers.p = kIrqEnabled;
            rig.target.SetRegisters (registers);
            rig.RunOk ("SRC ON");
            rig.machine.GetCpu()->SetInterruptLine (CpuInterruptKind::kMaskable, true);

            Assert::AreEqual (handler, Step (rig, "T"), L"the ROM's dispatch has no source and runs through; the handler has");
            Assert::AreEqual (34,      rig.LastStop().sourceLine);
        }


        TEST_METHOD (SrcOffStepsByInstruction)
        {
            Rig      rig;
            Program  program = Load (rig, 0x0302);



            rig.RunOk ("SRC ON");
            rig.RunOk ("SRC OFF");
            rig.RunOk ("P");

            Assert::AreEqual ((Word) (program.Symbol ("after") + 1), Step (rig, "T"), L"one instruction, even inside a line");
        }


        TEST_METHOD (SrcReportsTheLineAtPc)
        {
            Rig  rig;



            Load (rig, 0x0302);

            Assert::AreEqual (std::string ("$0302 is prog.a65 line 7."), rig.RunOk ("SRC").text.at (0));
            Assert::AreEqual (std::string ("Steps go by instruction."),   rig.RunOk ("SRC").text.at (1));
            Assert::AreEqual (std::string ("Steps go by source line."),   rig.RunOk ("SRC ON").text.at (1));
            rig.RunFails ("SRC SIDEWAYS", "invalid arguments");
        }


        TEST_METHOD (BreakpointOnASourceLine)
        {
            MachineHandlerRig<BreakpointHandlers>  rig;
            AssemblyResult                         result;
            TestCpu                                cpu;
            AssemblerOptions                       opts;
            std::vector<std::string>               lines;



            result = Assembler (cpu.GetInstructionSet(), opts).Assemble (s_kProgram);
            rig.session.SetDebugFile (DebugFileWriter::Build (result, { { "", { "src/prog.a65", 0 } } }), L"C:\\Work\\prog.dbg");

            lines = rig.RunOk ("BP prog.a65:10").text;
            Assert::IsTrue   (lines.at (0).ends_with (" at $0309, prog.a65 line 10."));

            lines = rig.RunOk ("BP src/prog.a65:5").text;
            Assert::AreEqual ((size_t) 2, lines.size(), L"the macro's closing line moves to the next line with code");
            Assert::AreEqual (std::string ("Line 5 produced no code; the breakpoint is on line 6."), lines.at (1));

            lines = rig.RunOk ("BP prog.a65:3").text;
            Assert::IsTrue   (lines.at (0).ends_with (" at $0307, prog.a65 line 3."), L"a macro body line, where its expansion put it");

            rig.RunFails ("BP other.a65:3", "no such file");
            Assert::IsTrue   (rig.RunOk ("BP 300:302").text.at (0).find ("$0300") != std::string::npos, L"an address range still parses as one");
        }
    };
}
