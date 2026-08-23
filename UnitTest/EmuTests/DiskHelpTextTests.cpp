#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "CommandLineHelp.h"
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
//  `/?` was shown one page in two forms and told to live with it.
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
        size_t                    at        = help.find (CommandLineHelp::kExampleHeading);
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
    //
    //  A SINGLE-DASH FLAG IS REDUCED TO THE FLAG ITSELF, because the assembler
    //  grammar glues values to flags and the example now types `-oprog.bin` as
    //  one word. Taking the word whole would ask the help to document an option
    //  called `-oprog.bin`, which is the filename's fault rather than the
    //  help's. Long options keep their whole word -- `--as` takes its value as
    //  a separate argument and is a complete token already.
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
                bool         isFlag  = word.size() > 1 && word[0] == '-';
                bool         isLong  = isFlag && word[1] == '-';
                bool         isNamed = isFlag && (isalnum ((unsigned char) word.back()) != 0);
                std::string  flag    = isLong ? word : word.substr (0, 2);

                if (isNamed && std::find (options.begin(), options.end(), flag) == options.end())
                {
                    options.push_back (flag);
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
        //  now, so the example that used to write `--raw` writes nothing.
        //  The output name is ATTACHED, which is as65's grammar and now this
        //  mode's only form.
        //  The first step names its dialect, because assembling does now:
        //  a bare source file is no longer an invocation this tool accepts.
        Assert::AreEqual (std::string ("  CassoCli as65 prog.a65 -oprog.bin"),
                          lines[0], L"assemble");
        Assert::AreEqual (std::string ("  CassoCli disk put mydisk.dsk prog.bin"
                                       " --as PROG --type B --addr $6000"),
                          lines[1], L"place the program");
        Assert::AreEqual (std::string ("  CassoCli disk put mydisk.dsk greet.bas"
                                       " --as STARTUP --basic"),
                          lines[2], L"place the greeting");
        Assert::AreEqual (std::string ("  CassoCli disk boot mydisk.dsk STARTUP"),
                          lines[3], L"set the greeting");
        //  The launch line names the MACHINE as well as the disk: a reader
        //  following the loop should land on the //e the rest of it assumes.
        Assert::AreEqual (std::string ("  Casso.exe --machine Apple2e --disk1 mydisk.dsk"),
                          lines[4], L"launch");
    }

    TEST_METHOD (HelpText_EveryOptionTheExampleTypes_IsDescribedElsewhereInTheSameHelp)
    {
        std::string               help    = DiskCommandRunner::BuildHelpText();
        std::string               rest    = HelpWithoutExampleCommands (help);
        std::vector<std::string>  options = OptionsUsedByExample (help);
        std::vector<std::string>  expected { "-o", "--as", "--type", "--addr",
                                             "--basic", "--machine", "--disk1" };

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
    //  do not, and this test is what says so: an assembly error exits 3 under
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
        Assert::IsTrue (assemble.find ("0  Assembled successfully") != std::string::npos);
        Assert::IsTrue (run.find      ("0  Ran to a stop") != std::string::npos);
        Assert::IsTrue (disk.find     ("0  The command was carried out") != std::string::npos);

        //  And the one they do not. Under the assembler, 1 is as65's bad
        //  command line; under `run`, 1 is a source file that did not
        //  assemble -- which the assembler itself calls 3.
        Assert::IsTrue (assemble.find ("1  Bad command line") != std::string::npos,
                        L"1 under the assembler is a command line that could not be acted on");
        Assert::IsTrue (run.find ("did not assemble; nothing ran") != std::string::npos,
                        L"1 under run is an assembly failure, which is 3 under the assembler");

        //  2 is a failure to produce anything in all three, which is the only
        //  part of the old shared block that was ever true everywhere. The
        //  assembler states it as a FILE that could not be opened rather than
        //  as nothing written, because since 3 was split out of it "wrote
        //  nothing" describes both statuses and distinguishes neither -- and
        //  since 1 became the bad command line, a command line that named no
        //  file at all is not 2 either.
        Assert::IsTrue (assemble.find ("2  Error opening source or output file") != std::string::npos);
        Assert::IsTrue (run.find      ("2  Nothing could be started") != std::string::npos);
        Assert::IsTrue (disk.find     ("2  Nothing was done")    != std::string::npos);

        //  3 IS SPENT ON A DIFFERENT THING IN THE TWO MODES THAT HAVE ONE, so
        //  it belongs in this test rather than beside either of them. The
        //  assembler reached it by splitting an assembly error out of "could
        //  not open a file", which is as65's own division; `run` had it
        //  already, for an illegal opcode, because under `run` an assembly
        //  error stops at 1 and nothing executes. `disk` has no 3 at all.
        Assert::IsTrue (run.find      ("3  The program reached an illegal opcode") != std::string::npos);
        Assert::IsTrue (assemble.find ("3  Error assembling source file") != std::string::npos);
        Assert::IsTrue (disk.find     ("3  ") == std::string::npos, L"disk has no 3");

        //  Disk's block reaches the disk help, which is the section it belongs
        //  under -- a status described in a header nobody prints is worth
        //  nothing.
        Assert::IsTrue (DiskCommandRunner::BuildHelpText().find (disk) != std::string::npos,
                        L"and disk's statuses are printed with the disk options");
    }

    //  AND THE THREE ARE WRITTEN IN ONE VOICE, which is the half the test above
    //  does not claim: it asserts that the modes disagree about what a number
    //  MEANS, and says nothing about how the meaning is worded. They had drifted
    //  into two styles -- the assembler's terse capitalized clause and, under
    //  `run` and `disk`, a lowercase sentence with a period -- so a reader
    //  moving between two pages met the same table typeset two ways.
    //
    //  Only the opening is asserted. The assembler's 4 is three full sentences
    //  on purpose, so "no trailing period" is a house style rather than a rule
    //  a test can hold every line to.
    TEST_METHOD (EveryModesStatusLine_OpensInTheSameVoice)
    {
        for (const char * block : { CommandLineParser::kAssembleExitStatusHelpText,
                                    CommandLineParser::kRunExitStatusHelpText,
                                    DiskCommandRunner::kExitStatusHelpText })
        {
            std::istringstream  lines (block);
            std::string         line;

            while (std::getline (lines, line))
            {
                //  A status line, rather than a continuation of the one above
                //  it: `    N  text`, with the number where the number goes.
                if (line.size() < 8 || isdigit ((unsigned char) line[4]) == 0)
                {
                    continue;
                }

                Assert::IsTrue (isupper ((unsigned char) line[7]) != 0,
                                Widen ("status line does not open capitalized: " + line).c_str());
            }
        }
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
    //  there rather than in an "also written" clause at the end of a sentence
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

        Assert::IsTrue (help.find ("Also written") == std::string::npos,
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

        //  Quoted WITHOUT the line breaks they used to carry. The page is
        //  composed one logical line per paragraph and folded to the
        //  reader's terminal at print time, so where a sentence breaks is
        //  the terminal's business and not a fact about the help.
        Assert::IsTrue (help.find ("defaults to the host file's own name") != std::string::npos,
                        L"--as has a default and the help states it");
        Assert::IsTrue (help.find ("only type the guest will RUN") != std::string::npos,
                        L"--type has one too, and --basic overrides it");
        Assert::IsTrue (help.find ("refused without one") != std::string::npos,
                        L"and --addr is required for exactly one kind of file");
    }

    TEST_METHOD (HelpText_WarnsAgainstDosBinIntoPut_BecauseTheDoubledHeaderRunsAsCode)
    {
        std::string  help = DiskCommandRunner::BuildHelpText();

        //  `--raw` used to be named here as the output to assemble with. It is
        //  the default now, so the warning names the default -- and would read
        //  as advice to type a flag that does nothing if it still said --raw.
        Assert::IsTrue (help.find ("the default output rather than --dos-bin") != std::string::npos,
                        L"which assembler output the placement path wants");
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

        //  The grammar's answer, because it is what makes the help's form
        //  right or wrong rather than merely a house style.
        Assert::IsTrue (slashed.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Text,
                        L"the slash form is accepted");
        Assert::IsTrue (dashed.disk.encoding  == CommandLineOptions::DiskOptions::Encoding::Text,
                        L"and so is the dash form");

        //  An option that takes a value has to work too, or `/out` would be
        //  swallowed as a positional and the file written somewhere else.
        Assert::AreEqual (std::string ("host.bin"), valued.disk.hostFile,
                          L"a slash-written option still consumes its value");
    }

    TEST_METHOD (DiskGrammar_LeavesAProDosPathAlone_EvenThoughItStartsWithASlash)
    {
        char *              argv[]  = { (char *) "CassoCli", (char *) "disk",
                                        (char *) "get", (char *) "d.po",
                                        (char *) "/VOLUME/STARTUP" };
        CommandLineOptions  parsed  = CommandLineParser::Parse (5, argv,
                                          [] (const std::string &) { return false; });

        //  THIS IS WHY THE SLASH FORM IS A TABLE LOOKUP AND NOT A REWRITE. A
        //  ProDOS path is written with a leading slash, and a parser that turned
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
        //  `--` form whichever prefix the assembler flags were given with.
        //  A concession is what a rule looks like when it is not being kept.
        Assert::IsTrue (help.find ("always take the") == std::string::npos,
                        L"the mixed-form excuse is gone");
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

    //  EVERY VERB THE GRAMMAR TAKES IS ON THE PAGE A REFUSAL PRINTS.
    //
    //  The refusal itself used to carry the list, and named the five original
    //  verbs long after eight aliases were added, so a user who mistyped
    //  `catalog` was told to try five words that did not include it. It names
    //  only what the user typed now, and the page above it carries the verbs.
    //  So the claim moves here, onto the page, and is still swept from the
    //  grammar's own table rather than from a list written out again.
    TEST_METHOD (EveryVerbTheGrammarTakes_IsOnThePageARefusalPrints)
    {
        CommandLineOptions  options;
        FakeDiskFileIo      fileIo;
        DiskCommandRunner   runner (fileIo);
        DiskCommandResult   result = runner.Run (options);
        std::string         page   = DiskCommandRunner::BuildHelpText();

        Assert::AreEqual (2, result.exitStatus, L"a word that names no verb produces nothing");
        Assert::IsTrue (result.badCommandLine, L"and the edge is told to print the page");

        for (const auto & verb : CommandLineParser::GetAllDiskVerbs())
        {
            Assert::IsTrue (ContainsAsWholeToken (page, verb.name),
                            (L"the page does not offer: " + Widen (verb.name)).c_str());
        }
    }

    TEST_METHOD (HelpText_PutsTheExampleAfterTheOptions_NotBetweenThem)
    {
        std::string  help     = DiskCommandRunner::BuildHelpText();
        size_t       verbs    = help.find ("CassoCli disk list");
        size_t       options  = help.find ("Write an extracted file here");
        size_t       example  = help.find (CommandLineHelp::kExampleHeading);

        //  Overview, then options, then the worked loop. The old order put the
        //  example in the middle, where a reader scanning for a flag walks over
        //  it and a reader who wants it has to scroll back.
        Assert::IsTrue (verbs   != std::string::npos, L"the grammar is there");
        Assert::IsTrue (options != std::string::npos, L"so are the options");
        Assert::IsTrue (example != std::string::npos, L"so is the example");

        Assert::IsTrue (verbs < options, L"the grammar comes before the options");
        Assert::IsTrue (options < example, L"and the example comes after both");
    }

    //
    //  THE WORKED LOOP IS ON TWO PAGES AND IS WRITTEN ONCE. The general page
    //  shows it because it is the one thing on that page which is not a table
    //  of contents; the disk page shows it because every flag in it is
    //  described there. Two copies would be two loops that can drift into
    //  placing different files under the same explanation, so both pages call
    //  one function -- and this asserts that they still do rather than that
    //  they agree today.
    //
    TEST_METHOD (ExampleLoop_IsOneBlock_ShownByBothTheGeneralPageAndTheDiskPage)
    {
        const char  kPrefixes[] = { '-', '/' };

        for (char prefix : kPrefixes)
        {
            std::string  commands = CommandLineHelp::BuildExampleCommands (prefix);
            std::string  general  = CommandLineHelp::BuildGeneralHelp ("banner\n", prefix);
            std::string  disk     = DiskCommandRunner::BuildHelpText (prefix);

            Assert::IsTrue (general.find (commands) != std::string::npos,
                            L"the general page shows the loop, whole");
            Assert::IsTrue (disk.find (commands) != std::string::npos,
                            L"and so does the disk page");
        }
    }

    //  The prose explaining the loop's two traps belongs to the page where
    //  every flag it names is described. On the general page it would be six
    //  paragraphs about flags that page does not document -- which is how the
    //  one page grew to four screens in the first place.
    TEST_METHOD (ExampleProse_StaysOnTheDiskPage_WhereTheFlagsItExplainsAreDescribed)
    {
        std::string  general = CommandLineHelp::BuildGeneralHelp ("banner\n", '-');
        std::string  disk    = DiskCommandRunner::BuildHelpText ('-');

        Assert::IsTrue (disk.find ("its own header loaded as code") != std::string::npos,
                        L"the disk page explains the doubled header");
        Assert::IsTrue (general.find ("its own header loaded as code") == std::string::npos,
                        L"and the general page does not");

        Assert::IsTrue (general.find ("Exit status") == std::string::npos,
                        L"nor does it claim an exit status any mode would disagree with");
    }

    //  The banner heads every help page, and this page is the one assembled in
    //  the library, where the build's version is not known. So it arrives as an
    //  argument, the way the general page already takes one -- and a page
    //  reached directly by `disk --help`, without ever passing the general
    //  page, still says which build is answering.
    //
    //  ASSERTED AT POSITION 0, not merely present: a banner anywhere in the
    //  page is a banner that could be under the exit statuses. And the empty
    //  default is asserted too, because every caller that is not the console
    //  executable relies on it -- including the other tests in this file, which
    //  would otherwise have to supply a version to ask a question about wording.
    TEST_METHOD (TheDiskPage_TakesItsBannerFromTheCaller_AndPutsItFirst)
    {
        const char   kBanner[] = "CassoCli - 6502 Assembler and Emulator  v0.0.0\n"
                                 "Copyright (c) 2025-2026 by Robert Elmer\n";
        std::string  withBanner = DiskCommandRunner::BuildHelpText ('-', kBanner);
        std::string  bare       = DiskCommandRunner::BuildHelpText ('-');

        Assert::IsTrue (withBanner.starts_with (kBanner),
                        L"the banner heads the page");
        Assert::IsTrue (withBanner.find ("Usage:") != std::string::npos,
                        L"and the page itself is still under it");

        Assert::IsTrue (bare.starts_with ("Usage:"),
                        L"no banner asked for, none printed and nothing left in its place");
    }
};
