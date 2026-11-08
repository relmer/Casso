#include "Pch.h"

#include "Cli/DebugBatchRunner.h"
#include "Debugger/AppleWinCommandTable.h"
#include "EmuTests/FixtureRomSource.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "WindowConsoleRig.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace WindowCommandRoutingTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RoutingRig
    //
    //  The window's console over a real //e: NOPs from $0200 to $0500, a JMP
    //  back to $0300 at $0500, and the PC at $0300.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class RoutingRig : public WindowConsoleRig
    {
    public:
        static constexpr Byte  kNop = 0xEA;



        RoutingRig()
        {
            Fill (0x0200, 0x0500, kNop);
            Poke (0x0500, { 0x4C, 0x00, 0x03 });
            SetPc (0x0300);
        }



        //  Every name the table marks as needing the window, each as a line a
        //  user could type: the memory names with an address.
        static std::vector<std::string> GetWindowOnlyLines()
        {
            std::vector<std::string>  lines;



            for (const AppleWinCommand & entry : AppleWinCommandTable::GetAll())
            {
                if (entry.availability == CommandAvailability::WindowOnly)
                {
                    lines.push_back (entry.family == AppleWinCommandFamily::MiniMemory ? std::string (entry.name) + " 1000" : std::string (entry.name));
                }
            }

            Assert::AreEqual ((size_t) 64, lines.size(), L"the table's window-only names");
            return lines;
        }



        static std::string GetName (const std::string & line)
        {
            return line.substr (0, line.find (' '));
        }



        //  Only a run of the machine can change the view of the code, so this
        //  compares everything else a window command could change.
        static void AssertNoWindowCommandRan (const DebuggerViewSnapshot & before, const DebuggerViewSnapshot & after,
                                              const DebuggerViewState & view, const std::string & what)
        {
            std::wstring  where = Widen (what);



            Assert::AreEqual (before.showPaneSerial,       after.showPaneSerial,       where.c_str());
            Assert::AreEqual (before.goTo.has_value(),     after.goTo.has_value(),     where.c_str());
            Assert::AreEqual (before.memoryWindows.size(), after.memoryWindows.size(), where.c_str());
            Assert::AreEqual (before.memoryWindows[0].first, after.memoryWindows[0].first, where.c_str());
            Assert::AreEqual (before.codeOpen[1],          after.codeOpen[1],          where.c_str());
            Assert::IsFalse  (view.GetCodeAddress (0).has_value(), where.c_str());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AppleWinRoutingTests
    //
    //  In AppleWin and Casso modes the window carries out the names the table
    //  marks as needing it. Every other line goes to the session exactly as
    //  batch runs it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AppleWinRoutingTests)
    {
    public:

        //  The same reply as batch gives, text and all, and nothing on the
        //  panes moves: shorthand, a blank line, an unknown name, help, and
        //  commands that change the machine.
        TEST_METHOD (EveryOtherLineRunsAsBatchRunsIt)
        {
            for (const char * line : { "U 300", "R", "BPL", "600:A9 41", "", "FOO", "HELP V", "D 300", "BP 302", "MEB 400 55", "STACK" })
            {
                RoutingRig            window;
                RoutingRig            batch;
                DebuggerViewSnapshot  before = window.Build();
                Reply                 typed  = window.Type (line);
                Reply                 ran    = DebuggerViewState::ExecuteLine (batch.Session(), line, CommandMode::AppleWin);



                WindowConsoleRig::AssertReply (typed, ran.status, ran.error.label, ran.error.detail, ran.text, line);
                Assert::AreEqual (ran.command, typed.command, RoutingRig::Widen (line).c_str());
                Assert::IsTrue   (ran.verb == typed.verb,     RoutingRig::Widen (line).c_str());

                RoutingRig::AssertPanesUnchanged (before, window.Build(), line);
            }
        }



        //  The effect is the command's own, not a pane's.
        TEST_METHOD (AnOrdinaryCommandStillDoesWhatItDoes)
        {
            RoutingRig  rig;



            rig.Type ("BP 302");
            rig.Type ("MEB 400 55");

            Assert::AreEqual ((size_t) 1,     rig.Build().breakpoints.size());
            Assert::AreEqual ((Word) 0x0302,  rig.Build().breakpoints[0].address);
            Assert::AreEqual ((Byte) 0x55,    rig.Peek (0x0400));
        }



        //  `/` marks an AppleWin line only in Monitor mode; in AppleWin and
        //  Casso modes it is part of the name, which is no command.
        TEST_METHOD (ASlashIsNoWindowCommandOutsideMonitorMode)
        {
            for (const char * mode : { "APPLEWIN", "CASSO" })
            {
                for (const char * line : { "/V", "/CODE", "/MD1 1000" })
                {
                    RoutingRig            rig;
                    DebuggerViewSnapshot  before;
                    std::string           name   = RoutingRig::GetName (line);
                    std::string           detail = name + " is not a command.";



                    rig.Type (std::string ("MODE ") + mode);
                    before = rig.Build();

                    RoutingRig::AssertReply (rig.Type (line), CommandStatus::Unknown, "unknown command", detail,
                                             { "Error: unknown command", "       " + detail }, line);
                    RoutingRig::AssertPanesUnchanged (before, rig.Build(), line);
                }
            }
        }



        //  A window reply is written in the session's output format: Monitor's
        //  marks an error with ERR, and GSSquared's and WinDbg's write these
        //  replies as AppleWin does.
        TEST_METHOD (AWindowReplyTakesTheSessionsOutputFormat)
        {
            static const char * const  kHgr = "HGR is not available here: the emulator window shows the screen.";



            for (const auto & [format, isMonitor] : { std::pair<const char *, bool> { "MONITOR",   true  },
                                                      std::pair<const char *, bool> { "GSSQUARED", false },
                                                      std::pair<const char *, bool> { "WINDBG",    false },
                                                      std::pair<const char *, bool> { "APPLEWIN",  false } })
            {
                RoutingRig                rig;
                std::vector<std::string>  text = { "Error: command not available", std::string ("       ") + kHgr };



                Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Type (std::string ("OUTPUT ") + format).status);
                rig.Build();

                if (isMonitor)
                {
                    text.insert (text.begin(), "ERR");
                }

                RoutingRig::AssertReply (rig.Type ("V"),   CommandStatus::Ok, "", "", { "The code pane is at $02F7." }, format);
                RoutingRig::AssertReply (rig.Type ("HGR"), CommandStatus::NotAvailable, "command not available", kHgr, text, format);
                RoutingRig::AssertReply (rig.Type ("WIN"), CommandStatus::Ok, "", "", { "This window shows every pane at once." }, format);
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MonitorRoutingTests
    //
    //  In Monitor mode a line without `/` is the Monitor's, and the Monitor
    //  reads one character at a time: hex digits gather and a command letter
    //  spends them. So several AppleWin window names typed there run Monitor
    //  commands. None of them moves a pane.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MonitorRoutingTests)
    {
    public:

        static void MakeMonitor (RoutingRig & rig)
        {
            rig.Poke (0x0000, { 0x77, 0x5A });
            rig.Poke (0x000A, { 0x3E });
            rig.Poke (0x00D1, { 0x39 });
            rig.Poke (0x1000, { 0x00 });
            rig.Poke (0x2000, { 0xFC });
            rig.Type ("MODE MONITOR");
        }



        static void EndRun (RoutingRig & rig)
        {
            rig.controller.RequestPause();
            rig.RunFrames (1);
            Assert::IsFalse (rig.controller.IsRunInProgress());
        }



        //  V is the Monitor's verify, RET is R with the file name ET, and `.`
        //  examines: none of them moves the code pane.
        TEST_METHOD (TheCursorNamesAreTheMonitors)
        {
            RoutingRig            rig;
            DebuggerViewSnapshot  before;



            MakeMonitor (rig);
            before = rig.Build();

            RoutingRig::AssertReply (rig.Type ("V"), CommandStatus::Ok, "", "", {}, "V verifies");
            RoutingRig::AssertReply (rig.Type ("RET"), CommandStatus::Error, "file not found", "ET could not be read.",
                                     { "ERR", "Error: file not found", "       ET could not be read." }, "RET reads ET");
            RoutingRig::AssertReply (rig.Type ("."), CommandStatus::Ok, "", "", { "0001- 5A" }, ". examines");

            RoutingRig::AssertPanesUnchanged (before, rig.Build(), "the Monitor's lines");
        }



        //  WIN and WINDOW are W with the file names IN and INDOW: each writes a
        //  file, from $0000 since no range came first.
        TEST_METHOD (WinWritesAFileNamedIn)
        {
            RoutingRig            rig;
            DebuggerViewSnapshot  before;
            std::string           content;



            MakeMonitor (rig);
            before = rig.Build();

            RoutingRig::AssertReply (rig.Type ("WIN"),    CommandStatus::Ok, "", "", { "IN: 1 of 1 bytes" },    "WIN");
            RoutingRig::AssertReply (rig.Type ("WINDOW"), CommandStatus::Ok, "", "", { "INDOW: 1 of 1 bytes" }, "WINDOW");

            Assert::AreEqual (S_OK, rig.files.ReadAllText (L"IN", content));
            Assert::AreEqual (std::string ("\x77"), content);
            Assert::AreEqual (S_OK, rig.files.ReadAllText (L"INDOW", content));
            Assert::AreEqual (std::string ("\x77"), content);

            RoutingRig::AssertPanesUnchanged (before, rig.Build(), "WIN");
        }



        //  DGR: D is a hex digit, so G runs the machine from $000D, and then R
        //  with no file name finds the machine running. In the window, the
        //  empty R is prompted for before the line runs. GR runs from the PC.
        TEST_METHOD (DgrAndGrRunTheMachine)
        {
            static const std::vector<std::string>  kRunning =
            {
                "ERR", "Error: machine running", "       R changes registers or memory. Pause the machine first.",
                "ERR", "Error: machine running", "       R changes registers or memory. Pause the machine first.",
            };

            for (const auto & [line, pc] : { std::pair<const char *, Word> { "DGR", 0x000D }, std::pair<const char *, Word> { "GR", 0x0300 } })
            {
                RoutingRig            rig;
                DebuggerViewSnapshot  before;



                MakeMonitor (rig);
                before = rig.Build();

                Assert::IsTrue (DebuggerViewState::GetMissingFileVerb (line, CommandMode::Monitor) == DebugVerb::ReadFile, L"the window asks for R's file");

                RoutingRig::AssertReply (rig.Type (line), CommandStatus::Error, "machine running",
                                         "R changes registers or memory. Pause the machine first.", kRunning, line);
                Assert::IsTrue   (rig.controller.IsRunInProgress(), RoutingRig::Widen (line).c_str());
                Assert::AreEqual (pc, rig.GetRegisters().pc, L"where the run starts");

                RoutingRig::AssertNoWindowCommandRan (before, rig.Build (false), rig.view, line);
                EndRun (rig);
            }
        }



        //  DATA: DA is hex, T steps from $00DA, then A examines $000A.
        TEST_METHOD (DataStepsFromDa)
        {
            RoutingRig            rig;
            DebuggerViewSnapshot  before;



            MakeMonitor (rig);
            before = rig.Build();

            RoutingRig::AssertReply (rig.Type ("DATA"), CommandStatus::Ok, "", "", { "000A- 3E" }, "DATA");
            Assert::AreEqual ((Word) 0x00DA, rig.GetRegisters().pc);
            Assert::IsTrue   (rig.controller.IsRunInProgress(), L"a step, carried by the frames");

            RoutingRig::AssertNoWindowCommandRan (before, rig.Build (false), rig.view, "DATA");
            EndRun (rig);
        }



        //  MD1, MT1 and M1: M moves a byte from $0000 to itself, then the
        //  digits examine. MT1 steps as well.
        TEST_METHOD (TheMemoryNamesMoveAndExamine)
        {
            for (const auto & [line, text, isRunning] :
                 { std::tuple<const char *, std::vector<std::string>, bool> { "MD1 1000", { "Moved 1 bytes from $0000 to $0000.", "00D1- 39", "1000- 00" }, false },
                   std::tuple<const char *, std::vector<std::string>, bool> { "M1 2000",  { "Moved 1 bytes from $0000 to $0000.", "0001- 5A", "2000- FC" }, false },
                   std::tuple<const char *, std::vector<std::string>, bool> { "MT1 2000", { "Moved 1 bytes from $0000 to $0000.", "0001- 5A", "2000- FC" }, true } })
            {
                RoutingRig            rig;
                DebuggerViewSnapshot  before;



                MakeMonitor (rig);
                before = rig.Build();

                RoutingRig::AssertReply (rig.Type (line), CommandStatus::Ok, "", "", text, line);
                Assert::AreEqual (isRunning, rig.controller.IsRunInProgress(), RoutingRig::Widen (line).c_str());
                Assert::AreEqual ((Word) 0x0000, rig.view.GetMemoryAddress(), L"the memory pane did not move");

                RoutingRig::AssertNoWindowCommandRan (before, rig.Build (false), rig.view, line);

                if (isRunning)
                {
                    EndRun (rig);
                }
            }
        }



        //  BW: B is hex, W has no file name, and the window prompts for one.
        TEST_METHOD (BwAsksForAFileToWrite)
        {
            RoutingRig  rig;



            MakeMonitor (rig);

            Assert::IsTrue (DebuggerViewState::GetMissingFileVerb ("BW", CommandMode::Monitor) == DebugVerb::WriteFile);
            RoutingRig::AssertReply (rig.Type ("BW"), CommandStatus::Error, "invalid arguments",
                                     "W takes a file name here. Only the debugger window can prompt for one.",
                                     { "ERR", "Error: invalid arguments", "       W takes a file name here. Only the debugger window can prompt for one." }, "BW");
        }



        //  A letter that is not a Monitor command rejects the whole line, so
        //  nothing runs even where a T came before it.
        TEST_METHOD (ALetterTheMonitorLacksRejectsTheLine)
        {
            static const std::pair<const char *, const char *>  kLines[] =
            {
                { "CODE",    "O is not a Monitor command." },
                { "TEXT",    "X is not a Monitor command." },
                { "SOURCE1", "O is not a Monitor command." },
                { "HGR",     "H is not a Monitor command." },
                { "->",      "> is not a Monitor command." },
                { "^",       "^ must be followed by a letter." },
            };
            RoutingRig            rig;
            DebuggerViewSnapshot  before;



            MakeMonitor (rig);
            before = rig.Build();

            for (const auto & [line, detail] : kLines)
            {
                RoutingRig::AssertReply (rig.Type (line), CommandStatus::Error, "invalid arguments", detail,
                                         { "ERR", "Error: invalid arguments", std::string ("       ") + detail }, line);
                Assert::IsFalse (rig.controller.IsRunInProgress(), RoutingRig::Widen (line).c_str());
            }

            Assert::AreEqual ((Word) 0x0300, rig.GetRegisters().pc);
            RoutingRig::AssertPanesUnchanged (before, rig.Build(), "the rejected lines");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  GSSquaredRoutingTests
    //
    //  GSSquared has no window commands and its words are not AppleWin's, so
    //  none of the names reaches the panes.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (GSSquaredRoutingTests)
    {
    public:

        TEST_METHOD (EveryWindowNameIsUnknown)
        {
            RoutingRig            rig;
            DebuggerViewSnapshot  before;



            rig.Type ("MODE GSSQUARED");
            before = rig.Build();

            for (const std::string & line : RoutingRig::GetWindowOnlyLines())
            {
                std::string  detail = RoutingRig::GetName (line) + " is not a command.";



                if (line == ".")
                {
                    continue;
                }

                RoutingRig::AssertReply (rig.Type (line), CommandStatus::Unknown, "unknown command", detail,
                                         { "Error: unknown command", "       " + detail }, line);
            }

            RoutingRig::AssertReply (rig.Type ("v"), CommandStatus::Unknown, "unknown command", "v is not a command.",
                                     { "Error: unknown command", "       v is not a command." }, "as typed");
            RoutingRig::AssertPanesUnchanged (before, rig.Build(), "GSSquared");
        }



        //  GSSquared's `.` is a range with nothing before it: an error, and no
        //  pane moves.
        TEST_METHOD (TheDotIsAnEmptyRange)
        {
            RoutingRig            rig;
            DebuggerViewSnapshot  before;
            Reply                 reply;



            rig.Type ("MODE GSSQUARED");
            before = rig.Build();
            reply  = rig.Type (".");

            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
            Assert::AreEqual (std::string ("invalid arguments"), reply.error.label);
            RoutingRig::AssertPanesUnchanged (before, rig.Build(), ".");
        }



        //  GSSquared's r steps out; it is not AppleWin's RET.
        TEST_METHOD (RIsStepOutNotRet)
        {
            RoutingRig            rig;
            DebuggerViewSnapshot  before;



            rig.Type ("MODE GSSQUARED");
            before = rig.Build();

            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Type ("r").status);
            Assert::IsTrue   (rig.controller.IsRunInProgress(), L"a step out started");
            RoutingRig::AssertNoWindowCommandRan (before, rig.Build (false), rig.view, "r");

            rig.controller.RequestPause();
            rig.RunFrames (1);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WinDbgRoutingTests
    //
    //  WinDbg reaches Casso's commands through `!`, but not the ones that only
    //  lay out a window; a bare name is no WinDbg word.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WinDbgRoutingTests)
    {
    public:

        TEST_METHOD (EveryWindowNameBehindTheMarkerDoesNotRun)
        {
            RoutingRig            rig;
            DebuggerViewSnapshot  before;



            rig.Type ("MODE WINDBG");
            before = rig.Build();

            for (const std::string & line : RoutingRig::GetWindowOnlyLines())
            {
                std::string  name   = RoutingRig::GetName (line);
                std::string  detail = std::format ("!{} does not run in WinDbg mode. Use .help {} to see where it runs.", name, name);



                RoutingRig::AssertReply (rig.Type ("!" + line), CommandStatus::Unknown, "unknown command", detail,
                                         { "Error: unknown command", "       " + detail }, "!" + line);
            }

            RoutingRig::AssertReply (rig.Type ("!v"), CommandStatus::Unknown, "unknown command",
                                     "!v does not run in WinDbg mode. Use .help v to see where it runs.",
                                     { "Error: unknown command", "       !v does not run in WinDbg mode. Use .help v to see where it runs." }, "as typed");
            RoutingRig::AssertPanesUnchanged (before, rig.Build(), "WinDbg");
        }



        TEST_METHOD (ABareWindowNameIsNoWinDbgWord)
        {
            RoutingRig            rig;
            DebuggerViewSnapshot  before;



            rig.Type ("MODE WINDBG");
            before = rig.Build();

            RoutingRig::AssertReply (rig.Type ("v"), CommandStatus::Unknown, "unknown command", "v is not a WinDbg-mode command.",
                                     { "Error: unknown command", "       v is not a WinDbg-mode command." }, "v");
            RoutingRig::AssertReply (rig.Type ("CODE"), CommandStatus::Unknown, "unknown command", "code is not a WinDbg-mode command.",
                                     { "Error: unknown command", "       code is not a WinDbg-mode command." }, "CODE");
            RoutingRig::AssertPanesUnchanged (before, rig.Build(), "WinDbg");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AssemblerRoutingTests
    //
    //  While A takes source, every line is an instruction, a window name
    //  included, and the panes go on showing the machine.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AssemblerRoutingTests)
    {
    public:

        TEST_METHOD (AWindowNameIsSourceWhileAssembling)
        {
            RoutingRig            rig;
            DebuggerViewSnapshot  before;
            Reply                 reply;



            before = rig.Build();
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Type ("A 300").status);

            for (const char * line : { "V", "CODE", "MD1 1000", "PANEL mmu" })
            {
                reply = rig.Type (line);

                Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status, RoutingRig::Widen (line).c_str());
                Assert::AreEqual (std::string ("assembly error"), reply.error.label, RoutingRig::Widen (line).c_str());
                Assert::AreEqual (std::string (line), reply.command);
            }

            RoutingRig::AssertPanesUnchanged (before, rig.Build(), "assembling");
            Assert::IsFalse (rig.view.IsPanelOpen ("mmu"));

            RoutingRig::AssertReply (rig.Type (""), CommandStatus::Ok, "", "", { "Assembly ended." }, "a blank line");
            RoutingRig::AssertReply (rig.Type ("V"), CommandStatus::Ok, "", "", { "The code pane is at $02F7." }, "V again");
        }



        //  The registers, the code being written, memory and the stack are all
        //  on screen while A takes source, and a pane V moved stays put.
        TEST_METHOD (ThePanesShowTheMachineWhileAssembling)
        {
            RoutingRig            rig;
            DebuggerViewSnapshot  snapshot;
            int                   row = -1;



            rig.Poke (0x0000, { 0x42 });
            rig.Type ("BP 305");
            rig.Build();
            rig.Type ("V");
            rig.Build();

            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Type ("A 300").status);
            Assert::AreEqual ((int) CommandStatus::Ok, (int) rig.Type ("LDA #$01").status);

            snapshot = rig.Build();
            row      = RoutingRig::GetRowOf (snapshot.codeViews[0], 0x0300);

            Assert::AreEqual ((Word) 0x0300,                  snapshot.pc);
            Assert::AreEqual ((size_t) 6,                     snapshot.registers.size());
            Assert::AreEqual ((Word) 0x02F7,                  snapshot.codeViews[0][0].address, L"where V put it");
            Assert::IsTrue   (row >= 0);
            Assert::AreEqual (std::string ("LDA #$01"),       snapshot.codeViews[0][(size_t) row].instruction, L"the line just assembled");
            Assert::IsTrue   (snapshot.codeViews[0][(size_t) row].isCurrent);
            Assert::IsTrue   (snapshot.memoryWindows[0].bytes[0] == std::optional<Byte> (0x42), L"memory");
            Assert::AreEqual ((size_t) 1,                     snapshot.breakpoints.size(), L"the breakpoints");
            Assert::IsTrue   (rig.Session().IsAssembling(), L"and the build did not end the assembly");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BatchRig
    //
    //  CassoCli debug over the fixture ROMs and an in-memory file system.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class BatchRig
    {
    public:
        FixtureRomSource                  roms;
        InMemoryFileSystem                files;
        CommandLineOptions::DebugOptions  options;
        DebugBatchResult                  result;



        BatchRig()
        {
            options.machine = "Apple2e";
        }



        int Run (const std::string & script)
        {
            DebugBatchRunner  runner (roms, files);
            HRESULT           hr     = runner.Run (options, script, result);



            IGNORE_RETURN_VALUE (hr, S_OK);
            return result.exitStatus;
        }



        //  What batch prints for a window-only name: the line behind the
        //  prompt, then the two-line error.
        static std::string GetNotAvailableText (const std::string & prompt, const std::string & line, bool isMonitor = false)
        {
            std::string  name = RoutingRig::GetName (isMonitor ? line.substr (1) : line);



            for (char & ch : name)
            {
                ch = (char) toupper ((unsigned char) ch);
            }

            return prompt + line + "\n" + (isMonitor ? "ERR\n" : "") +
                   "Error: command not available\n       " + name + " needs the debugger window.\n";
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BatchRoutingTests
    //
    //  A script has no window: every window-only name is not available, the
    //  script goes on, and the exit status does not count it as a failure.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BatchRoutingTests)
    {
    public:

        TEST_METHOD (EveryWindowNameNeedsTheWindowInAScript)
        {
            BatchRig     rig;
            std::string  script;
            std::string  expected;



            for (const std::string & line : RoutingRig::GetWindowOnlyLines())
            {
                script   += line + "\n";
                expected += BatchRig::GetNotAvailableText (">", line);
            }

            script   += "v\nPANEL mmu\nECHO done\n";
            expected += BatchRig::GetNotAvailableText (">", "v");
            expected += ">PANEL mmu\nError: command not available\n       PANEL needs the debugger window.\n";
            expected += ">ECHO done\ndone\n";

            Assert::AreEqual (0, rig.Run (script), L"not available is no failure");
            Assert::AreEqual (expected, rig.result.output);
        }



        TEST_METHOD (MonitorModeSlashLinesNeedTheWindowToo)
        {
            BatchRig  rig;



            Assert::AreEqual (0, rig.Run ("MODE MONITOR\n/V\n/CODE2\n/MD1 1000\n"));
            Assert::AreEqual (std::string (">MODE MONITOR\nMode: MONITOR\n") +
                              BatchRig::GetNotAvailableText ("*", "/V",        true) +
                              BatchRig::GetNotAvailableText ("*", "/CODE2",    true) +
                              BatchRig::GetNotAvailableText ("*", "/MD1 1000", true),
                              rig.result.output);
        }



        TEST_METHOD (AJsonScriptReportsNotAvailable)
        {
            BatchRig  rig;



            rig.options.json = true;

            Assert::AreEqual (0, rig.Run ("V\n"));
            Assert::AreEqual (std::string (R"({"type":"reply","status":"notAvailable","command":"V","text":["Error: command not available","       V needs the debugger window."],"error":{"label":"command not available","detail":"V needs the debugger window."}})") + "\n",
                              rig.result.output);
        }



        //  A run that ended on its budget still sets status 3 with a
        //  window-only name after it.
        TEST_METHOD (TheBudgetStatusSurvivesAWindowName)
        {
            BatchRig  rig;



            Assert::AreEqual (DebugBatchRunner::kBudgetStop, rig.Run ("BUDGET 100\nG\nCODE\n"));
            Assert::IsTrue   (rig.result.output.ends_with (BatchRig::GetNotAvailableText (">", "CODE")), RoutingRig::Widen (rig.result.output).c_str());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PipeRoutingTests
    //
    //  A channel client shares the window's session but not its panes: every
    //  window-only name is not available to it, and the panes do not move.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (PipeRoutingTests)
    {
    public:

        static std::string Escape (const std::string & text)
        {
            std::string  escaped;



            for (char ch : text)
            {
                escaped += (ch == '\\' || ch == '"') ? std::string ("\\") + ch : std::string (1, ch);
            }

            return escaped;
        }



        static std::string GetReply (int id, const std::string & line, const std::string & name, bool isMonitor)
        {
            std::string  detail = Escape (name) + " needs the debugger window.";



            return std::format (R"({{"type":"reply","id":{},"status":"notAvailable","command":"{}","text":[{}"Error: command not available","       {}"],"error":{{"label":"command not available","detail":"{}"}}}})",
                                id, Escape (line), isMonitor ? "\"ERR\"," : "", detail, detail);
        }



        TEST_METHOD (EveryWindowNameFromAClientIsNotAvailable)
        {
            RoutingRig                  rig;
            HRESULT                     hr     = rig.controller.Open();
            ChannelConnectionId         client = 0;
            DebuggerViewSnapshot        before;
            std::vector<std::string>    lines  = RoutingRig::GetWindowOnlyLines();
            int                         id     = 0;



            Assert::AreEqual (S_OK, hr);
            client = rig.transport.Connect();
            before = rig.Build();

            for (const std::string & line : lines)
            {
                rig.transport.Send (client, std::format (R"({{"type":"command","id":{},"line":"{}"}})", ++id, Escape (line)));
            }

            rig.transport.Send (client, std::format (R"({{"type":"command","id":{},"line":"/CODE","mode":"monitor"}})", ++id));
            rig.transport.Send (client, std::format (R"({{"type":"command","id":{},"line":"PANEL mmu"}})", ++id));
            rig.controller.Pump();

            Assert::AreEqual (lines.size() + 2, rig.transport.Written (client).size());

            for (size_t i = 0; i < lines.size(); i++)
            {
                Assert::AreEqual (GetReply ((int) i + 1, lines[i], RoutingRig::GetName (lines[i]), false), rig.transport.Written (client)[i]);
            }

            Assert::AreEqual (GetReply (id - 1, "/CODE",     "CODE",  true),  rig.transport.Written (client)[lines.size()], L"a Monitor-mode request");
            Assert::AreEqual (GetReply (id,     "PANEL mmu", "PANEL", false), rig.transport.Written (client).back(),        L"PANEL");
            Assert::IsFalse  (rig.view.IsPanelOpen ("mmu"));
            RoutingRig::AssertPanesUnchanged (before, rig.Build(), "the pipe");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WindowNameHelpTests
    //
    //  Help describes a window name in the modes that run it and says where
    //  it runs in the one that does not.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WindowNameHelpTests)
    {
    public:

        TEST_METHOD (HelpDescribesEachWindowNameWhereItRuns)
        {
            static const std::pair<const char *, const char *>  kLines[] =
            {
                { "HELP V",     "V: Move the code pane down one instruction" },
                { "HELP ^",     "^: Move the code pane up one instruction" },
                { "HELP .",     ".: Make the code pane follow PC again" },
                { "HELP ->",    "->: Move the code pane to the operand's address" },
                { "HELP RET",   "RET: Move the code pane to the return address" },
                { "HELP PAGEDN", "PAGEDN: Move the code pane down a page" },
                { "HELP CODE2", "CODE2: Bring the second disassembly forward, opening it" },
                { "HELP DATA2", "DATA2: Bring the second memory window forward, opening it" },
                { "HELP MD1",   "MD1 addr: Move the memory pane to a hex address" },
                { "HELP HGR",   "HGR: not available in Casso" },
            };



            for (const char * mode : { "APPLEWIN", "CASSO" })
            {
                RoutingRig  rig;



                rig.Type (std::string ("MODE ") + mode);

                for (const auto & [line, text] : kLines)
                {
                    RoutingRig::AssertReply (rig.Type (line), CommandStatus::Ok, "", "", { text }, std::string (mode) + " " + line);
                }
            }
        }



        TEST_METHOD (HelpInTheOtherModes)
        {
            RoutingRig  rig;



            rig.Type ("MODE WINDBG");
            RoutingRig::AssertReply (rig.Type (".help V"), CommandStatus::Ok, "", "",
                                     { "V does not run in WinDbg mode. It runs in AppleWin, Monitor as /V and Casso modes." }, ".help V");

            rig.Type ("!MODE MONITOR");
            RoutingRig::AssertReply (rig.Type ("/HELP V"), CommandStatus::Ok, "", "",
                                     { "dest<first.lastV: Compare first..last with the bytes at dest" }, "Monitor's own V");
        }



        //  GSSquared's and WinDbg's command lists give no window name, since
        //  neither mode runs one; AppleWin's gives them all.
        TEST_METHOD (OnlyTheModesThatRunThemListThem)
        {
            for (const auto & [mode, line, isListed] : { std::tuple<const char *, const char *, bool> { "APPLEWIN",  "HELP",  true  },
                                                         std::tuple<const char *, const char *, bool> { "GSSQUARED", "help",  false },
                                                         std::tuple<const char *, const char *, bool> { "WINDBG",    ".help", false } })
            {
                RoutingRig             rig;
                std::set<std::string>  listed;



                rig.Type (std::string ("MODE ") + mode);

                for (const std::string & text : rig.Type (line).text)
                {
                    std::istringstream  words (text);
                    std::string         first;



                    words >> first;
                    listed.insert (first.ends_with (',') ? first.substr (0, first.size() - 1) : first);
                }

                Assert::IsTrue (listed.size() > 10, RoutingRig::Widen (line).c_str());

                for (const char * name : { "V", "^", "PAGEDN", "PAGEDOWN4K", "RET", "CODE2", "DATA2", "CONSOLE", "MD1", "MT2" })
                {
                    Assert::AreEqual (isListed, listed.contains (name), RoutingRig::Widen (std::string (mode) + " " + name).c_str());
                }
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  KeySchemeTests
    //
    //  No key of any scheme sends a window command: each sends the line its
    //  button sends, in the words of the dialect in force, and the machine
    //  does what that line does. The cursor commands have no key at all.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (KeySchemeTests)
    {
    public:

        using Action = DebuggerKeySchemes::Action;



        //  The AppleWin line each action sends, with the row of $0302 selected
        //  where an action takes the selected line, and none for toggling.
        static std::map<Action, std::string> GetExpectedLines()
        {
            return { { Action::Run,              "G" },
                     { Action::StepInto,         "T" },
                     { Action::StepOver,         "P" },
                     { Action::StepOut,          "RTS" },
                     { Action::ToggleBreakpoint, "BP 0300" },
                     { Action::RunToCursor,      "G 0302" } };
        }



        //  Each dialect's words for those lines.
        static std::string GetExpectedModeLine (const std::string & line, CommandMode mode)
        {
            static const std::map<std::string, std::pair<std::string, std::string>>  kWords =
            {
                { "G",       { "g",       "g"       } },
                { "T",       { "s",       "t"       } },
                { "P",       { "o",       "p"       } },
                { "RTS",     { "r",       "gu"      } },
                { "BP 0300", { "bp 0300", "bp 0300" } },
                { "G 0302",  { "G 0302",  "g 0302"  } },
            };



            switch (mode)
            {
            case CommandMode::Monitor:   return "/" + line;
            case CommandMode::GSSquared: return kWords.at (line).first;
            case CommandMode::WinDbg:    return kWords.at (line).second;
            default:                     return line;
            }
        }



        TEST_METHOD (EveryKeySendsItsLineAndNoWindowCommand)
        {
            static const DebuggerKeyScheme  kSchemes[] = { DebuggerKeyScheme::VisualStudio, DebuggerKeyScheme::AppleWin, DebuggerKeyScheme::GSSquared };
            std::map<Action, std::string>   expected   = GetExpectedLines();
            RoutingRig                      rig;
            DebuggerViewSnapshot            snapshot   = rig.Build();
            int                             selected   = RoutingRig::GetRowOf (snapshot.codeViews[0], 0x0302);
            size_t                          chords     = 0;



            Assert::IsTrue (selected >= 0);

            for (DebuggerKeyScheme scheme : kSchemes)
            {
                for (const DxuiKeyChord & chord : DebuggerKeySchemes::GetMap (scheme).GetChords())
                {
                    Action                      action = (Action) chord.id;
                    std::optional<std::string>  line   = DebuggerViewState::GetActionLine (action, &snapshot, action == Action::ToggleBreakpoint ? -1 : selected);
                    std::wstring                where  = std::format (L"{} {}", DebuggerKeySchemes::GetMap (scheme).GetName(), chord.id);
                    const AppleWinCommand     * entry  = nullptr;



                    chords++;

                    if (action == Action::Pause)
                    {
                        Assert::IsFalse (line.has_value(), where.c_str());
                        continue;
                    }

                    Assert::IsTrue   (line.has_value(), where.c_str());
                    Assert::AreEqual (expected.at (action), *line, where.c_str());

                    entry = AppleWinCommandTable::Find (RoutingRig::GetName (*line));
                    Assert::IsTrue (entry != nullptr && entry->availability != CommandAvailability::WindowOnly, where.c_str());

                    for (CommandMode mode : { CommandMode::AppleWin, CommandMode::Casso, CommandMode::Monitor, CommandMode::GSSquared, CommandMode::WinDbg })
                    {
                        Assert::AreEqual (GetExpectedModeLine (*line, mode), DebuggerViewState::GetModeLine (*line, mode), where.c_str());
                    }
                }
            }

            Assert::AreEqual ((size_t) 21, chords, L"seven actions in each of three schemes");
        }



        //  Each line in each dialect does to the machine what the button does:
        //  a step moves the PC, a run starts, a breakpoint is set, and run to
        //  cursor stops at the selected line. No pane is brought forward.
        TEST_METHOD (EveryKeysLineWorksInEveryDialect)
        {
            static const std::pair<CommandMode, const char *>  kModes[] =
            {
                { CommandMode::AppleWin,  "APPLEWIN"  },
                { CommandMode::Casso,     "CASSO"     },
                { CommandMode::Monitor,   "MONITOR"   },
                { CommandMode::GSSquared, "GSSQUARED" },
                { CommandMode::WinDbg,    "WINDBG"    },
            };



            std::vector<std::tuple<CommandMode, std::string, Action, std::string>>  cases;



            //  GSSquared's run to cursor is not among them: its line does not
            //  run there.
            for (const auto & [mode, name] : kModes)
            {
                for (const auto & [action, line] : GetExpectedLines())
                {
                    if (mode != CommandMode::GSSquared || action != Action::RunToCursor)
                    {
                        cases.emplace_back (mode, name, action, line);
                    }
                }
            }

            Assert::AreEqual ((size_t) 29, cases.size());

            for (const auto & [mode, name, action, line] : cases)
            {
                RoutingRig            rig;
                DebuggerViewSnapshot  before;
                std::string           typed = DebuggerViewState::GetModeLine (line, mode);
                std::wstring          where = RoutingRig::Widen (name + " " + typed);
                Reply                 reply;



                rig.Type (DebuggerViewState::GetModeLine ("MODE " + name, CommandMode::AppleWin));
                before = rig.Build();
                reply  = rig.Type (typed);

                Assert::AreEqual ((int) CommandStatus::Ok, (int) reply.status, where.c_str());

                if (action == Action::ToggleBreakpoint)
                {
                    Assert::AreEqual ((size_t) 1,    rig.Build().breakpoints.size(), where.c_str());
                    Assert::AreEqual ((Word) 0x0300, rig.Build().breakpoints[0].address, where.c_str());
                }
                else if (action == Action::StepInto || action == Action::StepOver || action == Action::RunToCursor)
                {
                    Assert::IsTrue   (rig.RunUntilStopped(), where.c_str());
                    Assert::AreEqual (action == Action::RunToCursor ? (Word) 0x0302 : (Word) 0x0301, rig.GetRegisters().pc, where.c_str());
                }
                else
                {
                    Assert::IsTrue (rig.controller.IsRunInProgress(), where.c_str());
                    rig.controller.RequestPause();
                    rig.RunFrames (1);
                }

                Assert::AreEqual (before.showPaneSerial, rig.Build().showPaneSerial, where.c_str());
            }
        }



        //  Paging and arrow keys belong to the code list, never to a scheme, so
        //  the cursor commands can only be typed; AppleWin's Ctrl+Down is run
        //  to cursor.
        TEST_METHOD (NoSchemeBindsTheCursorKeys)
        {
            static const WPARAM  kKeys[] = { VK_PRIOR, VK_NEXT, VK_HOME, VK_END, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT };



            for (DebuggerKeyScheme scheme : { DebuggerKeyScheme::VisualStudio, DebuggerKeyScheme::AppleWin, DebuggerKeyScheme::GSSquared })
            {
                const DxuiKeyMap  & map = DebuggerKeySchemes::GetMap (scheme);



                for (WPARAM vk : kKeys)
                {
                    for (int modifiers = 0; modifiers < 8; modifiers++)
                    {
                        int   id       = 0;
                        bool  ctrl     = (modifiers & 1) != 0;
                        bool  isMapped = map.TryTranslate (vk, ctrl, (modifiers & 2) != 0, (modifiers & 4) != 0, id);



                        if (scheme == DebuggerKeyScheme::AppleWin && vk == VK_DOWN && modifiers == 1)
                        {
                            Assert::IsTrue (isMapped && (Action) id == Action::RunToCursor, L"AppleWin's Ctrl+Down");
                            continue;
                        }

                        Assert::IsFalse (isMapped, std::format (L"{} key {} modifiers {}", map.GetName(), vk, modifiers).c_str());
                    }
                }
            }
        }
    };
}
