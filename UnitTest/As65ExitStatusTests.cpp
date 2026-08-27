#include "Pch.h"

#include "As65ExitStatus.h"
#include "CommandLineParser.h"

#include "CppUnitTest.h"




using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace As65ExitStatusTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ForAssemblyTests
    //
    //  The status one attempt to assemble one source file reports to the shell.
    //
    //  These live here rather than beside the console executable because the
    //  test assembly does not link it. That is not a technicality: an assembly
    //  error reported as65's "unable to open input or output file" for as long
    //  as the decision sat where nothing could reach it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ForAssemblyTests)
    {
    public:
        TEST_METHOD (SourceReadAndAssembled_IsClean)
        {
            Assert::AreEqual (0, As65ExitStatus::ForAssembly (true, true));
        }

        //  WARNINGS ARE 5 AND ARE THE CALLER'S TO DECLARE. A source that
        //  assembled and warned is a success with something to say, and this
        //  is where the number for it is decided.
        //
        //  It reached the shell as 1 for a long time -- as65's bad command
        //  line -- because the executable called a second mapper with its own
        //  numbering and its own green tests. That mapper is gone; this is the
        //  only one, which is what stops the two disagreeing again.
        TEST_METHOD (SourceAssembledWithWarnings_IsWarned)
        {
            Assert::AreEqual (5, As65ExitStatus::ForAssembly (true, true, true));
            Assert::AreEqual (0, As65ExitStatus::ForAssembly (true, true, false),
                              L"and a clean assembly is still clean");
        }

        //  A FAILED ASSEMBLY THAT ALSO WARNED IS A FAILED ASSEMBLY. Reporting 5
        //  would tell a script an output file exists when none does.
        TEST_METHOD (WarningsDoNotSoftenAFailure)
        {
            Assert::AreEqual (3, As65ExitStatus::ForAssembly (true, false, true));
            Assert::AreEqual (2, As65ExitStatus::ForAssembly (false, false, true));
        }

        //  as65: "3 - Assembly gave errors." This was 2 -- the code for a file
        //  that could not be opened -- so every build script that branched on
        //  the status sent a syntax error down the "your path is wrong" arm.
        TEST_METHOD (SourceReadButNotAssembled_IsAssemblyErrors)
        {
            Assert::AreEqual (3, As65ExitStatus::ForAssembly (true, false));
        }

        //  as65: "2 - Unable to open input or output file." The neighbor that
        //  had to keep its meaning while 3 was split out of it.
        TEST_METHOD (SourceNeverRead_IsNoOutput)
        {
            Assert::AreEqual (2, As65ExitStatus::ForAssembly (false, false));
        }

        //  A file that was never read cannot have assembled, so the unreadable
        //  case outranks the assembled flag whatever it says. Stated because
        //  the alternative -- testing "assembled" first -- would report 0 for a
        //  run that produced nothing, which is the failure mode this whole
        //  status table exists to prevent.
        TEST_METHOD (UnreadableInputOutranksTheAssembledFlag)
        {
            Assert::AreEqual (2, As65ExitStatus::ForAssembly (false, true));
        }

        //  The five statuses are distinct values, which is the only property a
        //  script branching on them actually needs.
        TEST_METHOD (TheFiveStatusesAreDistinct)
        {
            std::set<int>  statuses = { As65ExitStatus::kClean,
                                        As65ExitStatus::kBadCommandLine,
                                        As65ExitStatus::kNoOutput,
                                        As65ExitStatus::kAssemblyErrors,
                                        As65ExitStatus::kWarned };

            Assert::AreEqual (size_t (5), statuses.size());
        }

        //  THE NUMBERS THEMSELVES ARE THE CONTRACT, not merely their being
        //  different from one another. 0 through 3 are as65's, quoted from its
        //  manual, and a build script ported from as65 branches on them without
        //  being read again -- so a change to any of them has to fail here
        //  rather than in somebody's build.
        TEST_METHOD (ZeroThroughThreeAreAs65sOwnNumbers)
        {
            Assert::AreEqual (0, As65ExitStatus::kClean,          L"source file assembled without errors");
            Assert::AreEqual (1, As65ExitStatus::kBadCommandLine, L"incorrect parameter specified on the commandline");
            Assert::AreEqual (2, As65ExitStatus::kNoOutput,       L"unable to open input or output file");
            Assert::AreEqual (3, As65ExitStatus::kAssemblyErrors, L"assembly gave errors");
        }

        //  WARNINGS TAKE 5 AND NOT 4. as65 has no status for an assembly that
        //  warned, and 4 is not free: as65 spends it on a failed allocation this
        //  tool cannot reach, so taking it would make a script that still tests
        //  for it catch the wrong thing.
        TEST_METHOD (WarningsTakeTheFirstNumberAs65DoesNotDefine)
        {
            Assert::AreEqual (5, As65ExitStatus::kWarned);
            Assert::AreNotEqual (4, As65ExitStatus::kWarned, L"4 is as65's out-of-memory and stays unused");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  HelpTextTests
    //
    //  What the assemble mode's help SAYS its statuses are. A status the code
    //  produces and the help does not name is one a caller can only find by
    //  experiment, and this is the pairing that catches it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (HelpTextTests)
    {
    public:
        //  THE LIST IS as65'S LIST, and the help has to print the same numbers
        //  the header assigns. It is the pairing that catches a status changed
        //  in one place and described in the other.
        TEST_METHOD (AssembleHelp_PrintsTheAs65Numbers)
        {
            std::string  help = CommandLineParser::kAssembleExitStatusHelpText;

            Assert::IsTrue (help.find ("0  Assembled successfully")             != std::string::npos);
            Assert::IsTrue (help.find ("1  Bad command line")                   != std::string::npos);
            Assert::IsTrue (help.find ("2  Error opening source or output file") != std::string::npos);
            Assert::IsTrue (help.find ("3  Error assembling source file")       != std::string::npos);
            Assert::IsTrue (help.find ("5  Assembled with warnings")            != std::string::npos);
        }

        //  4 IS LISTED, AND NOTHING RETURNS IT. Both halves matter. A script
        //  ported from as65 may still test for 4, and a list that skipped from
        //  3 to 5 would leave its author guessing whether the status had been
        //  renumbered or folded into another one. So it is named as as65's, and
        //  no constant here claims it.
        TEST_METHOD (AssembleHelp_ListsAs65sStatusFour_WhichNothingReturns)
        {
            std::string  help = CommandLineParser::kAssembleExitStatusHelpText;

            Assert::IsTrue (help.find ("    4  ") != std::string::npos,
                            L"4 is documented, because a ported script may test for it");
            Assert::IsTrue (help.find ("AS65") != std::string::npos,
                            L"and attributed, because it is AS65's status and not this tool's");

            Assert::AreNotEqual (4, As65ExitStatus::kClean);
            Assert::AreNotEqual (4, As65ExitStatus::kBadCommandLine);
            Assert::AreNotEqual (4, As65ExitStatus::kNoOutput);
            Assert::AreNotEqual (4, As65ExitStatus::kAssemblyErrors);
            Assert::AreNotEqual (4, As65ExitStatus::kWarned);
        }

        //  Every status the code can return is named in the text, swept from
        //  the constants rather than from a list written out again here -- a
        //  sample would visit only the ones somebody remembered.
        TEST_METHOD (EveryStatusTheAssemblerProduces_IsDescribedInTheHelp)
        {
            std::string  help     = CommandLineParser::kAssembleExitStatusHelpText;
            int          codes[]  = { As65ExitStatus::kClean,
                                      As65ExitStatus::kWarned,
                                      As65ExitStatus::kNoOutput,
                                      As65ExitStatus::kAssemblyErrors };

            for (int code : codes)
            {
                std::string  entry = "    " + std::to_string (code) + "  ";

                Assert::IsTrue (help.find (entry) != std::string::npos,
                                L"a status the tool returns must be described in its help");
            }
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ExitCodeForRefusalTests
    //
    //  What a command line the parser turned down reports to the shell.
    //
    //  THE EXECUTABLE USED TO RETURN A FLAT 2 FOR EVERY MODE, and worse, only
    //  when it happened to be a bad flag: a refusal with no message of its own
    //  -- a surplus argument -- set the verdict and then fell through to the
    //  dispatch, so `CassoCli as65 prog.a65 extra.a65` printed the refusal and
    //  assembled anyway, reporting that it could not read a source file the
    //  refusal had said nothing about.
    //
    //  Both halves of that are asserted here: the mapping, and its agreement
    //  with the text each mode prints. The number itself was never checkable
    //  before, because it was decided in main.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ExitCodeForRefusalTests)
    {
    public:
        //  as65: "1 - Incorrect parameter specified on the commandline." A
        //  script ported from as65 branches on this, which is the whole reason
        //  the assembling modes do not share `run`'s number.
        TEST_METHOD (AssemblingRefusesWithAs65sBadCommandLine)
        {
            Assert::AreEqual (As65ExitStatus::kBadCommandLine,
                              CommandLineParser::ExitCodeForRefusal (CommandLineOptions::Subcommand::As65));
            Assert::AreEqual (As65ExitStatus::kBadCommandLine,
                              CommandLineParser::ExitCodeForRefusal (CommandLineOptions::Subcommand::Merlin));
        }

        //  `run` and `disk` have no status for a bad command line and fold it
        //  into the one for having done nothing, which is 2 in both tables.
        TEST_METHOD (RunAndDiskRefuseWithNothingStarted)
        {
            Assert::AreEqual (CommandLineParser::kNothingStarted,
                              CommandLineParser::ExitCodeForRefusal (CommandLineOptions::Subcommand::Run));
            Assert::AreEqual (CommandLineParser::kNothingStarted,
                              CommandLineParser::ExitCodeForRefusal (CommandLineOptions::Subcommand::Disk));
        }

        //  AND EACH NUMBER IS THE ONE ITS OWN PAGE PROMISES. The mapping being
        //  self-consistent is worth nothing if the help describes a different
        //  status, and these two drifted apart once already.
        TEST_METHOD (EachRefusalCode_IsDescribedByTheModesOwnExitCodeTable)
        {
            std::string  assemble = CommandLineParser::kAssembleExitStatusHelpText;
            std::string  run      = CommandLineParser::kRunExitStatusHelpText;

            Assert::IsTrue (assemble.find ("    1  Bad command line") != std::string::npos,
                            L"the assembler's page calls 1 a bad command line");
            Assert::AreEqual (1, CommandLineParser::ExitCodeForRefusal (CommandLineOptions::Subcommand::As65),
                              L"and that is what it returns");

            Assert::IsTrue (run.find ("    2  Bad command line") != std::string::npos,
                            L"run's page folds a refusal into 2");
            Assert::AreEqual (2, CommandLineParser::ExitCodeForRefusal (CommandLineOptions::Subcommand::Run),
                              L"and that is what it returns");
        }
    };
}
