#include "Pch.h"

#include "EntryPointRig.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace SessionSettingTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ConfigureDisassemblyTests
    //
    //  DISASM lists, shows and sets eight 0-or-1 settings, which last as long
    //  as the session.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ConfigureDisassemblyTests)
    {
    public:

        static constexpr const char * kDefaults = "BRANCH = 1\nCLICK = 0\nCOLON = 1\nFENCE = 1\nOPCODE = 1\nPOINTER = 1\nSPACES = 1\nTARGET = 1";



        TEST_METHOD (ListsTheEightSettings_InEveryDialect)
        {
            const std::pair<CommandMode, const char *>  forms[] =
            {
                { CommandMode::AppleWin,  "DISASM"      },
                { CommandMode::AppleWin,  "disasm"      },
                { CommandMode::Casso,     "DISASM"      },
                { CommandMode::Monitor,   "/DISASM"     },
                { CommandMode::GSSquared, "disasm"      },
                { CommandMode::WinDbg,    "!disasm"     },
            };
            size_t  checked = 0;



            for (const auto & [mode, line] : forms)
            {
                ConsoleRig  rig;



                rig.SwitchTo (mode);
                ReplyCheck::IsOk (rig.Type (line), kDefaults, line);
                ++checked;
            }

            Assert::AreEqual (std::size (forms), checked);
        }


        //  Each setting, shown by name in either case, flipped and back, the
        //  list following along. Only the named one changes.
        TEST_METHOD (EachSettingShowsAndSets)
        {
            const std::tuple<const char *, const char *, char>  settings[] =
            {
                { "BRANCH",  "branch",  '1' },
                { "CLICK",   "click",   '0' },
                { "COLON",   "colon",   '1' },
                { "FENCE",   "fence",   '1' },
                { "OPCODE",  "opcode",  '1' },
                { "POINTER", "pointer", '1' },
                { "SPACES",  "spaces",  '1' },
                { "TARGET",  "target",  '1' },
            };
            ConsoleRig  rig;
            size_t      checked = 0;



            for (const auto & [upper, lower, initial] : settings)
            {
                std::string  name     (upper);
                std::string  flipped  (1, initial == '1' ? '0' : '1');
                std::string  original (1, initial);
                std::string  list     (kDefaults);



                list.replace (list.find (name + " = ") + name.size() + 3, 1, flipped);

                ReplyCheck::IsOk (rig.Type ("DISASM " + name), name + " = " + original, "DISASM " + name);
                ReplyCheck::IsOk (rig.Type (std::string ("disasm ") + lower), name + " = " + original, std::string ("disasm ") + lower);

                ReplyCheck::IsOk (rig.Type (std::string ("DISASM ") + lower + " " + flipped), name + " = " + flipped, "set " + name);
                ReplyCheck::IsOk (rig.Type ("DISASM " + name), name + " = " + flipped, "it sticks: " + name);
                ReplyCheck::IsOk (rig.Type ("DISASM"), list, "only " + name + " changed");

                ReplyCheck::IsOk (rig.Type ("DISASM " + name + " " + original), name + " = " + original, "back: " + name);
                ReplyCheck::IsOk (rig.Type ("DISASM"), kDefaults, "the list after " + name);
                ++checked;
            }

            Assert::AreEqual (std::size (settings), checked);
        }

        //  A setting lasts the session: across other commands, a run, and the
        //  window closing and reopening, which keeps the session.
        TEST_METHOD (ASettingLastsTheSession)
        {
            ConsoleRig  rig;
            HRESULT     hr = S_OK;



            ReplyCheck::IsOk (rig.Type ("DISASM OPCODE 0"), "OPCODE = 0", "set");
            (void) rig.Type ("MODE MONITOR");
            (void) rig.Type ("/BP 302");
            (void) rig.Type ("/G");
            rig.FinishRun();

            rig.controller.Close();
            hr = rig.controller.Open();
            Assert::AreEqual (S_OK, hr);

            ReplyCheck::IsOk (rig.Type ("/DISASM OPCODE"), "OPCODE = 0", "after a mode change, a run and a reopen");
            ReplyCheck::IsOk (rig.Request ("DISASM OPCODE", CommandMode::AppleWin), "OPCODE = 0", "the channel sees the window's setting");
        }


        TEST_METHOD (AnUnknownSetting_IsAnError_AndChangesNothing)
        {
            ConsoleRig  rig;



            for (const char * line : { "DISASM FOO", "DISASM FOO 1", "disasm bran 0", "DISASM 1" })
            {
                ReplyCheck::IsError (rig.Type (line), CommandStatus::Error, "invalid arguments",
                                     "DISASM takes BRANCH, CLICK, COLON, FENCE, OPCODE, POINTER, SPACES or TARGET, then 0 or 1.", line);
            }

            ReplyCheck::IsOk (rig.Type ("DISASM"), kDefaults, "nothing changed");
        }


        TEST_METHOD (AValueOtherThanZeroOrOne_IsAnError_AndChangesNothing)
        {
            ConsoleRig  rig;



            for (const char * line : { "DISASM BRANCH 2", "DISASM BRANCH on", "DISASM BRANCH -1", "DISASM CLICK 01", "DISASM TARGET true" })
            {
                ReplyCheck::IsError (rig.Type (line), CommandStatus::Error, "invalid arguments", "A DISASM setting is 0 or 1.", line);
            }

            ReplyCheck::IsOk (rig.Type ("DISASM"), kDefaults, "nothing changed");
        }


        TEST_METHOD (WordsAfterTheValue_AreIgnored)
        {
            ConsoleRig  rig;



            ReplyCheck::IsOk (rig.Type ("DISASM BRANCH 0 extra words"), "BRANCH = 0", "set");
            ReplyCheck::IsOk (rig.Type ("DISASM BRANCH"), "BRANCH = 0", "it took");
        }


        TEST_METHOD (InMonitorMode_AnErrorCarriesErr)
        {
            ConsoleRig  rig;



            rig.SwitchTo (CommandMode::Monitor);
            ReplyCheck::IsMonitorError (rig.Type ("/DISASM BRANCH 2"), CommandStatus::Error, "invalid arguments", "A DISASM setting is 0 or 1.", "/DISASM");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LogTests
    //
    //  LOG shows or sets which notifications print. Its one effect a batch run
    //  can show is the JSON mode-change record, which only INFO and ALL write.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (LogTests)
    {
    public:

        TEST_METHOD (ShowsInfoByDefault_InEveryDialect)
        {
            const std::pair<CommandMode, const char *>  forms[] =
            {
                { CommandMode::AppleWin,  "LOG"  },
                { CommandMode::AppleWin,  "log"  },
                { CommandMode::Casso,     "LOG"  },
                { CommandMode::Monitor,   "/LOG" },
                { CommandMode::GSSquared, "log"  },
                { CommandMode::WinDbg,    "!log" },
            };
            size_t  checked = 0;



            for (const auto & [mode, line] : forms)
            {
                ConsoleRig  rig;



                rig.SwitchTo (mode);
                ReplyCheck::IsOk (rig.Type (line), "Log: INFO", line);
                Assert::IsTrue   (rig.Session().GetLogLevel() == LogLevel::Info);
                ++checked;
            }

            Assert::AreEqual (std::size (forms), checked);
        }


        TEST_METHOD (EveryLevelName)
        {
            const std::tuple<const char *, const char *, LogLevel>  levels[] =
            {
                { "LOG NONE",    "Log: ERROR", LogLevel::Error },
                { "LOG OFF",     "Log: ERROR", LogLevel::Error },
                { "LOG error",   "Log: ERROR", LogLevel::Error },
                { "LOG WARN",    "Log: INFO",  LogLevel::Info  },
                { "LOG INFO",    "Log: INFO",  LogLevel::Info  },
                { "LOG default", "Log: INFO",  LogLevel::Info  },
                { "LOG ON",      "Log: INFO",  LogLevel::Info  },
                { "LOG ALL",     "Log: ALL",   LogLevel::All   },
                { "log all",     "Log: ALL",   LogLevel::All   },
            };
            ConsoleRig  rig;
            size_t      checked = 0;



            for (const auto & [line, text, level] : levels)
            {
                (void) rig.Type (level == LogLevel::All ? "LOG ERROR" : "LOG ALL");

                ReplyCheck::IsOk (rig.Type (line), text, line);
                Assert::IsTrue   (rig.Session().GetLogLevel() == level, ReplyCheck::Widen (line).c_str());
                ReplyCheck::IsOk (rig.Type ("LOG"), text, std::string ("LOG after ") + line);
                ++checked;
            }

            Assert::AreEqual (std::size (levels), checked);
        }


        TEST_METHOD (AnyOtherWord_IsAnError_AndChangesNothing)
        {
            ConsoleRig  rig;



            (void) rig.Type ("LOG ALL");

            for (const char * line : { "LOG loud", "LOG ALL x", "LOG 1", "LOG INFO ERROR" })
            {
                ReplyCheck::IsError (rig.Type (line), CommandStatus::Error, "invalid arguments",
                                     "LOG takes NONE, ERROR, WARN, INFO, DEFAULT, ALL, OFF or ON.", line);
                Assert::IsTrue (rig.Session().GetLogLevel() == LogLevel::All, ReplyCheck::Widen (line).c_str());
            }
        }


        //  At ERROR a mode change writes no record of its own; at INFO and ALL
        //  it does.
        TEST_METHOD (InBatchJson_GatesTheModeChangeRecord)
        {
            const std::pair<const char *, bool>  levels[] =
            {
                { "LOG ERROR", false },
                { "LOG INFO",  true  },
                { "LOG ALL",   true  },
            };
            size_t  checked = 0;



            for (const auto & [level, isWritten] : levels)
            {
                BatchScriptRig  rig;
                std::string     expected;



                rig.options.json     = true;
                rig.options.commands = { level, "MODE MONITOR" };

                Assert::AreEqual (0, rig.Run (""));

                expected = std::format (R"({{"type":"reply","status":"ok","command":"{}","data":{{"kind":"message"}},"text":["{}"]}})" "\n",
                                        level, level == std::string ("LOG ERROR") ? "Log: ERROR" : level == std::string ("LOG INFO") ? "Log: INFO" : "Log: ALL");
                expected += R"({"type":"reply","status":"ok","command":"MODE MONITOR","data":{"kind":"mode","mode":"monitor"},"text":["Mode: MONITOR"]})" "\n";
                expected += isWritten ? R"({"type":"modeChanged","mode":"monitor"})" "\n" : "";

                Assert::AreEqual (expected, rig.result.output, ReplyCheck::Widen (level).c_str());
                ++checked;
            }

            Assert::AreEqual (std::size (levels), checked);
        }


        //  In batch text a mode change prints nothing of its own at any level,
        //  and no other notification a batch run raises is gated, so the
        //  transcript differs only in LOG's own reply.
        TEST_METHOD (InBatchText_ChangesNothingElse)
        {
            BatchScriptRig  quiet;
            BatchScriptRig  loud;



            quiet.options.commands = { "LOG ERROR", "MEB 300 A9 41 8D 00 04 60", "= 300", "BP 302", "G", "MODE MONITOR", "/R" };
            loud.options.commands  = { "LOG ALL",   "MEB 300 A9 41 8D 00 04 60", "= 300", "BP 302", "G", "MODE MONITOR", "/R" };

            Assert::AreEqual (0, quiet.Run (""));
            Assert::AreEqual (0, loud.Run (""));

            Assert::IsTrue   (quiet.result.output.starts_with (">LOG ERROR\nLog: ERROR\n"));
            Assert::IsTrue   (loud.result.output.starts_with  (">LOG ALL\nLog: ALL\n"));
            Assert::AreEqual (quiet.result.output.substr (std::string (">LOG ERROR\nLog: ERROR\n").size()),
                              loud.result.output.substr  (std::string (">LOG ALL\nLog: ALL\n").size()));
            Assert::IsTrue   (quiet.result.output.find ("Breakpoint #0 at $0302\n") != std::string::npos, L"a stop prints at every level");
        }
    };
}
