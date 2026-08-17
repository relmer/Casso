#include "Pch.h"
#include "../EhmTestHelper.h"
#include "CommandLineParser.h"
#include "Devices/Disk/DiskCommandRunner.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHelpTextTests
//
//  What the tool tells a newcomer, checked against what the tool actually
//  accepts.
//
//  The help text is built in the library rather than beside the printing code
//  precisely so this can exist: the test assembly does not link the console
//  executable, and help nothing can read is help nothing can check. Every
//  assertion here reads exactly the string a user reads.
//
//  Two of these pin defects the help had rather than properties it always
//  satisfied. It documented the disk options with whichever prefix the reader
//  typed -- `/long`, `/addr` -- and the grammar accepts only the `--` spelling,
//  so a reader who typed `/?` was shown flags that silently do nothing. And it
//  was a flag list with no example, which is the half a newcomer cannot supply
//  for themselves.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskHelpTextTests)
{
public:

    //  The command lines of the worked example, without the prose around them.
    //  Options are gathered from these ALONE: the paragraphs beneath use `--`
    //  as a dash, and a scanner that swept the whole block would collect it and
    //  then have to explain it away.
    static std::vector<std::string> ExampleCommandLines (const std::string & help)
    {
        std::vector<std::string>  lines;
        size_t                    at        = help.find (DiskCommandRunner::kExampleHeading);
        size_t                    lineStart = 0;
        size_t                    lineEnd   = 0;

        if (at == std::string::npos)
        {
            return lines;
        }

        lineStart = help.find ('\n', at);

        while (lineStart != std::string::npos)
        {
            std::string  line;

            lineStart++;
            lineEnd = help.find ('\n', lineStart);

            if (lineEnd == std::string::npos)
            {
                break;
            }

            line = help.substr (lineStart, lineEnd - lineStart);

            if (line.rfind ("  CassoCli ", 0) == 0 || line.rfind ("  Casso.exe ", 0) == 0)
            {
                lines.push_back (line);
            }

            lineStart = lineEnd;
        }

        return lines;
    }

    //  Every flag the example actually types, deduplicated and in the order the
    //  reader meets them.
    static std::vector<std::string> OptionsUsedByExample (const std::string & help)
    {
        std::vector<std::string>  options;
        std::vector<std::string>  lines   = ExampleCommandLines (help);

        for (const auto & line : lines)
        {
            std::istringstream  words (line);
            std::string         word;

            while (words >> word)
            {
                bool  isFlag  = word.size() > 1 && word[0] == '-';
                bool  isNamed = isFlag && (isalnum ((unsigned char) word.back()) != 0);

                if (isNamed && std::find (options.begin(), options.end(), word) == options.end())
                {
                    options.push_back (word);
                }
            }
        }

        return options;
    }

    //  The help outside the example's command lines. "Documented" has to mean
    //  described somewhere else -- an option is not documented by the one line
    //  that uses it.
    static std::string HelpWithoutExampleCommands (const std::string & help)
    {
        std::vector<std::string>  lines = ExampleCommandLines (help);
        std::string               rest  = help;

        for (const auto & line : lines)
        {
            size_t  at = rest.find (line);

            while (at != std::string::npos)
            {
                rest.erase (at, line.size());
                at = rest.find (line);
            }
        }

        return rest;
    }

    //  Whether the help offers this exact word, rather than merely containing
    //  its letters somewhere.
    //
    //  A plain find() is not the question being asked, and the difference is
    //  not academic: renaming the `rm` alias to `del` left every assertion here
    //  green, because `delete` contains `del`. A verb the grammar accepts and
    //  the help never mentions was invisible to the sweep meant to find exactly
    //  that. Dashes count as part of a token so `-o` cannot be answered by the
    //  `-o` inside `--out`.
    static bool ContainsAsWholeToken (const std::string & help, const std::string & word)
    {
        size_t  at = help.find (word);

        while (at != std::string::npos)
        {
            bool  openLeft  = (at == 0) ||
                              (isalnum ((unsigned char) help[at - 1]) == 0 && help[at - 1] != '-');
            size_t  after   = at + word.size();
            bool  openRight = (after >= help.size()) ||
                              (isalnum ((unsigned char) help[after]) == 0 && help[after] != '-');

            if (openLeft && openRight)
            {
                return true;
            }

            at = help.find (word, at + 1);
        }

        return false;
    }

    static std::wstring Widen (const std::string & text)
    {
        return std::wstring (text.begin(), text.end());
    }

    TEST_METHOD (HelpText_CarriesAWorkedExampleOfTheWholeLoop_NotOnlyAFlagList)
    {
        std::string               help  = DiskCommandRunner::BuildHelpText();
        std::vector<std::string>  lines = ExampleCommandLines (help);

        //  Five commands: assemble, place the program, place the greeting, set
        //  the greeting, launch. The count is asserted first because every
        //  assertion below iterates, and a loop over nothing passes.
        Assert::AreEqual (size_t (5), lines.size(), L"the example runs the loop end to end");

        Assert::AreEqual (std::string ("  CassoCli prog.a65 -o prog.bin --raw"),
                          lines[0], L"assemble");
        Assert::AreEqual (std::string ("  CassoCli disk put mydisk.dsk prog.bin"
                                       " --as PROG --type B --addr $6000"),
                          lines[1], L"place the program");
        Assert::AreEqual (std::string ("  CassoCli disk put mydisk.dsk greet.bas"
                                       " --as STARTUP --basic"),
                          lines[2], L"place the greeting");
        Assert::AreEqual (std::string ("  CassoCli disk boot mydisk.dsk STARTUP"),
                          lines[3], L"set the greeting");
        Assert::AreEqual (std::string ("  Casso.exe --disk1 mydisk.dsk"),
                          lines[4], L"launch");
    }

    TEST_METHOD (HelpText_EveryOptionTheExampleTypes_IsDescribedElsewhereInTheSameHelp)
    {
        std::string               help    = DiskCommandRunner::BuildHelpText();
        std::string               rest    = HelpWithoutExampleCommands (help);
        std::vector<std::string>  options = OptionsUsedByExample (help);
        std::vector<std::string>  expected { "-o", "--raw", "--as", "--type",
                                             "--addr", "--basic", "--disk1" };

        //  The exact set, not merely a non-empty one. A scanner that found
        //  nothing, or found only the first line's two, would satisfy the loop
        //  beneath while checking almost none of the example.
        Assert::IsTrue (options == expected, L"the example's flags, exactly");

        for (const auto & option : options)
        {
            Assert::IsTrue (ContainsAsWholeToken (rest, option),
                            (L"undocumented option in the example: " + Widen (option)).c_str());
        }
    }

    TEST_METHOD (HelpText_EverySpellingTheDiskGrammarAccepts_AppearsInTheHelp)
    {
        std::string  help  = DiskCommandRunner::BuildHelpText();
        auto         verbs = CommandLineParser::GetAllDiskVerbs();

        //  A sweep of the parser's own table rather than a list retyped here:
        //  a verb added to the grammar and left out of the help is a capability
        //  the user cannot find, and only this direction notices.
        Assert::AreEqual (size_t (7), verbs.size(), L"five verbs and two aliases");

        for (const auto & verb : verbs)
        {
            Assert::IsTrue (ContainsAsWholeToken (help, verb.name),
                            (L"undocumented verb: " + Widen (verb.name)).c_str());
        }
    }

    TEST_METHOD (HelpText_DocumentsTheThreeExitStatuses_AndThatNoneAreDefinedAboveTwo)
    {
        std::string  help = DiskCommandRunner::BuildHelpText();

        Assert::IsTrue (help.find ("0 clean, 1 succeeded with complaints,"
                                   " 2 produced no output") != std::string::npos,
                        L"the three shared statuses");

        //  "There are none" IS the documentation. Saying nothing would read as
        //  an omission, and a caller has no way to tell the two apart.
        Assert::IsTrue (help.find ("defines no status above 2") != std::string::npos,
                        L"the subcommand's scoped statuses, of which there are none");
    }

    TEST_METHOD (HelpText_SaysPutAndGetAreNamedFromTheDisksPointOfView)
    {
        std::string  help = DiskCommandRunner::BuildHelpText();

        Assert::IsTrue (help.find ("put and get are named from the DISK's point of view")
                            != std::string::npos,
                        L"the direction of the two verbs that have one");
        Assert::IsTrue (help.find ("put places a") != std::string::npos &&
                        help.find ("host file on the disk, get takes one off it")
                            != std::string::npos,
                        L"and which way each one goes");
    }

    TEST_METHOD (HelpText_QuotesTheRoundTripAndInUseSentences_SoNeitherClaimCanDrift)
    {
        std::string  help = DiskCommandRunner::BuildHelpText();

        //  Both sentences live beside the code that has to keep them true. What
        //  this asserts is that they reach the user -- a claim kept accurate in
        //  a header nobody prints is worth nothing.
        Assert::IsTrue (help.find (ApplesoftTokenizer::kRoundTripHelpText) != std::string::npos,
                        L"what --basic does and does not round-trip");
        Assert::IsTrue (help.find (DiskCommandRunner::kInUseHelpText) != std::string::npos,
                        L"what the in-use probe can and cannot see");
    }

    TEST_METHOD (HelpText_WarnsAgainstDosBinIntoPut_BecauseTheDoubledHeaderRunsAsCode)
    {
        std::string  help = DiskCommandRunner::BuildHelpText();

        Assert::IsTrue (help.find ("--raw rather than --dos-bin") != std::string::npos,
                        L"which assembler shape the placement path wants");
        Assert::IsTrue (help.find ("its own header loaded as code") != std::string::npos,
                        L"and what goes wrong when the other one is used");
    }

    TEST_METHOD (HelpText_SaysTheBootProgramMustBeOneTheGreetingCanRun)
    {
        std::string  help = DiskCommandRunner::BuildHelpText();

        Assert::IsTrue (help.find ("RUNs its greeting") != std::string::npos,
                        L"what a booting DOS 3.3 does with the name");
        Assert::IsTrue (help.find ("boots without running it") != std::string::npos,
                        L"and what naming a binary there actually gets you");
    }

    TEST_METHOD (HelpText_SpellsDiskOptionsWithTwoDashes_WhichIsTheOnlySpellingAccepted)
    {
        char *              slashArgv[] = { (char *) "CassoCli", (char *) "disk",
                                            (char *) "list", (char *) "d.dsk",
                                            (char *) "/long" };
        char *              dashArgv[]  = { (char *) "CassoCli", (char *) "disk",
                                            (char *) "list", (char *) "d.dsk",
                                            (char *) "--long" };
        CommandLineOptions  slashed     = CommandLineParser::Parse (5, slashArgv,
                                              [] (const std::string &) { return false; });
        CommandLineOptions  dashed      = CommandLineParser::Parse (5, dashArgv,
                                              [] (const std::string &) { return false; });
        std::string         help        = DiskCommandRunner::BuildHelpText();
        const char *        slashForms[] = { "/long", "/out", "/as", "/type",
                                             "/addr", "/text", "/basic", "/verbatim" };

        //  The grammar's answer first, because it is what makes the help's
        //  spelling right or wrong rather than merely a house style.
        Assert::IsFalse (slashed.disk.longListing, L"the slash spelling is not accepted");
        Assert::IsTrue  (dashed.disk.longListing,  L"the dash spelling is");

        for (const char * form : slashForms)
        {
            Assert::IsTrue (help.find (form) == std::string::npos,
                            (L"help offers a spelling the parser rejects: " +
                             Widen (form)).c_str());
        }
    }
};
