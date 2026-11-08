#include "Pch.h"

#include "EntryPointRig.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DirectoryCommandTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PrintDirectoryTests
    //
    //  PWD in the window, on the channel and in batch, in every dialect's
    //  spelling of it. The directory it prints is one CD set, as a user sets
    //  it: no test here gives the session a directory by hand.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (PrintDirectoryTests)
    {
    public:

        TEST_METHOD (PrintsTheDirectoryCdSet_InEveryDialectsSpelling)
        {
            const std::pair<CommandMode, const char *>  forms[] =
            {
                { CommandMode::AppleWin,  "PWD"              },
                { CommandMode::AppleWin,  "pwd"              },
                { CommandMode::AppleWin,  "PWD and more"     },
                { CommandMode::Casso,     "PWD"              },
                { CommandMode::Monitor,   "/PWD"             },
                { CommandMode::Monitor,   "/pwd"             },
                { CommandMode::GSSquared, "pwd"              },
                { CommandMode::GSSquared, "PWD"              },
                { CommandMode::WinDbg,    "!pwd"             },
                { CommandMode::WinDbg,    "!PWD ignored"     },
            };
            size_t  checked = 0;



            for (const auto & [mode, line] : forms)
            {
                ConsoleRig  rig;
                Reply       reply;



                ReplyCheck::IsOk (rig.Type ("CD C:\\Work"), "C:\\Work", "CD");
                rig.SwitchTo (mode);

                reply = rig.Type (line);
                ReplyCheck::IsOk (reply, "C:\\Work", line);
                Assert::IsTrue (rig.Session().GetCurrentDirectory() == L"C:\\Work", ReplyCheck::Widen (line).c_str());
                ++checked;
            }

            Assert::AreEqual (std::size (forms), checked);
        }


        //  The channel runs a request's PWD in the mode it carried, against the
        //  directory the window's CD set: the session is one.
        TEST_METHOD (OnTheChannel_InTheRequestsDialect)
        {
            ConsoleRig  rig;



            ReplyCheck::IsOk (rig.Type ("CD D:\\disks"), "D:\\disks", "CD in the window");

            ReplyCheck::IsOk (rig.Request ("PWD"),                         "D:\\disks", "PWD");
            ReplyCheck::IsOk (rig.Request ("/PWD", CommandMode::Monitor),   "D:\\disks", "/PWD");
            ReplyCheck::IsOk (rig.Request ("pwd",  CommandMode::GSSquared), "D:\\disks", "pwd");
            ReplyCheck::IsOk (rig.Request ("!pwd", CommandMode::WinDbg),    "D:\\disks", "!pwd");
            Assert::IsTrue   (rig.Session().GetMode() == CommandMode::AppleWin, L"a request's mode is for its line only");
        }


        TEST_METHOD (InBatch_AfterCd)
        {
            BatchScriptRig  rig;



            rig.options.commands = { "CD C:\\Work", "PWD" };

            Assert::AreEqual (0, rig.Run (""));
            Assert::AreEqual (std::string (">CD C:\\Work\nC:\\Work\n>PWD\nC:\\Work\n"), rig.result.output);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ChangeDirectoryTests
    //
    //  CD's argument forms, its errors, and what it changes: the directory every
    //  later file command resolves a relative name against.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ChangeDirectoryTests)
    {
    public:

        //  A drive, a backslash, a slash, or a drive and a colon: each replaces
        //  the directory outright, whatever it was.
        TEST_METHOD (ARootedPathReplacesTheDirectory)
        {
            const char *  rooted[] = { "D:\\disks", "\\dir", "/dir", "C:dir", "e:\\Mixed\\Case" };
            size_t        checked  = 0;



            for (const char * path : rooted)
            {
                ConsoleRig    rig;
                std::string   line = std::string ("CD ") + path;
                std::wstring  wide (path, path + strlen (path));



                ReplyCheck::IsOk (rig.Type ("CD C:\\Work"), "C:\\Work", "CD C:\\Work");
                ReplyCheck::IsOk (rig.Type (line), path, line);
                Assert::IsTrue   (rig.Session().GetCurrentDirectory() == wide, ReplyCheck::Widen (line).c_str());
                ReplyCheck::IsOk (rig.Type ("PWD"), path, "PWD after " + line);
                ++checked;
            }

            Assert::AreEqual (std::size (rooted), checked);
        }


        TEST_METHOD (ARelativePathIsJoinedWithOneBackslash)
        {
            ConsoleRig  rig;



            ReplyCheck::IsOk (rig.Type ("CD C:\\Work"), "C:\\Work",           "CD C:\\Work");
            ReplyCheck::IsOk (rig.Type ("cd sub"),      "C:\\Work\\sub",      "cd sub");
            ReplyCheck::IsOk (rig.Type ("CD deeper"),   "C:\\Work\\sub\\deeper", "CD deeper");

            //  A directory that already ends in a backslash gets no second one.
            ReplyCheck::IsOk (rig.Type ("CD C:\\Work\\"), "C:\\Work\\",       "CD C:\\Work\\");
            ReplyCheck::IsOk (rig.Type ("CD sub"),        "C:\\Work\\sub",    "CD sub after a trailing backslash");
            ReplyCheck::IsOk (rig.Type ("PWD"),           "C:\\Work\\sub",    "PWD");
        }


        //  One pair of matching quotes is dropped; CD takes the rest of the
        //  line, so an unquoted path with spaces is kept whole.
        TEST_METHOD (QuotesAreDropped_AndSpacesKept)
        {
            const std::pair<const char *, const char *>  forms[] =
            {
                { "CD \"C:\\My Dir\"",   "C:\\My Dir"    },
                { "CD 'C:\\My Dir'",     "C:\\My Dir"    },
                { "CD C:\\My Dir",       "C:\\My Dir"    },
                { "CD   C:\\Spaced  ",   "C:\\Spaced"    },
                { "CD \"C:\\Odd'",       "C:\\Work\\\"C:\\Odd'" },
                { "CD \"sub dir\"",      "C:\\Work\\sub dir" },
            };
            size_t  checked = 0;



            for (const auto & [line, expected] : forms)
            {
                ConsoleRig  rig;



                ReplyCheck::IsOk (rig.Type ("CD C:\\Work"), "C:\\Work", "CD C:\\Work");
                ReplyCheck::IsOk (rig.Type (line), expected, line);
                ReplyCheck::IsOk (rig.Type ("PWD"), expected, std::string ("PWD after ") + line);
                ++checked;
            }

            Assert::AreEqual (std::size (forms), checked);
        }


        //  No argument, in each dialect's spelling, and a quoted empty name: an
        //  error that leaves the directory alone.
        TEST_METHOD (NoDirectory_IsAnError_AndChangesNothing)
        {
            const std::pair<CommandMode, const char *>  forms[] =
            {
                { CommandMode::AppleWin,  "CD"      },
                { CommandMode::AppleWin,  "cd   "   },
                { CommandMode::AppleWin,  "CD \"\"" },
                { CommandMode::AppleWin,  "CD ''"   },
                { CommandMode::Casso,     "CD"      },
                { CommandMode::WinDbg,    "!cd"     },
                { CommandMode::WinDbg,    "!CD \"\"" },
            };
            size_t  checked = 0;



            for (const auto & [mode, line] : forms)
            {
                ConsoleRig  rig;



                ReplyCheck::IsOk (rig.Type ("CD C:\\Work"), "C:\\Work", "CD C:\\Work");
                rig.SwitchTo (mode);

                ReplyCheck::IsError (rig.Type (line), CommandStatus::Error, "invalid arguments", "CD takes a directory.", line);
                Assert::IsTrue (rig.Session().GetCurrentDirectory() == L"C:\\Work", ReplyCheck::Widen (line).c_str());
                ++checked;
            }

            Assert::AreEqual (std::size (forms), checked);
        }


        TEST_METHOD (NoDirectory_InMonitorMode_CarriesErr)
        {
            ConsoleRig  rig;



            ReplyCheck::IsOk (rig.Type ("CD C:\\Work"), "C:\\Work", "CD C:\\Work");
            rig.SwitchTo (CommandMode::Monitor);

            ReplyCheck::IsMonitorError (rig.Type ("/CD"), CommandStatus::Error, "invalid arguments", "CD takes a directory.", "/CD");
            Assert::IsTrue (rig.Session().GetCurrentDirectory() == L"C:\\Work");
        }


        //  CD takes the text as it is: no check that the directory exists, and
        //  no folding of `.` or `..`, which the host resolves when a file is
        //  opened.
        TEST_METHOD (TheTextIsTakenAsItIs)
        {
            ConsoleRig  rig;



            ReplyCheck::IsOk (rig.Type ("CD C:\\Work"),        "C:\\Work",           "CD C:\\Work");
            ReplyCheck::IsOk (rig.Type ("CD .."),              "C:\\Work\\..",       "CD ..");
            ReplyCheck::IsOk (rig.Type ("CD ."),               "C:\\Work\\..\\.",    "CD .");
            ReplyCheck::IsOk (rig.Type ("CD C:\\nonexistent"), "C:\\nonexistent",    "CD C:\\nonexistent");
            Assert::AreEqual ((size_t) 0, rig.files.FileCount(), L"and nothing was read or written to decide it");
        }


        //  Every file command resolves a relative name against the directory
        //  CD set; a rooted name ignores it.
        TEST_METHOD (LaterFileCommandsResolveAgainstIt)
        {
            ConsoleRig   rig;
            std::string  saved;



            rig.files.WriteAllText (L"C:\\s\\x.txt",               "MEB 300 EA\n");
            rig.files.WriteAllText (L"C:\\s\\DebuggerAutoRun.txt", "MEB 301 EB\n");
            rig.files.WriteAllText (L"C:\\s\\in.bin",              std::string ("\x11\x22", 2));
            rig.files.WriteAllText (L"C:\\s\\syms.sym",            "WHERE=$0340\n");
            rig.files.WriteAllText (L"D:\\other\\y.txt",           "MEB 302 EC\n");

            ReplyCheck::IsOk (rig.Type ("CD C:\\s"), "C:\\s", "CD C:\\s");

            (void) rig.Type ("RUN x.txt");
            Assert::AreEqual ((Byte) 0xEA, rig.Peek (0x300), L"RUN read C:\\s\\x.txt");

            (void) rig.Type ("STARTUP");
            Assert::AreEqual ((Byte) 0xEB, rig.Peek (0x301), L"STARTUP read C:\\s\\DebuggerAutoRun.txt");

            (void) rig.Type ("RUN D:\\other\\y.txt");
            Assert::AreEqual ((Byte) 0xEC, rig.Peek (0x302), L"a rooted name is read where it says");

            ReplyCheck::IsOk (rig.Type ("SAVE all.txt"), "Saved the breakpoints, watches, zero-page pointers and bookmarks to all.txt.", "SAVE");
            Assert::IsTrue   (rig.files.Exists (L"C:\\s\\all.txt"), L"SAVE wrote into C:\\s");

            Assert::AreEqual (std::string ("ok"), ReplyCheck::GetStatusName (rig.Type ("BSAVE out.bin 300:301").status));
            Assert::AreEqual (S_OK, rig.files.ReadAllText (L"C:\\s\\out.bin", saved));
            Assert::AreEqual (std::string ("\xEA\xEB", 2), saved, L"BSAVE wrote into C:\\s");

            Assert::AreEqual (std::string ("ok"), ReplyCheck::GetStatusName (rig.Type ("BLOAD in.bin 310").status));
            Assert::AreEqual ((Byte) 0x11, rig.Peek (0x310), L"BLOAD read C:\\s\\in.bin");
            Assert::AreEqual ((Byte) 0x22, rig.Peek (0x311));

            Assert::AreEqual (std::string ("ok"), ReplyCheck::GetStatusName (rig.Type ("SYM LOAD syms.sym").status));
            ReplyCheck::IsOk (rig.Type ("PRINT WHERE"), "0340", "a symbol from C:\\s\\syms.sym");
        }


        //  A relative CD in a session no CD has touched leaves a relative
        //  directory, and names are then resolved under it, relative to where
        //  the host process runs.
        TEST_METHOD (ARelativeCdFromTheStartingState_StaysRelative)
        {
            ConsoleRig  rig;



            rig.files.WriteAllText (L"sub\\x.txt", "MEB 300 5A\n");

            ReplyCheck::IsOk (rig.Type ("CD sub"), "sub", "CD sub");
            (void) rig.Type ("RUN x.txt");
            Assert::AreEqual ((Byte) 0x5A, rig.Peek (0x300), L"RUN read sub\\x.txt");
        }


        TEST_METHOD (DialectForms_ReachCd)
        {
            ConsoleRig  rig;



            rig.SwitchTo (CommandMode::Monitor);
            ReplyCheck::IsOk (rig.Type ("/CD C:\\mon"), "C:\\mon", "/CD");
            ReplyCheck::IsOk (rig.Type ("/cd sub"),     "C:\\mon\\sub", "/cd");

            rig.SwitchTo (CommandMode::WinDbg);
            ReplyCheck::IsOk (rig.Type ("!cd C:\\dbg"), "C:\\dbg", "!cd");

            rig.SwitchTo (CommandMode::Casso);
            ReplyCheck::IsOk (rig.Type ("CD C:\\casso"), "C:\\casso", "CD");
            Assert::IsTrue   (rig.Session().GetCurrentDirectory() == L"C:\\casso");
        }


        //  `cd` is a hex word to GSSquared: an address to examine. With an
        //  argument it is an examine that takes none, and the directory stays;
        //  `? cd` gives the dialects that do have CD.
        TEST_METHOD (InGSSquared_CdIsAnAddress)
        {
            ConsoleRig  rig;



            ReplyCheck::IsOk (rig.Type ("CD C:\\Work"), "C:\\Work", "CD C:\\Work");
            (void) rig.Type ("MEB 00CD 5A");
            rig.SwitchTo (CommandMode::GSSquared);

            ReplyCheck::IsOk    (rig.Type ("cd"), "00/00CD: 5A", "cd examines $00CD");
            ReplyCheck::IsError (rig.Type ("cd C:\\dir"), CommandStatus::Error, "invalid arguments",
                                 "An address to examine takes nothing after it.", "cd C:\\dir");
            Assert::IsTrue      (rig.Session().GetCurrentDirectory() == L"C:\\Work");

            ReplyCheck::IsOk (rig.Type ("? cd"), "CD does not run in GSSquared mode. It runs in AppleWin, Monitor as /CD, WinDbg as !CD and Casso modes.", "? cd");
        }


        //  The directory is the session's, which the window and every channel
        //  client share: a CD from either moves where the other's names go.
        TEST_METHOD (TheDirectoryIsSharedByTheWindowAndTheChannel)
        {
            ConsoleRig  rig;



            rig.files.WriteAllText (L"C:\\client\\a.txt", "MEB 300 11\n");
            rig.files.WriteAllText (L"C:\\window\\b.txt", "MEB 301 22\n");

            ReplyCheck::IsOk (rig.Request ("CD C:\\client"), "C:\\client", "a client's CD");
            (void) rig.Type ("RUN a.txt");
            Assert::AreEqual ((Byte) 0x11, rig.Peek (0x300), L"the window's RUN read the client's directory");

            ReplyCheck::IsOk (rig.Type ("CD C:\\window"), "C:\\window", "the window's CD");
            (void) rig.Request ("RUN b.txt");
            Assert::AreEqual ((Byte) 0x22, rig.Peek (0x301), L"the client's RUN read the window's directory");
        }


        TEST_METHOD (InBatch_ARelativeNameGoesUnderIt)
        {
            BatchScriptRig  rig;



            rig.files.WriteAllText (L"C:\\scripts\\x.txt", "ECHO from C:\\scripts\n");
            rig.options.commands = { "CD C:\\scripts", "RUN x.txt" };

            Assert::AreEqual (0, rig.Run (""));
            Assert::AreEqual (std::string (">CD C:\\scripts\nC:\\scripts\n>RUN x.txt\nfrom C:\\scripts\n"), rig.result.output);
        }
    };
}
