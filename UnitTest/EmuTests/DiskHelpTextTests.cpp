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
    //  green, because `delete` contains `del`. A command the grammar accepts and
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

    ////////////////////////////////////////////////////////////////////////////
    //
    //  ArgVector
    //
    //  Owns the storage behind a synthetic argv, for the tests that run one of
    //  the page's own examples through the parser.
    //
    ////////////////////////////////////////////////////////////////////////////

    class ArgVector
    {
    public:
        explicit ArgVector (const std::vector<std::string> & args)
        {
            m_storage = args;

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




    static CommandLineParser::FileExistsFn NoProbe()
    {
        return [] (const std::string &) { return false; };
    }




    static std::wstring Widen (const std::string & text)
    {
        return std::wstring (text.begin(), text.end());
    }

    TEST_METHOD (HelpText_CarriesAWorkedExampleOfTheWholeLoop_NotOnlyAFlagList)
    {
        std::string               help  = DiskCommandRunner::BuildHelpText();
        std::vector<std::string>  lines = ExampleCommandLines (help);

        //  Six commands: make the disk, assemble, place the program, place
        //  the greeting, set the greeting, launch. The count is asserted
        //  first because every assertion below iterates, and a loop over
        //  nothing passes.
        //
        //  THE FIRST STEP WAS MISSING FOR A LONG TIME. Every other line
        //  wrote to mydisk.dsk and nothing made it, so a reader following
        //  the example from an empty directory failed at step two.
        Assert::AreEqual (size_t (6), lines.size(), L"the example runs the loop end to end");

        Assert::AreEqual (std::string ("  CassoCli disk create mydisk.dsk --bootable"),
                          lines[0], L"make the disk");

        //  No shape flag: the assembled bytes are what a bare invocation writes
        //  now, so the example that used to write `--raw` writes nothing.
        //  The output name is ATTACHED, which is as65's grammar and now this
        //  mode's only form.
        //  The first step names its dialect, because assembling does now:
        //  a bare source file is no longer an invocation this tool accepts.
        Assert::AreEqual (std::string ("  CassoCli as65 prog.a65 -oprog.bin"),
                          lines[1], L"assemble");
        Assert::AreEqual (std::string ("  CassoCli disk put mydisk.dsk prog.bin"
                                       " --as PROG --type B --load $6000"),
                          lines[2], L"place the program");
        Assert::AreEqual (std::string ("  CassoCli disk put mydisk.dsk greet.bas"
                                       " --as STARTUP --basic"),
                          lines[3], L"place the greeting");
        Assert::AreEqual (std::string ("  CassoCli disk boot mydisk.dsk STARTUP"),
                          lines[4], L"set the greeting");
        //  The launch line names the MACHINE as well as the disk: a reader
        //  following the loop should land on the //e the rest of it assumes.
        Assert::AreEqual (std::string ("  Casso.exe --machine Apple2e --disk1 mydisk.dsk"),
                          lines[5], L"launch");
    }

    TEST_METHOD (HelpText_EveryOptionTheExampleTypes_IsDescribedElsewhereInTheSameHelp)
    {
        std::string               help    = DiskCommandRunner::BuildHelpText();
        std::string               rest    = HelpWithoutExampleCommands (help);
        std::vector<std::string>  options = OptionsUsedByExample (help);
        std::vector<std::string>  expected { "--bootable", "-o", "--as", "--type",
                                             "--load", "--basic", "--machine", "--disk1" };

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

    TEST_METHOD (HelpText_EveryCommandTheDiskGrammarAccepts_AppearsInTheHelp)
    {
        std::string  help     = DiskCommandRunner::BuildHelpText();
        auto         commands = CommandLineParser::GetAllDiskCommands();

        //  A sweep of the parser's own table rather than a list retyped here:
        //  a command added to the grammar and left out of the help is a capability
        //  the user cannot find, and only this direction notices.
        Assert::AreEqual (size_t (19), commands.size(), L"nine commands and ten aliases");

        for (const auto & command : commands)
        {
            Assert::IsTrue (ContainsAsWholeToken (help, command.name),
                            (L"undocumented command: " + Widen (command.name)).c_str());
        }
    }

    //  Whether a status text defines a given number, structurally: a line that
    //  opens on it.
    static bool DefinesStatus (const std::string & text, char digit)
    {
        std::string  marker = std::string ("    ") + digit + "  ";

        return text.find (marker) == 0 || text.find ("\n" + marker) != std::string::npos;
    }

    //
    //  EACH MODE STATES ITS OWN STATUSES BECAUSE THE MODES DISAGREE.
    //
    //  One combined block used to stand at the top of the page claiming to
    //  describe all three, and it described none of them: an assembly error is
    //  2 under the assembler and 1 under `run`, and status 1 means "written
    //  anyway" in one mode and "nothing ran" in another.
    //
    //  This asserts the disagreement rather than the sentences. The three texts
    //  used to be pinned phrase by phrase, which failed the moment any of them
    //  was reworded and would have passed happily if two modes had silently
    //  converged on one meaning.
    //
    TEST_METHOD (EachMode_DefinesItsOwnStatuses_AndTheyAreNotTheSameText)
    {
        std::string  assemble = CommandLineParser::kAssembleExitStatusHelpText;
        std::string  run      = CommandLineParser::kRunExitStatusHelpText;
        std::string  disk     = DiskCommandRunner::kExitStatusHelpText;

        Assert::IsFalse (assemble.empty(), L"the assembler states its own");
        Assert::IsFalse (run.empty(),      L"so does run");
        Assert::IsFalse (disk.empty(),     L"so does disk");

        //  Every mode has to say what 0, 1 and 2 mean, because every mode can
        //  return them.
        for (char digit : { '0', '1', '2' })
        {
            Assert::IsTrue (DefinesStatus (assemble, digit), L"the assembler omits a status");
            Assert::IsTrue (DefinesStatus (run,      digit), L"run omits a status");
            Assert::IsTrue (DefinesStatus (disk,     digit), L"disk omits a status");
        }

        //  And where they differ is the point: disk has no 3 to describe, and
        //  the other two do.
        Assert::IsFalse (DefinesStatus (disk, '3'), L"disk has no status 3");
        Assert::IsTrue  (DefinesStatus (run,  '3'), L"run does");

        Assert::IsTrue (assemble != run,  L"two modes sharing one text is the bug this replaced");
        Assert::IsTrue (run      != disk, L"and so is any other pairing");
        Assert::IsTrue (assemble != disk);
    }

    //  The page is built in one order: the commands, then each command in
    //  detail, then the statuses, then the loop that no single command shows.
    //  Landmarks rather than sentences, so rewording any block leaves this
    //  alone and reordering them does not.
    TEST_METHOD (ThePage_RunsCommandsThenDetailThenStatusesThenTheLoop)
    {
        std::string  help     = DiskCommandRunner::BuildHelpText();
        auto         page     = DiskCommandRunner::GetCommandHelp();
        size_t       contents = help.find ("Disk commands:");
        size_t       detail   = help.find (DiskCommandRunner::ApplyPrefixes (page[0].grammar, '-'));
        size_t       statuses = help.find ("Exit codes:");
        size_t       loop     = help.find (CommandLineHelp::kExampleHeading);

        Assert::IsTrue (contents != std::string::npos, L"the contents list is there");
        Assert::IsTrue (detail   != std::string::npos, L"so is the first command's grammar");
        Assert::IsTrue (statuses != std::string::npos, L"so are the statuses");
        Assert::IsTrue (loop     != std::string::npos, L"so is the loop");

        Assert::IsTrue (contents < detail,   L"the contents list comes first");
        Assert::IsTrue (detail   < statuses, L"the commands come before the statuses");
        Assert::IsTrue (statuses < loop,     L"and the loop closes the page");
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

    //  Every alias of a command leads the line that describes it, so a reader
    //  scanning the left margin for the word they already have in mind finds it
    //  there rather than in an "also written" clause at the end of a sentence
    //  about a word they do not use.
    TEST_METHOD (HelpText_LeadsEachCommandLineWithEveryAlias_NotWithAnAlsoSpelledFootnote)
    {
        std::string   help    = DiskCommandRunner::BuildHelpText();
        //  THE PLAIN NAME LEADS AND THE ALIASES FOLLOW IT. Both orders put
        //  every spelling on the line, which is what this test is for; what
        //  decided it is that the block below each entry writes its grammar as
        //  `CassoCli disk list <image>`, and a contents line that opened on
        //  `cat` sent a reader looking for a `cat` heading that is not there.
        const char *  leads[] =
        {
            "  list | cat | catalog | dir | ls   ",
            "  get | read                        ",
            "  put | write                       ",
            "  delete | del | rm                 ",
            "  boot                              ",
        };

        for (const char * lead : leads)
        {
            Assert::IsTrue (help.find (lead) != std::string::npos,
                            (L"command line missing or misaligned: " + Widen (lead)).c_str());
        }

        Assert::IsTrue (help.find ("Also written") == std::string::npos,
                        L"and no alias is left trailing in a footnote");
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  THE PAGE IS CHECKED AGAINST THE GRAMMAR, NOT AGAINST ITS OWN PROSE.
    //
    //  Seven tests here used to quote sentences: "Write a host file to the
    //  disk", "defaults to the host file's own name", what boot refuses. Every
    //  one of them failed the moment the wording improved, and not one of them
    //  would have failed if the sentence had stayed word-perfect while the
    //  command underneath it changed. They asserted that nobody had edited the
    //  file.
    //
    //  What is worth pinning is that the page and the grammar agree: that
    //  every command has a grammar line and a worked example, that the example
    //  PARSES, that it names the command whose block it sits in, and that every
    //  option it types is documented right there rather than three screens
    //  away. The facts the prose used to carry are asserted where they belong,
    //  against the runner, in DiskCommandRunnerTests.
    //
    ////////////////////////////////////////////////////////////////////////////

    //  Every option token in a line of help or of example, as typed.
    static std::vector<std::string> OptionsIn (const std::string & text)
    {
        std::vector<std::string>  found;
        size_t                    at = 0;

        while ((at = text.find ("--", at)) != std::string::npos)
        {
            size_t  end = at + 2;

            while (end < text.size() && (isalnum ((unsigned char) text[end]) || text[end] == '-'))
            {
                end++;
            }

            if (end > at + 2)
            {
                found.push_back (text.substr (at, end - at));
            }

            at = end;
        }

        return found;
    }

    TEST_METHOD (EveryCommandBlock_CarriesAGrammarLineAndAWorkedExample)
    {
        auto  page = DiskCommandRunner::GetCommandHelp();

        Assert::IsTrue (page.size() >= 8, L"the page describes every command");

        for (const auto & entry : page)
        {
            std::wstring  which = Widen (entry.forms);

            Assert::IsTrue (entry.grammar != nullptr && *entry.grammar != '\0',
                            (L"no grammar line: " + which).c_str());
            Assert::IsTrue (entry.example != nullptr && *entry.example != '\0',
                            (L"no example: " + which).c_str());
            Assert::IsTrue (std::string (entry.grammar).find ("CassoCli disk ") == 0,
                            (L"the grammar does not open on the command: " + which).c_str());
        }
    }

    //  THE EXAMPLES ARE RUN THROUGH THE PARSER. A page may print anything; an
    //  example that the tool would refuse is worse than none, because the
    //  reader types it verbatim.
    TEST_METHOD (EveryExampleOnThePage_ParsesCleanly_AndNamesItsOwnCommand)
    {
        for (const auto & entry : DiskCommandRunner::GetCommandHelp())
        {
            std::string               example = DiskCommandRunner::ApplyPrefixes (entry.example, '-');
            std::vector<std::string>  words;
            std::istringstream        reader (example);
            std::string               word;

            while (reader >> word)
            {
                words.push_back (word);
            }

            ArgVector           args (words);
            CommandLineOptions  opts  = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());
            std::wstring        which = Widen (example);

            Assert::IsTrue (opts.refusalMessage.empty(),
                            (L"refused: " + which + L" -- " + Widen (opts.refusalMessage)).c_str());
            Assert::IsTrue (opts.unrecognizedFlag.empty(),
                            (L"unknown option in: " + which).c_str());
            Assert::IsTrue (opts.subcommand == CommandLineOptions::Subcommand::Disk,
                            (L"not a disk command: " + which).c_str());

            //  The example belongs to the block it is printed under.
            std::string  forms = entry.forms;
            Assert::IsTrue (forms.find (opts.disk.commandWord) != std::string::npos,
                            (L"example runs `" + Widen (opts.disk.commandWord)
                             + L"` under " + Widen (entry.forms)).c_str());
        }
    }

    //  An example that types an option the block does not document sends the
    //  reader looking for it under some other command.
    TEST_METHOD (EveryOptionAnExampleTypes_IsDocumentedInItsOwnBlock)
    {
        for (const auto & entry : DiskCommandRunner::GetCommandHelp())
        {
            std::string  documented = (entry.options == nullptr) ? "" : entry.options;

            documented = DiskCommandRunner::ApplyPrefixes (documented, '-');

            for (const std::string & option : OptionsIn (
                     DiskCommandRunner::ApplyPrefixes (entry.example, '-')))
            {
                Assert::IsTrue (documented.find (option) != std::string::npos,
                                (L"the example types " + Widen (option) + L" and "
                                 + Widen (entry.forms) + L" does not document it").c_str());
            }
        }
    }

    //  And a block may only document options the grammar actually accepts.
    TEST_METHOD (EveryOptionABlockDocuments_IsOneTheGrammarAccepts)
    {
        std::set<std::string>  accepted;

        for (const char * option : CommandLineParser::GetDiskOptionNames())
        {
            accepted.insert (std::string ("--") + option);
        }

        for (const auto & entry : DiskCommandRunner::GetCommandHelp())
        {
            std::string  block = (entry.options == nullptr) ? "" : entry.options;

            for (const std::string & option : OptionsIn (
                     DiskCommandRunner::ApplyPrefixes (block, '-')))
            {
                Assert::IsTrue (accepted.count (option) == 1,
                                (L"the page invents " + Widen (option) + L" under "
                                 + Widen (entry.forms)).c_str());
            }
        }
    }

    //  The grammar line and the option list under it describe one command, so
    //  an option in one and not the other is a page arguing with itself.
    TEST_METHOD (EveryOptionInAGrammarLine_IsListedUnderTheSameCommand)
    {
        for (const auto & entry : DiskCommandRunner::GetCommandHelp())
        {
            std::string  documented = (entry.options == nullptr) ? "" : entry.options;

            documented = DiskCommandRunner::ApplyPrefixes (documented, '-');

            for (const std::string & option : OptionsIn (
                     DiskCommandRunner::ApplyPrefixes (entry.grammar, '-')))
            {
                Assert::IsTrue (documented.find (option) != std::string::npos,
                                (L"the grammar of " + Widen (entry.forms) + L" takes "
                                 + Widen (option) + L", and the block does not list it").c_str());
            }
        }
    }

    //  Every accepted spelling of a command reaches the same command, which is
    //  what the aliases on each heading promise.
    //  The operands a grammar line shows as REQUIRED, in the order it shows
    //  them: a <token> outside any [optional] group, and not the value of an
    //  option that precedes it.
    static std::vector<std::string> RequiredOperandsIn (const std::string & grammar)
    {
        std::vector<std::string>  required;
        std::istringstream        reader (grammar);
        std::string               word;
        int                       depth    = 0;
        bool                      afterOpt = false;

        while (reader >> word)
        {
            std::string  bare;
            bool         wasOption    = afterOpt;
            //  THE DEPTH THIS WORD SITS AT, not the depth it leaves behind.
            //  `<file>]` closes its group, so measuring afterwards reads it as
            //  a required operand when it is the value of an optional flag.
            int          depthAtStart = depth;

            for (char c : word)
            {
                if      (c == '[') { depth++; }
                else if (c == ']') { depth--; }
                else               { bare += c; }
            }

            //  `%L` and `%S` stand in for the reader's prefix in these tables,
            //  so an option is spelled `%Ltrack` here rather than `--track`.
            afterOpt = !bare.empty() && (bare[0] == '-' || bare[0] == '/' || bare[0] == '%');

            if (depthAtStart == 0 && !wasOption && bare.size() > 2
                && bare.front() == '<' && bare.back() == '>')
            {
                required.push_back (bare);
            }
        }

        return required;
    }

    //
    //  WHAT THE GRAMMAR SHOWS AND WHAT THE RUNNER DEMANDS ARE ONE LIST.
    //
    //  MissingParameters names the operands by hand, command by command, and
    //  the grammar line above each block names them again. Two lists of the
    //  same thing drift: a command whose grammar gains an operand and whose
    //  check does not would print usage asking for something it never
    //  complains about, and the reverse would complain about something the
    //  usage never mentions.
    //
    TEST_METHOD (WhatEachCommandDemands_IsWhatItsGrammarShows)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);

        for (const auto & entry : DiskCommandRunner::GetCommandHelp())
        {
            CommandLineOptions        options;
            std::vector<std::string>  fromGrammar = RequiredOperandsIn (entry.grammar);
            std::vector<std::string>  fromRunner;

            options.subcommand   = CommandLineOptions::Subcommand::Disk;
            options.disk.command = entry.command;

            //  Nothing supplied, so everything required is missing.
            fromRunner = runner.MissingParameters (options);

            Assert::AreEqual (fromGrammar.size(), fromRunner.size(),
                              (L"operand count disagrees for " + Widen (entry.forms)).c_str());

            for (size_t i = 0; i < fromGrammar.size(); i++)
            {
                Assert::AreEqual (fromGrammar[i], fromRunner[i],
                                  (L"operand " + std::to_wstring (i) + L" disagrees for "
                                   + Widen (entry.forms)).c_str());
            }
        }
    }

    //  And the reporting names every one of them, not the first noticed.
    //  `disk get` with nothing at all used to complain that <name> was
    //  missing and never mention <image>, which comes first.
    TEST_METHOD (ACommandMissingTwoOperands_ReportsBoth)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options;
        DiskCommandResult   result;

        options.subcommand   = CommandLineOptions::Subcommand::Disk;
        options.disk.command = CommandLineOptions::DiskOptions::Command::Get;

        result = runner.Run (options);

        Assert::IsTrue (result.diagnostics.find ("<image>") != std::string::npos,
                        L"the first operand is named");
        Assert::IsTrue (result.diagnostics.find ("<name>") != std::string::npos,
                        L"and so is the second");
        Assert::IsTrue (result.diagnostics.find ("parameters") != std::string::npos,
                        L"and the sentence agrees with itself about how many");
    }

    TEST_METHOD (EverySpellingOnACommandHeading_ReachesThatCommand)
    {
        for (const auto & entry : DiskCommandRunner::GetCommandHelp())
        {
            std::string         forms = entry.forms;
            std::string         word;
            std::istringstream  reader (forms);
            std::string         first;

            while (std::getline (reader, word, '|'))
            {
                size_t  from = word.find_first_not_of (" ");
                size_t  to   = word.find_last_not_of (" ");

                if (from == std::string::npos)
                {
                    continue;
                }

                word = word.substr (from, to - from + 1);

                ArgVector           args ({ "CassoCli", "disk", word, "img.dsk" });
                CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.disk.command != CommandLineOptions::DiskOptions::Command::None,
                                (L"the heading offers `" + Widen (word)
                                 + L"` and the grammar does not take it").c_str());

                if (first.empty())
                {
                    first = opts.disk.commandWord;
                }
            }
        }
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
        //  EVERY OPTION THE PAGE DOCUMENTS, which is what the name promises.
        //  The list used to name six, one of them `addr` -- an option that had
        //  been renamed to `load` and survived here only because a stale
        //  sentence of prose still said it. A list that covers the table is
        //  what catches the next rename; a list of six covers whichever six
        //  were current when it was written.
        std::string   slashHelp = DiskCommandRunner::BuildHelpText ('/');
        std::string   dashHelp  = DiskCommandRunner::BuildHelpText ('-');
        const char *  options[] = { "out", "as", "type", "load", "text", "basic",
                                    "format", "volume", "bootable", "boot", "exec",
                                    "track", "sector" };

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
    //  WHAT THE COLUMNS ARE is asserted against the LISTING, in
    //  DiskCommandRunnerTests, where a rendered ProDOS row is compared whole.
    //  This test used to also require the help to contain the words "eof= and
    //  aux=", which pinned a sentence rather than a behavior and kept a
    //  paragraph on the page that a reader gets from running the command.
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

    //  `disk --help` printed "unknown disk command" on the error stream and exited
    //  2 -- a refusal, for a question the tool knows the answer to.
    TEST_METHOD (HelpCommand_PrintsTheDiskHelpOnOutput_AndSucceeds)
    {
        CommandLineOptions  options;
        FakeDiskFileIo      fileIo;
        DiskCommandRunner   runner (fileIo);

        options.disk.command = CommandLineOptions::DiskOptions::Command::Help;

        DiskCommandResult  result = runner.Run (options);

        Assert::AreEqual (0, result.exitStatus, L"asking for help is not a failure");
        Assert::AreEqual (std::string(), result.diagnostics, L"and it is not a complaint");
        Assert::AreEqual (DiskCommandRunner::BuildHelpText ('-'), result.output,
                          L"the disk section of the help, on the output stream");
    }

    TEST_METHOD (HelpCommand_SpellsItselfWithThePrefixTheReaderTyped)
    {
        CommandLineOptions  options;
        FakeDiskFileIo      fileIo;
        DiskCommandRunner   runner (fileIo);

        options.disk.command  = CommandLineOptions::DiskOptions::Command::Help;
        options.flagPrefix = '/';

        DiskCommandResult  result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::BuildHelpText ('/'), result.output);
    }

    //  EVERY COMMAND THE GRAMMAR TAKES IS ON THE PAGE A REFUSAL PRINTS.
    //
    //  The refusal itself used to carry the list, and named the five original
    //  commands long after eight aliases were added, so a user who mistyped
    //  `catalog` was told to try five words that did not include it. It names
    //  only what the user typed now, and the page above it carries the commands.
    //  So the claim moves here, onto the page, and is still swept from the
    //  grammar's own table rather than from a list written out again.
    TEST_METHOD (EveryCommandTheGrammarTakes_IsOnThePageARefusalPrints)
    {
        CommandLineOptions  options;
        FakeDiskFileIo      fileIo;
        DiskCommandRunner   runner (fileIo);
        DiskCommandResult   result = runner.Run (options);
        std::string         page   = DiskCommandRunner::BuildHelpText();

        Assert::AreEqual (2, result.exitStatus, L"a word that names no command produces nothing");
        Assert::IsTrue (result.badCommandLine, L"and the edge is told to print the page");

        for (const auto & command : CommandLineParser::GetAllDiskCommands())
        {
            Assert::IsTrue (ContainsAsWholeToken (page, command.name),
                            (L"the page does not offer: " + Widen (command.name)).c_str());
        }
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

        Assert::IsTrue (disk.find ("its own header indicating") != std::string::npos,
                        L"the disk page explains the doubled header");
        Assert::IsTrue (general.find ("its own header indicating") == std::string::npos,
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
