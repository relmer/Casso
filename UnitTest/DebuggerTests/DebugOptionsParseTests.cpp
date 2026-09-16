#include "Pch.h"

#include "Cli/CliMain.h"
#include "CommandLineParser.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebugOptionsParseTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ArgVector
    //
    //  Owns the storage behind a synthetic argv, which the parser takes the
    //  way main does.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ArgVector
    {
    public:
        ArgVector (std::initializer_list<const char *> args)
        {
            for (const char * arg : args)
            {
                m_storage.push_back (std::string (arg));
            }

            for (std::string & arg : m_storage)
            {
                m_pointers.push_back (arg.data());
            }
        }

        int      Count() const { return (int) m_pointers.size(); }
        char * * Data()        { return m_pointers.data(); }

    private:
        std::vector<std::string>  m_storage;
        std::vector<char *>       m_pointers;
    };





    static CommandLineOptions Parse (std::initializer_list<const char *> argv)
    {
        ArgVector  args (argv);

        return CommandLineParser::Parse (args.Count(), args.Data(), [] (const std::string &) { return false; });
    }





    static bool IsRefused (const CommandLineOptions & options)
    {
        return options.parseVerdict == CommandLineOptions::ParseVerdict::Refused;
    }





    static std::wstring Widen (const std::string & text)
    {
        return std::wstring (text.begin(), text.end());
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebugOptionsParseTests
    //
    //  The `debug` grammar: every option lands on DebugOptions, every mistake
    //  is a refusal, and help wins wherever it appears.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebugOptionsParseTests)
    {
    public:
        TEST_METHOD (MachineAndScript_AreTaken_WithTheDefaultsIntact)
        {
            CommandLineOptions  options = Parse ({ "CassoCli", "debug", "--machine", "Apple2e", "--script", "stop.txt" });



            Assert::IsTrue  (options.subcommand == CommandLineOptions::Subcommand::Debug);
            Assert::IsFalse (IsRefused (options), Widen (options.refusalMessage).c_str());
            Assert::IsFalse (options.showHelp);

            Assert::AreEqual (std::string ("Apple2e"),  options.debug.machine);
            Assert::AreEqual (std::string ("stop.txt"), options.debug.scriptPath);
            Assert::AreEqual (std::string ("applewin"), options.debug.mode);
            Assert::IsTrue   (options.debug.disk1.empty());
            Assert::IsTrue   (options.debug.disk2.empty());
            Assert::IsTrue   (options.debug.commands.empty());
            Assert::IsFalse  (options.debug.json);
            Assert::IsFalse  (options.debug.writeDisks);
            Assert::AreEqual ((uint64_t) 100000000, options.debug.maxCycles);
            Assert::AreEqual ((uint64_t) 0xCA550001, options.debug.seed);
        }

        //  --list describes running instances and runs nothing, so it needs no
        //  machine, and anything that would run beside it is refused rather
        //  than silently ignored.
        TEST_METHOD (List_NeedsNoMachine_AndTakesNothingElse)
        {
            CommandLineOptions  alone = Parse ({ "CassoCli", "debug", "--list" });
            CommandLineOptions  mixed = Parse ({ "CassoCli", "debug", "--list", "--command", "r" });



            Assert::IsFalse (IsRefused (alone), Widen (alone.refusalMessage).c_str());
            Assert::IsTrue  (alone.debug.list);

            Assert::IsTrue  (IsRefused (mixed));
        }



        //  --attach runs against a running Casso's machine, so it needs no
        //  --machine and refuses one; it still needs something to run.
        TEST_METHOD (Attach_TakesAPid_AndRefusesAMachine)
        {
            CommandLineOptions  attach    = Parse ({ "CassoCli", "debug", "--attach", "1234", "--command", "r", "--timeout", "30" });
            CommandLineOptions  both      = Parse ({ "CassoCli", "debug", "--attach", "1234", "--machine", "Apple2e", "--command", "r" });
            CommandLineOptions  nothing   = Parse ({ "CassoCli", "debug", "--attach", "1234" });
            CommandLineOptions  notANum   = Parse ({ "CassoCli", "debug", "--attach", "abc", "--command", "r" });



            Assert::IsFalse  (IsRefused (attach), Widen (attach.refusalMessage).c_str());
            Assert::IsTrue   (attach.debug.isAttach);
            Assert::AreEqual ((uint32_t) 1234, attach.debug.attachPid);
            Assert::AreEqual ((uint32_t) 30,   attach.debug.timeoutSeconds);

            Assert::IsTrue   (IsRefused (both),     L"a machine to build is meaningless when attaching");
            Assert::IsTrue   (IsRefused (nothing),  L"still needs a script or a command");
            Assert::IsTrue   (IsRefused (notANum),  L"a process id is a number");
        }



        TEST_METHOD (Timeout_DefaultsToTwoMinutes)
        {
            CommandLineOptions  options = Parse ({ "CassoCli", "debug", "--attach", "5", "--command", "r" });



            Assert::AreEqual ((uint32_t) 120, options.debug.timeoutSeconds);
        }



        TEST_METHOD (ScriptDash_IsStandardInput)
        {
            CommandLineOptions  options = Parse ({ "CassoCli", "debug", "--machine", "Apple2e", "--script", "-" });



            Assert::IsFalse  (IsRefused (options), Widen (options.refusalMessage).c_str());
            Assert::AreEqual (std::string ("-"), options.debug.scriptPath);
        }

        TEST_METHOD (Commands_RepeatInOrder)
        {
            CommandLineOptions  options = Parse ({ "CassoCli", "debug", "--machine", "Apple2e",
                                                   "--command", "bpmr C000", "--command", "g", "--command", "r" });



            Assert::IsFalse  (IsRefused (options), Widen (options.refusalMessage).c_str());
            Assert::AreEqual (size_t (3), options.debug.commands.size());
            Assert::AreEqual (std::string ("bpmr C000"), options.debug.commands[0]);
            Assert::AreEqual (std::string ("g"),         options.debug.commands[1]);
            Assert::AreEqual (std::string ("r"),         options.debug.commands[2]);
        }

        TEST_METHOD (Disks_TakeEachDrive)
        {
            CommandLineOptions  options = Parse ({ "CassoCli", "debug", "--machine", "Apple2e", "--command", "r",
                                                   "--disk1", "a.dsk", "--disk2", "b.woz" });



            Assert::AreEqual (std::string ("a.dsk"), options.debug.disk1);
            Assert::AreEqual (std::string ("b.woz"), options.debug.disk2);
        }

        TEST_METHOD (Mode_IsFoldedToLowerCase_AndAnotherWordIsRefused)
        {
            CommandLineOptions  monitor = Parse ({ "CassoCli", "debug", "--machine", "Apple2e", "--command", "r", "--mode", "Monitor" });
            CommandLineOptions  other   = Parse ({ "CassoCli", "debug", "--machine", "Apple2e", "--command", "r", "--mode", "basic" });



            Assert::AreEqual (std::string ("monitor"), monitor.debug.mode);
            Assert::IsFalse  (IsRefused (monitor));

            Assert::IsTrue (IsRefused (other));
            Assert::IsTrue (other.refusalMessage.find ("--mode") != std::string::npos, Widen (other.refusalMessage).c_str());
        }

        TEST_METHOD (Json_AndWriteDisks_AreFlags)
        {
            CommandLineOptions  options = Parse ({ "CassoCli", "debug", "--machine", "Apple2e", "--command", "r", "--json", "--write-disks" });



            Assert::IsTrue (options.debug.json);
            Assert::IsTrue (options.debug.writeDisks);
        }

        TEST_METHOD (MaxCycles_IsDecimal_AndAnythingElseIsRefused)
        {
            CommandLineOptions  taken = Parse ({ "CassoCli", "debug", "--machine", "Apple2e", "--command", "r", "--max-cycles", "1000000" });
            CommandLineOptions  bad   = Parse ({ "CassoCli", "debug", "--machine", "Apple2e", "--command", "r", "--max-cycles", "lots" });



            Assert::AreEqual ((uint64_t) 1000000, taken.debug.maxCycles);
            Assert::IsFalse  (IsRefused (taken));

            Assert::IsTrue (IsRefused (bad));
            Assert::IsTrue (bad.refusalMessage.find ("max-cycles") != std::string::npos, Widen (bad.refusalMessage).c_str());
        }

        TEST_METHOD (Seed_TakesDecimal_0xHex_AndDollarHex)
        {
            CommandLineOptions  decimal = Parse ({ "CassoCli", "debug", "--machine", "Apple2e", "--command", "r", "--seed", "12345" });
            CommandLineOptions  hex     = Parse ({ "CassoCli", "debug", "--machine", "Apple2e", "--command", "r", "--seed", "0xCAFE" });
            CommandLineOptions  dollar  = Parse ({ "CassoCli", "debug", "--machine", "Apple2e", "--command", "r", "--seed", "$BEEF" });



            Assert::AreEqual ((uint64_t) 12345,  decimal.debug.seed);
            Assert::AreEqual ((uint64_t) 0xCAFE, hex.debug.seed);
            Assert::AreEqual ((uint64_t) 0xBEEF, dollar.debug.seed);
        }

        TEST_METHOD (Seed_ThatIsNotANumber_IsRefused)
        {
            CommandLineOptions  trailing = Parse ({ "CassoCli", "debug", "--machine", "Apple2e", "--command", "r", "--seed", "12zz" });
            CommandLineOptions  bare     = Parse ({ "CassoCli", "debug", "--machine", "Apple2e", "--command", "r", "--seed", "0x" });



            Assert::IsTrue (IsRefused (trailing));
            Assert::IsTrue (IsRefused (bare));
            Assert::IsTrue (trailing.refusalMessage.find ("seed") != std::string::npos, Widen (trailing.refusalMessage).c_str());
        }

        TEST_METHOD (Help_InEveryForm_ShowsTheDebugPage)
        {
            for (const char * form : { "--help", "-h", "/?", "?" })
            {
                CommandLineOptions  options = Parse ({ "CassoCli", "debug", form });



                Assert::IsTrue (options.showHelp, Widen (form).c_str());
                Assert::IsTrue (options.helpPage == CommandLineOptions::HelpPage::Debug, Widen (form).c_str());
                Assert::IsFalse (IsRefused (options), Widen (form).c_str());
            }
        }

        TEST_METHOD (Help_WinsAfterOtherOptions_AndKeepsTheSlashPrefix)
        {
            CommandLineOptions  late  = Parse ({ "CassoCli", "debug", "--machine", "Apple2e", "--help" });
            CommandLineOptions  slash = Parse ({ "CassoCli", "debug", "/?" });



            Assert::IsTrue   (late.showHelp);
            Assert::IsTrue   (late.helpPage == CommandLineOptions::HelpPage::Debug);
            Assert::AreEqual ('/', slash.flagPrefix);
        }

        TEST_METHOD (SlashOptions_AreTheSameGrammar)
        {
            CommandLineOptions  options = Parse ({ "CassoCli", "debug", "/machine", "Apple2c", "/script", "s.txt", "/json" });



            Assert::IsFalse  (IsRefused (options), Widen (options.refusalMessage).c_str());
            Assert::AreEqual (std::string ("Apple2c"), options.debug.machine);
            Assert::AreEqual (std::string ("s.txt"),   options.debug.scriptPath);
            Assert::IsTrue   (options.debug.json);
            Assert::AreEqual ('/', options.flagPrefix);
        }

        TEST_METHOD (MissingMachine_IsRefused)
        {
            CommandLineOptions  options = Parse ({ "CassoCli", "debug", "--script", "s.txt" });



            Assert::IsTrue (IsRefused (options));
            Assert::IsTrue (options.refusalMessage.find ("--machine") != std::string::npos, Widen (options.refusalMessage).c_str());
        }

        TEST_METHOD (NothingToRun_IsRefused)
        {
            CommandLineOptions  options = Parse ({ "CassoCli", "debug", "--machine", "Apple2e" });



            Assert::IsTrue (IsRefused (options));
            Assert::IsTrue (options.refusalMessage.find ("--script") != std::string::npos, Widen (options.refusalMessage).c_str());
            Assert::IsTrue (options.refusalMessage.find ("--command") != std::string::npos, Widen (options.refusalMessage).c_str());
        }

        TEST_METHOD (UnknownOption_IsRefused_AndRecorded)
        {
            CommandLineOptions  options = Parse ({ "CassoCli", "debug", "--machine", "Apple2e", "--command", "r", "--nonsense" });



            Assert::IsTrue  (IsRefused (options));
            Assert::IsFalse (options.unrecognizedFlag.empty());
        }

        TEST_METHOD (SurplusArgument_IsRefused)
        {
            CommandLineOptions  options = Parse ({ "CassoCli", "debug", "stop.txt", "--machine", "Apple2e" });



            Assert::IsTrue (IsRefused (options));
            Assert::IsTrue (options.refusalMessage.find ("surplus") != std::string::npos, Widen (options.refusalMessage).c_str());
        }

        TEST_METHOD (OptionWithoutItsValue_IsRefused)
        {
            CommandLineOptions  options = Parse ({ "CassoCli", "debug", "--command", "r", "--machine" });



            Assert::IsTrue (IsRefused (options));
            Assert::IsTrue (options.refusalMessage.find ("needs a value") != std::string::npos, Widen (options.refusalMessage).c_str());
        }

        //  The status a refused `debug` line earns, and what the two arms of
        //  CliMain that print the page return.
        TEST_METHOD (Refusal_ExitsTwo_AndHelpExitsZero)
        {
            ArgVector  refused ({ "CassoCli", "debug", "--machine", "Apple2e" });
            ArgVector  help    ({ "CassoCli", "debug", "--help" });



            Assert::AreEqual (2, CommandLineParser::GetExitCodeForRefusal (CommandLineOptions::Subcommand::Debug));
            Assert::AreEqual (2, CliMain (refused.Count(), refused.Data()));
            Assert::AreEqual (0, CliMain (help.Count(),    help.Data()));
        }

        TEST_METHOD (TheExitStatusText_NamesAllFour)
        {
            std::string  text = CommandLineParser::kDebugExitStatusHelpText;



            for (const char * status : { "0  ", "1  ", "2  ", "3  " })
            {
                Assert::IsTrue (text.find (status) != std::string::npos, Widen (status).c_str());
            }
        }
    };
}
