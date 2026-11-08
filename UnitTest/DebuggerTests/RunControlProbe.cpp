#include "Pch.h"

#include "RunControlRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace DebuggerTests
{
    TEST_CLASS (RunControlProbe)
    {
    public:
        static void Dump (const std::string & name, const std::vector<std::string> & lines, bool json = false)
        {
            BatchScriptRig  rig;
            std::ofstream   out ("C:\\Users\\relmer\\AppData\\Local\\Temp\\claude\\C--Users-relmer-source-repos-relmer-Casso-worktrees-035-debugger\\1c1cd0a8-d304-442e-8b5d-62af4ccf9e22\\scratchpad\\wf33\\probe-" + name + ".txt");

            rig.options.json = json;
            int status = rig.Run (lines);
            out << rig.result.output << "\n=== diag\n" << rig.result.diagnostics << "\n=== exit " << status << "\n";
        }

        TEST_METHOD (Probe)
        {
            Dump ("applewin", {
                "MEB 300 E8 E8 4C 00 03",
                "MEB 310 A9 41 60",
                "= 300",
                "MODE",
                "MODE sideways",
                "PAUSE",
                "BUDGET 100",
                "BUDGET",
                "BUDGET C000",
                "BUDGET $100",
                "BUDGET -5",
                "BUDGET #10",
                "BUDGET 4294967295",
                "G NOSUCH",
                "G 300 junk",
                "G 10000",
                "G FFFF 300,0",
                "=",
                "= 10000",
                "JSR",
                "JSR -1",
                "T 10000",
                "= 300",
                "SYM START = 300",
                "BUDGET 50",
                "STARTG",
                "NOP",
                "R",
                "OUTPUT",
                "LOG",
                "LOG ERROR",
                "LOG INFO",
                "MODE CASSO",
                "R",
                "MODE",
                "!MODE",
                "/MODE",
                "300G",
                "g 0x310",
                "A",
                "",
                "A 300",
                "LDA #$41",
                "",
            });
            Dump ("monitor", {
                "MEB 300 E8 E8 4C 00 03",
                "= 300",
                "MODE MONITOR",
                "MODE APPLEWIN",
                "PAUSE",
                "BUDGET 100",
                "/MODE sideways",
                "/PAUSE",
                "/BUDGET 100",
                "/BUDGET",
                "/MODE",
                "300.302",
                "300S 300S",
                "300S FROB",
                "300.302 C080: 41 300.301",
                "/=",
                "/= 300",
                "/NOP",
                "/JSR",
                "!",
                "300:LDA #41",
                "LDA #41",
                "",
                "300!",
                "",
                "F666G",
                "",
                "T",
                "300T",
                "G",
                "/G",
                "/T 3",
                "/P",
                "/RTS",
                "/GG",
            });
            Dump ("gssquared", {
                "MEB 300 E8 E8 4C 00 03",
                "= 300",
                "MODE GSSQUARED",
                "mode",
                "MODE",
                "mode sideways",
                "pause",
                "budget 100",
                "g 300",
                "s 1",
                "r 1",
                "o 1",
                "300G",
                "T 3",
                "tl",
                "TRACE",
                "ZAP",
                "zap",
                "nop",
                "A",
                "a 300",
                "rts",
                "p",
                "=",
                "= 300",
                "JSR",
                "gg",
                "GG 300",
                "g",
            });
            Dump ("windbg", {
                "MEB 300 E8 E8 4C 00 03",
                "= 300",
                "MODE WINDBG",
                "mode",
                "!mode",
                "!mode sideways",
                "pause",
                "budget 5",
                "!pause",
                "!budget 100",
                "pa",
                "pa 1 2",
                "ta",
                "g 0x310",
                "gu 5",
                "a",
                "!a 300",
                "",
                "!=",
                "!= 300",
                "!nop",
                "!zap",
                "!jsr",
                "t 0n2",
                "p 3",
                "!t",
                "!gg",
                "!g",
            });
            Assert::Fail (L"probed");
        }
    };
}
