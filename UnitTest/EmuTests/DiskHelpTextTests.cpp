#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
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
//  satisfied. It was a flag list with no example, which is the half a newcomer
//  cannot supply for themselves. And it printed the disk options as `--out`
//  and `--addr` no matter which prefix the reader typed, then documented the
//  inconsistency in a sentence of its own -- a reader who asked for help with
//  `/?` was shown one page in two spellings and told to live with it.
//
//  THAT ONE IS NOW ASSERTED IN BOTH DIRECTIONS, and the parser is asked first.
//  A help that offers `/out` is only correct because `/out` is accepted;
//  checking the text alone would let the two drift apart again in the other
//  direction, with the help right and the grammar wrong.
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

        //  No shape flag: the assembled bytes are what a bare invocation writes
        //  now, so the example that used to spell `--raw` spells nothing.
        Assert::AreEqual (std::string ("  CassoCli prog.a65 -o prog.bin"),
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
        std::vector<std::string>  expected { "-o", "--as", "--type",
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

    TEST_METHOD (HelpText_EveryVerbTheDiskGrammarAccepts_AppearsInTheHelp)
    {
        std::string  help  = DiskCommandRunner::BuildHelpText();
        auto         verbs = CommandLineParser::GetAllDiskVerbs();

        //  A sweep of the parser's own table rather than a list retyped here:
        //  a verb added to the grammar and left out of the help is a capability
        //  the user cannot find, and only this direction notices.
        Assert::AreEqual (size_t (13), verbs.size(), L"five verbs and eight aliases");

        for (const auto & verb : verbs)
        {
            Assert::IsTrue (ContainsAsWholeToken (help, verb.name),
                            (L"undocumented verb: " + Widen (verb.name)).c_str());
        }
    }

    //
    //  EACH MODE STATES ITS OWN STATUSES, UNDER ITSELF, BECAUSE THEY DIFFER.
    //
    //  They were documented under `disk` alone, then moved to one shared block
    //  at the top of the page on the belief that the three modes agreed. They
    //  do not, and this test is what says so: an assembly error exits 2 under
    //  the assembler and 1 under `run`, and status 1 means "the output was
    //  written anyway" in one mode and "nothing ran" in the other. A shared
    //  block cannot be true of both.
    //
    //  Every claim below was measured against the built binary rather than read
    //  off the text it describes.
    //
    TEST_METHOD (ExitStatusHelpText_IsStatedPerMode_BecauseTheModesDisagree)
    {
        std::string  assemble = CommandLineParser::kAssembleExitStatusHelpText;
        std::string  run      = CommandLineParser::kRunExitStatusHelpText;
        std::string  disk     = DiskCommandRunner::kExitStatusHelpText;

        //  The status the three modes genuinely share.
        Assert::IsTrue (assemble.find ("0  assembled cleanly") != std::string::npos);
        Assert::IsTrue (run.find      ("0  the program ran to a stop") != std::string::npos);
        Assert::IsTrue (disk.find     ("0  the command was carried out") != std::string::npos);

        //  And the one they do not. Under the assembler, 1 still wrote the
        //  output; under `run`, 1 means nothing ran at all.
        Assert::IsTrue (assemble.find ("The output was still\n       written") != std::string::npos,
                        L"1 under the assembler still produced output");
        Assert::IsTrue (run.find ("did not assemble. Nothing ran") != std::string::npos,
                        L"1 under run produced none -- the opposite claim");

        //  2 means "nothing was produced" in all three, which is the only part
        //  of the old shared block that was ever true everywhere.
        Assert::IsTrue (assemble.find ("2  wrote nothing")       != std::string::npos);
        Assert::IsTrue (run.find      ("2  nothing could be started") != std::string::npos);
        Assert::IsTrue (disk.find     ("2  nothing was done")    != std::string::npos);

        //  3 belongs to `run` and appears nowhere else, so a script branching
        //  on the assembler's statuses cannot meet it.
        Assert::IsTrue (run.find      ("3  the program reached an illegal opcode") != std::string::npos);
        Assert::IsTrue (assemble.find ("3  ") == std::string::npos, L"the assembler has no 3");
        Assert::IsTrue (disk.find     ("3  ") == std::string::npos, L"neither has disk");

        //  Disk's block reaches the disk help, which is the section it belongs
        //  under -- a status described in a header nobody prints is worth
        //  nothing.
        Assert::IsTrue (DiskCommandRunner::BuildHelpText().find (disk) != std::string::npos,
                        L"and disk's statuses are printed with the disk options");
    }

    //  The old help carried a paragraph explaining that `put` and `get` are
    //  named from the disk's point of view. It was there to rescue two verb
    //  names the descriptions did not state the direction of; the descriptions
    //  state it now, so the paragraph is redundant rather than merely wordy.
    TEST_METHOD (HelpText_PutsTheDirectionInTheVerbDescriptions_NotInAParagraphExcusingTheNames)
    {
        std::string  help = DiskCommandRunner::BuildHelpText();

        Assert::IsTrue (help.find ("point of view") == std::string::npos,
                        L"the paragraph excusing the two verb names is gone");

        Assert::IsTrue (help.find ("Read a file from the disk") != std::string::npos,
                        L"get says which way it goes, on its own line");
        Assert::IsTrue (help.find ("Write a host file to the disk") != std::string::npos,
                        L"and so does put");
    }

    //  Every alias of a verb leads the line that describes it, so a reader
    //  scanning the left margin for the word they already have in mind finds it
    //  there rather than in an "also spelled" clause at the end of a sentence
    //  about a word they do not use.
    TEST_METHOD (HelpText_LeadsEachVerbLineWithEveryAlias_NotWithAnAlsoSpelledFootnote)
    {
        std::string   help    = DiskCommandRunner::BuildHelpText();
        const char *  leads[] =
        {
            "  cat | catalog | dir | list | ls   ",
            "  read | get                        ",
            "  write | put                       ",
            "  del | delete | rm                 ",
            "  boot                              ",
        };

        for (const char * lead : leads)
        {
            Assert::IsTrue (help.find (lead) != std::string::npos,
                            (L"verb line missing or misaligned: " + Widen (lead)).c_str());
        }

        Assert::IsTrue (help.find ("Also spelled") == std::string::npos,
                        L"and no alias is left trailing in a footnote");
    }

    TEST_METHOD (HelpText_QuotesTheRoundTripSentence_SoTheClaimCannotDrift)
    {
        std::string  help = DiskCommandRunner::BuildHelpText();

        //  The sentence lives beside the code that has to keep it true. What
        //  this asserts is that it reaches the user -- a claim kept accurate in
        //  a header nobody prints is worth nothing.
        Assert::IsTrue (help.find (ApplesoftTokenizer::RoundTripHelpText ('-')) != std::string::npos,
                        L"what --basic does and does not round-trip");

        //  It sits under its own flag rather than closing the section. A reader
        //  who has just read the --basic row has stopped looking two paragraphs
        //  down, which is where this used to be.
        Assert::IsTrue (help.find ("--basic                Convert to and from an Applesoft listing\n"
                                   "\n  --basic is real tokenization") != std::string::npos,
                        L"and it follows the row it explains");

        //  The in-use paragraph is gone: a locked image is refused by name where
        //  it happens, and the help does not document error messages.
        Assert::IsTrue (help.find ("in-use check") == std::string::npos,
                        L"the in-use paragraph is gone");
    }

    TEST_METHOD (HelpText_SaysWhatBootWillRefuse_BecauseNothingInAListingShowsThatSettingAtAll)
    {
        std::string  help = DiskCommandRunner::BuildHelpText();

        Assert::IsTrue (help.find ("has to be on the volume already") != std::string::npos,
                        L"the program must exist");
        Assert::IsTrue (help.find ("operating system on the tracks a") != std::string::npos,
                        L"the image must be bootable at all");
        Assert::IsTrue (help.find ("type SYS, and not the kernel") != std::string::npos,
                        L"what ProDOS will launch");
    }

    TEST_METHOD (HelpText_SaysWhatPutsThreeNamingOptionsDefaultTo)
    {
        std::string  help = DiskCommandRunner::BuildHelpText();

        Assert::IsTrue (help.find ("defaults\n  to the host file's own name") != std::string::npos,
                        L"--as has a default and the help states it");
        Assert::IsTrue (help.find ("only\n  type the guest will RUN") != std::string::npos,
                        L"--type has one too, and --basic overrides it");
        Assert::IsTrue (help.find ("refused without one") != std::string::npos,
                        L"and --addr is required for exactly one kind of file");
    }

    TEST_METHOD (HelpText_WarnsAgainstDosBinIntoPut_BecauseTheDoubledHeaderRunsAsCode)
    {
        std::string  help = DiskCommandRunner::BuildHelpText();

        //  `--raw` used to be named here as the shape to assemble with. It is
        //  the default now, so the warning names the default -- and would read
        //  as advice to type a flag that does nothing if it still said --raw.
        Assert::IsTrue (help.find ("the default shape rather than --dos-bin") != std::string::npos,
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

    TEST_METHOD (DiskGrammar_AcceptsEitherPrefix_SoTheHelpMaySpellEitherOne)
    {
        char *              slashArgv[] = { (char *) "CassoCli", (char *) "disk",
                                            (char *) "get", (char *) "d.dsk",
                                            (char *) "F", (char *) "/text" };
        char *              dashArgv[]  = { (char *) "CassoCli", (char *) "disk",
                                            (char *) "get", (char *) "d.dsk",
                                            (char *) "F", (char *) "--text" };
        char *              valueArgv[] = { (char *) "CassoCli", (char *) "disk",
                                            (char *) "get", (char *) "d.dsk",
                                            (char *) "F", (char *) "/out",
                                            (char *) "host.bin" };
        CommandLineOptions  slashed     = CommandLineParser::Parse (6, slashArgv,
                                              [] (const std::string &) { return false; });
        CommandLineOptions  dashed      = CommandLineParser::Parse (6, dashArgv,
                                              [] (const std::string &) { return false; });
        CommandLineOptions  valued      = CommandLineParser::Parse (7, valueArgv,
                                              [] (const std::string &) { return false; });

        //  The grammar's answer, because it is what makes the help's spelling
        //  right or wrong rather than merely a house style.
        Assert::IsTrue (slashed.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Text,
                        L"the slash spelling is accepted");
        Assert::IsTrue (dashed.disk.encoding  == CommandLineOptions::DiskOptions::Encoding::Text,
                        L"and so is the dash spelling");

        //  An option that takes a value has to work too, or `/out` would be
        //  swallowed as a positional and the file written somewhere else.
        Assert::AreEqual (std::string ("host.bin"), valued.disk.hostFile,
                          L"a slash-spelled option still consumes its value");
    }

    TEST_METHOD (DiskGrammar_LeavesAProDosPathAlone_EvenThoughItStartsWithASlash)
    {
        char *              argv[]  = { (char *) "CassoCli", (char *) "disk",
                                        (char *) "get", (char *) "d.po",
                                        (char *) "/VOLUME/STARTUP" };
        CommandLineOptions  parsed  = CommandLineParser::Parse (5, argv,
                                          [] (const std::string &) { return false; });

        //  THIS IS WHY THE SLASH FORM IS A TABLE LOOKUP AND NOT A REWRITE. A
        //  ProDOS path is spelled with a leading slash, and a parser that turned
        //  every one of them into a flag would lose the operand entirely.
        Assert::AreEqual (std::string ("/VOLUME/STARTUP"), parsed.disk.path,
                          L"a path that begins with a slash stays an operand");
    }

    TEST_METHOD (HelpText_SpellsEveryDiskOptionWithThePrefixTheReaderAsked_AndNeverTheOther)
    {
        std::string   slashHelp = DiskCommandRunner::BuildHelpText ('/');
        std::string   dashHelp  = DiskCommandRunner::BuildHelpText ('-');
        const char *  options[] = { "out", "as", "type", "addr", "text", "basic" };

        for (const char * option : options)
        {
            std::string  slashForm = std::string ("/")  + option;
            std::string  dashForm  = std::string ("--") + option;

            Assert::IsTrue (ContainsAsWholeToken (slashHelp, slashForm),
                            (L"slash help omits: " + Widen (slashForm)).c_str());
            Assert::IsTrue (slashHelp.find (dashForm) == std::string::npos,
                            (L"slash help still shows: " + Widen (dashForm)).c_str());

            Assert::IsTrue (ContainsAsWholeToken (dashHelp, dashForm),
                            (L"dash help omits: " + Widen (dashForm)).c_str());
            Assert::IsTrue (dashHelp.find (slashForm) == std::string::npos,
                            (L"dash help still shows: " + Widen (slashForm)).c_str());
        }
    }

    //
    //  A RETIRED OPTION MUST LEAVE THE HELP WITH IT. An option deleted from the
    //  grammar and left in the usage text is worse than one that never existed:
    //  the reader types what they were shown, and the grammar now refuses it.
    //
    //  `--long` went because the two columns it withheld -- eof= and aux=, the
    //  exact length of a file and the address a binary loads at -- are filled
    //  by ProDosVolume::Enumerate whether or not anyone asks, and are the two a
    //  build loop most wants. `--verbatim` went because verbatim is the
    //  default, leaving the flag nothing to do but cancel a `--text` earlier on
    //  the same line.
    //
    TEST_METHOD (HelpText_OffersNeitherRetiredOption_InEitherPrefix)
    {
        const char *  kRetired[] = { "long", "verbatim" };

        for (const char * option : kRetired)
        {
            std::string  dashForm  = std::string ("--") + option;
            std::string  slashForm = std::string ("/")  + option;

            Assert::IsTrue (DiskCommandRunner::BuildHelpText ('-').find (dashForm) == std::string::npos,
                            (L"the dash help still offers: " + Widen (dashForm)).c_str());
            Assert::IsTrue (DiskCommandRunner::BuildHelpText ('/').find (slashForm) == std::string::npos,
                            (L"the slash help still offers: " + Widen (slashForm)).c_str());
        }

        //  And what replaced the `--long` row: the columns are described as
        //  something a listing always shows, rather than as a flag.
        Assert::IsTrue (DiskCommandRunner::BuildHelpText().find ("eof= and aux=") != std::string::npos,
                        L"the columns are still explained, just not as an option");
    }

    TEST_METHOD (HelpText_NoLongerExcusesTheMixedSpelling_BecauseThereIsNoLongerOne)
    {
        std::string  help = DiskCommandRunner::BuildHelpText();

        //  The old help carried a sentence conceding that disk options took the
        //  `--` spelling whichever prefix the assembler flags were given with.
        //  A concession is what a rule looks like when it is not being kept.
        Assert::IsTrue (help.find ("always take the") == std::string::npos,
                        L"the mixed-spelling excuse is gone");
        Assert::IsTrue (help.find ("whichever prefix") == std::string::npos,
                        L"and so is the sentence that explained it away");
    }

    //  `disk --help` printed "unknown disk verb" on the error stream and exited
    //  2 -- a refusal, for a question the tool knows the answer to.
    TEST_METHOD (HelpVerb_PrintsTheDiskHelpOnOutput_AndSucceeds)
    {
        CommandLineOptions  options;
        FakeDiskFileIo      fileIo;
        DiskCommandRunner   runner (fileIo);

        options.disk.verb = CommandLineOptions::DiskOptions::Verb::Help;

        DiskCommandResult  result = runner.Run (options);

        Assert::AreEqual (0, result.exitStatus, L"asking for help is not a failure");
        Assert::AreEqual (std::string(), result.diagnostics, L"and it is not a complaint");
        Assert::AreEqual (DiskCommandRunner::BuildHelpText ('-'), result.output,
                          L"the disk section of the help, on the output stream");
    }

    TEST_METHOD (HelpVerb_SpellsItselfWithThePrefixTheReaderTyped)
    {
        CommandLineOptions  options;
        FakeDiskFileIo      fileIo;
        DiskCommandRunner   runner (fileIo);

        options.disk.verb  = CommandLineOptions::DiskOptions::Verb::Help;
        options.flagPrefix = '/';

        DiskCommandResult  result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::BuildHelpText ('/'), result.output);
    }

    //  The refusal named the five original verbs long after eight aliases were
    //  added, so a user who mistyped `catalog` was told to try five words that
    //  did not include it.
    TEST_METHOD (UnknownVerb_IsRefusedWithEveryVerbTheGrammarActuallyTakes)
    {
        CommandLineOptions  options;
        FakeDiskFileIo      fileIo;
        DiskCommandRunner   runner (fileIo);
        DiskCommandResult   result = runner.Run (options);

        Assert::AreEqual (2, result.exitStatus, L"a word that names no verb produces nothing");

        for (const auto & verb : CommandLineParser::GetAllDiskVerbs())
        {
            Assert::IsTrue (ContainsAsWholeToken (result.diagnostics, verb.name),
                            (L"the refusal does not offer: " + Widen (verb.name)).c_str());
        }
    }

    TEST_METHOD (HelpText_PutsTheExampleAfterTheOptions_NotBetweenThem)
    {
        std::string  help     = DiskCommandRunner::BuildHelpText();
        size_t       verbs    = help.find ("CassoCli disk list");
        size_t       options  = help.find ("Write an extracted file here");
        size_t       example  = help.find (DiskCommandRunner::kExampleHeading);

        //  Overview, then options, then the worked loop. The old order put the
        //  example in the middle, where a reader scanning for a flag walks over
        //  it and a reader who wants it has to scroll back.
        Assert::IsTrue (verbs   != std::string::npos, L"the grammar is there");
        Assert::IsTrue (options != std::string::npos, L"so are the options");
        Assert::IsTrue (example != std::string::npos, L"so is the example");

        Assert::IsTrue (verbs < options, L"the grammar comes before the options");
        Assert::IsTrue (options < example, L"and the example comes after both");
    }
};
