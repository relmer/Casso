#include "Pch.h"

#include "Version.h"
#include "Debugger/Handlers/ConfigHandlers.h"
#include "EntryPointRig.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace ScriptCommandTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ScriptText
    //
    //  What a script's lines print when they are typed one at a time into a
    //  fresh window: the reference a RUN of the same lines is held to.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ScriptText
    {
    public:
        static std::string OfTyped (const std::vector<std::string> & lines)
        {
            ConsoleRig                reference;
            std::vector<std::string>  text;



            for (const std::string & line : lines)
            {
                Reply  reply = reference.Type (line);



                text.insert (text.end(), reply.text.begin(), reply.text.end());
            }

            return ReplyCheck::Join (text);
        }


        static std::string Repeat (const std::string & lines, int count)
        {
            std::string  repeated;



            for (int i = 0; i < count; ++i)
            {
                repeated += "\n" + lines;
            }

            return repeated;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RunScriptTests
    //
    //  RUN and LOAD: a script's lines run through the session exactly as typed
    //  lines do, in the window, on the channel and in batch, with what each
    //  line printed collected into the reply.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (RunScriptTests)
    {
    public:

        TEST_METHOD (AScriptChangesTheMachine_AsItsLinesTypedWould)
        {
            const std::vector<std::string>  lines    = { "MEB 300 EA EB", "R A 41", "BP 310", "= 305", "PRINT A" };
            std::string                     expected = ScriptText::OfTyped (lines);
            size_t                          checked  = 0;



            for (const char * command : { "RUN x.txt", "LOAD x.txt", "run x.txt", "Load x.txt" })
            {
                ConsoleRig  rig;



                rig.files.WriteAllText (L"x.txt", "MEB 300 EA EB\nR A 41\nBP 310\n= 305\nPRINT A\n");

                ReplyCheck::IsOk (rig.Type (command), expected, command);
                Assert::AreEqual ((Byte) 0xEA, rig.Peek (0x300), ReplyCheck::Widen (command).c_str());
                Assert::AreEqual ((Byte) 0xEB, rig.Peek (0x301), ReplyCheck::Widen (command).c_str());
                Assert::AreEqual ((Byte) 0x41, rig.Session().GetTarget().GetRegisters().a, ReplyCheck::Widen (command).c_str());
                Assert::AreEqual ((Word) 0x305, rig.GetPc(), ReplyCheck::Widen (command).c_str());
                ReplyCheck::IsOk (rig.Type ("BPL"), "#0 enabled  at $0310, hits 0", command);
                ++checked;
            }

            Assert::AreEqual ((size_t) 4, checked);
            Assert::IsTrue   (expected.ends_with ("\n0041"), L"the last line printed A as the script left it");
        }


        TEST_METHOD (InCassoMode_TheSame)
        {
            ConsoleRig  rig;



            rig.files.WriteAllText (L"x.txt", "MEB 300 5A\nECHO done\n");
            rig.SwitchTo (CommandMode::Casso);

            ReplyCheck::IsOk (rig.Type ("RUN x.txt"), ScriptText::OfTyped ({ "MEB 300 5A", "ECHO done" }), "RUN in Casso mode");
            Assert::AreEqual ((Byte) 0x5A, rig.Peek (0x300));
        }


        TEST_METHOD (CommentsPrintNothing_AndRunNothing)
        {
            ConsoleRig  rig;
            Byte        before300 = rig.Peek (0x300);
            Byte        before301 = rig.Peek (0x301);



            rig.files.WriteAllText (L"c.txt", "; MEB 300 FF\n   ; MEB 301 FF indented\n\t;tabbed\nECHO after\n;last");

            ReplyCheck::IsOk (rig.Type ("RUN c.txt"), "after", "RUN c.txt");
            Assert::AreEqual (before300, rig.Peek (0x300), L"a comment ran nothing");
            Assert::AreEqual (before301, rig.Peek (0x301), L"an indented comment ran nothing");
        }


        TEST_METHOD (LineEndings)
        {
            const std::pair<const char *, const char *>  scripts[] =
            {
                { "ECHO a\r\nECHO b\r\n", "a\nb" },
                { "ECHO a\nECHO b",       "a\nb" },
                { "ECHO a\n",             "a"    },
                { "ECHO a\r\n",           "a"    },
                { "",                     ""     },
                { "\r\n",                 ""     },
            };
            size_t  checked = 0;



            for (const auto & [content, expected] : scripts)
            {
                ConsoleRig  rig;



                rig.files.WriteAllText (L"e.txt", content);
                ReplyCheck::IsOk (rig.Type ("RUN e.txt"), expected, std::string ("RUN of ") + ReplyCheck::Join ({ content }));
                ++checked;
            }

            Assert::AreEqual (std::size (scripts), checked);
        }


        //  Blank and whitespace-only lines are skipped, as batch skips them:
        //  in Monitor mode a blank line would otherwise be the Monitor's
        //  Return and print the next row of memory.
        TEST_METHOD (BlankLinesAreSkipped)
        {
            ConsoleRig  appleWin;
            ConsoleRig  monitor;



            appleWin.files.WriteAllText (L"b.txt", "ECHO a\n\n   \n\t\nECHO b\n");
            ReplyCheck::IsOk (appleWin.Type ("RUN b.txt"), "a\nb", "RUN b.txt");

            (void) monitor.Type ("MEB 300 A9 41");
            monitor.SwitchTo (CommandMode::Monitor);
            monitor.files.WriteAllText (L"m.txt", "300.301\n\n  \n\n");

            ReplyCheck::IsOk (monitor.Type ("/RUN m.txt"), "0300- A9 41", "/RUN m.txt");
            ReplyCheck::IsOk (monitor.Type ("300.301"),    "0300- A9 41", "the same line typed");
        }


        TEST_METHOD (TheLineAssembler_TakesTheLinesAfterA_UntilABlankLine)
        {
            ConsoleRig  rig;



            rig.files.WriteAllText (L"a.txt", "A 300\nLDA #1\nRTS\n\nECHO after\n");

            ReplyCheck::IsOk (rig.Type ("RUN a.txt"), ScriptText::OfTyped ({ "A 300", "LDA #1", "RTS", "", "ECHO after" }), "RUN a.txt");
            Assert::AreEqual ((Byte) 0xA9, rig.Peek (0x300));
            Assert::AreEqual ((Byte) 0x01, rig.Peek (0x301));
            Assert::AreEqual ((Byte) 0x60, rig.Peek (0x302));
            Assert::IsFalse  (rig.Session().IsAssembling());
        }


        //  A script that ends inside the assembler ends the assembly, so the
        //  next line typed is a command and not source.
        TEST_METHOD (AScriptEndingInTheAssembler_EndsTheAssembly)
        {
            ConsoleRig      rig;
            BatchScriptRig  batch;
            Reply           registers;



            rig.files.WriteAllText (L"asm.txt", "A 300\nLDA #1\n");

            ReplyCheck::IsOk (rig.Type ("RUN asm.txt"), ScriptText::OfTyped ({ "A 300", "LDA #1", "" }), "RUN asm.txt");
            Assert::IsFalse  (rig.Session().IsAssembling(), L"the assembly ended with the script");

            registers = rig.Type ("R");
            Assert::IsTrue (std::holds_alternative<RegistersData> (registers.data), L"R ran as a command");

            batch.files.WriteAllText (L"asm.txt", "A 300\nLDA #1\n");
            batch.options.commands = { "RUN asm.txt", "VERSION" };

            Assert::AreEqual (0, batch.Run (""));
            Assert::IsTrue   (batch.result.output.ends_with (">VERSION\nCasso " VERSION_STRING "\n"), ReplyCheck::Widen (batch.result.output).c_str());
        }


        //  A line that fails prints its error and the script goes on; the RUN
        //  then fails too, saying which line, so batch exits 1 and a channel
        //  client sees an error. A line that is only not available is not a
        //  failure, as it is not in batch.
        TEST_METHOD (AFailingLine_FailsTheScript_AndTheRestStillRuns)
        {
            ConsoleRig  rig;
            ConsoleRig  one;
            ConsoleRig  quiet;



            rig.files.WriteAllText (L"bad.txt", "ECHO one\nFROB\nECHO two\nCD\nDISK EJECT\nMEB 300 5A\n");

            ReplyCheck::IsErrorAfter (rig.Type ("RUN bad.txt"),
                                      "one\n"
                                      "Error: unknown command\n"
                                      "       FROB is not a command.\n"
                                      "two\n"
                                      "Error: invalid arguments\n"
                                      "       CD takes a directory.\n"
                                      "Error: command not available\n"
                                      "       DISK EJECT is not available in this session.\n" +
                                      ScriptText::OfTyped ({ "MEB 300 5A" }),
                                      CommandStatus::Error, "script failed", "2 lines of bad.txt failed, the first at line 2.", "RUN bad.txt");
            Assert::AreEqual ((Byte) 0x5A, rig.Peek (0x300), L"the lines after a failure ran");

            one.files.WriteAllText (L"one.txt", "ECHO fine\nFROB\n");
            ReplyCheck::IsErrorAfter (one.Type ("LOAD one.txt"), "fine\nError: unknown command\n       FROB is not a command.",
                                      CommandStatus::Error, "script failed", "Line 2 of one.txt failed.", "LOAD one.txt");

            quiet.files.WriteAllText (L"na.txt", "DISK EJECT\nECHO ok\n");
            ReplyCheck::IsOk (quiet.Type ("RUN na.txt"),
                              "Error: command not available\n       DISK EJECT is not available in this session.\nok", "RUN na.txt");
        }


        TEST_METHOD (AFailingLine_SetsBatchsExitStatus)
        {
            BatchScriptRig  failing;
            BatchScriptRig  notAvailable;



            failing.files.WriteAllText (L"bad.txt", "FROB\nECHO after\n");
            failing.options.commands = { "RUN bad.txt" };

            Assert::AreEqual (1, failing.Run (""));
            Assert::AreEqual (std::string (">RUN bad.txt\n"
                                           "Error: unknown command\n"
                                           "       FROB is not a command.\n"
                                           "after\n"
                                           "Error: script failed\n"
                                           "       Line 1 of bad.txt failed.\n"), failing.result.output);

            notAvailable.files.WriteAllText (L"na.txt", "DISK EJECT\n");
            notAvailable.options.commands = { "RUN na.txt" };
            Assert::AreEqual (0, notAvailable.Run (""));
        }


        //  A reply record carries the script's text; its status is the
        //  script's, so a JSON reader can tell a script that failed.
        TEST_METHOD (AsJson_OneRecordWithEveryLinesText)
        {
            BatchScriptRig  good;
            BatchScriptRig  bad;



            good.files.WriteAllText (L"x.txt", "ECHO hi\nPRINT 1\n");
            good.options.json     = true;
            good.options.commands = { "RUN x.txt" };

            Assert::AreEqual (0, good.Run (""));
            Assert::AreEqual (std::string (R"({"type":"reply","status":"ok","command":"RUN x.txt","data":{"kind":"message"},"text":["hi","0001"]})" "\n"),
                              good.result.output);

            bad.files.WriteAllText (L"x.txt", "FROB\n");
            bad.options.json     = true;
            bad.options.commands = { "RUN x.txt" };

            Assert::AreEqual (1, bad.Run (""));
            Assert::AreEqual (std::string (R"({"type":"reply","status":"error","command":"RUN x.txt","text":["Error: unknown command","       FROB is not a command.","Error: script failed","       Line 1 of x.txt failed."],"error":{"label":"script failed","detail":"Line 1 of x.txt failed."}})" "\n"),
                              bad.result.output);
        }


        //  Each line is read in the mode in force when it runs: a MODE line
        //  switches the rest of the script, and the session stays switched.
        TEST_METHOD (AModeLine_SwitchesTheRestOfTheScript)
        {
            ConsoleRig  rig;



            (void) rig.Type ("MEB 300 A9 41");
            rig.files.WriteAllText (L"m.txt", "MODE MONITOR\n300.301\n");

            ReplyCheck::IsOk (rig.Type ("RUN m.txt"), "Mode: MONITOR\n0300- A9 41", "RUN m.txt");
            Assert::IsTrue   (rig.Session().GetMode()         == CommandMode::Monitor);
            Assert::IsTrue   (rig.Session().GetOutputFormat() == OutputFormat::Monitor);
        }


        TEST_METHOD (AScriptRunsAnother_Nested)
        {
            ConsoleRig  rig;



            rig.files.WriteAllText (L"a.txt", "ECHO a\nRUN b.txt\nECHO a2\n");
            rig.files.WriteAllText (L"b.txt", "ECHO b\nLOAD c.txt\n");
            rig.files.WriteAllText (L"c.txt", "ECHO c\n");

            ReplyCheck::IsOk (rig.Type ("RUN a.txt"), "a\nb\nc\na2", "RUN a.txt");
        }


        //  A script that runs itself would recurse until the stack ran out;
        //  the seventeenth level fails instead, and every level above it
        //  fails in turn. The session is usable afterwards.
        TEST_METHOD (NestingStopsAtItsLimit)
        {
            const std::tuple<const char *, const wchar_t *, const char *>  selves[] =
            {
                { "RUN self.txt",  L"self.txt",            "self.txt"            },
                { "LOAD self.txt", L"self.txt",            "self.txt"            },
                { "STARTUP",       L"DebuggerAutoRun.txt", "DebuggerAutoRun.txt" },
            };
            size_t  checked = 0;



            for (const auto & [line, file, fileName] : selves)
            {
                ConsoleRig    rig;
                std::string   name (fileName);
                std::string   inner;



                rig.files.WriteAllText (file, std::string (line) + "\n");
                rig.files.WriteAllText (L"ok.txt", "ECHO fine\n");

                inner = "Error: scripts nested too deep\n       RUN, LOAD and STARTUP nest at most 16 deep." +
                        ScriptText::Repeat ("Error: script failed\n       Line 1 of " + name + " failed.", ConfigHandlers::kMaxScriptDepth - 1);

                ReplyCheck::IsErrorAfter (rig.Type (line), inner, CommandStatus::Error, "script failed",
                                          "Line 1 of " + name + " failed.", line);
                ReplyCheck::IsOk (rig.Type ("RUN ok.txt"), "fine", std::string ("a script after ") + line);
                ++checked;
            }

            Assert::AreEqual (std::size (selves), checked);
        }


        //  Sixteen scripts deep runs; seventeen does not.
        TEST_METHOD (NestingLimit_IsSixteenScripts)
        {
            ConsoleRig  sixteen;
            ConsoleRig  seventeen;



            for (int i = 1; i < ConfigHandlers::kMaxScriptDepth; ++i)
            {
                std::wstring  file = std::format (L"n{}.txt", i);
                std::string   next = std::format ("RUN n{}.txt\n", i + 1);



                sixteen.files.WriteAllText   (file, next);
                seventeen.files.WriteAllText (file, next);
            }

            sixteen.files.WriteAllText   (std::format (L"n{}.txt", ConfigHandlers::kMaxScriptDepth), "ECHO bottom\n");
            seventeen.files.WriteAllText (std::format (L"n{}.txt", ConfigHandlers::kMaxScriptDepth), std::format ("RUN n{}.txt\n", ConfigHandlers::kMaxScriptDepth + 1));
            seventeen.files.WriteAllText (std::format (L"n{}.txt", ConfigHandlers::kMaxScriptDepth + 1), "ECHO bottom\n");

            ReplyCheck::IsOk (sixteen.Type ("RUN n1.txt"), "bottom", "sixteen deep");
            Assert::AreEqual (std::string ("error"), ReplyCheck::GetStatusName (seventeen.Type ("RUN n1.txt").status), L"seventeen deep");
        }


        TEST_METHOD (NoFileName_IsAnError_InEveryDialect)
        {
            const std::pair<CommandMode, const char *>  forms[] =
            {
                { CommandMode::AppleWin,  "RUN"    },
                { CommandMode::AppleWin,  "LOAD"   },
                { CommandMode::AppleWin,  "run   " },
                { CommandMode::Casso,     "RUN"    },
                { CommandMode::GSSquared, "run"    },
                { CommandMode::WinDbg,    "!run"   },
                { CommandMode::WinDbg,    "!load"  },
            };
            size_t  checked = 0;



            for (const auto & [mode, line] : forms)
            {
                ConsoleRig  rig;



                rig.SwitchTo (mode);
                ReplyCheck::IsError (rig.Type (line), CommandStatus::Error, "invalid arguments", "Give the script file to run.", line);
                ++checked;
            }

            Assert::AreEqual (std::size (forms), checked);

            {
                ConsoleRig  monitor;



                monitor.SwitchTo (CommandMode::Monitor);
                ReplyCheck::IsMonitorError (monitor.Type ("/RUN"),  CommandStatus::Error, "invalid arguments", "Give the script file to run.", "/RUN");
                ReplyCheck::IsMonitorError (monitor.Type ("/LOAD"), CommandStatus::Error, "invalid arguments", "Give the script file to run.", "/LOAD");
            }
        }


        //  Every entry point attaches a file system, so this cannot happen in
        //  Casso; the defensive branch is reached by taking the file system
        //  away, which no production path does.
        TEST_METHOD (NoFileSystem_IsAnError)
        {
            ConsoleRig  rig;



            rig.Session().SetFileSystem (nullptr);

            for (const char * line : { "RUN x.txt", "LOAD x.txt", "STARTUP", "SAVE x.txt" })
            {
                ReplyCheck::IsError (rig.Type (line), CommandStatus::Error, "no file access", "This session cannot read or write host files.", line);
            }
        }


        //  The name as it was typed, quotes and all.
        TEST_METHOD (AMissingFile_IsAnError)
        {
            ConsoleRig  rig;



            ReplyCheck::IsError (rig.Type ("RUN missing.txt"),      CommandStatus::Error, "file not found", "missing.txt could not be read.",       "RUN");
            ReplyCheck::IsError (rig.Type ("LOAD \"missing.txt\""), CommandStatus::Error, "file not found", "\"missing.txt\" could not be read.",   "LOAD quoted");
            ReplyCheck::IsError (rig.Type ("RUN 'gone.txt'"),       CommandStatus::Error, "file not found", "'gone.txt' could not be read.",        "RUN single-quoted");
            ReplyCheck::IsError (rig.Type ("STARTUP"),              CommandStatus::Error, "file not found", "DebuggerAutoRun.txt could not be read.", "STARTUP");
        }


        TEST_METHOD (TheNameIsResolvedAgainstTheCurrentDirectory)
        {
            ConsoleRig  rig;



            rig.files.WriteAllText (L"C:\\s\\x.txt",       "MEB 300 01\n");
            rig.files.WriteAllText (L"C:\\s\\my file.txt", "MEB 301 02\n");
            rig.files.WriteAllText (L"C:\\s\\q.txt",       "MEB 302 03\n");
            rig.files.WriteAllText (L"D:\\r\\y.txt",       "MEB 303 04\n");
            rig.files.WriteAllText (L"C:\\s\\u.txt",       "MEB 304 05\n");

            (void) rig.Type ("CD C:\\s");

            (void) rig.Type ("RUN x.txt");
            (void) rig.Type ("LOAD \"my file.txt\"");
            (void) rig.Type ("LOAD 'q.txt'");
            (void) rig.Type ("RUN D:\\r\\y.txt");
            (void) rig.Type ("RUN  u.txt  ");

            Assert::AreEqual ((Byte) 0x01, rig.Peek (0x300), L"a relative name");
            Assert::AreEqual ((Byte) 0x02, rig.Peek (0x301), L"a double-quoted name with a space");
            Assert::AreEqual ((Byte) 0x03, rig.Peek (0x302), L"a single-quoted name");
            Assert::AreEqual ((Byte) 0x04, rig.Peek (0x303), L"a rooted name");
            Assert::AreEqual ((Byte) 0x05, rig.Peek (0x304), L"spaces around the name");

            rig.files.WriteAllText (L"C:\\s\\two words.txt", "MEB 305 06\n");
            (void) rig.Type ("RUN two words.txt");
            Assert::AreEqual ((Byte) 0x06, rig.Peek (0x305), L"RUN takes the rest of the line, so a name with a space needs no quotes");
        }


        //  A nested script's name goes under the current directory, not under
        //  the directory of the script that runs it; a CD in a script lasts.
        TEST_METHOD (ANestedName_GoesUnderTheCurrentDirectory_AndACdLasts)
        {
            ConsoleRig  rig;



            rig.files.WriteAllText (L"C:\\s\\a.txt", "RUN b.txt\n");
            rig.files.WriteAllText (L"C:\\s\\b.txt", "ECHO wrong\n");
            rig.files.WriteAllText (L"C:\\w\\b.txt", "ECHO from C:\\w\n");
            rig.files.WriteAllText (L"C:\\s\\cd.txt", "CD D:\\x\n");

            (void) rig.Type ("CD C:\\w");
            ReplyCheck::IsOk (rig.Type ("RUN C:\\s\\a.txt"), "from C:\\w", "RUN C:\\s\\a.txt");

            ReplyCheck::IsOk (rig.Type ("RUN C:\\s\\cd.txt"), "D:\\x", "a script's CD");
            ReplyCheck::IsOk (rig.Type ("PWD"), "D:\\x", "PWD after the script");
        }


        //  Each line's text is in the session's output format, and so is the
        //  RUN's own error.
        TEST_METHOD (TheTextIsInTheOutputFormat)
        {
            ConsoleRig  rig;



            (void) rig.Type ("MEB 300 A9 41");
            (void) rig.Type ("OUTPUT MONITOR");
            rig.files.WriteAllText (L"f.txt", "CALC 41\nD 300:301\nFROB\n");

            ReplyCheck::IsErrorAfter (rig.Type ("RUN f.txt"), "=41\n0300- A9 41\nERR\nError: unknown command\n       FROB is not a command.\nERR",
                                      CommandStatus::Error, "script failed", "Line 3 of f.txt failed.", "RUN f.txt");
        }


        TEST_METHOD (AByteOrderMark_IsNotPartOfTheFirstLine)
        {
            ConsoleRig  rig;



            rig.files.WriteAllText (L"bom.txt", "\xEF\xBB\xBFMEB 300 EA\nECHO done\n");

            ReplyCheck::IsOk (rig.Type ("RUN bom.txt"), ScriptText::OfTyped ({ "MEB 300 EA", "ECHO done" }), "RUN bom.txt");
            Assert::AreEqual ((Byte) 0xEA, rig.Peek (0x300));
        }


        //  Each dialect's RUN and LOAD. RUN's lines are read in the dialect in
        //  force; LOAD's are AppleWin's, the syntax SAVE writes.
        TEST_METHOD (DialectForms)
        {
            ConsoleRig  rig;



            rig.files.WriteAllText (L"mon.txt",   "300: 01\n");
            rig.files.WriteAllText (L"load.txt",  "MEB 301 02\n");
            rig.files.WriteAllText (L"gs.txt",    "302: 03\n");
            rig.files.WriteAllText (L"dbg.txt",   "eb 303 04\n");
            rig.files.WriteAllText (L"load2.txt", "MEB 304 05\n");

            rig.SwitchTo (CommandMode::Monitor);
            Assert::AreEqual (std::string ("ok"), ReplyCheck::GetStatusName (rig.Type ("/RUN mon.txt").status),  L"/RUN");
            Assert::AreEqual (std::string ("ok"), ReplyCheck::GetStatusName (rig.Type ("/LOAD load.txt").status), L"/LOAD");

            rig.SwitchTo (CommandMode::GSSquared);
            Assert::AreEqual (std::string ("ok"), ReplyCheck::GetStatusName (rig.Type ("run gs.txt").status), L"run");

            rig.SwitchTo (CommandMode::WinDbg);
            Assert::AreEqual (std::string ("ok"), ReplyCheck::GetStatusName (rig.Type ("!run dbg.txt").status),   L"!run");
            Assert::AreEqual (std::string ("ok"), ReplyCheck::GetStatusName (rig.Type ("!load load2.txt").status), L"!load");

            Assert::AreEqual ((Byte) 0x01, rig.Peek (0x300), L"/RUN read Monitor lines");
            Assert::AreEqual ((Byte) 0x02, rig.Peek (0x301), L"/LOAD read AppleWin lines");
            Assert::AreEqual ((Byte) 0x03, rig.Peek (0x302), L"run read GSSquared lines");
            Assert::AreEqual ((Byte) 0x04, rig.Peek (0x303), L"!run read WinDbg lines");
            Assert::AreEqual ((Byte) 0x05, rig.Peek (0x304), L"!load read AppleWin lines");
        }


        //  SAVE writes AppleWin lines and LOAD replays them as AppleWin lines,
        //  so a SAVE and LOAD pair round-trips in every dialect that has both.
        TEST_METHOD (LoadReplaysASaveFile_InEveryDialect)
        {
            const std::tuple<CommandMode, const char *, const char *, const char *>  dialects[] =
            {
                { CommandMode::AppleWin, "",  "BPL",  "WL"  },
                { CommandMode::Casso,    "",  "BPL",  "WL"  },
                { CommandMode::Monitor,  "/", "/BPL", "/WL" },
                { CommandMode::WinDbg,   "!", "bl",   "!WL" },
            };
            size_t  checked = 0;



            for (const auto & [mode, marker, listBreakpoints, listWatches] : dialects)
            {
                ConsoleRig   rig;
                std::string  m (marker);
                std::string  breakpoints;
                std::string  watches;



                rig.SwitchTo (mode);
                (void) rig.Type (m + "BP 300");
                (void) rig.Type (m + "BP 310");
                (void) rig.Type (m + "BPD 1");
                (void) rig.Type (m + "WA 36");

                breakpoints = ReplyCheck::Join (rig.Type (listBreakpoints).text);
                watches     = ReplyCheck::Join (rig.Type (listWatches).text);

                ReplyCheck::IsOk (rig.Type (m + "SAVE f.txt"), "Saved the breakpoints, watches, zero-page pointers and bookmarks to f.txt.", m + "SAVE");
                (void) rig.Type (m + "BPC *");
                (void) rig.Type (m + "WC *");

                Assert::AreEqual (std::string ("ok"), ReplyCheck::GetStatusName (rig.Type (m + "LOAD f.txt").status), ReplyCheck::Widen (m + "LOAD").c_str());
                Assert::AreEqual (breakpoints, ReplyCheck::Join (rig.Type (listBreakpoints).text), ReplyCheck::Widen (m + "LOAD restored the breakpoints").c_str());
                Assert::AreEqual (watches,     ReplyCheck::Join (rig.Type (listWatches).text),     ReplyCheck::Widen (m + "LOAD restored the watches").c_str());
                Assert::AreEqual ((size_t) 2, rig.Session().GetBreakpoints().GetAll().size(), ReplyCheck::Widen (m + "LOAD: two breakpoints").c_str());
                Assert::IsTrue   (rig.Session().GetBreakpoints().GetAll()[0].enabled, ReplyCheck::Widen (m + "LOAD: the first enabled").c_str());
                Assert::IsFalse  (rig.Session().GetBreakpoints().GetAll()[1].enabled, ReplyCheck::Widen (m + "LOAD: the second disabled again").c_str());
                Assert::AreEqual ((Word) 0x310, rig.Session().GetBreakpoints().GetAll()[1].first);
                ++checked;
            }

            Assert::AreEqual (std::size (dialects), checked);
        }


        //  GSSquared's `load` and `save` are its BLOAD and BSAVE, so it has no
        //  LOAD; its `run` of a SAVE file reads the lines as GSSquared lines,
        //  and a disabled breakpoint's BPD line is GSSquared's data breakpoint.
        TEST_METHOD (InGSSquared_RunOfASaveFile_ReadsGSSquaredLines)
        {
            ConsoleRig  rig;



            (void) rig.Type ("BP 300");
            (void) rig.Type ("BPD 0");
            (void) rig.Type ("SAVE f.txt");
            Assert::AreEqual (std::string ("BPC *\nBP 0300\nBPD 0\nWC *\nZPC *\nBMC *\n"), rig.files.PeekContent (L"f.txt"));

            rig.SwitchTo (CommandMode::GSSquared);
            Assert::AreEqual (std::string ("error"), ReplyCheck::GetStatusName (rig.Type ("run f.txt").status));
            Assert::AreEqual (std::string ("Line 3 of f.txt failed."), rig.Type ("run f.txt").error.detail);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ScriptRunPathTests
    //
    //  A run command in a script. Batch runs synchronously, so the line after
    //  a G runs after the stop; the window and the channel start the run and
    //  go on, so the lines after it run with the machine running, and the stop
    //  arrives later through the channel rather than in RUN's text.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ScriptRunPathTests)
    {
    public:

        TEST_METHOD (InBatch_TheNextLineRunsAfterTheStop)
        {
            BatchScriptRig  rig;
            size_t          at = 0;



            rig.files.WriteAllText (L"run.txt", "BP 302\nG\nPRINT PC\n");
            rig.options.commands = { "MEB 300 A9 41 8D 00 04 60", "= 300", "RUN run.txt", "ECHO next" };

            Assert::AreEqual (0, rig.Run (""));

            at = rig.result.output.find (">RUN run.txt\n");
            Assert::AreNotEqual (std::string::npos, at, ReplyCheck::Widen (rig.result.output).c_str());
            Assert::AreEqual (std::string (">RUN run.txt\nBreakpoint #0 set at $0302\n0302\nBreakpoint #0 at $0302\n>ECHO next\nnext\n"),
                              rig.result.output.substr (at));
            Assert::AreEqual ((Word) 0x0302, rig.GetRegisters().pc);
            Assert::AreEqual ((Byte) 0x41,   rig.GetRegisters().a);
        }


        TEST_METHOD (InTheWindow_TheLinesAfterAGoRunWhileItRuns)
        {
            ConsoleRig           rig;
            Byte                 before     = rig.Peek (0x310);
            ChannelConnectionId  connection = rig.transport.Connect();
            std::string          stopped;



            rig.controller.Pump();
            rig.files.WriteAllText (L"run.txt", "BP 302\nG\nPRINT PC\n= 400\nMEB 310 EA\nT\nG\nECHO end\n");

            ReplyCheck::IsErrorAfter (rig.Type ("RUN run.txt"),
                                      "Breakpoint #0 set at $0302\n"
                                      "0300\n"
                                      "Error: machine running\n"
                                      "       = changes registers or memory. Pause the machine first.\n"
                                      "Error: machine running\n"
                                      "       MEB changes registers or memory. Pause the machine first.\n"
                                      "Error: already running\n"
                                      "       A run is in progress. Use PAUSE to stop it.\n"
                                      "Error: already running\n"
                                      "       A run is in progress. Use PAUSE to stop it.\n"
                                      "end",
                                      CommandStatus::Error, "script failed", "4 lines of run.txt failed, the first at line 4.", "RUN run.txt");

            Assert::IsTrue   (rig.Session().GetRunState() == RunState::DebugRun, L"the script's G is still going");
            rig.FinishRun();

            Assert::AreEqual ((Word) 0x0302, rig.GetPc(), L"and stops at the script's breakpoint");
            Assert::AreEqual (before, rig.Peek (0x310), L"the MEB during the run changed nothing");

            rig.controller.Pump();
            for (const std::string & record : rig.transport.Written (connection))
            {
                stopped = record.find ("\"stopped\"") != std::string::npos ? record : stopped;
            }

            Assert::IsTrue (stopped.find (R"("reason":"breakpoint","pc":770)") != std::string::npos, ReplyCheck::Widen (stopped).c_str());
        }


        TEST_METHOD (OnTheChannel_TheSame)
        {
            ConsoleRig  rig;



            rig.files.WriteAllText (L"run.txt", "BP 302\nG\n= 400\n");

            ReplyCheck::IsErrorAfter (rig.Request ("RUN run.txt"),
                                      "Breakpoint #0 set at $0302\n"
                                      "Error: machine running\n"
                                      "       = changes registers or memory. Pause the machine first.",
                                      CommandStatus::Error, "script failed", "Line 3 of run.txt failed.", "RUN run.txt");
            rig.FinishRun();
            Assert::AreEqual ((Word) 0x0302, rig.GetPc());
        }


        //  None of these writes registers or memory, so each runs while the
        //  machine does; a script's lines are checked one at a time.
        TEST_METHOD (TheseCommandsRunWhileTheMachineRuns)
        {
            ConsoleRig  rig;
            size_t      checked = 0;



            rig.files.WriteAllText (L"C:\\w\\ok.txt",    "ECHO ok\n");
            rig.files.WriteAllText (L"C:\\w\\write.txt", "MEB 300 EA\n");

            (void) rig.Type ("G");
            Assert::IsTrue (rig.Session().GetRunState() == RunState::DebugRun);

            for (const char * line : { "CD C:\\w", "PWD", "ECHO x", "HELP", "HELP BP", "?", "VERSION", "MOTD", "OUTPUT", "OUTPUT APPLEWIN",
                                       "LOG", "LOG INFO", "DISASM", "DISASM BRANCH 1", "PRINT 1", "PRINTF \"%d\",1", "CALC 1", "DISK",
                                       "DISK SLOT", "SAVE s.txt", "RUN ok.txt", "LOAD ok.txt" })
            {
                Assert::AreEqual (std::string ("ok"), ReplyCheck::GetStatusName (rig.Type (line).status), ReplyCheck::Widen (line).c_str());
                ++checked;
            }

            Assert::AreEqual ((size_t) 22, checked);

            ReplyCheck::IsErrorAfter (rig.Type ("RUN write.txt"),
                                      "Error: machine running\n       MEB changes registers or memory. Pause the machine first.",
                                      CommandStatus::Error, "script failed", "Line 1 of write.txt failed.", "RUN write.txt");

            (void) rig.Type ("PAUSE");
            rig.FinishRun();
            Assert::IsTrue (rig.Session().GetRunState() == RunState::Paused);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RunStartupTests
    //
    //  STARTUP runs DebuggerAutoRun.txt from the current directory as RUN runs
    //  a script. Nothing runs it on its own.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (RunStartupTests)
    {
    public:

        TEST_METHOD (EveryFormRunsDebuggerAutoRun)
        {
            //  The script's lines are read in the dialect in force, as RUN's are.
            const std::tuple<CommandMode, const char *, const char *>  forms[] =
            {
                { CommandMode::AppleWin,  "STARTUP",          "MEB 300 5A\n" },
                { CommandMode::AppleWin,  "startup",          "MEB 300 5A\n" },
                { CommandMode::AppleWin,  "STARTUP x y",      "MEB 300 5A\n" },
                { CommandMode::Casso,     "STARTUP",          "MEB 300 5A\n" },
                { CommandMode::Monitor,   "/STARTUP",         "300: 5A\n"    },
                { CommandMode::GSSquared, "startup",          "300: 5A\n"    },
                { CommandMode::WinDbg,    "!startup",         "eb 300 5a\n"  },
                { CommandMode::WinDbg,    "!STARTUP ignored", "eb 300 5a\n"  },
            };
            size_t  checked = 0;



            for (const auto & [mode, line, script] : forms)
            {
                ConsoleRig  rig;



                rig.files.WriteAllText (L"DebuggerAutoRun.txt", script);
                rig.SwitchTo (mode);

                Assert::AreEqual (std::string ("ok"), ReplyCheck::GetStatusName (rig.Type (line).status), ReplyCheck::Widen (line).c_str());
                Assert::AreEqual ((Byte) 0x5A, rig.Peek (0x300), ReplyCheck::Widen (line).c_str());
                ++checked;
            }

            Assert::AreEqual (std::size (forms), checked);
        }


        TEST_METHOD (ItsTextIsTheScripts)
        {
            ConsoleRig  rig;



            rig.files.WriteAllText (L"DebuggerAutoRun.txt", "MEB 300 5A\nECHO started\n");
            ReplyCheck::IsOk (rig.Type ("STARTUP"), ScriptText::OfTyped ({ "MEB 300 5A", "ECHO started" }), "STARTUP");
        }


        TEST_METHOD (NoStartupScript_IsAnError)
        {
            ConsoleRig  window;
            ConsoleRig  monitor;



            ReplyCheck::IsError (window.Type ("STARTUP"), CommandStatus::Error, "file not found", "DebuggerAutoRun.txt could not be read.", "STARTUP");

            monitor.SwitchTo (CommandMode::Monitor);
            ReplyCheck::IsMonitorError (monitor.Type ("/STARTUP"), CommandStatus::Error, "file not found", "DebuggerAutoRun.txt could not be read.", "/STARTUP");
        }


        //  With no CD the name is left relative, for the host to find where
        //  the process runs; after a CD it is found there.
        TEST_METHOD (ItIsFoundInTheCurrentDirectory)
        {
            ConsoleRig      window;
            BatchScriptRig  batch;



            window.files.WriteAllText (L"DebuggerAutoRun.txt",        "MEB 300 11\n");
            window.files.WriteAllText (L"C:\\s\\DebuggerAutoRun.txt", "MEB 300 22\n");

            (void) window.Type ("STARTUP");
            Assert::AreEqual ((Byte) 0x11, window.Peek (0x300), L"no CD: the relative name");

            (void) window.Type ("CD C:\\s");
            (void) window.Type ("STARTUP");
            Assert::AreEqual ((Byte) 0x22, window.Peek (0x300), L"after CD: that directory's");

            batch.files.WriteAllText (L"DebuggerAutoRun.txt", "MEB 300 33\n");
            batch.options.commands = { "STARTUP" };
            Assert::AreEqual (0, batch.Run (""));
            Assert::AreEqual ((Byte) 0x33, batch.Peek (0x300), L"batch: the relative name");
        }


        //  Opening the debugger, reopening it, or starting a batch run does not
        //  run the startup script; only STARTUP does.
        TEST_METHOD (NothingRunsItOnItsOwn)
        {
            ConsoleRig      window;
            BatchScriptRig  batch;
            Byte            before = window.Peek (0x300);
            HRESULT         hr     = S_OK;



            window.files.WriteAllText (L"DebuggerAutoRun.txt", "MEB 300 5A\n");
            window.controller.Close();
            hr = window.controller.Open();

            Assert::AreEqual (S_OK, hr);
            Assert::AreEqual (before, window.Peek (0x300), L"reopening the debugger");

            batch.files.WriteAllText (L"DebuggerAutoRun.txt", "MEB 300 5A\n");
            batch.options.commands = { "MEB 301 00" };
            Assert::AreEqual (0, batch.Run (""));
            Assert::AreNotEqual ((Byte) 0x5A, batch.Peek (0x300), L"a batch run");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AttachedScriptTests
    //
    //  `CassoCli debug --attach` runs its lines through the channel, over the
    //  emulator's session; its exit status goes by each reply's status.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AttachedScriptTests)
    {
    public:

        TEST_METHOD (ARunWhoseLinesFailed_ExitsOne)
        {
            ConsoleRig                        rig;
            LoopbackChannelClient             client (rig);
            CommandLineOptions::DebugOptions  options;
            DebugAttachResult                 result;



            rig.files.WriteAllText (L"bad.txt", "FROB\n");
            options.isAttach       = true;
            options.timeoutSeconds = 1;
            options.commands       = { "RUN bad.txt" };

            DebugAttachRunner::Run (client, options, "", result);

            Assert::AreEqual (DebugAttachRunner::kCommandFailed, result.exitStatus);
            Assert::AreEqual (std::string (">RUN bad.txt\n"
                                           "Error: unknown command\n"
                                           "       FROB is not a command.\n"
                                           "Error: script failed\n"
                                           "       Line 1 of bad.txt failed.\n"), result.output);
        }
    };
}
