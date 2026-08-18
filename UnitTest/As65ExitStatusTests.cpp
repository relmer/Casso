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

        //  The four statuses are distinct values, which is the only property a
        //  script branching on them actually needs.
        TEST_METHOD (TheFourStatusesAreDistinct)
        {
            std::set<int>  statuses = { As65ExitStatus::kClean,
                                        As65ExitStatus::kWarned,
                                        As65ExitStatus::kNoOutput,
                                        As65ExitStatus::kAssemblyErrors };

            Assert::AreEqual (size_t (4), statuses.size());
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
        //  The help claimed an assembly error exited 2, because it did. Both
        //  moved together, and this is what says so.
        TEST_METHOD (AssembleHelp_StatesAssemblyErrorsAsThree)
        {
            std::string  help = CommandLineParser::kAssembleExitStatusHelpText;

            Assert::IsTrue (help.find ("3  the source was read and did not assemble") != std::string::npos);
            Assert::IsTrue (help.find ("assembly error") == std::string::npos,
                            L"and 2 no longer claims it");
        }

        //  as65 defines a fifth status this assembler does not produce. It is
        //  named as as65's rather than left out, so a caller porting a script
        //  can see that the difference was noticed instead of guessing which
        //  status replaced it.
        TEST_METHOD (AssembleHelp_NamesTheAs65StatusItDoesNotProduce)
        {
            std::string  help = CommandLineParser::kAssembleExitStatusHelpText;

            Assert::IsTrue (help.find ("as65 also defines 4") != std::string::npos);
            Assert::IsTrue (help.find ("does not produce it") != std::string::npos);
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
}
