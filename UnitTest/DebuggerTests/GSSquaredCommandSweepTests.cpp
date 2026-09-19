#include "Pch.h"

#include "Debugger/DebugHandlerSet.h"
#include "Debugger/GSSquaredParser.h"
#include "HandlerTestRig.h"
#include "TestHelpers.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SweepRig
    //
    //  A session with every command family over the mock target, set up the
    //  same way each time: a program at $0300, a breakpoint at $0300 (id 0), a
    //  watch at $0006, a binary and a symbol file on disk. Two rigs that ran
    //  the same operation are equal in every table, every byte, every run
    //  request and every file.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class SweepRig
    {
    public:
        MockDebugTarget            target;
        RecordingNotificationSink  sink;
        InMemoryFileSystem         files;
        DebugSession               session { target, sink, RunState::Paused };
        DebugHandlerSet            handlers;
        TestCpu                    cpu;

        explicit SweepRig (const char * mode)
        {
            static constexpr Byte  kProgram[] = { 0xAD, 0x19, 0xC0, 0x4C, 0x00, 0x03 };
            Reply                  reply;



            cpu.InitForTest();
            target.instructionSet = cpu.GetInstructionSet();

            handlers.Attach             (session);
            session.SetFileSystem       (&files);
            session.SetCurrentDirectory (L"C:\\Work");

            std::copy (std::begin (kProgram), std::end (kProgram), target.memory.begin() + 0x300);
            files.WriteAllText (L"C:\\Work\\prog.bin", std::string ("\xA9\x41\x60", 3));
            files.WriteAllText (L"C:\\Work\\labels.sym", "0300 START\n0303 LOOP\n");

            reply = session.ExecuteLine ("BP 300");
            Assert::IsTrue (reply.status == CommandStatus::Ok);
            reply = session.ExecuteLine ("W 6");
            Assert::IsTrue (reply.status == CommandStatus::Ok);
            reply = session.ExecuteLine (std::string ("MODE ") + mode);
            Assert::IsTrue (reply.status == CommandStatus::Ok);
        }

        //  Everything a command could have changed, as text, so two rigs can
        //  be compared in one assertion that shows where they differ.
        std::string DescribeEffect()
        {
            std::string  text;
            std::string  saved;
            HRESULT      hr = S_OK;



            text += std::format ("regs {:04X} {:02X} {:02X} {:02X} {:02X} {:02X}\n",
                                 target.registers.pc, target.registers.a, target.registers.x,
                                 target.registers.y, target.registers.sp, target.registers.p);

            for (size_t i = 0; i < target.memory.size(); ++i)
            {
                text += (target.memory[i] != 0) ? std::format ("{:04X}={:02X} ", i, target.memory[i]) : std::string();
            }

            text += "\n";

            for (const Breakpoint & bp : session.GetBreakpoints().GetAll())
            {
                text += std::format ("bp {} {} {:04X}-{:04X} {} [{}]\n", bp.id, (int) bp.kind, bp.first, bp.last, bp.enabled, bp.condition.text);
            }

            for (const Watchpoint & wp : session.GetWatchpoints().GetAll())
            {
                text += std::format ("wp {} {} {:04X}-{:04X} {} [{}]\n", wp.id, (int) wp.access, wp.first, wp.last, (int) wp.mode, wp.condition.text);
            }

            for (const WatchItem & watch : session.GetWatches().GetAll())
            {
                text += std::format ("w {} {:04X}\n", watch.id, watch.address);
            }

            for (const RunRequest & run : target.runs)
            {
                text += std::format ("run {} {} {} {:04X}\n", (int) run.kind, run.count, run.hasUntilPc, run.untilPc);
            }

            hr = files.ReadAllText (L"C:\\Work\\out.bin", saved);
            text += SUCCEEDED (hr) ? std::format ("out.bin {} bytes\n", saved.size()) : std::string();
            text += std::format ("symbols {}\n", session.GetSymbols().GetCount (SymbolTableId::Main));
            text += std::format ("filter {}\n", session.GetStepFilter().GetEntries().size());

            return text;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  GSSquaredCommandSweepTests
    //
    //  SC-016: every command in contracts/gssquared-mode.md's table has the
    //  engine effect its AppleWin equivalent has. Each row runs in a fresh
    //  rig in each mode and the two rigs are compared whole.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (GSSquaredCommandSweepTests)
    {
    public:
        struct Row
        {
            const char  * gssquared;
            const char  * appleWin;
        };

        static constexpr Row  kRows[] =
        {
            { "300",                        "D 300:300"                },
            { "300.30F",                    "D 300:30F"                },
            { "00/300",                     "D 300:300"                },
            { "300: A9 41",                 "MEB 300 A9 41"            },
            { "set 300 A9 41",              "MEB 300 A9 41"            },
            { "move 300.305 400",           "M 400 300:305"            },
            { "l 300",                      "U 300"                    },
            { "list 300",                   "U 300"                    },
            { "l",                          "U"                        },
            { "300l",                       "U 300"                    },
            { "bp 400",                     "BP 400"                   },
            { "bp 400.40F",                 "BP 400:40F"               },
            { "bp 400 if A == 1",           "BP 400 IF A == 1"         },
            { "bp",                         "BPL"                      },
            { "bpd C019 r",                 "BPMR C019"                },
            { "bpd 400 w",                  "BPMW 400"                 },
            { "bpd 400.40F rw",             "BPM 400:40F"              },
            { "bpd 400 w if Y == 2",        "BPMW 400 IF Y == 2"       },
            { "bpi C010 rw",                "BPM C010"                 },
            { "nobp 0",                     "BPC 0"                    },
            { "nobp 300",                   "BPC 0"                    },
            { "watch 7",                    "W 7"                      },
            { "watch",                      "WL"                       },
            { "nowatch 0",                  "WC 0"                     },
            { "load \"prog.bin\" 300",      "BLOAD \"prog.bin\" 300"   },
            { "save \"out.bin\" 300.30F",   "BSAVE \"out.bin\" 300:30F" },
            { "sload \"labels.sym\"",       "SYM LOAD \"labels.sym\""  },
            { "slookup FDED",               "SYM FDED"                 },
            { "sclear",                     "SYM CLEAR"                },
            { "help",                       "HELP"                     },
            { "s",                          "T"                        },
            { "o",                          "P"                        },
            { "r",                          "RTS"                      },
            { "g",                          "G"                        },
        };

        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }



        TEST_METHOD (EveryCommand_HasItsAppleWinEquivalentsEffect)
        {
            for (const Row & row : kRows)
            {
                SweepRig  gssquared ("GSSQUARED");
                SweepRig  appleWin  ("APPLEWIN");
                Reply     viaGs     = gssquared.session.ExecuteLine (row.gssquared);
                Reply     viaAw     = appleWin.session.ExecuteLine  (row.appleWin);



                Assert::AreEqual ((int) CommandStatus::Ok, (int) viaAw.status, Widen (std::string (row.appleWin) + ": " + viaAw.error.detail).c_str());
                Assert::AreEqual ((int) viaAw.status, (int) viaGs.status, Widen (std::string (row.gssquared) + ": " + viaGs.error.detail).c_str());
                Assert::AreEqual (viaAw.data.index(), viaGs.data.index(), Widen (row.gssquared).c_str());
                Assert::AreEqual (Widen (appleWin.DescribeEffect()), Widen (gssquared.DescribeEffect()), Widen (row.gssquared).c_str());
            }
        }

        //  `watch first.last` is one AppleWin W per address.
        TEST_METHOD (WatchRange_IsAWatchPerAddress)
        {
            SweepRig  gssquared ("GSSQUARED");
            SweepRig  appleWin  ("APPLEWIN");



            Assert::IsTrue (gssquared.session.ExecuteLine ("watch 6.8").status == CommandStatus::Ok);
            Assert::IsTrue (appleWin.session.ExecuteLine  ("W 6").status == CommandStatus::Ok);
            Assert::IsTrue (appleWin.session.ExecuteLine  ("W 7").status == CommandStatus::Ok);
            Assert::IsTrue (appleWin.session.ExecuteLine  ("W 8").status == CommandStatus::Ok);
            Assert::AreEqual (Widen (appleWin.DescribeEffect()), Widen (gssquared.DescribeEffect()));
        }

        //  A breakpoint set in GSSquared mode is the one AppleWin's BPL lists
        //  (US12 scenario 5), and the Monitor's `/bpl` too.
        TEST_METHOD (ABreakpointSetHere_IsListedByBplAndSlashBpl)
        {
            SweepRig  rig ("GSSQUARED");
            Reply     bpl;
            Reply     slashBpl;



            Assert::IsTrue (rig.session.ExecuteLine ("bpd C010 rw").status == CommandStatus::Ok);

            bpl      = rig.session.ExecuteLine ("BPL", CommandMode::AppleWin);
            slashBpl = rig.session.ExecuteLine ("/bpl", CommandMode::Monitor);

            Assert::AreEqual (size_t (2), std::get<BreakpointListData> (bpl.data).breakpoints.size());
            Assert::AreEqual (size_t (2), std::get<BreakpointListData> (slashBpl.data).breakpoints.size());
            Assert::AreEqual (0xC010, (int) std::get<BreakpointListData> (bpl.data).breakpoints[1].address);
        }

        //  A nobp number no entry has as an id and no breakpoint has as an
        //  address clears nothing.
        TEST_METHOD (Nobp_WithNoMatch_IsAnError_AndClearsNothing)
        {
            SweepRig     rig    ("GSSQUARED");
            std::string  before = rig.DescribeEffect();
            Reply        reply  = rig.session.ExecuteLine ("nobp 500");



            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
            Assert::AreEqual (Widen (before), Widen (rig.DescribeEffect()));
        }

        //  Every command word GSSquared mode carries out is swept above; the
        //  ones it reports as not available change nothing.
        TEST_METHOD (EveryCommandWord_IsSweptOrReportedNotAvailable)
        {
            size_t  swept = 0;



            for (const GSSquaredCommand & command : GSSquaredParser::GetCommands())
            {
                bool  isSwept = std::any_of (std::begin (kRows), std::end (kRows), [&] (const Row & row)
                {
                    std::string  first = row.gssquared;

                    first = first.substr (0, first.find (' '));
                    return first == command.name;
                });

                if (command.reason != nullptr)
                {
                    SweepRig     rig    ("GSSQUARED");
                    std::string  before = rig.DescribeEffect();
                    Reply        reply  = rig.session.ExecuteLine (command.name);



                    Assert::AreEqual ((int) CommandStatus::NotAvailable, (int) reply.status, Widen (command.name).c_str());
                    Assert::AreEqual (Widen (before), Widen (rig.DescribeEffect()), Widen (command.name).c_str());
                    continue;
                }

                Assert::IsTrue (isSwept, Widen (command.name).c_str());
                ++swept;
            }

            Assert::IsTrue (swept > 0);
        }
    };
}
