#include "Pch.h"

#include "Cli/CliMain.h"
#include "As65ExitStatus.h"
#include "CommandLineParser.h"

#include "CppUnitTest.h"




using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace CliMainTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ExitCodeTests
    //
    //  What the tool returns to a shell, asserted against the tool rather than
    //  against the function it is supposed to ask.
    //
    //  NONE OF THIS COULD BE WRITTEN BEFORE. CliMain was `main`, inside an
    //  executable the test assembly does not link, so the only thing reachable
    //  from here was the mapper it was meant to call. Two mappers existed, each
    //  with its own green suite asserting a different number for the same event,
    //  and nothing could see which one the dispatch reached for. It reached for
    //  the wrong one, and every page of the help documented statuses the tool did
    //  not return: an assembly error exited 2 and a warning exited 1.
    //
    //  These go through the dispatch, so that cannot happen again quietly.
    //
    //  THEY NAME FILES THAT DO NOT EXIST, deliberately. An unreadable input is a
    //  documented status and needs no fixture, and a command line refused before
    //  anything is opened never reaches a disk at all.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ExitCodeTests)
    {
    public:
        //  argv as a process would hand it over: an array of writable pointers
        //  with the program name in slot zero.
        static int Run (std::initializer_list<const char *> typed)
        {
            std::vector<std::string>  storage;
            std::vector<char *>       pointers;

            for (const char * word : typed)
            {
                storage.push_back (std::string (word));
            }

            for (std::string & word : storage)
            {
                pointers.push_back (word.data());
            }

            return CliMain ((int) pointers.size(), pointers.data());
        }

        //  as65: "1 - Incorrect parameter specified on the commandline."
        TEST_METHOD (ABadCommandLine_IsOne)
        {
            Assert::AreEqual (As65ExitStatus::kBadCommandLine,
                              Run ({ "CassoCli", "as65", "one.a65", "two.a65" }),
                              L"a surplus argument");
            Assert::AreEqual (As65ExitStatus::kBadCommandLine,
                              Run ({ "CassoCli", "as65", "prog.a65", "-Z" }),
                              L"and an option this grammar does not have");
        }

        //  as65: "2 - Unable to open input or output file."
        TEST_METHOD (AnUnreadableSource_IsTwo)
        {
            Assert::AreEqual (As65ExitStatus::kNoOutput,
                              Run ({ "CassoCli", "as65", "no-such-file-here.a65" }));
        }

        //  A HELP REQUEST SUCCEEDS AND A USAGE ERROR DOES NOT, which is the whole
        //  reason both print the same page. A script that invokes the tool wrongly
        //  has to fail; one that asks how it works must not.
        TEST_METHOD (AskingForHelp_IsZero_AndNamingNothing_IsNot)
        {
            Assert::AreEqual (0, Run ({ "CassoCli", "--help" }),          L"the general page");
            Assert::AreEqual (0, Run ({ "CassoCli", "as65", "--help" }),  L"the assembler's");
            Assert::AreEqual (0, Run ({ "CassoCli", "?" }),               L"as65's own request");
            Assert::AreEqual (0, Run ({ "CassoCli", "--version" }));

            Assert::AreNotEqual (0, Run ({ "CassoCli" }),
                                 L"naming no mode at all is a usage error");
        }

        //  A MODE NAMED WITH NOTHING AFTER IT OPENS ITS PAGE AND STILL FAILS. It
        //  answered "No input file specified", which told a reader who had just
        //  found the mode the one thing they had already worked out.
        TEST_METHOD (AModeWithNoArguments_FailsWithItsOwnStatus)
        {
            Assert::AreEqual (As65ExitStatus::kBadCommandLine, Run ({ "CassoCli", "as65" }));
            Assert::AreEqual (As65ExitStatus::kBadCommandLine, Run ({ "CassoCli", "merlin" }));
            Assert::AreEqual (CommandLineParser::kNothingStarted, Run ({ "CassoCli", "run" }));
        }

        //  A first word that names no mode is a usage error rather than a source
        //  file, which is what it used to be taken for.
        TEST_METHOD (AWordThatNamesNoMode_IsOne)
        {
            Assert::AreEqual (1, Run ({ "CassoCli", "frobnicate" }));
        }

        //  `disk` folds a refused command line into the status it spends on
        //  having done nothing, which is not the assembler's 1.
        TEST_METHOD (ARefusedDiskCommandLine_IsNothingDone)
        {
            Assert::AreEqual (CommandLineParser::kNothingStarted,
                              Run ({ "CassoCli", "disk", "frobnicate", "img.dsk" }));
            Assert::AreEqual (CommandLineParser::kNothingStarted,
                              Run ({ "CassoCli", "disk", "list", "img.dsk", "--bogus" }));
        }
    };
}
