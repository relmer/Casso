#include "Pch.h"

#include "Debugger/AppleWinParser.h"
#include "Debugger/DebugHandlerSet.h"
#include "Debugger/WinDbgParser.h"
#include "HandlerTestRig.h"
#include "MockExpressionContext.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WinDbgCommandSweepTests
    //
    //  SC-019: every WinDbg-mode command has the engine effect of its AppleWin
    //  equivalent. The effect is the parsed command, which is all Execute
    //  sees, so two lines that parse alike do the same thing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WinDbgCommandSweepTests)
    {
    public:
        //  A session over the mock target with every handler attached.
        struct Rig
        {
            MockDebugTarget            target;
            RecordingNotificationSink  sink;
            DebugSession               session { target, sink, RunState::Paused };
            DebugHandlerSet            handlers;

            Rig()
            {
                handlers.Attach (session);
            }

            Reply Run (const std::string & line)
            {
                Reply  reply = session.ExecuteLine (line);



                session.FormatReply (reply);
                return reply;
            }
        };

        struct Case
        {
            const char  * windbg;
            const char  * appleWin;
        };

        static std::span<const Case> GetCases()
        {
            static constexpr Case  kCases[] =
            {
                { "t",                 "T"            },
                { "t 3",               "T 3"          },
                { "p",                 "P"            },
                { "gu",                "RTS"          },
                { "g",                 "G"            },
                { "g 310",             "G 310"        },
                { "pa 310",            "G 310"        },
                { "ta 310",            "G 310"        },
                { "bp 300",            "BP 300"       },
                { "bp `main.s:4`",     "BP main.s:4"  },
                { "bl",                "BPL"          },
                { "bc *",              "BPC *"        },
                { "bd 1",              "BPD 1"        },
                { "be 1",              "BPE 1"        },
                { "ba r1 c000",        "BPMR C000"    },
                { "ba w1 400",         "BPMW 400"     },
                { "ba e1 300",         "BP 300"       },
                { "db 2000 l20",       "D 2000,20"    },
                { "dw 2000 l10",       "D 2000,20"    },
                { "dd 2000 l8",        "D 2000,20"    },
                { "da 2000 l20",       "D 2000,20"    },
                { "eb 300 a9 41",      "MEB 300 A9 41" },
                { "ew 300 1234",       "MEW 300 1234" },
                { "ea 300 \"AB\"",     "MEB 300 41 42" },
                { "f 2000 l20 00",     "F 2000,20 00" },
                { "s 300 l100 a9 41",  "S 300,100 A9 41" },
                { "m 300 l10 2000",    "M 2000 300,10" },
                { "r",                 "R"            },
                { "r a=41",            "R A=41"       },
                { "u 300",             "U 300"        },
                { "x cout",            "SYM cout"     },
                { "k",                 "CALLS"        },
                { "? 300+10",          "CALC 300+10"  },
                { ".formats 41",       "CALC 41"      },
                { "l+s",               "SRC ON"       },
                { "l-s",               "SRC OFF"      },
                { "lsa",               "SRC"          },
                { ".help",             "HELP"         },
                { "!switches",         "SWITCHES"     },
                { "!mode applewin",    "MODE APPLEWIN" },
                { "!skip cout",        "SKIP cout"    },
            };

            return kCases;
        }

        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }

        static std::string GetName (const std::string & line)
        {
            return line.substr (0, line.find (' '));
        }



        TEST_METHOD (EveryCommand_ParsesToItsAppleWinEquivalent)
        {
            MockExpressionContext  context;
            size_t                 checked = 0;



            for (const Case & sample : GetCases())
            {
                WinDbgParseResult    windbg   = WinDbgParser::Parse (sample.windbg, context);
                AppleWinParseResult  appleWin = AppleWinParser::Parse (sample.appleWin, context);
                std::wstring         where    = Widen (std::string (sample.windbg) + " / " + sample.appleWin + ": " + windbg.error + appleWin.error);



                Assert::IsTrue   (windbg.status   == ParseStatus::Ok, where.c_str());
                Assert::IsTrue   (appleWin.status == ParseStatus::Ok, where.c_str());
                Assert::AreEqual ((int) appleWin.command.verb, (int) windbg.command.verb, where.c_str());
                Assert::AreEqual (appleWin.command.a1,          windbg.command.a1,        where.c_str());
                Assert::AreEqual (appleWin.command.a2,          windbg.command.a2,        where.c_str());
                Assert::AreEqual (appleWin.command.a3,          windbg.command.a3,        where.c_str());
                Assert::AreEqual (appleWin.command.hasA1,       windbg.command.hasA1,     where.c_str());
                Assert::AreEqual (appleWin.command.hasA2,       windbg.command.hasA2,     where.c_str());
                Assert::AreEqual (appleWin.command.hasA3,       windbg.command.hasA3,     where.c_str());
                Assert::IsTrue   (appleWin.command.values == windbg.command.values,       where.c_str());
                Assert::IsTrue   (appleWin.command.mask   == windbg.command.mask,         where.c_str());
                Assert::AreEqual (appleWin.command.text,        windbg.command.text,      where.c_str());
                Assert::AreEqual (appleWin.command.count,       windbg.command.count,     where.c_str());
                Assert::AreEqual ((int) appleWin.command.mode,  (int) windbg.command.mode, where.c_str());
                ++checked;
            }

            Assert::IsTrue (checked > 30);
        }

        //  The command table and the sweep agree: every command WinDbg mode
        //  has is swept, so a command added without a case fails here.
        TEST_METHOD (EveryTableCommand_IsSwept)
        {
            for (const WinDbgCommand & command : WinDbgParser::GetCommands())
            {
                bool  isSwept = false;



                for (const Case & sample : GetCases())
                {
                    std::string  name = (sample.windbg[0] == '?') ? std::string ("?") : GetName (sample.windbg);



                    isSwept = isSwept || name == command.name;
                }

                Assert::IsTrue (isSwept, Widen (command.name).c_str());
            }
        }

        //  Every DebugVerb a WinDbg line reaches comes from a command in the
        //  table, or from an engine command through `!`: no case reaches a
        //  verb by a name the table does not list.
        TEST_METHOD (EveryReachableVerb_IsInTheTable)
        {
            for (const Case & sample : GetCases())
            {
                std::string  name    = (sample.windbg[0] == '?') ? std::string ("?") : GetName (sample.windbg);
                bool         isKnown = sample.windbg[0] == '!';



                for (const WinDbgCommand & command : WinDbgParser::GetCommands())
                {
                    isKnown = isKnown || name == command.name;
                }

                Assert::IsTrue (isKnown, Widen (sample.windbg).c_str());
            }
        }

        //  Executed end to end, a WinDbg dump and AppleWin's return the same
        //  bytes: the reply data is identical, only the text differs.
        TEST_METHOD (Dump_ExecutesTheSameAsD)
        {
            Rig                          windbg;
            Rig                          appleWin;
            Reply                        fromWinDbg;
            Reply                        fromAppleWin;



            windbg.session.ExecuteLine ("MODE WINDBG");

            for (const char * line : { "db 0x300 l8", "db 300 l8", "db $300 l8" })
            {
                fromWinDbg   = windbg.session.ExecuteLine (line);
                fromAppleWin = appleWin.session.ExecuteLine ("D 300,8");

                Assert::IsTrue   (std::get<MemoryData> (fromWinDbg.data).rows.size() == std::get<MemoryData> (fromAppleWin.data).rows.size(), Widen (line).c_str());
                Assert::IsTrue   (std::get<MemoryData> (fromWinDbg.data).rows[0].bytes == std::get<MemoryData> (fromAppleWin.data).rows[0].bytes, Widen (line).c_str());
            }
        }

        //  Every excluded example in the contract replies with its family.
        TEST_METHOD (EveryExclusion_RepliesWithItsFamily)
        {
            Rig                          rig;
            size_t                       checked = 0;



            rig.session.ExecuteLine ("MODE WINDBG");

            for (const WinDbgExclusion & exclusion : WinDbgParser::GetExclusions())
            {
                Reply  reply = rig.Run (exclusion.name);



                Assert::AreEqual ((int) CommandStatus::NotAvailable, (int) reply.status, Widen (exclusion.name).c_str());
                Assert::AreEqual (std::string ("Error: no meaning on this machine"), reply.text[0], Widen (exclusion.name).c_str());
                Assert::IsTrue   (reply.error.detail.find (exclusion.family) != std::string::npos, Widen (exclusion.name).c_str());
                ++checked;
            }

            Assert::IsTrue (checked > 30);
        }
    };
}
