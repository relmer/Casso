#include "Pch.h"

#include "Debugger/Handlers/BreakpointHandlers.h"
#include "HandlerTestRig.h"
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BreakpointHandlersTests
//
//  Setting, managing, editing and saving breakpoints and watchpoints through
//  the parser and the formatter, against the mock target.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (BreakpointHandlersTests)
    {
    public:

        using Rig        = HandlerRig<BreakpointHandlers>;
        using MachineRig = MachineHandlerRig<BreakpointHandlers>;

        static std::vector<std::string> List (Rig & rig)
        {
            return rig.RunOk ("BPL").text;
        }

        // The opcodes BRK <length> ON arms on the machine's own CPU, in
        // ascending order and written as hex, so a mismatch shows both lists.
        static std::string ArmOpcodes (const char * machine, int length)
        {
            MachineRig   rig (machine);
            std::string  opcodes;
            std::string  on  = std::format ("BRK {} ON", length);
            std::string  off = std::format ("BRK {} OFF", length);



            rig.RunOk (on);

            for (const Breakpoint & entry : rig.session.GetBreakpoints().GetAll())
            {
                if (entry.kind == BreakpointKind::Opcode)
                {
                    opcodes += std::format ("{:02X} ", entry.opcode);
                }
            }

            rig.RunOk (off);
            Assert::IsTrue (rig.session.GetBreakpoints().GetAll().empty(), L"OFF clears every entry ON added");
            return opcodes;
        }



        TEST_METHOD (BRK_Lengths_NmosTable)
        {
            // Stable undocumented opcodes take their table lengths, the
            // unstable ones not in the table take their column's operand
            // length, and only the twelve JAMs count as one byte.
            Assert::AreEqual (std::string ("02 12 1A 22 32 3A 42 52 5A 62 72 7A 92 B2 D2 DA F2 FA "),
                              ArmOpcodes ("Apple2e", 1));

            Assert::AreEqual (std::string ("03 04 07 0B 13 14 17 23 27 2B 33 34 37 43 44 47 4B 53 54 57 63 64 67 6B 73 74 77 "
                                           "80 82 83 87 89 8B 93 97 A3 A7 AB B3 B7 C2 C3 C7 CB D3 D4 D7 E2 E3 E7 EB F3 F4 F7 "),
                              ArmOpcodes ("Apple2e", 2));

            Assert::AreEqual (std::string ("0C 0F 1B 1C 1F 2F 3B 3C 3F 4F 5B 5C 5F 6F 7B 7C 7F "
                                           "8F 9B 9C 9E 9F AF BB BF CF DB DC DF EF FB FC FF "),
                              ArmOpcodes ("Apple2e", 3));
        }



        TEST_METHOD (BRK_Lengths_CmosTable)
        {
            // Every 65C02 opcode is defined; the undocumented ones are the NOP
            // fill, at their real one-, two- and three-byte lengths.
            Assert::AreEqual (std::string ("03 0B 13 1B 23 2B 33 3B 43 4B 53 5B 63 6B 73 7B "
                                           "83 8B 93 9B A3 AB B3 BB C3 CB D3 DB E3 EB F3 FB "),
                              ArmOpcodes ("Apple2eEnhanced", 1));

            Assert::AreEqual (std::string ("02 22 42 44 54 62 82 C2 D4 E2 F4 "), ArmOpcodes ("Apple2eEnhanced", 2));
            Assert::AreEqual (std::string ("5C DC FC "),                         ArmOpcodes ("Apple2eEnhanced", 3));
        }



        TEST_METHOD (BP_BPX_SetAndList)
        {
            Rig                       rig;
            std::vector<std::string>  list;



            Assert::AreEqual (std::string ("Breakpoint #0 set at $0300"),       rig.RunOk ("BP 300").text.at (0));
            Assert::AreEqual (std::string ("Breakpoint #1 set at $0300-$030F"), rig.RunOk ("bpx 300,10").text.at (0));
            Assert::AreEqual (std::string ("Breakpoint #2 set when PC<FA62"),   rig.RunOk ("BP < FA62").text.at (0));
            Assert::IsTrue   (rig.target.hookInstalled);

            list = List (rig);
            Assert::AreEqual ((size_t) 3, list.size());
            Assert::AreEqual (std::string ("#0 enabled  at $0300, hits 0"),       list[0]);
            Assert::AreEqual (std::string ("#1 enabled  at $0300-$030F, hits 0"), list[1]);
            Assert::AreEqual (std::string ("#2 enabled  when PC<FA62, hits 0"),   list[2]);
        }



        TEST_METHOD (BPR_Condition_StopsWhenTrue)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("Breakpoint #0 set when A=41"), rig.RunOk ("BPR A = 41").text.at (0));
            Assert::AreEqual (std::string ("Breakpoint #1 set when S<10"), rig.RunOk ("bpr s < 10").text.at (0));

            Assert::IsFalse (rig.session.ShouldStopBefore (0x0300), L"A is not $41 yet");
            rig.target.registers.a = 0x41;
            Assert::IsTrue  (rig.session.ShouldStopBefore (0x0300));
        }



        TEST_METHOD (Watchpoints_ModesAliasesAndBPA)
        {
            Rig                       rig;
            std::vector<std::string>  both;



            Assert::AreEqual (std::string ("Breakpoint #0 set on read of $C019"),                        rig.RunOk ("BPMR C019").text.at (0));
            Assert::AreEqual (std::string ("Breakpoint #1 set on write of $0400, before the access"),    rig.RunOk ("BPMW 400 before").text.at (0));
            Assert::AreEqual (std::string ("Breakpoint #2 set on read or write of $C030"),               rig.RunOk ("BPIO C030").text.at (0));
            Assert::IsTrue   (rig.target.watchedPages[0xC0]);
            Assert::IsFalse  (rig.target.watchedPages[0x04], L"a before-mode watch puts no page in the mask");

            both = rig.RunOk ("BPA 300").text;
            Assert::AreEqual ((size_t) 2, both.size());
            Assert::AreEqual (std::string ("#3 enabled  at $0300, hits 0"),                  both[0]);
            Assert::AreEqual (std::string ("#4 enabled  on read or write of $0300, hits 0"), both[1]);
            Assert::IsTrue   (rig.target.watchedPages[0x03]);
        }



        TEST_METHOD (BRK_BRKOP_BRKINT_Forms)
        {
            static constexpr size_t  kNmosOneByte = 18;   // six implied NOPs and twelve JAMs

            TestCpu                   cpu;
            Rig                       rig;
            std::vector<std::string>  lines;



            cpu.InitForTest();
            rig.target.instructionSet = cpu.GetInstructionSet();

            Assert::AreEqual (std::string ("#0 enabled  on BRK, hits 0"), rig.RunOk ("BRK ON").text.at (0));
            lines = rig.RunOk ("BRK").text;
            Assert::AreEqual (std::string ("BRK opcode: on"),                                  lines.at (0));
            Assert::AreEqual (std::string ("Invalid opcodes: 1-byte off 2-byte off 3-byte off"), lines.at (1));
            Assert::IsTrue   (rig.session.ShouldStopBefore (0x0300), L"memory holds $00, a BRK");

            Assert::AreEqual (std::string ("BRK opcode: off"), rig.RunOk ("BRK 0 OFF").text.at (0));
            Assert::IsFalse  (rig.session.ShouldStopBefore (0x0300));

            lines = rig.RunOk ("BRK 1 ON").text;
            Assert::AreEqual (kNmosOneByte, lines.size(), L"one entry per undocumented one-byte opcode");
            Assert::AreEqual (std::string ("Invalid opcodes: 1-byte on 2-byte off 3-byte off"), rig.RunOk ("BRK").text.at (1));
            Assert::AreEqual (std::string ("Invalid opcodes: 1-byte off 2-byte off 3-byte off"), rig.RunOk ("BRK 1 OFF").text.at (1));
            Assert::IsTrue   (List (rig).at (0) == "No breakpoints.");

            // Ids keep counting past the cleared opcode entries, so only the
            // description is pinned here.
            Assert::IsTrue   (rig.RunOk ("BRKOP 6C").text.at (0).ends_with ("on opcode $6C, hits 0"));
            Assert::AreEqual ((size_t) 1, rig.RunOk ("BRKOP").text.size());
            rig.RunFails ("BRK sideways", "invalid arguments");

            Assert::IsTrue   (rig.RunOk ("BRKINT ON").text.at (0).ends_with ("set on interrupt"));
            Assert::AreEqual (std::string ("Break on interrupt: on"),         rig.RunOk ("BRKINT").text.at (0));
            Assert::AreEqual (std::string ("Break on interrupt: off"),        rig.RunOk ("BRKINT OFF").text.at (0));
            Assert::AreEqual ((size_t) 1, List (rig).size(), L"only the opcode entry remains");
            rig.RunFails ("BRKINT maybe", "invalid arguments");
        }



        TEST_METHOD (BRK_All_CoversTheBrkOpcode)
        {
            Rig  rig;



            rig.RunOk ("BRK ALL ON");
            Assert::AreEqual (std::string ("BRK opcode: on"), rig.RunOk ("BRK").text.at (0));
            Assert::IsTrue   (rig.session.ShouldStopBefore (0x0300), L"memory holds $00, a BRK");

            rig.RunOk ("BRK ALL OFF");
            Assert::AreEqual (std::string ("BRK opcode: off"), rig.RunOk ("BRK").text.at (0));
            Assert::IsFalse  (rig.session.ShouldStopBefore (0x0300));
        }



        TEST_METHOD (BRK_ReportAndOff_CoverAnOpcodeEntryOnBrk)
        {
            Rig  rig;



            rig.RunOk ("BRKOP 00");
            Assert::AreEqual (std::string ("BRK opcode: on"),  rig.RunOk ("BRK").text.at (0));
            Assert::AreEqual (std::string ("BRK opcode: off"), rig.RunOk ("BRK 0 OFF").text.at (0));
            Assert::IsFalse  (rig.session.ShouldStopBefore (0x0300), L"the opcode entry on BRK is cleared too");
        }



        TEST_METHOD (BPSAVE_OneEntry_IsSingular)
        {
            Rig  rig;



            rig.RunOk ("BP 300");
            Assert::AreEqual (std::string ("Saved 1 breakpoint to bp.txt."), rig.RunOk ("BPSAVE bp.txt").text.at (0));
        }



        TEST_METHOD (Manage_ClearDisableEnable)
        {
            Rig  rig;



            rig.RunOk ("BP 300");
            rig.RunOk ("BPM 400");

            Assert::AreEqual (std::string ("Breakpoint #1 disabled."), rig.RunOk ("BPD 1").text.at (0));
            Assert::AreEqual (std::string ("#1 disabled on read or write of $0400, hits 0"), List (rig).at (1));
            Assert::IsFalse  (rig.target.watchedPages[0x04], L"a disabled watch leaves the mask");

            Assert::AreEqual (std::string ("Breakpoint #1 enabled."), rig.RunOk ("BPE 1").text.at (0));
            Assert::IsTrue   (rig.target.watchedPages[0x04]);

            Assert::AreEqual (std::string ("Breakpoint #0 cleared."), rig.RunOk ("BPC 0").text.at (0));
            rig.RunFails ("BPC 7", "no such breakpoint");
            rig.RunFails ("BPD 7", "no such breakpoint");

            Assert::AreEqual ((size_t) 1, rig.RunOk ("BPD *").text.size());
            Assert::IsFalse  (rig.target.hookInstalled, L"nothing enabled, no hook");

            Assert::AreEqual (std::string ("All breakpoints cleared."), rig.RunOk ("BPC *").text.at (0));
            Assert::AreEqual (std::string ("No breakpoints."),          List (rig).at (0));
            Assert::AreEqual (std::string ("Breakpoint #0 set at $0300"), rig.RunOk ("BP 300").text.at (0), L"numbering restarts once the table is empty");
        }



        TEST_METHOD (BPEDIT_ReplacesUnderSameId_ResetsHits)
        {
            Rig         rig;
            Breakpoint  breakpoint;
            Watchpoint  watchpoint;



            rig.RunOk ("BP 300");
            rig.RunOk ("BPM 400");
            rig.RunOk ("BPCHANGE 0 T");
            Assert::IsTrue   (rig.session.ShouldStopBefore (0x0300));
            Assert::IsTrue   (rig.session.GetBreakpoints().TryFind (0, breakpoint));
            Assert::AreEqual ((uint32_t) 1, breakpoint.hits);

            Assert::AreEqual (std::string ("Breakpoint #0 set on write of $0500, temporary"), rig.RunOk ("BPEDIT 0 BPMW 500").text.at (0));
            Assert::IsFalse  (rig.session.GetBreakpoints().TryFind (0, breakpoint));
            Assert::IsTrue   (rig.session.GetWatchpoints().TryFind (0, watchpoint));
            Assert::AreEqual ((uint32_t) 0, watchpoint.hits);
            Assert::IsTrue   (watchpoint.temporary, L"flags carry over to the new definition");
            Assert::IsTrue   (rig.target.watchedPages[0x05]);
            Assert::AreEqual (std::string ("#0 enabled  on write of $0500, temporary, hits 0"), List (rig).at (0));

            Assert::AreEqual (std::string ("Breakpoint #1 set at $0310"), rig.RunOk ("BPEDIT 1 BP 310").text.at (0));
            Assert::IsFalse  (rig.target.watchedPages[0x04]);
            Assert::IsTrue   (rig.session.GetBreakpoints().TryFind (1, breakpoint));

            Assert::AreEqual (std::string ("Breakpoint #1 set when A=1"),        rig.RunOk ("BPEDIT 1 BPR A 1").text.at (0));
            Assert::AreEqual (std::string ("Breakpoint #1 set on opcode $6C"),   rig.RunOk ("BPEDIT 1 BRKOP 6C").text.at (0));

            rig.RunFails ("BPEDIT 5 BP 300", "no such breakpoint");
            rig.RunFails ("BPEDIT 1 BPL",    "invalid arguments");
            rig.RunFails ("BPEDIT 1 FROB",   "invalid arguments");
            rig.RunFails ("BPEDIT 1",        "invalid arguments");
            Assert::AreEqual ((size_t) 2, List (rig).size());
        }



        TEST_METHOD (BPCHANGE_Flags)
        {
            Rig  rig;



            rig.RunOk ("BP 300");

            Assert::AreEqual (std::string ("Breakpoint #0 set at $0300"),                        rig.RunOk ("BPCHANGE 0 e").text.at (0));
            Assert::AreEqual (std::string ("#0 disabled at $0300, hits 0"),                      List (rig).at (0));
            Assert::AreEqual (std::string ("#0 enabled  at $0300, temporary, hits 0"),           (rig.RunOk ("BPCHANGE 0 ET"), List (rig).at (0)));
            Assert::AreEqual (std::string ("#0 enabled  at $0300, temporary, counts only, hits 0"), (rig.RunOk ("BPCHANGE 0 s"), List (rig).at (0)));
            Assert::AreEqual (std::string ("#0 enabled  at $0300, hits 0"),                      (rig.RunOk ("BPCHANGE 0 tS"), List (rig).at (0)));

            rig.RunFails ("BPCHANGE 0 Q", "invalid arguments");
            rig.RunFails ("BPCHANGE 4 E", "no such breakpoint");
            rig.RunFails ("BPCHANGE 0",   "invalid arguments");
        }



        TEST_METHOD (BPSAVE_WritesScript_ReplayRestoresTheTable)
        {
            static constexpr const char * kExpected =
                "BPC *\n"
                "BP 0300\n"
                "BPM 0400:040F BEFORE\n"
                "BPR A =41\n"
                "BRKOP 6C\n"
                "BPD 1\n"
                "BPCHANGE 2 Ts\n";

            Rig                       rig;
            std::vector<std::string>  before;
            std::string               script;
            size_t                    replayed = 0;



            rig.RunOk ("BP 300");
            rig.RunOk ("BPM 400:40F BEFORE");
            rig.RunOk ("BPR A = 41");
            rig.RunOk ("BRKOP 6C");
            rig.RunOk ("BPD 1");
            rig.RunOk ("BPCHANGE 2 Ts");
            before = List (rig);

            Assert::AreEqual (std::string ("Saved 4 breakpoints to bp.txt."), rig.RunOk ("BPSAVE bp.txt").text.at (0));
            Assert::AreEqual (std::string (kExpected), rig.files.PeekContent (L"C:\\Work\\bp.txt"));

            rig.RunOk ("BPC *");
            rig.RunOk ("BP 1234");
            script = rig.files.PeekContent (L"C:\\Work\\bp.txt");

            for (size_t start = 0, end = script.find ('\n'); end != std::string::npos; start = end + 1, end = script.find ('\n', start))
            {
                rig.RunOk (script.substr (start, end - start));
                ++replayed;
            }

            Assert::AreEqual ((size_t) 7, replayed);
            Assert::IsTrue   (before == List (rig), L"the replayed table lists as the saved one did");

            rig.RunFails ("BPSAVE", "invalid arguments");
            rig.session.SetFileSystem (nullptr);
            rig.RunFails ("BPSAVE x.txt", "no file access");
        }
    };
}
