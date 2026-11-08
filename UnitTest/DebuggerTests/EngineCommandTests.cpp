#include "Pch.h"

#include "Debugger/CommandModeNames.h"
#include "RunControlRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ShowModeTests
    //
    //  MODE with nothing after it, in each dialect's form: it reports the mode
    //  and changes nothing, and runs while the machine does.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ShowModeTests)
    {
    public:

        TEST_METHOD (EachDialectsFormReportsTheModeOnTheConsole)
        {
            struct Form
            {
                CommandMode   mode;
                const char  * line;
            };

            static constexpr Form  kForms[] =
            {
                { CommandMode::AppleWin,  "MODE"  },
                { CommandMode::Monitor,   "/MODE" },
                { CommandMode::GSSquared, "mode"  },
                { CommandMode::GSSquared, "MODE"  },
                { CommandMode::WinDbg,    "!mode" },
                { CommandMode::Casso,     "MODE"  },
            };

            EmulatorRunRig  rig;
            size_t          checked = 0;



            for (const Form & form : kForms)
            {
                OutputFormat  format  = OutputFormat::AppleWin;
                size_t        changes = 0;
                Reply         reply;



                if (rig.Session().GetMode() != form.mode)
                {
                    rig.EnterMode (form.mode);
                }

                format  = rig.Session().GetOutputFormat();
                changes = rig.RecordsOfType ("modeChanged").size();
                reply   = rig.ConsoleOk (form.line);

                Assert::AreEqual (std::string ("Mode: ") + Dialects::GetName (form.mode) + "\n", RunText::Join (reply.text), RunText::Widen (form.line).c_str());
                Assert::IsTrue   (rig.Session().GetMode() == form.mode,       L"the mode is unchanged");
                Assert::IsTrue   (rig.Session().GetOutputFormat() == format,   L"and so is the format");
                Assert::AreEqual (changes, rig.RecordsOfType ("modeChanged").size(), L"and nothing was announced");
                ++checked;
            }

            Assert::AreEqual (std::size (kForms), checked);
        }



        TEST_METHOD (ShowModeRunsWhileTheMachineRuns)
        {
            EmulatorRunRig  free (true);
            EmulatorRunRig  rig;



            Assert::AreEqual (std::string ("Mode: APPLEWIN\n"), RunText::Join (free.ConsoleOk ("MODE").text), L"a free-running machine");

            rig.Load (0x0300, { 0xE8, 0xE8, 0x4C, 0x00, 0x03 }, 0x0300);
            rig.ConsoleOk ("G");
            rig.Pass();
            Assert::IsTrue   (rig.controller.GetRunDriver().IsRunning());
            Assert::AreEqual (std::string ("Mode: APPLEWIN\n"), RunText::Join (rig.ConsoleOk ("MODE").text), L"during a debugger run");
            Assert::IsTrue   (rig.controller.GetRunDriver().IsRunning(), L"and the run goes on");
            Assert::IsTrue   (rig.Session().GetRunState() == RunState::DebugRun);
        }



        TEST_METHOD (EachDialectsFormReportsTheModeInBatch)
        {
            BatchScriptRig  rig;



            Assert::AreEqual (0, rig.Run ({ "MODE", "MODE MONITOR", "/MODE", "/MODE GSSQUARED", "mode", "MODE",
                                            "mode windbg", "!mode", "!mode casso", "MODE" }));
            rig.AssertOutput ({ ">MODE",               "Mode: APPLEWIN",
                                ">MODE MONITOR",       "Mode: MONITOR",
                                "*/MODE",              "Mode: MONITOR",
                                "*/MODE GSSQUARED",    "Mode: GSSQUARED",
                                ">mode",               "Mode: GSSQUARED",
                                ">MODE",               "Mode: GSSQUARED",
                                ">mode windbg",        "Mode: WINDBG",
                                "0:000> !mode",        "Mode: WINDBG",
                                "0:000> !mode casso",  "Mode: CASSO",
                                ">MODE",               "Mode: CASSO" });
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SetModeTests
    //
    //  MODE with a name: the session's mode and output format for every way in,
    //  announced to every client.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SetModeTests)
    {
    public:

        //  Every dialect's form switches to every mode, and each switch sets the
        //  mode's own output format and is announced.
        TEST_METHOD (EveryDialectSwitchesToEveryMode)
        {
            EmulatorRunRig  rig;
            size_t          checked = 0;



            for (CommandMode from : Dialects::kAll)
            {
                for (CommandMode to : Dialects::kAll)
                {
                    std::string             line;
                    std::wstring            where;
                    size_t                  before = 0;
                    Reply                   reply;
                    std::vector<JsonValue>  changes;



                    if (rig.Session().GetMode() != from)
                    {
                        rig.EnterMode (from);
                    }

                    line   = Dialects::GetSwitchLine (from, to);
                    where  = RunText::Widen (line + " in " + Dialects::GetName (from));
                    before = rig.RecordsOfType ("modeChanged").size();
                    reply  = rig.ConsoleOk (line);

                    Assert::AreEqual (std::string ("Mode: ") + Dialects::GetName (to) + "\n", RunText::Join (reply.text), where.c_str());
                    Assert::IsTrue   (rig.Session().GetMode() == to, where.c_str());
                    Assert::IsTrue   (rig.Session().GetOutputFormat() == CommandModeNames::GetOutputFormat (to), where.c_str());

                    changes = rig.RecordsOfType ("modeChanged");
                    Assert::AreEqual (before + 1, changes.size(), where.c_str());
                    Assert::AreEqual (std::string (CommandModeNames::GetName (to)), RunText::GetString (changes.back(), "mode"), where.c_str());
                    ++checked;
                }
            }

            Assert::AreEqual ((size_t) 25, checked);
        }



        TEST_METHOD (AModeSwitchPutsBackTheModesOwnOutputFormat)
        {
            EmulatorRunRig  rig;



            rig.ConsoleOk ("OUTPUT MONITOR");
            Assert::IsTrue (rig.Session().GetOutputFormat() == OutputFormat::Monitor);

            rig.ConsoleOk ("MODE WINDBG");
            Assert::IsTrue (rig.Session().GetOutputFormat() == OutputFormat::WinDbg);

            rig.ConsoleOk ("!output gssquared");
            rig.ConsoleOk ("!mode applewin");
            Assert::IsTrue (rig.Session().GetOutputFormat() == OutputFormat::AppleWin);
        }



        TEST_METHOD (ModeNamesAreTakenInAnyCaseAndWhatFollowsIsIgnored)
        {
            EmulatorRunRig  rig;



            rig.ConsoleOk ("MODE monitor junk");
            Assert::IsTrue (rig.Session().GetMode() == CommandMode::Monitor);

            rig.ConsoleOk ("/mode GsSquared and more");
            Assert::IsTrue (rig.Session().GetMode() == CommandMode::GSSquared);

            rig.ConsoleOk ("MoDe WinDbg");
            Assert::IsTrue (rig.Session().GetMode() == CommandMode::WinDbg);

            rig.ConsoleOk ("!Mode Casso x y");
            Assert::IsTrue (rig.Session().GetMode() == CommandMode::Casso);
        }



        //  Casso mode reads AppleWin's lines, engine commands by bare name, and
        //  writes AppleWin's text.
        TEST_METHOD (CassoModeReadsAndWritesAppleWin)
        {
            EmulatorRunRig  rig;
            BatchScriptRig  batch;



            rig.Load (0x0300, { 0xE8, 0xE8, 0xEA }, 0x0400);
            rig.EnterMode (CommandMode::Casso);
            Assert::IsTrue   (rig.Session().GetOutputFormat() == OutputFormat::AppleWin);
            Assert::AreEqual (std::string (">"), std::string (DebugSession::GetPrompt (CommandMode::Casso)));

            Assert::AreEqual (std::string ("A:00 X:00 Y:00 P:34 S:FF PC:0400  ..RB.I..\n"), RunText::Join (rig.ConsoleOk ("R").text));
            Assert::IsTrue   (rig.ConsoleOk ("BUDGET 100").text.empty());
            rig.ConsoleOk    ("BP 302");
            rig.ConsoleOk    ("300G");
            Assert::IsTrue   (rig.RunUntilPaused());
            Assert::AreEqual (0x0302, RunText::GetInt (rig.OnlyStop(), "pc"), L"addrG ran from $0300");

            batch.Load (0x0300, { 0xE8, 0xE8, 0xEA }, 0x0300);
            Assert::AreEqual (0, batch.Run ({ "MODE CASSO", "BP 302", "G", "R" }));
            batch.AssertOutput ({ ">MODE CASSO", "Mode: CASSO",
                                  ">BP 302",     "Breakpoint #0 set at $0302",
                                  ">G",          "Breakpoint #0 at $0302",
                                  ">R",          "A:00 X:02 Y:00 P:24 S:FD PC:0302  ..R..I.." });
        }



        //  An unknown name is an error in every dialect, and leaves the mode and
        //  the format as they were. The detail lists every mode.
        TEST_METHOD (AnUnknownModeChangesNothing)
        {
            EmulatorRunRig  rig;
            size_t          checked = 0;



            for (CommandMode mode : Dialects::kAll)
            {
                std::string   line    = Dialects::GetEngineLine (mode, "MODE sideways");
                std::wstring  where   = RunText::Widen (line);
                size_t        before  = 0;
                OutputFormat  format  = OutputFormat::AppleWin;
                Reply         reply;



                if (rig.Session().GetMode() != mode)
                {
                    rig.EnterMode (mode);
                }

                before = rig.RecordsOfType ("modeChanged").size();
                format = rig.Session().GetOutputFormat();
                reply  = rig.Console (line);

                Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status, where.c_str());
                Assert::AreEqual (std::string ("invalid arguments"), reply.error.label, where.c_str());
                Assert::IsTrue   (reply.error.detail.starts_with ("The modes are APPLEWIN, MONITOR, GSSQUARED, WINDBG, "), where.c_str());
                Assert::IsTrue   (reply.error.detail.ends_with (" CASSO."), where.c_str());
                Assert::IsTrue   (rig.Session().GetMode() == mode, where.c_str());
                Assert::IsTrue   (rig.Session().GetOutputFormat() == format, where.c_str());
                Assert::AreEqual (before, rig.RecordsOfType ("modeChanged").size(), where.c_str());
                ++checked;
            }

            Assert::AreEqual ((size_t) 5, checked);
        }



        //  The mode is the session's: a switch from the window changes how a
        //  pipe client's lines are read, and one from a client changes the
        //  window's. Breakpoints outlive every switch.
        TEST_METHOD (TheModeIsTheSessionsForEveryWayIn)
        {
            EmulatorRunRig      rig;
            JsonValue           reply;
            const JsonValue   * text = nullptr;



            rig.Load (0x0300, { 0xA9, 0x41, 0x60 }, 0x0300);
            rig.ConsoleOk ("BP 300");
            rig.ConsoleOk ("MODE MONITOR");

            reply = rig.Command ("300.302", 1);
            Assert::AreEqual (std::string ("ok"), RunText::GetString (reply, "status"), L"the client's line is read as the Monitor reads it");
            Assert::IsTrue   (reply.HasArray ("text", text));
            Assert::AreEqual (std::string ("0300- A9 41 60"), text->GetArrayElement (0).GetString());

            reply = rig.Command ("/MODE WINDBG", 2);
            Assert::IsTrue   (rig.Session().GetMode() == CommandMode::WinDbg);
            Assert::AreEqual (std::string ("0 e 0300 0001 (0001)\n"), RunText::Join (rig.ConsoleOk ("bl").text), L"the window reads WinDbg now, and the breakpoint is still there");
        }



        //  A request's mode reads and writes that line only.
        TEST_METHOD (ARequestModeAppliesToItsLineOnly)
        {
            EmulatorRunRig       rig;
            ChannelConnectionId  other = 0;
            JsonValue            reply;
            const JsonValue    * text  = nullptr;



            rig.Load (0x0300, { 0xA9, 0x41, 0x60 }, 0x0300);
            other = rig.transport.Connect();

            reply = rig.Request (R"({"type":"command","id":1,"line":"300.302","mode":"monitor"})");
            Assert::IsTrue   (reply.HasArray ("text", text));
            Assert::AreEqual (std::string ("0300- A9 41 60"), text->GetArrayElement (0).GetString());
            Assert::IsTrue   (rig.Session().GetMode() == CommandMode::AppleWin, L"the session's mode is unchanged");

            reply = rig.Request (R"({"type":"command","id":2,"line":"r","mode":"windbg"})");
            Assert::IsTrue   (reply.HasArray ("text", text));
            Assert::AreEqual (std::string ("a=00 x=00 y=00 sp=ff pc=0300 nv-BdIzc"), text->GetArrayElement (0).GetString());

            reply = rig.Command ("R", 3);
            Assert::IsTrue   (reply.HasArray ("text", text));
            Assert::AreEqual (std::string ("A:00 X:00 Y:00 P:34 S:FF PC:0300  ..RB.I.."), text->GetArrayElement (0).GetString(), L"a line with no mode is the session's");

            //  A line that itself switches the mode switches it for everyone.
            reply = rig.Request (R"({"type":"command","id":4,"line":"/MODE WINDBG","mode":"monitor"})");
            Assert::IsTrue   (rig.Session().GetMode() == CommandMode::WinDbg);
            Assert::IsTrue   (reply.HasArray ("text", text));
            Assert::AreEqual (std::string ("Mode: WINDBG"), text->GetArrayElement (0).GetString());
            Assert::AreEqual ((size_t) 1, rig.RecordsOfType ("modeChanged", other).size(), L"announced to the other client");
        }



        //  A request in the session's own mode takes the session's output
        //  format, OUTPUT included; a request in another mode takes that mode's.
        TEST_METHOD (ARequestInTheSessionsModeTakesItsOutputFormat)
        {
            EmulatorRunRig      rig;
            JsonValue           reply;
            const JsonValue   * text = nullptr;



            rig.Load (0x0300, { 0xA9, 0x41, 0x60 }, 0x0300);
            rig.ConsoleOk ("OUTPUT MONITOR");

            reply = rig.Request (R"({"type":"command","id":1,"line":"R","mode":"applewin"})");
            Assert::IsTrue   (reply.HasArray ("text", text));
            Assert::AreEqual (std::string ("A=00 X=00 Y=00 P=34 S=FF"), text->GetArrayElement (0).GetString());

            reply = rig.Request (R"({"type":"command","id":2,"line":"R","mode":"casso"})");
            Assert::IsTrue   (reply.HasArray ("text", text));
            Assert::AreEqual (std::string ("A:00 X:00 Y:00 P:34 S:FF PC:0300  ..RB.I.."), text->GetArrayElement (0).GetString(), L"Casso's own format");
        }



        //  Monitor and WinDbg read a bare engine name as their own words, so
        //  only the marked form reaches the engine.
        TEST_METHOD (BareEngineNamesAreNotCommandsInMonitorOrWinDbg)
        {
            struct Case
            {
                CommandMode   mode;
                const char  * line;
                const char  * text;
            };

            static constexpr Case  kCases[] =
            {
                { CommandMode::Monitor, "MODE APPLEWIN", "ERR\nError: invalid arguments\n       O is not a Monitor command.\n" },
                { CommandMode::Monitor, "PAUSE",         "ERR\nError: invalid arguments\n       P is not a Monitor command.\n" },
                { CommandMode::Monitor, "BUDGET 100",    "ERR\nError: invalid arguments\n       U is not a Monitor command.\n" },
                { CommandMode::WinDbg,  "mode",          "Error: unknown command\n       mode is not a WinDbg-mode command.\n" },
                { CommandMode::WinDbg,  "pause",         "Error: unknown command\n       pause is not a WinDbg-mode command.\n" },
                { CommandMode::WinDbg,  "budget 5",      "Error: unknown command\n       budget is not a WinDbg-mode command.\n" },
            };

            size_t  checked = 0;



            for (const Case & each : kCases)
            {
                EmulatorRunRig  rig;
                Reply           reply;



                rig.EnterMode (each.mode);
                reply = rig.Console (each.line);

                Assert::AreEqual (std::string (each.text), RunText::Join (reply.text), RunText::Widen (each.line).c_str());
                Assert::IsTrue   (rig.Session().GetMode() == each.mode);
                ++checked;
            }

            Assert::AreEqual (std::size (kCases), checked);
        }



        //  A batch mode change is a record of its own only as JSON, and only at
        //  LOG INFO; the startup --mode switch announces nothing.
        TEST_METHOD (ABatchModeChangeIsARecordOnlyAsJsonAtLogInfo)
        {
            BatchScriptRig          json;
            BatchScriptRig          text;
            BatchScriptRig          startup;
            std::vector<JsonValue>  changes;



            json.options.json = true;
            Assert::AreEqual (0, json.Run ({ "MODE MONITOR", "/LOG ERROR", "/MODE APPLEWIN", "LOG INFO", "MODE WINDBG" }));
            changes = json.RecordsOfType ("modeChanged");
            Assert::AreEqual ((size_t) 2, changes.size(), L"MONITOR and WINDBG; APPLEWIN came at LOG ERROR");
            Assert::AreEqual (std::string ("monitor"), RunText::GetString (changes[0], "mode"));
            Assert::AreEqual (std::string ("windbg"),  RunText::GetString (changes[1], "mode"));
            Assert::AreEqual ((size_t) 7, json.Records().size(), L"five replies and the two changes");

            Assert::AreEqual (0, text.Run ({ "MODE MONITOR", "/MODE APPLEWIN" }));
            text.AssertOutput ({ ">MODE MONITOR", "Mode: MONITOR", "*/MODE APPLEWIN", "Mode: APPLEWIN" });

            startup.options.json = true;
            startup.options.mode = "windbg";
            Assert::AreEqual (0, startup.Run ({ "!mode" }));
            Assert::AreEqual ((size_t) 1, startup.Records().size(), L"the reply alone");
            Assert::AreEqual (std::string ("reply"), RunText::GetString (startup.Records()[0], "type"));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SetBudgetTests
    //
    //  BUDGET n: the cycle budget every later run gets, on both run paths, in
    //  every dialect, with its boundaries and its errors.
    //
    //  The loop is INX / INX / JMP $0300 at $0300, seven cycles a turn.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SetBudgetTests)
    {
    public:

        static void LoadLoop (BatchScriptRig & rig)
        {
            rig.Load (0x0300, { 0xE8, 0xE8, 0x4C, 0x00, 0x03 }, 0x0300);
        }



        static void LoadLoop (EmulatorRunRig & rig)
        {
            rig.Load (0x0300, { 0xE8, 0xE8, 0x4C, 0x00, 0x03 }, 0x0300);
        }



        //  The budget ends every run after it, JSR's included, and the reply
        //  to BUDGET has no text.
        TEST_METHOD (ATypedBudgetEndsEveryLaterRunInBatch)
        {
            BatchScriptRig  rig;



            LoadLoop (rig);
            rig.Load (0x0310, { 0x4C, 0x10, 0x03 });

            Assert::AreEqual (3, rig.Run ({ "BUDGET 1000", "G", "R", "= 300", "G", "= 300", "JSR 310", "R" }));
            rig.AssertOutput ({ ">BUDGET 1000",
                                ">G",        "Budget at $0300",
                                ">R",        "A:00 X:1E Y:00 P:24 S:FD PC:0300  ..R..I..",
                                ">= 300",    "A:00 X:1E Y:00 P:24 S:FD PC:0300  ..R..I..",
                                ">G",        "Budget at $0300",
                                ">= 300",    "A:00 X:3C Y:00 P:24 S:FD PC:0300  ..R..I..",
                                ">JSR 310",  "Budget at $0310",
                                ">R",        "A:00 X:3C Y:00 P:24 S:FB PC:0310  ..R..I.." });
        }



        TEST_METHOD (ATypedBudgetIsTheRunsCycleCountInBatch)
        {
            BatchScriptRig          rig;
            std::vector<JsonValue>  stops;



            LoadLoop (rig);
            rig.Load (0x0310, { 0x4C, 0x10, 0x03 });
            rig.options.json = true;

            Assert::AreEqual (3, rig.Run ({ "BUDGET 1000", "G", "= 300", "JSR 310" }));
            stops = rig.RecordsOfType ("stopped");
            Assert::AreEqual ((size_t) 2, stops.size());
            Assert::AreEqual (std::string ("budget"), RunText::GetString (stops[0], "reason"));
            Assert::AreEqual (1001, RunText::GetInt (stops[0], "cycles"), L"143 turns: past the budget by less than an instruction");
            Assert::AreEqual (1002, RunText::GetInt (stops[1], "cycles"), L"334 JMPs");
            Assert::AreEqual (0x0310, RunText::GetInt (stops[1], "pc"));
        }



        //  Each dialect's BUDGET, then that dialect's run.
        TEST_METHOD (EveryDialectsBudgetFormEndsItsRuns)
        {
            BatchScriptRig          rig;
            std::vector<JsonValue>  stops;
            std::vector<JsonValue>  replies;
            const JsonValue       * text = nullptr;



            LoadLoop (rig);
            rig.options.json = true;

            Assert::AreEqual (3, rig.Run ({ "MODE MONITOR",    "/BUDGET 500", "300G",
                                            "/MODE GSSQUARED", "budget 400",  "= 300",  "g",
                                            "mode windbg",     "!budget 300", "!= 300", "g",
                                            "!mode casso",     "BUDGET 200",  "= 300",  "G",
                                            "MODE APPLEWIN",   "BUDGET 100",  "= 300",  "G" }));

            stops = rig.RecordsOfType ("stopped");
            Assert::AreEqual ((size_t) 5, stops.size());

            for (const JsonValue & stop : stops)
            {
                Assert::AreEqual (std::string ("budget"), RunText::GetString (stop, "reason"));
            }

            Assert::AreEqual (501, RunText::GetInt (stops[0], "cycles"), L"Monitor");
            Assert::AreEqual (401, RunText::GetInt (stops[1], "cycles"), L"GSSquared");
            Assert::AreEqual (301, RunText::GetInt (stops[2], "cycles"), L"WinDbg");
            Assert::AreEqual (200, RunText::GetInt (stops[3], "cycles"), L"Casso");
            Assert::AreEqual (100, RunText::GetInt (stops[4], "cycles"), L"AppleWin");

            replies = rig.RecordsOfType ("reply");

            for (size_t index : { (size_t) 1, (size_t) 4, (size_t) 8, (size_t) 12, (size_t) 16 })
            {
                Assert::IsTrue   (replies.at (index).HasArray ("text", text));
                Assert::AreEqual ((size_t) 0, text->GetArraySize(), L"BUDGET replies with no text");
                Assert::AreEqual (std::string ("ok"), RunText::GetString (replies[index], "status"));
            }
        }



        //  BUDGET 0 removes the budget: a run longer than the old one ends
        //  where it was going.
        TEST_METHOD (BudgetZeroRemovesTheBudget)
        {
            BatchScriptRig  rig;



            // $0300: LDX #$00 / INX / BNE $0302 / NOP: 1281 cycles to $0305.
            rig.Load (0x0300, { 0xA2, 0x00, 0xE8, 0xD0, 0xFD, 0xEA }, 0x0300);
            rig.options.maxCycles = 1000;

            Assert::AreEqual (0, rig.Run ({ "G 305", "= 300", "BUDGET 0", "G 305", "R" }));
            rig.AssertOutput ({ ">G 305",     "Budget at $0302",
                                ">= 300",     "A:00 X:C8 Y:00 P:A4 S:FD PC:0300  N.R..I..",
                                ">BUDGET 0",
                                ">G 305",     "Run to at $0305",
                                ">R",         "A:00 X:00 Y:00 P:26 S:FD PC:0305  ..R..IZ." });
        }



        TEST_METHOD (BudgetZeroLeavesAnEmulatorRunUnbounded)
        {
            EmulatorRunRig  rig;



            LoadLoop (rig);
            rig.ConsoleOk ("BUDGET 100");
            rig.ConsoleOk ("BUDGET 0");
            rig.ConsoleOk ("G");

            Assert::IsFalse (rig.RunUntilPaused (5), L"five frames, far past 100 cycles");
            Assert::IsTrue  (rig.RecordsOfType ("stopped").empty());

            rig.ConsoleOk ("PAUSE");
            Assert::IsTrue   (rig.RunUntilPaused());
            Assert::AreEqual (std::string ("pause"), RunText::GetString (rig.OnlyStop(), "reason"));
        }



        //  A BUDGET that is not a decimal count is an error, and the budget it
        //  would have replaced stays. What follows the count is ignored.
        TEST_METHOD (ABadBudgetIsAnErrorAndKeepsTheOldOne)
        {
            BatchScriptRig  rig;
            std::string     detail = "       BUDGET takes a number of cycles in decimal. Use BUDGET 0 to remove the budget.";



            LoadLoop (rig);

            Assert::AreEqual (1, rig.Run ({ "BUDGET 700", "BUDGET", "BUDGET C000", "BUDGET $100", "BUDGET -5", "BUDGET #10",
                                            "G", "R", "BUDGET 10 20", "= 300", "G" }));
            rig.AssertOutput ({ ">BUDGET 700",
                                ">BUDGET",       "Error: invalid arguments", detail,
                                ">BUDGET C000",  "Error: invalid arguments", detail,
                                ">BUDGET $100",  "Error: invalid arguments", detail,
                                ">BUDGET -5",    "Error: invalid arguments", detail,
                                ">BUDGET #10",   "Error: invalid arguments", detail,
                                ">G",            "Budget at $0300",
                                ">R",            "A:00 X:C8 Y:00 P:A4 S:FD PC:0300  N.R..I..",
                                ">BUDGET 10 20",
                                ">= 300",        "A:00 X:C8 Y:00 P:A4 S:FD PC:0300  N.R..I..",
                                ">G",            "Budget at $0302" });
        }



        TEST_METHOD (ABadBudgetIsAnErrorInEveryDialect)
        {
            BatchScriptRig  rig;
            std::string     detail = "       BUDGET takes a number of cycles in decimal. Use BUDGET 0 to remove the budget.";



            Assert::AreEqual (1, rig.Run ({ "MODE MONITOR", "/BUDGET", "/MODE GSSQUARED", "budget x", "mode windbg", "!budget" }));
            rig.AssertOutput ({ ">MODE MONITOR",     "Mode: MONITOR",
                                "*/BUDGET",          "ERR", "Error: invalid arguments", detail,
                                "*/MODE GSSQUARED",  "Mode: GSSQUARED",
                                ">budget x",         "Error: invalid arguments", detail,
                                ">mode windbg",      "Mode: WINDBG",
                                "0:000> !budget",    "Error: invalid arguments", detail });
        }



        //  The largest count BUDGET takes, and a run it does not end.
        TEST_METHOD (TheLargestBudgetIsTaken)
        {
            BatchScriptRig  rig;



            rig.Load (0x0300, { 0xA2, 0x00, 0xE8, 0xD0, 0xFD, 0xEA }, 0x0300);

            Assert::AreEqual (0, rig.Run ({ "BUDGET 4294967295", "G 305" }));
            rig.AssertOutput ({ ">BUDGET 4294967295", ">G 305", "Run to at $0305" });
        }



        //  In the emulator the budget is counted across slices, so a run ends
        //  in the slice that spent it.
        TEST_METHOD (ABudgetEndsAnEmulatorRunInTheSliceThatSpentIt)
        {
            EmulatorRunRig  rig;
            JsonValue       stop;
            int             cycles = 0;



            LoadLoop (rig);
            Assert::IsTrue   (rig.ConsoleOk ("BUDGET 5000").text.empty());
            Assert::IsTrue   (rig.ConsoleOk ("G").text.empty());
            Assert::IsTrue   (rig.Session().GetRunState() == RunState::DebugRun);

            Assert::IsTrue   (rig.RunUntilPaused());
            stop   = rig.OnlyStop();
            cycles = RunText::GetInt (stop, "cycles");
            Assert::AreEqual (std::string ("budget"), RunText::GetString (stop, "reason"));
            Assert::IsTrue   (cycles >= 5000 && cycles < 5000 + (int) EmulatorRunRig::kSliceCycles + 3, RunText::Widen (std::to_string (cycles)).c_str());
            Assert::IsTrue   (rig.Session().GetRunState() == RunState::Paused);
        }



        //  A step or a run to an address that completes on the instruction that
        //  spends the budget ends as the step or the run to, not the budget.
        TEST_METHOD (ARunThatCompletesAsTheBudgetRunsOutReportsItsOwnEnd)
        {
            BatchScriptRig  rig;



            LoadLoop (rig);

            Assert::AreEqual (0, rig.Run ({ "BUDGET 2", "T", "BUDGET 7", "= 300", "G 300", "BUDGET 2", "= 300", "P" }));
            rig.AssertOutput ({ ">BUDGET 2",
                                ">T",         "Step at $0301",
                                ">BUDGET 7",
                                ">= 300",     "A:00 X:01 Y:00 P:24 S:FD PC:0300  ..R..I..",
                                ">G 300",     "Run to at $0300",
                                ">BUDGET 2",
                                ">= 300",     "A:00 X:03 Y:00 P:24 S:FD PC:0300  ..R..I..",
                                ">P",         "Step at $0301" });
        }



        TEST_METHOD (AStepThatCompletesAsAnEmulatorSliceSpendsTheBudgetReportsTheStep)
        {
            EmulatorRunRig  rig;



            LoadLoop (rig);
            rig.ConsoleOk ("BUDGET 2");
            rig.ConsoleOk ("T");
            rig.Pass (2);
            rig.controller.Pump();

            Assert::AreEqual (std::string ("step"), RunText::GetString (rig.OnlyStop(), "reason"));
            Assert::AreEqual (0x0301, RunText::GetInt (rig.OnlyStop(), "pc"));
        }



        //  A request's budget bounds the run its line starts; the session's
        //  own comes back for the runs after it.
        TEST_METHOD (ARequestBudgetBoundsOnlyItsOwnRun)
        {
            EmulatorRunRig  rig;
            JsonValue       reply;
            JsonValue       stop;
            bool            running = false;
            int             cycles  = 0;



            LoadLoop (rig);
            rig.ConsoleOk ("BUDGET 50000");

            reply = rig.Request (R"({"type":"command","id":7,"line":"G","budget":3000})");
            Assert::IsTrue   (reply.HasBool ("running", running) && running);
            Assert::IsTrue   (rig.RunUntilPaused());
            stop   = rig.LastStop();
            cycles = RunText::GetInt (stop, "cycles");
            Assert::AreEqual (std::string ("budget"), RunText::GetString (stop, "reason"));
            Assert::AreEqual (7, RunText::GetInt (stop, "causeId"));
            Assert::IsTrue   (cycles >= 3000 && cycles < 3000 + (int) EmulatorRunRig::kSliceCycles + 3, RunText::Widen (std::to_string (cycles)).c_str());
            Assert::AreEqual ((uint64_t) 50000, rig.Session().GetBudget().value_or (0));

            rig.ConsoleOk ("G");
            Assert::IsTrue   (rig.RunUntilPaused());
            cycles = RunText::GetInt (rig.LastStop(), "cycles");
            Assert::IsTrue   (cycles >= 50000 && cycles < 50000 + (int) EmulatorRunRig::kSliceCycles + 3, RunText::Widen (std::to_string (cycles)).c_str());
        }



        //  A request budget of 0 makes that one run unbounded, whatever the
        //  session's budget.
        TEST_METHOD (ARequestBudgetOfZeroMakesItsRunUnbounded)
        {
            EmulatorRunRig  rig;
            JsonValue       stop;



            LoadLoop (rig);
            rig.ConsoleOk ("BUDGET 2000");

            rig.Request (R"({"type":"command","id":3,"line":"G","budget":0})");
            Assert::IsFalse  (rig.RunUntilPaused (5), L"five frames, far past 2000 cycles");

            rig.Request (R"({"type":"pause","id":4})");
            Assert::IsTrue   (rig.RunUntilPaused());
            stop = rig.OnlyStop();
            Assert::AreEqual (std::string ("pause"), RunText::GetString (stop, "reason"));
            Assert::AreEqual (3, RunText::GetInt (stop, "causeId"));
            Assert::AreEqual ((uint64_t) 2000, rig.Session().GetBudget().value_or (0));
        }



        //  A BUDGET line sent with a request budget keeps the value it set.
        TEST_METHOD (ABudgetLineSentWithARequestBudgetKeepsItsValue)
        {
            EmulatorRunRig  rig;



            rig.Request (R"({"type":"command","id":1,"line":"BUDGET 500","budget":1000})");
            Assert::AreEqual ((uint64_t) 500, rig.Session().GetBudget().value_or (0));

            rig.Request (R"({"type":"command","id":2,"line":"BUDGET 0","budget":1000})");
            Assert::IsFalse  (rig.Session().GetBudget().has_value());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PauseTests
    //
    //  PAUSE in every dialect, the channel's pause request and the window's
    //  Pause, with and without a debugger run, and in batch.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (PauseTests)
    {
    public:

        struct Form
        {
            CommandMode   mode;
            const char  * run;
            const char  * pause;
        };

        static constexpr Form  kForms[] =
        {
            { CommandMode::AppleWin,  "G", "PAUSE"  },
            { CommandMode::Monitor,   "G", "/PAUSE" },
            { CommandMode::GSSquared, "g", "pause"  },
            { CommandMode::WinDbg,    "g", "!pause" },
            { CommandMode::Casso,     "G", "PAUSE"  },
        };



        static void LoadLoop (EmulatorRunRig & rig)
        {
            rig.Load (0x0300, { 0xE8, 0xE8, 0x4C, 0x00, 0x03 }, 0x0300);
        }



        //  During a run the pause is taken at the next slice, and the stop
        //  carries what the run spent.
        TEST_METHOD (PauseEndsADebuggerRunInEveryDialect)
        {
            size_t  checked = 0;



            for (const Form & form : kForms)
            {
                EmulatorRunRig  rig;
                std::wstring    where = Dialects::Describe (form.mode);
                Reply           reply;
                JsonValue       stop;
                JsonValue       registers;



                LoadLoop (rig);

                if (form.mode != CommandMode::AppleWin)
                {
                    rig.EnterMode (form.mode);
                }

                rig.ConsoleOk (form.run);
                rig.Pass();
                Assert::IsTrue   (rig.controller.GetRunDriver().IsRunning(), where.c_str());

                reply = rig.ConsoleOk (form.pause);
                Assert::IsTrue   (reply.text.empty(), where.c_str());
                Assert::IsTrue   (rig.RunUntilPaused(), where.c_str());

                stop      = rig.OnlyStop();
                registers = RunText::GetObject (stop, "registers");
                Assert::AreEqual (std::string ("pause"), RunText::GetString (stop, "reason"), where.c_str());
                Assert::IsTrue   (RunText::GetInt (stop, "cycles") > (int) EmulatorRunRig::kFrameCycles, L"a frame and a slice ran");
                Assert::AreEqual ((int) rig.Registers().pc, RunText::GetInt (registers, "pc"), where.c_str());
                Assert::AreEqual ((int) rig.Registers().x,  RunText::GetInt (registers, "x"),  where.c_str());
                Assert::IsTrue   (rig.Session().GetRunState() == RunState::Paused, where.c_str());
                ++checked;
            }

            Assert::AreEqual (std::size (kForms), checked);
        }



        //  With no run, PAUSE stops the running machine, the session knows it
        //  is paused, and every client hears the stop.
        TEST_METHOD (PauseStopsAFreeRunningMachineInEveryDialect)
        {
            size_t  checked = 0;



            for (const Form & form : kForms)
            {
                EmulatorRunRig  rig (true);
                std::wstring    where = Dialects::Describe (form.mode);
                JsonValue       stop;



                LoadLoop (rig);

                if (form.mode != CommandMode::AppleWin)
                {
                    rig.EnterMode (form.mode);
                }

                rig.Pass();
                Assert::IsTrue   (rig.ConsoleOk (form.pause).text.empty(), where.c_str());
                Assert::IsTrue   (rig.cpuManager.IsPaused(), where.c_str());
                Assert::IsTrue   (rig.Session().GetRunState() == RunState::Paused, where.c_str());

                stop = rig.OnlyStop();
                Assert::AreEqual (std::string ("pause"), RunText::GetString (stop, "reason"), where.c_str());
                Assert::AreEqual ((int) rig.Registers().pc, RunText::GetInt (stop, "pc"), where.c_str());
                Assert::IsFalse  (RunText::Has (stop, "causeId"), where.c_str());
                rig.ConsoleOk    (Dialects::GetEngineLine (form.mode, "= 300"));
                Assert::AreEqual ((Word) 0x0300, rig.Registers().pc, L"a paused machine takes an edit");
                ++checked;
            }

            Assert::AreEqual (std::size (kForms), checked);
        }



        //  After a PAUSE the registers can be changed, as on any paused
        //  machine.
        TEST_METHOD (AfterPausingAFreeRunningMachineItCanBeEdited)
        {
            EmulatorRunRig  rig (true);



            LoadLoop (rig);
            rig.Pass();
            Assert::AreEqual (std::string ("machine running"), rig.Console ("= 300").error.label);

            rig.ConsoleOk ("PAUSE");
            rig.ConsoleOk ("= 300");
            Assert::AreEqual ((Word) 0x0300, rig.Registers().pc);
        }



        //  A PAUSE line from a pipe client stops the machine, and the stop names
        //  that command.
        TEST_METHOD (APauseLineFromTheChannelNamesItselfAsTheCause)
        {
            EmulatorRunRig  rig (true);
            JsonValue       reply;



            LoadLoop (rig);
            rig.Pass();
            reply = rig.Command ("PAUSE", 12);
            Assert::AreEqual (std::string ("ok"), RunText::GetString (reply, "status"));
            Assert::IsFalse  (RunText::Has (reply, "running"));
            Assert::IsTrue   (rig.cpuManager.IsPaused());
            Assert::AreEqual (12, RunText::GetInt (rig.OnlyStop(), "causeId"));
        }



        TEST_METHOD (PausingAPausedMachineAnnouncesNothing)
        {
            EmulatorRunRig  rig;
            JsonValue       reply;



            rig.ConsoleOk ("PAUSE");
            reply = rig.Request (R"({"type":"pause","id":2})");
            rig.controller.RequestPause();
            rig.controller.Pump();

            Assert::AreEqual (std::string ("pause"), RunText::GetString (reply, "command"));
            Assert::IsTrue   (rig.RecordsOfType ("stopped").empty(), L"the stop last heard is still the true one");
        }



        //  The pause request is answered at once; the stop follows, after the
        //  reply, and names nothing when no command's run was going.
        TEST_METHOD (ThePauseRequestIsAnsweredAtOnceAndTheStopFollows)
        {
            EmulatorRunRig          rig (true);
            std::vector<JsonValue>  records;



            LoadLoop (rig);
            rig.Pass();
            rig.transport.Send (rig.client, R"({"type":"pause","id":9})");
            rig.controller.Pump();

            records = rig.Records();
            Assert::AreEqual ((size_t) 2, records.size());
            Assert::AreEqual (std::string ("reply"),   RunText::GetString (records[0], "type"));
            Assert::AreEqual (9,                       RunText::GetInt    (records[0], "id"));
            Assert::AreEqual (std::string ("pause"),   RunText::GetString (records[0], "command"));
            Assert::AreEqual (std::string ("stopped"), RunText::GetString (records[1], "type"));
            Assert::AreEqual (std::string ("pause"),   RunText::GetString (records[1], "reason"));
            Assert::IsFalse  (RunText::Has (records[1], "causeId"));
            Assert::IsTrue   (rig.Session().GetRunState() == RunState::Paused);
        }



        //  During a run a client started, the pause request's reply comes
        //  first and the stop names the command whose run it ended.
        TEST_METHOD (APauseDuringAClientsRunNamesThatRun)
        {
            EmulatorRunRig          rig;
            std::vector<JsonValue>  records;



            LoadLoop (rig);
            rig.Command ("G", 4);
            rig.Pass();

            rig.transport.Send (rig.client, R"({"type":"pause","id":5})");
            rig.controller.Pump();
            Assert::IsTrue   (rig.RecordsOfType ("stopped").empty(), L"the run ends at the next slice, not in the pump");
            Assert::AreEqual (5, RunText::GetInt (rig.Records().back(), "id"));

            Assert::IsTrue   (rig.RunUntilPaused());
            Assert::AreEqual (4, RunText::GetInt (rig.OnlyStop(), "causeId"));
            Assert::AreEqual (std::string ("pause"), RunText::GetString (rig.OnlyStop(), "reason"));
        }



        //  The window's Pause key and button reach the controller's
        //  RequestPause, as the channel's pause does.
        TEST_METHOD (TheWindowsPauseEndsARunAndStopsAFreeMachine)
        {
            EmulatorRunRig  running;
            EmulatorRunRig  free (true);



            LoadLoop (running);
            running.ConsoleOk ("G");
            running.Pass();
            running.controller.RequestPause();
            Assert::IsTrue   (running.RunUntilPaused());
            Assert::AreEqual (std::string ("pause"), RunText::GetString (running.OnlyStop(), "reason"));

            LoadLoop (free);
            free.Pass();
            free.controller.RequestPause();
            free.controller.Pump();
            Assert::IsTrue   (free.cpuManager.IsPaused());
            Assert::AreEqual (std::string ("pause"), RunText::GetString (free.OnlyStop(), "reason"));
            Assert::IsTrue   (free.Session().GetRunState() == RunState::Paused);
        }



        //  In batch nothing runs between commands, so PAUSE has nothing to stop,
        //  and a later run is not ended by it.
        TEST_METHOD (PauseInBatchStopsNothingLater)
        {
            BatchScriptRig  rig;



            rig.Load (0x0300, { 0xE8, 0xE8, 0x4C, 0x00, 0x03 }, 0x0300);

            Assert::AreEqual (0, rig.Run ({ "BP 302", "PAUSE", "G", "MODE MONITOR", "/PAUSE", "/MODE APPLEWIN", "G" }));
            rig.AssertOutput ({ ">BP 302",          "Breakpoint #0 set at $0302",
                                ">PAUSE",
                                ">G",               "Breakpoint #0 at $0302",
                                ">MODE MONITOR",    "Mode: MONITOR",
                                "*/PAUSE",
                                "*/MODE APPLEWIN",  "Mode: APPLEWIN",
                                ">G",               "Breakpoint #0 at $0302" });
        }



        //  The user pausing during a run ends it once: one stop, reason pause.
        TEST_METHOD (AUserPauseDuringARunEndsItWithOneStop)
        {
            EmulatorRunRig  rig;



            LoadLoop (rig);
            rig.ConsoleOk ("G");
            rig.Pass();

            rig.UserPause();
            Assert::IsFalse  (rig.controller.GetRunDriver().IsRunning());
            Assert::AreEqual (std::string ("pause"), RunText::GetString (rig.OnlyStop(), "reason"));
            Assert::IsTrue   (RunText::GetInt (rig.OnlyStop(), "cycles") >= (int) EmulatorRunRig::kFrameCycles);
            Assert::IsTrue   (rig.Session().GetRunState() == RunState::Paused);
        }



        //  The user resuming leaves the conditions armed, and says so once.
        TEST_METHOD (AUserResumeLeavesConditionsArmedAndIsHeardOnce)
        {
            EmulatorRunRig  rig;



            LoadLoop (rig);
            rig.ConsoleOk ("BP 302");

            rig.UserResume();
            rig.UserResume();
            Assert::IsTrue    (rig.Session().GetRunState() == RunState::FreeRunning);
            Assert::AreEqual  ((size_t) 1, rig.RecordsOfType ("resumed").size(), L"a machine already running is not resumed again");
            Assert::IsNotNull (rig.machine.GetDebugHook());

            Assert::IsTrue    (rig.RunUntilPaused(), L"the breakpoint still stops it");
            Assert::AreEqual  (0x0302, RunText::GetInt (rig.OnlyStop(), "pc"));
        }
    };
}
