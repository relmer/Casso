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

        using Rig = HandlerRig<BreakpointHandlers>;

        static std::vector<std::string> List (Rig & rig)
        {
            return rig.RunOk ("BPL").text;
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
            TestCpu                   cpu;
            Rig                       rig;
            std::vector<std::string>  lines;
            size_t                    invalidOneByte = 0;



            cpu.InitForTest();
            rig.target.instructionSet = cpu.GetInstructionSet();

            for (int opcode = 0; opcode < 256; ++opcode)
            {
                invalidOneByte += !cpu.GetInstructionSet()[opcode].isLegal ? 1 : 0;
            }

            Assert::IsTrue (invalidOneByte > 0, L"the 6502 table has undefined opcodes to break on");

            Assert::AreEqual (std::string ("#0 enabled  on BRK, hits 0"), rig.RunOk ("BRK ON").text.at (0));
            lines = rig.RunOk ("BRK").text;
            Assert::AreEqual (std::string ("BRK opcode: on"),                                  lines.at (0));
            Assert::AreEqual (std::string ("Invalid opcodes: 1-byte off 2-byte off 3-byte off"), lines.at (1));
            Assert::IsTrue   (rig.session.ShouldStopBefore (0x0300), L"memory holds $00, a BRK");

            Assert::AreEqual (std::string ("BRK opcode: off"), rig.RunOk ("BRK 0 OFF").text.at (0));
            Assert::IsFalse  (rig.session.ShouldStopBefore (0x0300));

            lines = rig.RunOk ("BRK 1 ON").text;
            Assert::AreEqual (invalidOneByte, lines.size(), L"one entry per undefined one-byte opcode");
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



        TEST_METHOD (Manage_ClearDisableEnable)
        {
            Rig  rig;



            rig.RunOk ("BP 300");
            rig.RunOk ("BPM 400");

            Assert::AreEqual (std::string ("Breakpoint #1 set on read or write of $0400"), rig.RunOk ("BPD 1").text.at (0));
            Assert::AreEqual (std::string ("#1 disabled on read or write of $0400, hits 0"), List (rig).at (1));
            Assert::IsFalse  (rig.target.watchedPages[0x04], L"a disabled watch leaves the mask");

            rig.RunOk ("BPE 1");
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
