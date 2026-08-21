#include "Pch.h"

#include "CommandLineHelp.h"
#include "CommandLineParser.h"

#include "CppUnitTest.h"




using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace CommandLineTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ArgVector
    //
    //  Owns the storage behind a synthetic argv. The parser takes `char *[]`
    //  the way main does, so the strings must be mutable and outlive the call.
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





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  NoProbe
    //
    //  The "nothing exists" file predicate, for the many cases where
    //  auto-extension is irrelevant.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static CommandLineParser::FileExistsFn NoProbe()
    {
        return [] (const std::string &) { return false; };
    }




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Widen
    //
    //  A message for the assert framework, which speaks wide strings while
    //  every argument under test is narrow. ASCII only, which is all a flag
    //  form ever is.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static std::wstring Widen (const std::string & text)
    {
        return std::wstring (text.begin(), text.end());
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ProbeFor
    //
    //  A predicate that reports exactly one path as existing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static CommandLineParser::FileExistsFn ProbeFor (const std::string & existing)
    {
        return [existing] (const std::string & path) { return path == existing; };
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DispatchTests
    //
    //  Which grammar an argv selects. The fallback is the load-bearing case:
    //  an unrecognized first argument is a SOURCE FILENAME, not an error, which
    //  is how as65 was invoked and what every existing build script relies on.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DispatchTests)
    {
    public:
        TEST_METHOD (NoArguments_RequestsHelpWithNoSubcommand)
        {
            ArgVector           args = { "CassoCli" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.showHelp);
            Assert::IsTrue (opts.subcommand == CommandLineOptions::Subcommand::None);
        }

        TEST_METHOD (LongHelp_SelectsHelp)
        {
            ArgVector           args = { "CassoCli", "--help" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.showHelp);
            Assert::IsTrue (opts.subcommand == CommandLineOptions::Subcommand::Help);
        }

        TEST_METHOD (SlashHelp_SelectsHelpAndRemembersPrefix)
        {
            ArgVector           args = { "CassoCli", "/?" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.showHelp);
            Assert::AreEqual ('/', opts.flagPrefix, L"the prefix the user typed must come back in usage text");
        }

        //  as65's DIAGNOSTICS section: "Help message if only parameter is a
        //  question mark". The prefixed forms were already accepted; the
        //  bare one was read as a source filename, so `CassoCli ?` went looking
        //  for a file called `?` and exited saying it could not open one.
        //
        //  IT OPENS THE ASSEMBLER'S PAGE rather than the general one, and is the
        //  only way there. The request belongs to as65, and assembling is as65
        //  mode, so it lands on the page describing the grammar it comes from.
        TEST_METHOD (BareQuestionMarkAlone_OpensTheAssemblersPage)
        {
            ArgVector           args = { "CassoCli", "?" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.showHelp);
            Assert::IsTrue (opts.subcommand == CommandLineOptions::Subcommand::Help);
            Assert::IsTrue (opts.helpPage == CommandLineOptions::HelpPage::Assemble,
                            L"a lone ? is as65's own usage request");
            Assert::IsTrue (opts.inputFile.empty(), L"and it is not a source file to assemble");
        }

        //  "ONLY parameter" is as65's condition and is kept literally. A second
        //  argument means the question mark is somebody's operand -- and on a
        //  host that allows the character, somebody's filename.
        TEST_METHOD (QuestionMarkWithAnotherArgument_IsNotAHelpRequest)
        {
            ArgVector           args = { "CassoCli", "?", "-q" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsFalse (opts.showHelp, L"two parameters is not the documented case");
            Assert::AreEqual (std::string ("?"), opts.unrecognizedArgument,
                              L"and it names no subcommand, so it is reported rather than assumed to be source");
        }

        TEST_METHOD (Version_SelectsVersion)
        {
            ArgVector           args = { "CassoCli", "--version" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.showVersion);
            Assert::IsTrue (opts.subcommand == CommandLineOptions::Subcommand::Version);
        }

        TEST_METHOD (BareFilename_FallsBackToAs65)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.subcommand == CommandLineOptions::Subcommand::As65);
            Assert::AreEqual (std::string ("demo.a65"), opts.inputFile);
        }

        TEST_METHOD (RunSubcommand_SelectsRun)
        {
            ArgVector           args = { "CassoCli", "run", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.subcommand == CommandLineOptions::Subcommand::Run);
            Assert::AreEqual (std::string ("demo.a65"), opts.inputFile);
        }

        //  Sweeps the table rather than a hand-picked sample, so a subcommand
        //  added to it is covered without anyone editing this test.
        TEST_METHOD (EverySubcommandInTheTable_ParsesToItsToken)
        {
            for (const CommandLineParser::SubcommandName & entry : CommandLineParser::GetAllSubcommands())
            {
                ArgVector           args = { "CassoCli", entry.name };
                CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.subcommand == entry.token,
                                L"a table row must parse to the token it names");
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  HelpRoutingTests
    //
    //  Which PAGE each way of asking lands on.
    //
    //  THE HELP IS TIERED, AND THE ROUTE IS THE BEHAVIOR. One page describing
    //  three grammars ran to four screens: every flag of the assembler, of
    //  `run` and of `disk`, three blocks of exit statuses, and the worked loop
    //  at the bottom where a reader who had scrolled past the flags never
    //  arrived. The general page now names the modes and each mode's flags wait
    //  behind a request for that mode -- so a route that stops working strands
    //  a page, and nothing about the text of that page would show it.
    //
    //  BOTH PREFIXES ARE SWEPT EVERYWHERE, because a page writes itself with
    //  the prefix the reader typed: a route accepted in only one of them offers
    //  the other back in text and then refuses it on the next command line.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (HelpRoutingTests)
    {
    public:
        //  Every form of "help" that both grammars beneath the top level
        //  answer to. `-h` is among them and is safe there: the page height it
        //  collides with lives in the assembler's own flag walk, which no
        //  argument of `run` or `disk` ever reaches.
        static std::vector<std::string> Forms()
        {
            return { "--help", "-help", "-?", "-h", "/help", "/?", "/h" };
        }

        static CommandLineOptions ParseTyped (std::initializer_list<const char *> typed)
        {
            ArgVector  args = typed;

            return CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());
        }

        TEST_METHOD (EverySpellingOfHelpAtTheTopLevel_OpensTheGeneralPage)
        {
            for (const std::string & form : Forms())
            {
                CommandLineOptions  opts = ParseTyped ({ "CassoCli", form.c_str() });

                Assert::IsTrue (opts.showHelp,
                    (L"not read as a help request: " + Widen (form)).c_str());
                Assert::IsTrue (opts.helpPage == CommandLineOptions::HelpPage::General,
                    (L"did not open the general page: " + Widen (form)).c_str());
            }
        }

        //  Typing the tool's name and nothing else is somebody who does not yet
        //  know what it does, which is exactly who the general page is for.
        TEST_METHOD (NoArgumentsAtAll_OpensTheGeneralPage)
        {
            CommandLineOptions  opts = ParseTyped ({ "CassoCli" });

            Assert::IsTrue (opts.showHelp);
            Assert::IsTrue (opts.helpPage == CommandLineOptions::HelpPage::General);
        }

        TEST_METHOD (ASlashSpelledRequest_KeepsTheSlashForThePageItOpens)
        {
            const char *  kSlashed[] = { "/help", "/?", "/h" };

            for (const char * form : kSlashed)
            {
                CommandLineOptions  opts = ParseTyped ({ "CassoCli", form });

                Assert::AreEqual ('/', opts.flagPrefix,
                    (L"the prefix was not remembered: " + Widen (form)).c_str());
            }
        }

        //  A LONE `?` IS THE ONLY ROUTE TO THE ASSEMBLER'S PAGE, so every other
        //  form has to land somewhere else even when a source file is
        //  standing on the command line beside it.
        TEST_METHOD (HelpBesideASourceFile_StillOpensTheGeneralPage)
        {
            CommandLineOptions  opts = ParseTyped ({ "CassoCli", "as65", "prog.a65", "--help" });

            Assert::IsTrue (opts.showHelp);
            Assert::IsTrue (opts.helpPage == CommandLineOptions::HelpPage::General,
                            L"only a lone ? reaches the assembler's page");
        }

        //  `run --help` was an option this grammar does not have: a diagnostic,
        //  a refusal, and exit 2 -- answering a question the tool knows the
        //  answer to by complaining about being asked.
        TEST_METHOD (RunTakesAHelpRequestInEverySpelling_AndOpensTheRunPage)
        {
            for (const std::string & form : Forms())
            {
                CommandLineOptions  opts = ParseTyped ({ "CassoCli", "run", form.c_str() });

                Assert::IsTrue (opts.showHelp,
                    (L"not read as a help request: " + Widen (form)).c_str());
                Assert::IsTrue (opts.helpPage == CommandLineOptions::HelpPage::Run,
                    (L"did not open the run page: " + Widen (form)).c_str());
                Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean,
                    (L"asking for help is not a mistake: " + Widen (form)).c_str());
            }
        }

        //  A reader who has typed half a command and wants the grammar back
        //  adds the request to the end of what they already have.
        TEST_METHOD (RunHelpAfterTheInputFile_IsStillAHelpRequest)
        {
            CommandLineOptions  opts = ParseTyped ({ "CassoCli", "run", "prog.a65", "--help" });

            Assert::IsTrue (opts.helpPage == CommandLineOptions::HelpPage::Run);
        }

        TEST_METHOD (RunHelpWithASlash_RecordsTheSlashPrefix)
        {
            CommandLineOptions  opts = ParseTyped ({ "CassoCli", "run", "/?" });

            Assert::AreEqual ('/', opts.flagPrefix);
        }

        //  THE DISK PAGE IS ANSWERED BY THE DISK RUNNER, not by the usage
        //  printer, which is what lets it be assembled and tested beside the
        //  code it describes. So a disk help request must NOT set showHelp:
        //  doing so would have the executable print a general page over it and
        //  never dispatch the verb.
        TEST_METHOD (DiskTakesAHelpRequestInEverySpelling_AndItStaysAVerbOfTheDiskGrammar)
        {
            for (const std::string & form : Forms())
            {
                CommandLineOptions  opts = ParseTyped ({ "CassoCli", "disk", form.c_str() });

                Assert::IsTrue (opts.subcommand == CommandLineOptions::Subcommand::Disk,
                    (L"left the disk grammar: " + Widen (form)).c_str());
                Assert::IsTrue (opts.disk.verb == CommandLineOptions::DiskOptions::Verb::Help,
                    (L"not read as a help request: " + Widen (form)).c_str());
                Assert::IsFalse (opts.showHelp,
                    (L"the general page would be printed over it: " + Widen (form)).c_str());
            }
        }

        //
        //  THE GENERAL PAGE IS ONE SCREEN AND HAS TO STAY ONE. What it replaced
        //  was 180 lines, and it grew there a section at a time, each addition
        //  reasonable on its own. The banner is counted with it because a page
        //  is what the reader sees, not what the builder returns.
        //
        TEST_METHOD (GeneralPage_FitsOnOneScreen_SoItCannotGrowBackIntoFourPages)
        {
            const char    kBanner[]   = "CassoCli - 6502 Assembler and Emulator  v0.0.0\n"
                                        "Copyright (c) 2025-2026 by Robert Elmer\n";
            const size_t  kScreenful  = 30;
            const char    kPrefixes[] = { '-', '/' };

            for (char prefix : kPrefixes)
            {
                std::string  page  = CommandLineHelp::BuildGeneralHelp (kBanner, prefix);
                size_t       lines = (size_t) std::count (page.begin(), page.end(), '\n');

                Assert::IsTrue (lines < kScreenful,
                    (L"the general page has grown to " + std::to_wstring (lines) +
                     L" lines").c_str());
            }
        }

        //  The page is a table of contents, so the contents have to be on it --
        //  in the prefix the reader typed, since that is the one they will type
        //  next.
        TEST_METHOD (GeneralPage_OffersTheRouteToEveryModesOwnPage_InThePrefixTheReaderTyped)
        {
            std::string  dashed  = CommandLineHelp::BuildGeneralHelp ("banner\n", '-');
            std::string  slashed = CommandLineHelp::BuildGeneralHelp ("banner\n", '/');

            Assert::IsTrue (dashed.find ("CassoCli ?")           != std::string::npos);
            Assert::IsTrue (dashed.find ("CassoCli run --help")  != std::string::npos);
            Assert::IsTrue (dashed.find ("CassoCli disk --help") != std::string::npos);
            Assert::IsTrue (dashed.find ("CassoCli --version")   != std::string::npos);

            //  `?` carries no prefix in either page: it is as65's own request
            //  and as65 writes it bare.
            Assert::IsTrue (slashed.find ("CassoCli ?")          != std::string::npos);
            Assert::IsTrue (slashed.find ("CassoCli run /help")  != std::string::npos);
            Assert::IsTrue (slashed.find ("CassoCli disk /help") != std::string::npos);
            Assert::IsTrue (slashed.find ("CassoCli /version")   != std::string::npos);
            Assert::IsTrue (slashed.find ("--help")              == std::string::npos,
                            L"and never the form the reader did not type");
        }

        //  ONE DESCRIPTION OF ONE INVOCATION. A mode's page opens with the same
        //  usage line the general page lists it by, from the same function, so
        //  the two cannot come to describe different grammars.
        TEST_METHOD (EveryModesUsageLine_IsTheOneTheGeneralPageLists)
        {
            std::string  page = CommandLineHelp::BuildGeneralHelp ("banner\n", '-');

            for (const CommandLineParser::SubcommandName & entry : CommandLineParser::GetAllSubcommands())
            {
                std::string  line = CommandLineHelp::UsageLineFor (entry.token);

                Assert::IsTrue (page.find (line) != std::string::npos,
                    (L"the general page does not list: " + Widen (entry.name)).c_str());
            }

            Assert::IsTrue (page.find (CommandLineHelp::UsageLineFor (
                                CommandLineOptions::Subcommand::As65)) != std::string::npos,
                            L"nor the assembler, which is the fallback rather than a named mode");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  As65FlagTests
    //
    //  The historical grammar: concatenation, both prefixes, and values that
    //  may be glued to their flag or separated from it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (As65FlagTests)
    {
    public:
        TEST_METHOD (ConcatenatedFlags_SplitIntoIndividualFlags)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-tlfile.lst" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.symbolTable,     L"-t must be seen inside -tlfile.lst");
            Assert::IsTrue (opts.generateListing, L"-l must be seen inside -tlfile.lst");
            Assert::AreEqual (std::string ("file.lst"), opts.listingFile);
        }

        TEST_METHOD (ListingFlagAlone_GoesToStdout)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-l" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.generateListing);
            Assert::IsTrue (opts.listingToStdout);
            Assert::IsTrue (opts.listingFile.empty());
        }

        //  BOTH BARE FORMS ARE as65's, WHICH IS WHY -l KEEPS ITS ONE. The
        //  manual lists the flag twice -- "-l  Generate pass 2 listing" and
        //  "-l<filename>  Listing file name" -- so a bare -l is a real form
        //  rather than a missing argument, unlike a bare -o.
        //
        //  The SEPARATED form is gone with every other one: `-l` has a string
        //  parameter, so it takes the rest of its own argument and reaches no
        //  further. `-l out.lst` asks for a listing on stdout and leaves
        //  out.lst to be read as the source file.
        TEST_METHOD (ListingFlag_DoesNotTakeASeparatedFilename)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-l", "out.lst" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.generateListing);
            Assert::IsTrue (opts.listingToStdout, L"a bare -l is the stdout form as65 documents");
            Assert::IsTrue (opts.listingFile.empty(), L"and it does not reach the next argument");
        }

        TEST_METHOD (ListingFlag_TakesAnAttachedFilename)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-lout.lst" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("out.lst"), opts.listingFile);
            Assert::IsFalse (opts.listingToStdout);
        }

        TEST_METHOD (SlashPrefix_IsAcceptedAndRemembered)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "/t" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.symbolTable, L"/t must mean what -t means");
        }

        //  -i IS ACCEPTED AND RECORDS NOTHING, which is the whole of what this
        //  grammar owes it.
        //
        //  These two tests used to assert a stored flag. The flag is gone: as65's
        //  -i asks for case-insensitive opcodes with case-sensitive labels, this
        //  assembler does exactly that unconditionally, and a field nothing could
        //  usefully read was an invitation to implement a conditional folding it
        //  does not need. What is left to check is that the flag still parses --
        //  an as65 command line carrying it must not be refused -- and that it
        //  does not swallow the source file.
        //
        //  The behavior itself is pinned in AssemblerTests, against the assembler,
        //  where it can actually be measured.
        TEST_METHOD (IgnoreCaseFlag_IsAccepted_AndChangesNothingAboutTheParse)
        {
            ArgVector           with    = { "CassoCli", "as65", "demo.a65", "-i" };
            ArgVector           without = { "CassoCli", "as65", "demo.a65" };
            CommandLineOptions  a       = CommandLineParser::Parse (with.Count(),    with.Data(),    NoProbe());
            CommandLineOptions  b       = CommandLineParser::Parse (without.Count(), without.Data(), NoProbe());

            Assert::IsTrue (a.parseVerdict == CommandLineOptions::ParseVerdict::Clean,
                            L"an as65 command line carrying -i is not refused");
            Assert::AreEqual (std::string ("demo.a65"), a.inputFile,
                              L"and -i does not consume the source file");
            Assert::IsTrue (a.parseVerdict == b.parseVerdict);
        }

        //  It concatenates like every other valueless flag, so -it is -i -t.
        //  Worth pinning because a flag that records nothing is a flag whose
        //  position in a group nothing else would catch.
        TEST_METHOD (IgnoreCaseFlag_StillConcatenates)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-it" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean);
            Assert::IsTrue (opts.symbolTable, L"the -t after -i is still read");
        }

        TEST_METHOD (FillZeroFlag_SetsZeroFill)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-z" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.fillZero);
            Assert::AreEqual ((int) 0x00, (int) opts.fillByte);
        }

        TEST_METHOD (DefaultFill_IsFF)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual ((int) 0xFF, (int) opts.fillByte);
        }

        TEST_METHOD (PredefineWithValue_IsRecorded)
        {
            ArgVector           args  = { "CassoCli", "as65", "demo.a65", "-dDEBUG=5" };
            CommandLineOptions  opts  = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());
            auto                found = opts.predefinedSymbols.find ("DEBUG");
            bool                isSet = found != opts.predefinedSymbols.end();

            Assert::IsTrue (isSet);
            Assert::AreEqual ((int32_t) 5, found->second);
        }

        //  A glued -dNAME with no value defines the symbol as 1, so `ifdef`
        //  works without the caller inventing a value.
        TEST_METHOD (PredefineWithoutValue_DefaultsToOne)
        {
            ArgVector           args  = { "CassoCli", "as65", "demo.a65", "-dFAST" };
            CommandLineOptions  opts  = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());
            auto                found = opts.predefinedSymbols.find ("FAST");
            bool                isSet = found != opts.predefinedSymbols.end();

            Assert::IsTrue (isSet);
            Assert::AreEqual ((int32_t) 1, found->second);
        }

        //  THE NAME IS GLUED AND THE SEPARATED FORM IS GONE. as65 notates the
        //  flag `-d<name>` and its parameter is a string, which is the case its
        //  concatenation rule singles out. `-d NAME` was this tool's own
        //  invention, propped up by a heuristic that guessed whether the next
        //  argument looked enough like a symbol to eat; as65 needs no such rule
        //  because it never reaches past the argument the flag is in.
        TEST_METHOD (PredefineWithASeparatedName_DefinesDebugAndLeavesTheNameAlone)
        {
            ArgVector           args  = { "CassoCli", "as65", "demo.a65", "-d", "FAST" };
            CommandLineOptions  opts  = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.predefinedSymbols.find ("FAST") == opts.predefinedSymbols.end(),
                            L"-d does not reach the argument after it");
            Assert::IsTrue (opts.predefinedSymbols.find ("DEBUG") != opts.predefinedSymbols.end(),
                            L"the bare flag is the DEBUG default as65 documents");

            //  And FAST, having nowhere left to go, is now said out loud. It
            //  used to be dropped: the caller asked for a symbol, got DEBUG,
            //  and was told the run had worked.
            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                            L"the name that reached no slot is reported, not discarded");
        }

        //  as65: "If no name is specified, DEBUG is defined. The label is
        //  EQUated to be 1." A bare -d used to define nothing at all when it
        //  ended the command line, which is the one place the default was
        //  unambiguously being asked for.
        TEST_METHOD (PredefineWithNoNameAtAll_DefinesDebugAsOne)
        {
            ArgVector           args  = { "CassoCli", "as65", "demo.a65", "-d" };
            CommandLineOptions  opts  = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());
            auto                found = opts.predefinedSymbols.find ("DEBUG");
            bool                isSet = found != opts.predefinedSymbols.end();

            Assert::IsTrue (isSet, L"a bare -d defines DEBUG");
            Assert::AreEqual ((int32_t) 1, found->second);
        }

        //  A bare -d took whatever stood next to it, and what usually stands
        //  next to it is the source file. `-d demo.a65` defined a label called
        //  `demo.a65` and left the run with no input, which then failed saying
        //  no input file was given -- a diagnostic about the argument the flag
        //  had eaten. It reads correctly now for the plain reason rather than
        //  the guessed one: -d never looks past its own argument.
        TEST_METHOD (PredefineBeforeTheSourceFile_LeavesTheSourceFileAlone)
        {
            ArgVector           args  = { "CassoCli", "as65", "-d", "demo.a65" };
            CommandLineOptions  opts  = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());
            bool                named = opts.predefinedSymbols.find ("demo.a65") != opts.predefinedSymbols.end();

            Assert::AreEqual (std::string ("demo.a65"), opts.inputFile, L"the source is still the source");
            Assert::IsFalse (named, L"and is not a symbol name");
            Assert::IsTrue (opts.predefinedSymbols.find ("DEBUG") != opts.predefinedSymbols.end(),
                            L"the bare flag still means DEBUG");
        }

        //  The other thing that follows a bare -d is the next flag, and eating
        //  one costs the argument BEHIND it too: `-d -oout.bin` defined a
        //  label called `-oout.bin`, so no output name was ever read and the
        //  derived one was written instead -- a file the caller did not ask
        //  for, under a name they did not choose, reported as success.
        TEST_METHOD (PredefineBeforeAnotherFlag_LeavesThatFlagAlone)
        {
            ArgVector           args  = { "CassoCli", "as65", "demo.a65", "-d", "-oout.bin" };
            CommandLineOptions  opts  = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("out.bin"), opts.outputFile, L"-o kept its value");
            Assert::IsTrue (opts.predefinedSymbols.find ("DEBUG") != opts.predefinedSymbols.end());
            Assert::IsTrue (opts.predefinedSymbols.find ("-oout.bin") == opts.predefinedSymbols.end(),
                            L"a flag is not a symbol name");
        }

        TEST_METHOD (PageWidthFlag_TakesAttachedValue)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-w133" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (133, opts.pageWidth);
        }

        //  THE SEPARATED FORM IS GONE. as65 notates the flag `-h<lines>` and
        //  glues the value; `-h 60` was added here on the strength of this
        //  tool's own help text, which documented a form the parser did not
        //  read. The help was the thing that was wrong, and it has been
        //  corrected instead.
        //
        //  What is left of `-h 10` is a bare -h, which is refused in its own
        //  right, so the height is never reached and the `10` is never read as
        //  one.
        TEST_METHOD (PageHeightFlag_DoesNotTakeASeparatedValue)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-h", "10" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (0, opts.pageHeight, L"the number after -h is the next argument, not the height");
            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                            L"and the command line is refused rather than assembled without it");
        }

        //  A BARE -h IS AN ERROR, AND THE MANUAL'S SILENCE IS THE EVIDENCE.
        //  as65 documents the bare form of -w -- "If the -w option is given
        //  without a number following it, then the listing will be 133 columns
        //  wide" -- and documents no bare form of -h on the same page. It used
        //  to do nothing at all: the height kept whatever it had, the assembly
        //  ran, and the status was 0.
        //
        //  IT IS NOT FIRST IN THIS ARGV ON PURPOSE. A leading `-h` is the
        //  top-level help request and never reaches the assembler's flag walk,
        //  so a test that put it there would pass without touching this rule.
        TEST_METHOD (PageHeightFlagAlone_IsRefused_BecauseAs65DocumentsNoBareForm)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-h" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                            L"a parameter-taking option given no parameter is refused, not shrugged at");
            Assert::IsFalse (opts.showHelp,
                             L"and it is still the height flag, not the help request the first position writes");
        }

        //  The bare form ends the group, so nothing after it is read either --
        //  which is what stops `-ht` from quietly delivering a symbol table off
        //  a command line that was refused.
        TEST_METHOD (PageHeightFlagAlone_StopsTheRestOfTheGroup)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-ht" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue  (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused);
            Assert::IsFalse (opts.symbolTable, L"the rest of the group is not applied to a refused line");
        }

        //  The documented bare forms are untouched by that ruling, and this is
        //  the sweep that says so: -w is 133 columns, -l generates the listing,
        //  -d defines DEBUG, -g takes no parameter at all. All four are as65's
        //  own, and none of them may become a refusal.
        TEST_METHOD (TheDocumentedBareForms_AreStillAccepted)
        {
            const char *  kBare[] = { "-w", "-l", "-d", "-g" };

            for (const char * flag : kBare)
            {
                ArgVector           args = { "CassoCli", "as65", "demo.a65", flag };
                CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean,
                    (std::wstring (L"as65 documents this bare form: ") + Widen (flag)).c_str());
            }
        }

        //  as65's OWN worked example of its concatenation rule: "-h80t which
        //  specifies 80 lines per page and a symbol table". Its parameter is
        //  NUMERIC, and the rule is that "other options can follow one that has
        //  a numeric parameter" -- so the digits end the value and the walk
        //  resumes at `t`. This ran to the end of the argument instead and threw
        //  the `t` away without a word, which is the bug: a listing asked for
        //  with a symbol table came back without one, at status 0.
        TEST_METHOD (PageHeightFlag_LetsAnotherFlagFollowItInTheGroup)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-h80t" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (80, opts.pageHeight, L"80 lines per page");
            Assert::IsTrue (opts.symbolTable, L"and a symbol table -- as65's own example");
        }

        //  The same rule, one flag along, so the walk is shown resuming rather
        //  than merely stopping in the right place.
        TEST_METHOD (PageWidthFlag_LetsAnotherFlagFollowItInTheGroup)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-w100tq" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (100, opts.pageWidth);
            Assert::IsTrue (opts.symbolTable, L"-t followed the numeric value");
            Assert::IsTrue (opts.quiet,       L"and -q followed -t");
        }

        //  `-h0` is as65's documented special case, and it is also this tool's
        //  default -- so the flag has to be READ rather than merely tolerated,
        //  or a later `-h60` on the same line could not be cancelled by it.
        TEST_METHOD (PageHeightZero_IsTheInfinitePageAs65Documents)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-h60", "-h0" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (0, opts.pageHeight, L"an infinite page length");
        }

        TEST_METHOD (PageHeightFlag_TakesAnAttachedValue)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-h10" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (10, opts.pageHeight);
        }

        //  The bare flag must not reach past its own argument on its way to
        //  being refused. A count is a number, and the input file is what a
        //  bare -h is usually standing in front of -- so the refusal has to be
        //  about the flag, with the neighbor left unread rather than swallowed
        //  as a height.
        TEST_METHOD (PageHeightFlagAlone_DoesNotReadItsNeighborAsAHeight)
        {
            //  -q leads only because a bare -h in the first position is the
            //  top-level help form and never reaches this grammar at all.
            ArgVector           args = { "CassoCli", "as65", "-q", "-h", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue   (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused);
            Assert::AreEqual (0, opts.pageHeight, L"the source file is not the page height");
        }

        //  Gone with every other separated form. `-w 100` leaves the bare-flag
        //  133 standing and hands `100` back to the walk -- where it is now a
        //  surplus argument and is said so, rather than dropped.
        TEST_METHOD (PageWidthFlag_DoesNotTakeASeparatedValue)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-w", "100" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (CommandLineParser::kWideListingColumns, opts.pageWidth,
                              L"a bare -w is the 133-column listing, whatever follows it");
            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                            L"and the 100 that went nowhere is reported rather than discarded");
        }

        //  as65 states the default itself: "Normally, the listing is printed
        //  using 79 columns for output to a 80-column screen or printer." It
        //  was 80 here -- the screen's width, not the listing's.
        TEST_METHOD (PageWidthDefault_IsTheSeventyNineAs65Documents)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (79, opts.pageWidth);
        }

        TEST_METHOD (PageWidthFlag_TakesAnAttachedValueOfItsOwn)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-w100" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (100, opts.pageWidth);
        }

        //  A bare -w means the wide listing, which is what the help says it
        //  means. It used to mean nothing at all and leave the default width.
        TEST_METHOD (PageWidthFlagAlone_SelectsTheWideListing)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-w" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (CommandLineParser::kWideListingColumns, opts.pageWidth);
        }

        //  -g TAKES NO PARAMETER AT ALL. Its whole as65 entry is "Generate
        //  source-level debug information file. This file can then be used in
        //  in-system debugging or a software simulator" -- no filename, no
        //  extension, no format. Both named forms were added here, so removing
        //  them takes away a capability as65 never had rather than matching
        //  one; naming the file will need a form of this project's own.
        TEST_METHOD (DebugFlag_TakesNoFileName_SeparatedOrAttached)
        {
            ArgVector           separated = { "CassoCli", "as65", "demo.a65", "-g", "out.dbg" };
            ArgVector           attached  = { "CassoCli", "as65", "demo.a65", "-gout.dbg" };
            CommandLineOptions  sepOpts   = CommandLineParser::Parse (separated.Count(), separated.Data(), NoProbe());
            CommandLineOptions  attOpts   = CommandLineParser::Parse (attached.Count(), attached.Data(), NoProbe());

            Assert::IsTrue (sepOpts.debugInfo);
            Assert::AreEqual (std::string ("demo.dbg"), sepOpts.debugFile,
                              L"the derived name, not the one standing next to the flag");

            Assert::IsTrue (attOpts.debugInfo);
            Assert::AreEqual (std::string ("demo.dbg"), attOpts.debugFile,
                              L"and not the one glued to it either");

            //  WHAT `-gout.dbg` NOW MEANS IS `-g -out.dbg`, pinned here rather
            //  than left to be discovered. A flag taking no parameter hands the
            //  rest of its group straight to the walk, so the `o` is read as -o
            //  and `ut.dbg` as its attached filename. That is as65's
            //  concatenation working as designed -- it is the same reading that
            //  makes `/out` mean `-o ut` -- and it is surprising enough to be
            //  worth a test that says so out loud.
            Assert::AreEqual (std::string ("ut.dbg"), attOpts.outputFile,
                              L"the group continued into -o, as concatenation requires");
        }

        //  Taking no parameter is also what lets other flags follow it in a
        //  group, exactly as they follow -t.
        TEST_METHOD (DebugFlag_LetsAnotherFlagFollowItInTheGroup)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-gt" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.debugInfo);
            Assert::IsTrue (opts.symbolTable, L"-t followed -g, which consumes nothing");
        }

        //  A bare -g still derives its name from the source, so the flag on its
        //  own keeps working -- and the flag before a source file must not
        //  swallow the source file.
        TEST_METHOD (DebugFlagAlone_StillDerivesTheNameFromTheSource)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-g" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("demo.dbg"), opts.debugFile);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CpuTargetTests
    //
    //  The default stays strict 6502 so a 65C02-only opcode never assembles by
    //  accident.
    //
    ////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ParseVerdictTests
    //
    //  What the parser leaves behind after it complains, which is what the exit
    //  code is made of.
    //
    //  Every one of these printed a diagnostic and recorded nothing, so the
    //  executable had no way to know a diagnostic had happened and returned 0.
    //  The user saw the complaint on their screen; their build script saw
    //  success.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ParseVerdictTests)
    {
    public:
        TEST_METHOD (CleanCommandLine_LeavesNoVerdict)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-t" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean);
        }

        TEST_METHOD (RunOption_ItDoesNotKnow_IsRefused)
        {
            ArgVector           args = { "CassoCli", "run", "prog.a65", "--cpu", "65c02" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                            L"an option that might have changed where the image runs");
        }

        //  A value it could not read is the same refusal: the program would run
        //  somewhere other than where it was told to.
        TEST_METHOD (RunValue_ItCouldNotRead_IsRefused)
        {
            const char *  kFlags[] = { "--load", "--entry", "--stop", "--max-cycles", "--fill" };

            for (const char * flag : kFlags)
            {
                ArgVector           args = { "CassoCli", "run", "prog.bin", flag, "zzz" };
                CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                                (std::wstring (L"silently accepted a bad value for ") +
                                 std::wstring (flag, flag + strlen (flag))).c_str());
            }
        }

        //  AN ILLEGAL OPTION IS REFUSED, AND THIS REVERSES A SHIPPED DECISION.
        //  as65's DIAGNOSTICS: "Help message if only parameter is a question
        //  mark, or if an illegal option has been specified." The flag used to
        //  be dropped with a warning while the assembly ran on and wrote its
        //  output at status 1 -- so a makefile passing a flag this assembler
        //  does not have got a binary shaped by the flags that survived,
        //  reported under the status an ordinary assembler warning earns.
        TEST_METHOD (UnknownAs65Flag_IsRefused_AndAsksForTheAssemblerPage)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-Y" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                            L"nothing is assembled");
            Assert::AreEqual (std::string ("-Y"), opts.unrecognizedFlag,
                              L"and the flag is named, rather than answered with a wall of usage");
            Assert::IsTrue (opts.subcommand == CommandLineOptions::Subcommand::As65,
                            L"under the assembler, so the help that follows is the assembler's");
        }

        //  The refusal has to stop the walk, or a flag AFTER the illegal one
        //  goes on shaping a run that is not going to happen -- and, worse, the
        //  concatenation walk would keep reading the same argument.
        TEST_METHOD (UnknownAs65Flag_StopsTheRestOfTheCommandLine)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-Yt", "-oout.bin" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused);
            Assert::IsFalse (opts.symbolTable, L"the flag packed behind it is not obeyed");
            Assert::AreNotEqual (std::string ("out.bin"), opts.outputFile,
                                 L"nor is the argument after it");
        }

        //  A BARE INVOCATION PRODUCED NOTHING AND SAID IT HAD WORKED. It prints
        //  the general page and exited 0, so a script running the tool with an
        //  argument variable that happened to be empty was told the run was
        //  fine. This is not an as65 parity question -- nothing has entered the
        //  assembler's grammar -- so it is decided on this tool's own table,
        //  where the verdict is what the exit status is read from.
        TEST_METHOD (NoArgumentsAtAll_IsRefused_ThoughItStillPrintsThePage)
        {
            ArgVector           args = { "CassoCli" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.showHelp, L"the general page is still printed");
            Assert::IsTrue (opts.helpPage == CommandLineOptions::HelpPage::General);
            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                            L"and nothing was produced, which the status has to say");
        }

        //  The counterpart, and the reason the verdict rather than showHelp
        //  carries it: ASKING for the page is not the same event as being shown
        //  it, and asking still succeeds.
        TEST_METHOD (ExplicitHelpRequest_IsNotRefused)
        {
            const char *  kSpellings[] = { "--help", "-help", "-?", "/?", "/help", "-h", "?" };

            for (const char * form : kSpellings)
            {
                ArgVector           args = { "CassoCli", form };
                CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean,
                                (std::wstring (L"asking for help was refused: ") +
                                 std::wstring (form, form + strlen (form))).c_str());
            }
        }

        //  A `--` option this grammar does not have is refused outright rather
        //  than handed to the concatenation walk as a run of single letters.
        //  Printing a diagnostic is the ANSWER to the mistake, not evidence
        //  there was none, so it is a refusal and the script is told.
        TEST_METHOD (UnknownLongOption_IsRefused_EvenThoughItIsDiagnosed)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "--bogus" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                            L"the script is told it was wrong");
        }

        //  Asking for help is not a complaint about anything.
        TEST_METHOD (HelpRequest_LeavesNoVerdict)
        {
            ArgVector           args = { "CassoCli", "--help" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SurplusArgumentTests
    //
    //  An argument that reaches no slot at all, in all three grammars.
    //
    //  Every one of these used to be accepted and thrown away. The command line
    //  that found it -- `CassoCli pg.a65 -opg.bin -h 60` -- assembled, wrote the
    //  binary, exited 0, and never said that `60` had gone nowhere; the caller's
    //  build script had no way to learn that half of what it asked for was not
    //  read.
    //
    //  as65 SETTLES NOTHING HERE. Its synopsis is `as65 [-cdghilnopqstvwxz]
    //  file` -- one file -- and it documents no behavior for a surplus argument,
    //  so this is a choice rather than a parity question, and the choice is to
    //  refuse.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SurplusArgumentTests)
    {
    public:
        TEST_METHOD (Assembly_ASecondSourceFile_IsRefused)
        {
            ArgVector           args = { "CassoCli", "as65", "prog.a65", "-oprog.bin", "extra.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                            L"assembling takes one source file");
            Assert::AreEqual (std::string ("prog.a65"), opts.inputFile,
                              L"and the first one is still the one that was named");
        }

        //  The shape of the reported defect, with the bare -h taken out of it so
        //  this pins the surplus rule rather than the bare-flag rule. `-w` is
        //  as65's own bare form and stays legal, so `100` is what has nowhere to
        //  go.
        TEST_METHOD (Assembly_ASeparatedValue_IsRefusedRatherThanDropped)
        {
            ArgVector           args = { "CassoCli", "as65", "prog.a65", "-w", "100" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused);
        }

        //  Nothing is refused while there is still a slot open -- one source
        //  file, however many flags surround it, is the ordinary command line
        //  and must stay clean.
        TEST_METHOD (Assembly_OneSourceFileAmongFlags_IsStillClean)
        {
            ArgVector           args = { "CassoCli", "as65", "-t", "prog.a65", "-oprog.bin", "-w133" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean);
            Assert::AreEqual (std::string ("prog.a65"), opts.inputFile);
        }

        //  The two conditions that earn the "as65 glues its values" sentence,
        //  checked through the thing that decides them. A word that is neither a
        //  number nor the neighbor of a parameter-taking flag gets the plain
        //  message, because guessing at a cause there would invent one.
        TEST_METHOD (TheGluedValueHint_IsOfferedOnlyWhenThereIsReasonTo)
        {
            Assert::IsTrue  (CommandLineParser::IsPlainDecimal ("60"),  L"a separated value looks like this");
            Assert::IsFalse (CommandLineParser::IsPlainDecimal ("60t"), L"and this is not a number");
            Assert::IsFalse (CommandLineParser::IsPlainDecimal (""),    L"nor is nothing");

            Assert::AreEqual ((int) 'l', (int) CommandLineParser::TrailingParameterFlag ("-l"),
                              L"-l takes a filename, so what follows it may have been meant as one");
            Assert::AreEqual ((int) 'd', (int) CommandLineParser::TrailingParameterFlag ("-td"),
                              L"read from the END of a group, which is where a value would attach");
            Assert::AreEqual (0, (int) CommandLineParser::TrailingParameterFlag ("-t"),
                              L"-t takes nothing, so nothing follows from it");
            Assert::AreEqual (0, (int) CommandLineParser::TrailingParameterFlag ("prog.a65"),
                              L"and a filename is not a flag at all");
        }

        //
        //  THE SHELL-SPLIT SIGNATURE, which is the other cause a surplus
        //  argument can have and by far the commonest one on this project's own
        //  machines.
        //
        //  PowerShell parses a token beginning with a single `-` as a parameter
        //  name and a parameter name may not contain a `.`, so it cuts
        //  `-oprog.bin` into `-oprog` and `.bin` before this program starts.
        //  Both halves reach the parser, the second has nowhere to go, and
        //  "surplus argument: .bin" describes nothing the reader typed.
        //
        //  EVERY CASE BELOW WAS MEASURED by handing the argument to a native
        //  executable that prints its argv, under PowerShell 7.6.5 and Windows
        //  PowerShell 5.1. None of it is inferred from documentation.
        //
        //  These pin the RULE rather than the sentence. The message goes to the
        //  error stream, which the parser writes and does not keep, so the only
        //  place a test can hold the decision is the predicate that makes it --
        //  the same reason IsPlainDecimal and TrailingParameterFlag are public.
        //
        TEST_METHOD (TheShellSplitSignature_IsTheHalvesAndNotTheShell)
        {
            Assert::IsTrue (CommandLineParser::IsShellSplitFragment ("-oprog", ".bin"),
                L"the first dot is where PowerShell ends a parameter name");
            Assert::IsTrue (CommandLineParser::IsShellSplitFragment ("-lprog", ".lst"),
                L"every flag that attaches a NAME can be cut, not just -o");
            Assert::IsTrue (CommandLineParser::IsShellSplitFragment ("-osub\\x", ".bin"),
                L"a path separator does not cut, so the front half can hold one");

            Assert::IsFalse (CommandLineParser::IsShellSplitFragment ("-t", ".bin"),
                L"-t attaches no value, so nothing of it could have been cut off");
            Assert::IsFalse (CommandLineParser::IsShellSplitFragment ("-h60", ".bin"),
                L"a numeric parameter is digits, and digits carry no dot");
            Assert::IsFalse (CommandLineParser::IsShellSplitFragment ("-oprog", "extra.a65"),
                L"a whole word is a second filename, not the back half of one");
            Assert::IsFalse (CommandLineParser::IsShellSplitFragment ("--flat", ".bin"),
                L"a `--` option is not a parameter name and arrives whole");
            Assert::IsFalse (CommandLineParser::IsShellSplitFragment ("/oprog", ".bin"),
                L"nor is a `/` option -- /oprog.bin was measured arriving whole");

            //  TWO CONDITIONS KEEP ORDINARY COMMAND LINES OUT, and both read
            //  the FRONT half, because the front half is what shows whether a
            //  cut was made. `-oout.bin` still carries the dot the shell would
            //  have cut at, and `-oC:` carries a colon, which suppresses the
            //  cut outright -- `-oC:\out\prog.bin` was measured arriving whole.
            Assert::IsFalse (CommandLineParser::IsShellSplitFragment ("-oout.bin", "./prog.a65"),
                L"a front half still holding its own dot was never cut");
            Assert::IsFalse (CommandLineParser::IsShellSplitFragment ("-oC:", "\\out\\prog.bin"),
                L"and a colon before the first dot means nothing was cut at all");
        }

        //  THE MANGLING AS IT ACTUALLY ARRIVES, and what the parser now makes
        //  of it. This pair used to assert a refusal, and the refusal was the
        //  defect: `CassoCli prog.a65 -oprog.bin` is a correct as65 command
        //  line that PowerShell takes apart on the way in, so the user was told
        //  to quote something they had typed correctly.
        TEST_METHOD (Assembly_AMangledOutputName_IsRejoinedAndAssembled)
        {
            ArgVector           args = { "CassoCli", "as65", "prog.a65", "-oprog", ".bin" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean,
                L"the halves go back together rather than earning a diagnostic");
            Assert::AreEqual (std::string ("prog.bin"), opts.outputFile,
                L"and the whole filename is what the output is named");
            Assert::AreEqual (std::string ("prog.a65"), opts.inputFile,
                L"the source file is untouched by the repair");
        }

        //  THE SAME MISTAKE TYPED THE OTHER WAY ROUND, where the back half
        //  would otherwise land in the source-file slot and displace the real
        //  source. It is why the repair walks the whole command line rather
        //  than examining the pair that happened to fail.
        TEST_METHOD (Assembly_AMangledOutputNameBeforeTheSource_IsRejoined)
        {
            ArgVector           args = { "CassoCli", "as65", "-oprog", ".bin", "prog.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean);
            Assert::AreEqual (std::string ("prog.a65"), opts.inputFile,
                L"prog.a65 keeps the source slot the fragment used to take");
            Assert::AreEqual (std::string ("prog.bin"), opts.outputFile);
        }

        //  A FLAG WITH NO SEPARATED FORM IS REPAIRED TOO, which is the half
        //  of this that quoting advice could never fix: -o could always be
        //  written `-o prog.bin`, and -l could not be written at all in
        //  PowerShell without quotes.
        TEST_METHOD (Assembly_AMangledListingName_IsRejoined)
        {
            ArgVector           args = { "CassoCli", "as65", "prog.a65", "-lprog", ".lst" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean);
            Assert::AreEqual (std::string ("prog.lst"), opts.listingFile);
        }

        //  THE REPAIR IS A FUNCTION OF THE ARGUMENT LIST ALONE, so the rule can
        //  be stated here rather than inferred from what a parse happened to
        //  produce.
        //
        //  THE NEGATIVE CASES ARE THE POINT. A repair that fires on an ordinary
        //  command line would silently change what it means, and the two below
        //  are the ones that come closest: a relative path standing behind an
        //  output name is not a fragment, because the front half still carries
        //  the dot a shell would have cut at; and two real arguments in a row
        //  are left as two.
        TEST_METHOD (RejoiningPutsBackWhatTheShellCut_AndTouchesNothingElse)
        {
            auto  rejoin = [] (std::initializer_list<const char *> raw)
            {
                ArgVector  args = raw;
                return CommandLineParser::RejoinShellSplitArguments (args.Count(), args.Data());
            };

            auto  cut = rejoin ({ "CassoCli", "as65", "prog.a65", "-oprog", ".bin" });
            Assert::AreEqual (size_t (4), cut.size(), L"five arguments in, four out");
            Assert::AreEqual (std::string ("-oprog.bin"), cut[3]);

            auto  path = rejoin ({ "CassoCli", "as65", "-oout.bin", "./prog.a65" });
            Assert::AreEqual (size_t (4), path.size(),
                L"a relative path is not a fragment: the front half already has its dot");
            Assert::AreEqual (std::string ("./prog.a65"), path[3]);

            auto  plain = rejoin ({ "CassoCli", "as65", "prog.a65", "extra.a65" });
            Assert::AreEqual (size_t (4), plain.size(),
                L"two real arguments stay two, and the surplus one is still surplus");

            auto  none = rejoin ({ "CassoCli" });
            Assert::AreEqual (size_t (1), none.size(),
                L"nothing to rejoin, and no first argument consumed looking");
        }

        TEST_METHOD (Run_ASecondInputFile_IsRefused)
        {
            ArgVector           args = { "CassoCli", "run", "prog.a65", "extra" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused);
            Assert::AreEqual (std::string ("prog.a65"), opts.inputFile);
        }

        //  `list` NAMES A DISK AND NOTHING ELSE, so its second operand is
        //  surplus and not merely unused. `disk list img.dsk PROG` cataloged
        //  the whole disk, said nothing about PROG, and exited 0 -- which reads
        //  exactly like a listing filtered to PROG that happened to match
        //  everything.
        TEST_METHOD (Disk_ListTakesOnlyTheImage)
        {
            ArgVector           args = { "CassoCli", "disk", "list", "my.dsk", "PROG" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused);
            Assert::AreEqual (std::string ("my.dsk"), opts.disk.imagePath,
                              L"and the image it did name is untouched");
        }

        //  Every verb that names a file gets two operands and no more.
        TEST_METHOD (Disk_TheFileVerbsTakeTwoOperandsAndNoMore)
        {
            struct { const char * verb; const char * second; }  kCases[] =
            {
                { "get",    "PROG"     },
                { "put",    "prog.bin" },
                { "delete", "PROG"     },
                { "boot",   "PROG"     },
            };

            for (const auto & test : kCases)
            {
                ArgVector           two  = { "CassoCli", "disk", test.verb, "my.dsk", test.second };
                CommandLineOptions  ok   = CommandLineParser::Parse (two.Count(), two.Data(), NoProbe());

                Assert::IsTrue (ok.parseVerdict == CommandLineOptions::ParseVerdict::Clean,
                    (std::wstring (L"two operands is the shape of: ") + Widen (test.verb)).c_str());

                ArgVector           three = { "CassoCli", "disk", test.verb, "my.dsk", test.second, "extra" };
                CommandLineOptions  bad   = CommandLineParser::Parse (three.Count(), three.Data(), NoProbe());

                Assert::IsTrue (bad.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                    (std::wstring (L"and a third is refused by: ") + Widen (test.verb)).c_str());
            }
        }

        //  A VERB THE TABLE DOES NOT KNOW IS LEFT ALONE. The runner reports that
        //  in its own words -- "unknown disk verb -- try: ..." -- and a
        //  complaint about operand three instead would answer a question nobody
        //  asked.
        TEST_METHOD (Disk_AnUnknownVerb_IsNotDiagnosedAsAnOperandCount)
        {
            ArgVector           args = { "CassoCli", "disk", "format", "my.dsk", "PROG", "extra" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean,
                            L"the verb is what is wrong here, and the runner says so");
        }

        //  The operand count is the verb's, so an alias has to carry the same
        //  one the descriptive word does.
        TEST_METHOD (Disk_AnAliasCarriesItsVerbsOperandCount)
        {
            ArgVector           args = { "CassoCli", "disk", "cat", "my.dsk", "PROG" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                            L"`cat` is `list`, and `list` names a disk and nothing else");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DiscardedValueTests
    //
    //  The rest of the same class: a value the user typed that was read,
    //  understood to be unreadable, and then quietly replaced or dropped.
    //
    //  The tool already states the rule these restore -- a command line is
    //  Refused when it carries "a value that could not be read" -- and `run`
    //  applies it to every address and count it takes. These were the places
    //  that did not.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DiscardedValueTests)
    {
    public:
        //  `disk put img prog.bin --addr zzz` dropped the address and then told
        //  the caller "is a binary, which has to be told where it loads -- give
        //  --addr $XXXX", which is a message contradicting the command line it
        //  was answering.
        TEST_METHOD (Disk_AnAddressThatCouldNotBeRead_IsRefused)
        {
            ArgVector           args = { "CassoCli", "disk", "put", "my.dsk", "prog.bin", "--addr", "zzz" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue  (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused);
            Assert::IsFalse (opts.disk.hasLoadAddress, L"and no address was invented in its place");
        }

        TEST_METHOD (Disk_AnAddressThatCouldBeRead_IsStillTaken)
        {
            ArgVector           args = { "CassoCli", "disk", "put", "my.dsk", "prog.bin", "--addr", "$6000" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue   (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean);
            Assert::IsTrue   (opts.disk.hasLoadAddress);
            Assert::AreEqual ((Word) 0x6000, opts.disk.loadAddress);
        }

        //  An option that ran out of command line is not an unknown one. The
        //  refusal was right; the words were not -- "unknown disk option:
        //  --addr", followed by a list of options to try with `--addr` in it.
        TEST_METHOD (Disk_AnOptionWithNoValueLeft_IsStillRefused)
        {
            const char *  kNeedsValue[] = { "--out", "--as", "--type", "--addr" };

            for (const char * flag : kNeedsValue)
            {
                ArgVector           args = { "CassoCli", "disk", "get", "my.dsk", "PROG", flag };
                CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                    (std::wstring (L"not refused: ") + Widen (flag)).c_str());

                Assert::IsTrue (CommandLineParser::IsDiskOptionNeedingValue (flag),
                    L"and it is reported as an option that needs a value, not as one that does not exist");
            }
        }

        TEST_METHOD (Run_AnOptionWithNoValueLeft_IsStillRefused)
        {
            const char *  kNeedsValue[] = { "-o", "-l", "--fill", "--load", "--entry",
                                            "--stop", "--max-cycles" };

            for (const char * flag : kNeedsValue)
            {
                ArgVector           args = { "CassoCli", "run", "prog.bin", flag };
                CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                    (std::wstring (L"not refused: ") + Widen (flag)).c_str());

                Assert::IsTrue (CommandLineParser::IsRunOptionNeedingValue (flag),
                    L"and it is a known option that ran out of arguments, not an unknown one");
            }
        }

        //  `-dNAME=VALUE` IS THIS TOOL'S OWN EXTENSION -- as65 documents only
        //  `-d<name>`, equated to 1 -- so a value it cannot read is nobody's
        //  parity requirement. It fell back to 1 in silence, which meant
        //  `-dADDR=$6000` and `-dVER=1.0` each defined the symbol as 1 and
        //  assembled a source that then took a branch nobody chose.
        TEST_METHOD (Predefine_AValueThatCouldNotBeRead_IsRefused)
        {
            const char *  kUnreadable[] = { "-dADDR=$6000", "-dVER=1.0", "-dX=zz", "-dY=" };

            for (const char * flag : kUnreadable)
            {
                ArgVector           args = { "CassoCli", "as65", "demo.a65", flag };
                CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                    (std::wstring (L"silently defined as 1: ") + Widen (flag)).c_str());
            }
        }

        TEST_METHOD (Predefine_ANameWithNothingInFrontOfTheEquals_IsRefused)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-d=5" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                            L"it defined nothing at all and said nothing about it");
        }

        //  The forms that DO read are untouched, decimal and 0x alike.
        TEST_METHOD (Predefine_TheValuesThatCanBeRead_AreStillTaken)
        {
            struct { const char * flag; int32_t value; }  kCases[] =
            {
                { "-dA=5",    5    },
                { "-dB=0x10", 0x10 },
                { "-dC=-3",   -3   },
            };

            for (const auto & test : kCases)
            {
                ArgVector           args  = { "CassoCli", "as65", "demo.a65", test.flag };
                CommandLineOptions  opts  = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean,
                    (std::wstring (L"refused a readable value: ") + Widen (test.flag)).c_str());
            }
        }
    };





    TEST_CLASS (CpuTargetTests)
    {
    public:
        TEST_METHOD (Default_IsStrict6502)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.cpuTarget == CommandLineOptions::CpuTarget::M6502);
        }

        //  `--cpu` IS WITHDRAWN AND `-x` REPLACES IT. Both forms selected
        //  the same instruction set, and `-x` is as65's own name for the
        //  switch, so the tool carried two ways to ask for one capability.
        //
        //  BOTH FORMS ARE ANSWERED BY NAME rather than falling into the generic
        //  `--` refusal, because command lines carrying `--cpu` already exist
        //  and "unknown option: --cpu" would not tell their author what to type
        //  instead.
        TEST_METHOD (CpuOption_IsWithdrawn_AndPointsAtTheAs65Spelling)
        {
            const char *  kForms[] = { "--cpu", "--cpu=65C02" };

            for (const char * form : kForms)
            {
                ArgVector           args = { "CassoCli", "as65", "demo.a65", form, "65c02" };
                CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                                (std::wstring (L"still accepted: ") +
                                 std::wstring (form, form + strlen (form))).c_str());
                Assert::IsTrue (opts.cpuTarget == CommandLineOptions::CpuTarget::M6502,
                                L"and selects nothing");
            }
        }

        //  as65: "Use 65SC02 extensions. This CPU has several additional
        //  instructions." It was not accepted at all -- the flag fell through
        //  to the unknown-flag warning, was dropped, and the source then failed
        //  to assemble on a strict 6502 with a diagnostic about the opcode
        //  rather than about the flag.
        TEST_METHOD (As65ExtensionFlag_Selects65C02)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-x" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.cpuTarget == CommandLineOptions::CpuTarget::M65C02);
            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean,
                            L"and is no longer complained about as unknown");
        }

        //  It is an as65 flag, so it packs with the others the way they do.
        TEST_METHOD (As65ExtensionFlag_Concatenates)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-xq" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.cpuTarget == CommandLineOptions::CpuTarget::M65C02);
            Assert::IsTrue (opts.quiet, L"the flag after it is still read");
        }

        TEST_METHOD (As65ExtensionFlag_AcceptsTheSlashPrefix)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "/x" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.cpuTarget == CommandLineOptions::CpuTarget::M65C02);
        }

        //  -x IS THE ONLY WAY TO THE CMOS TIER NOW, which is what makes the
        //  withdrawal of `--cpu` a tidying rather than a loss of capability.
        TEST_METHOD (TheAs65Flag_IsTheOnlyRouteToTheCmosTier)
        {
            ArgVector           as65    = { "CassoCli", "as65", "demo.a65", "-x" };
            ArgVector           nothing = { "CassoCli", "as65", "demo.a65" };
            CommandLineOptions  a       = CommandLineParser::Parse (as65.Count(), as65.Data(), NoProbe());
            CommandLineOptions  n       = CommandLineParser::Parse (nothing.Count(), nothing.Data(), NoProbe());

            Assert::IsTrue (a.cpuTarget == CommandLineOptions::CpuTarget::M65C02);
            Assert::IsTrue (n.cpuTarget == CommandLineOptions::CpuTarget::M6502,
                            L"and the default is still a strict 6502");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  InferredNameTests
    //
    //  The names AS65 mode derives rather than requires. Every one of them is
    //  taken from the RESOLVED input path, and an explicit flag always wins.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (InferredNameTests)
    {
    public:
        TEST_METHOD (BinaryOutputName_DerivesFromInput)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("demo.bin"), opts.outputFile);
        }

        TEST_METHOD (ExplicitOutputName_Wins)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-ocustom.out" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("custom.out"), opts.outputFile);
        }

        //  A TRAILING `-o` USED TO HANG THE TOOL FOREVER. With nothing glued to
        //  the flag and nothing after it, neither branch of `case 'o'` ran and
        //  neither advanced the concatenation walk, so the loop reread the same
        //  character until the process was killed: `casso demo.a65 -o` printed
        //  nothing and never returned. Every other value-taking flag already
        //  had the missing `pos++`.
        //
        //  A regression here HANGS this test rather than failing it, which is
        //  the honest cost of pinning the fix at the seam where the defect
        //  lives; the alternative is not pinning it at all.
        //
        //  IT IS NOW A REFUSAL RATHER THAN A FALLBACK. as65 notates the flag
        //  `-o<filename>`, so a bare -o names nothing and there is no such
        //  form; falling back to the inferred name meant a caller who typed
        //  `-o out.bin` got `demo.bin` and was told it had worked.
        TEST_METHOD (TrailingOutputFlag_WithNothingAfterIt_IsRefusedRatherThanSpinning)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-o" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                L"a bare -o names no file, which as65 has no form for");
        }

        //  THE SEPARATED `-o <file>` IS TAKEN, by owner decision, and it is the
        //  one form that survives PowerShell: that shell cuts an unquoted
        //  `-oprog.bin` into `-oprog` and `.bin`, because a parameter name
        //  cannot hold a `.`. Accepting it takes MORE than as65 does and never
        //  less, so no as65 command line changes meaning.
        TEST_METHOD (SeparatedOutputName_IsTaken_AndDoesNotBecomeTheSourceFile)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-o", "custom.out" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean);
            Assert::AreEqual (std::string ("custom.out"), opts.outputFile);
            Assert::AreEqual (std::string ("demo.a65"), opts.inputFile,
                L"and the filename beside it is not read as the source");
        }

        //  The flag may stand before the source file as well as after it, which
        //  is where a caller who reaches for the separated form usually puts it.
        TEST_METHOD (SeparatedOutputName_WorksBeforeTheSourceFile)
        {
            ArgVector           args = { "CassoCli", "as65", "-o", "custom.out", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean);
            Assert::AreEqual (std::string ("custom.out"), opts.outputFile);
            Assert::AreEqual (std::string ("demo.a65"), opts.inputFile);
        }

        //  AN ABSOLUTE PATH, ATTACHED AND SEPARATED, because a caller reaching
        //  for the separated form is usually naming a directory. Both are
        //  checked together: whatever the separated form gains, the glued one
        //  as65 documents has to keep working unchanged.
        TEST_METHOD (OutputName_TakesAnAbsolutePathAttachedOrSeparated)
        {
            ArgVector           separated = { "CassoCli", "as65", "demo.a65", "-o", "C:\\tmp\\prog.bin" };
            CommandLineOptions  apart     = CommandLineParser::Parse (separated.Count(), separated.Data(), NoProbe());

            Assert::IsTrue (apart.parseVerdict == CommandLineOptions::ParseVerdict::Clean);
            Assert::AreEqual (std::string ("C:\\tmp\\prog.bin"), apart.outputFile);

            ArgVector           glued  = { "CassoCli", "as65", "demo.a65", "-oC:\\tmp\\prog.bin" };
            CommandLineOptions  joined = CommandLineParser::Parse (glued.Count(), glued.Data(), NoProbe());

            Assert::IsTrue (joined.parseVerdict == CommandLineOptions::ParseVerdict::Clean);
            Assert::AreEqual (std::string ("C:\\tmp\\prog.bin"), joined.outputFile,
                L"the glued form as65 documents is untouched");
        }

        //  THE SEPARATED VALUE IS TAKEN VERBATIM. Skipping one that "looks like
        //  a flag" would be a guess about what the caller meant, and a file may
        //  legitimately be named that way.
        TEST_METHOD (SeparatedOutputName_IsTakenWhateverItLooksLike)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-o", "-t" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("-t"), opts.outputFile);
            Assert::IsFalse (opts.symbolTable, L"and it is a filename, not the flag it resembles");
        }

        //  NO OTHER FLAG GAINS A SEPARATED FORM, and the reason is ambiguity
        //  rather than caution: -l, -d, -w and -g each have a bare form as65
        //  documents, so the word after one of them is genuinely ambiguous with
        //  that bare reading and telling the two apart takes a guess. -o has no
        //  bare form, which is what leaves nothing to guess about.
        TEST_METHOD (TheSeparatedForm_ReachesNoOtherFlag)
        {
            const char *  kGluedOnly[] = { "-l", "-d", "-w", "-g" };

            for (const char * flag : kGluedOnly)
            {
                ArgVector           args = { "CassoCli", "as65", "demo.a65", flag, "value" };
                CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                    (std::wstring (L"the word after this flag is still surplus: ") + Widen (flag)).c_str());
            }
        }

        TEST_METHOD (SRecordFlag_InfersS19Extension)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-s" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::SRecord);
            Assert::AreEqual (std::string ("demo.s19"), opts.outputFile);
        }

        TEST_METHOD (IntelHexFlag_InfersHexExtension)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-s2" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::IntelHex);
            Assert::AreEqual (std::string ("demo.hex"), opts.outputFile);
        }

        TEST_METHOD (DebugFlag_InfersDbgExtension)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-g" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.debugInfo);
            Assert::AreEqual (std::string ("demo.dbg"), opts.debugFile);
        }

        TEST_METHOD (NoInputFile_InfersNothing)
        {
            ArgVector           args = { "CassoCli", "-t" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFile.empty(), L"nothing to derive a name from");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AutoExtensionTests
    //
    //  Resolving an extensionless input, which is the one place the grammar
    //  consults the filesystem -- through an injected predicate, so these run
    //  without touching a disk.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AutoExtensionTests)
    {
    public:
        TEST_METHOD (ExtensionlessInput_ResolvesToExistingA65)
        {
            ArgVector           args = { "CassoCli", "as65", "build" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), ProbeFor ("build.a65"));

            Assert::AreEqual (std::string ("build.a65"), opts.inputFile);
            Assert::AreEqual (std::string ("build.bin"), opts.outputFile,
                              L"the derived name follows the RESOLVED input");
        }

        TEST_METHOD (ExtensionlessInput_FallsThroughToAsm)
        {
            ArgVector           args = { "CassoCli", "as65", "build" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), ProbeFor ("build.asm"));

            Assert::AreEqual (std::string ("build.asm"), opts.inputFile);
        }

        TEST_METHOD (ExtensionlessInput_WithNoMatch_IsLeftAsTyped)
        {
            ArgVector           args = { "CassoCli", "as65", "build" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("build"), opts.inputFile,
                              L"reported against the name the user actually typed");
        }

        TEST_METHOD (InputWithExtension_IsNeverProbed)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), ProbeFor ("demo.a65.a65"));

            Assert::AreEqual (std::string ("demo.a65"), opts.inputFile);
        }

        //  A dot in a DIRECTORY name is not an extension, so this path is still
        //  a candidate for auto-extension.
        TEST_METHOD (DotInDirectoryName_DoesNotCountAsExtension)
        {
            ArgVector           args = { "CassoCli", "as65", "src/v1.2/build" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(),
                                                                ProbeFor ("src/v1.2/build.a65"));

            Assert::AreEqual (std::string ("src/v1.2/build.a65"), opts.inputFile);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RunOptionTests
    //
    //  The modern grammar: separated values, and address fields whose
    //  has-flags distinguish "given $0000" from "not given".
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (RunOptionTests)
    {
    public:
        TEST_METHOD (LoadAddress_IsParsedWithDollarPrefix)
        {
            ArgVector           args = { "CassoCli", "run", "demo.bin", "--load", "$6000" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.hasLoadAddress);
            Assert::AreEqual ((int) 0x6000, (int) opts.loadAddress);
        }

        TEST_METHOD (LoadAddress_IsParsedWithoutDollarPrefix)
        {
            ArgVector           args = { "CassoCli", "run", "demo.bin", "--load", "6000" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual ((int) 0x6000, (int) opts.loadAddress);
        }

        //  Zero is a legal address, so the has-flag is the only thing that can
        //  tell "load at $0000" from "no load address given".
        TEST_METHOD (ZeroLoadAddress_SetsTheHasFlag)
        {
            ArgVector           args = { "CassoCli", "run", "demo.bin", "--load", "$0000" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.hasLoadAddress);
            Assert::AreEqual (0, (int) opts.loadAddress);
        }

        TEST_METHOD (NoLoadAddress_LeavesHasFlagClearAndDefaultIntact)
        {
            ArgVector           args = { "CassoCli", "run", "demo.bin" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsFalse (opts.hasLoadAddress);
            Assert::AreEqual ((int) 0x8000, (int) opts.loadAddress);
        }

        TEST_METHOD (MalformedAddress_LeavesHasFlagClear)
        {
            ArgVector           args = { "CassoCli", "run", "demo.bin", "--load", "12zz" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsFalse (opts.hasLoadAddress, L"a trailing garbage suffix must not read as $12");
        }

        TEST_METHOD (MaxCycles_IsParsedAsDecimal)
        {
            ArgVector           args = { "CassoCli", "run", "demo.bin", "--max-cycles", "500" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual ((uint32_t) 500, opts.maxCycles);
        }

        TEST_METHOD (ResetVectorFlag_IsRecognized)
        {
            ArgVector           args = { "CassoCli", "run", "demo.bin", "--reset-vector" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.useResetVector);
        }

        TEST_METHOD (WarningModeFlags_AreRecognized)
        {
            ArgVector           fatal = { "CassoCli", "run", "demo.bin", "--fatal-warnings" };
            ArgVector           none  = { "CassoCli", "run", "demo.bin", "--no-warn" };
            CommandLineOptions  optsF = CommandLineParser::Parse (fatal.Count(), fatal.Data(), NoProbe());
            CommandLineOptions  optsN = CommandLineParser::Parse (none.Count(),  none.Data(),  NoProbe());

            Assert::IsTrue (optsF.warningMode == WarningMode::FatalWarnings);
            Assert::IsTrue (optsN.warningMode == WarningMode::NoWarn);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  OutputShapeTests
    //
    //  Selecting a binary shape.
    //
    //  THE DEFAULT IS THE ASSEMBLED BYTES. It was the as65 full 64 KB padded
    //  image, which is what somebody burning a ROM or diffing against a
    //  reference wants and is not what somebody assembling a routine wants --
    //  they got 64 KB and sliced it down by hand. `--flat` asks for the old
    //  shape now.
    //
    //  NOTHING WRITES THE DEFAULT. `--raw` did for one revision, on the
    //  reasoning that command lines already carrying it should keep working; it
    //  is gone, because an option whose only effect is to select what naming
    //  nothing already selects is a line of help buying no capability.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (OutputShapeTests)
    {
    public:
        TEST_METHOD (Default_IsTheAssembledBytes_NotAPaddedFullImage)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::Raw,
                L"naming no shape writes only what was assembled");
            Assert::IsFalse (opts.outputFormatNamed,
                L"and nothing was named, which is a separate fact from which shape it is");
        }

        TEST_METHOD (FlatFlag_SelectsTheFullPaddedImage)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "--flat" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::Binary,
                L"--flat is how the old default is asked for");
            Assert::IsTrue (opts.outputFormatNamed);
            Assert::AreEqual (std::string ("demo.bin"), opts.outputFile);
        }

        //  `--raw` is GONE, and what matters is how it is gone: refused, not
        //  fed to the concatenation walk as -r -a -w. That walk would complain
        //  about two flags that do not exist, quietly set the listing column
        //  width from `w`, and assemble anyway -- which is a command line
        //  answered wrongly rather than one turned down.
        TEST_METHOD (RawFlag_IsRefused_NotWalkedAsThePackedLettersRAW)
        {
            ArgVector           args      = { "CassoCli", "as65", "demo.a65", "--raw" };
            CommandLineOptions  opts      = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());
            CommandLineOptions  untouched;

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                L"an option this grammar does not have is turned down");
            Assert::IsFalse (opts.showHelp,
                L"and answered with the two lines that explain it, not with 180 of usage");

            Assert::IsFalse (opts.outputFormatNamed,
                L"nothing claimed a shape");
            Assert::AreEqual (untouched.pageWidth, opts.pageWidth,
                L"and the `w` in `raw` did not become the column width");
        }

        //  The extension fallback used to key off the shape equalling Binary,
        //  which stopped meaning "nobody said" the moment the default moved.
        TEST_METHOD (ExtensionFallback_StillAppliesOnlyWhenNoShapeWasNamed)
        {
            ArgVector           silent = { "CassoCli", "as65", "demo.a65", "-oout.s19" };
            ArgVector           spoken = { "CassoCli", "as65", "demo.a65", "--flat", "-oout.s19" };
            CommandLineOptions  quiet  = CommandLineParser::Parse (silent.Count(), silent.Data(), NoProbe());
            CommandLineOptions  loud   = CommandLineParser::Parse (spoken.Count(), spoken.Data(), NoProbe());

            Assert::IsFalse (quiet.outputFormatNamed,
                L"a .s19 filename with no flag leaves the shape unclaimed, so the "
                L"executable may infer an S-record from it");
            Assert::IsTrue (loud.outputFormatNamed,
                L"and an explicit shape flag claims it, so the filename may not");
        }

        TEST_METHOD (DosBinFlag_SelectsDosBinary)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "--dos-bin" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::DosBinary);
        }

        TEST_METHOD (ShapeFlag_ComposesWithOtherFlags)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "--flat", "-t", "-oout.obj" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::Binary);
            Assert::IsTrue (opts.symbolTable);
            Assert::AreEqual (std::string ("out.obj"), opts.outputFile);
        }

        TEST_METHOD (ShapeFlag_DoesNotConsumeTheInputFile)
        {
            ArgVector           args = { "CassoCli", "as65", "--dos-bin", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("demo.a65"), opts.inputFile);
            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::DosBinary);
        }

        //
        //  `--out` IS THE DISK GRAMMAR'S FLAG AND STAYS THERE. `-o` is as65's,
        //  and as65 argument compatibility is what this grammar exists for, so
        //  the two are not unified -- but the collision must be REPORTED rather
        //  than absorbed.
        //
        //  Absorbed is what it was. The walk over concatenated flags read the
        //  second `-` as a flag named `-` and warned about it, read `o` and
        //  took `ut` as its glued value, and set the output file to a file
        //  called `ut` in the working directory -- then took the NEXT argument
        //  as the input file. Three wrong decisions and exit 1, which is the
        //  status meaning "assembled, and the output was written".
        //
        TEST_METHOD (AssemblyMode_RefusesTheDiskGrammarsOut_RatherThanWritingAFileCalledUt)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "--out", "demo.bin" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                L"refused, not warned about and carried past");

            Assert::AreNotEqual (std::string ("ut"), opts.outputFile,
                L"and above all not written to a file named from the flag's own letters");
        }

        //  Every `--` argument this grammar does not know, not only the one
        //  that collides with `disk`. A refusal special-cased to `--out` would
        //  leave `--outfile` and `--output` walking the same path into `ut`.
        TEST_METHOD (AssemblyMode_RefusesAnyLongOptionItDoesNotHave)
        {
            const char *  kUnknown[] = { "--out", "--output", "--verbatim", "--long" };

            for (const char * flag : kUnknown)
            {
                ArgVector           args = { "CassoCli", "as65", "demo.a65", flag };
                CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                    (std::wstring (L"not refused: ") + Widen (flag)).c_str());
            }
        }

        //  And the two it DOES have still work, which is what keeps the
        //  refusal above from being a blanket ban on the `--` prefix. There
        //  were three until `--cpu` was withdrawn in favor of `-x`; both that
        //  remain are shapes as65 has no equivalent for, which is why they stay
        //  where `--cpu` went.
        TEST_METHOD (AssemblyMode_StillTakesTheLongOptionsItDoesHave)
        {
            ArgVector           flat = { "CassoCli", "as65", "demo.a65", "--flat" };
            ArgVector           dos  = { "CassoCli", "as65", "demo.a65", "--dos-bin" };

            Assert::IsTrue (CommandLineParser::Parse (flat.Count(), flat.Data(), NoProbe()).outputFormat
                                == CommandLineOptions::OutputFormat::Binary, L"--flat");
            Assert::IsTrue (CommandLineParser::Parse (dos.Count(), dos.Data(), NoProbe()).outputFormat
                                == CommandLineOptions::OutputFormat::DosBinary, L"--dos-bin");
        }

        //  The `/` forms deliberately do NOT get the same refusal. `/oFILE` is
        //  the glued form as65 itself documents, so `/out` genuinely means
        //  `-o ut` in the grammar this mode exists to be compatible with --
        //  and as65 compatibility outranks uniformity here by decision.
        TEST_METHOD (AssemblyMode_LeavesTheSlashFormAlone_BecauseAs65GluesValuesToFlags)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "/oout.bin" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean);
            Assert::AreEqual (std::string ("out.bin"), opts.outputFile);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PathPredicateTests
    //
    //  The two path tests the executable shares with the parser.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (PathPredicateTests)
    {
    public:
        TEST_METHOD (IsAssemblySource_AcceptsEverySourceExtension)
        {
            Assert::IsTrue (CommandLineParser::IsAssemblySource ("a.asm"));
            Assert::IsTrue (CommandLineParser::IsAssemblySource ("a.s"));
            Assert::IsTrue (CommandLineParser::IsAssemblySource ("a.a65"));
            Assert::IsTrue (CommandLineParser::IsAssemblySource ("a.a65c"));
        }

        TEST_METHOD (IsAssemblySource_IsCaseInsensitive)
        {
            Assert::IsTrue (CommandLineParser::IsAssemblySource ("BUILD.A65"));
        }

        TEST_METHOD (IsAssemblySource_RejectsBinaries)
        {
            Assert::IsFalse (CommandLineParser::IsAssemblySource ("a.bin"));
            Assert::IsFalse (CommandLineParser::IsAssemblySource ("a.s19"));
            Assert::IsFalse (CommandLineParser::IsAssemblySource ("noextension"));
        }

        TEST_METHOD (EndsWith_RejectsSuffixLongerThanSubject)
        {
            Assert::IsFalse (CommandLineParser::EndsWith ("a", ".a65"));
        }
    };



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DiskSubcommandTests
    //
    //  The `disk` grammar. Its options are NESTED rather than flattened into the
    //  top-level struct, because a verb, an image path, and an encoding selector
    //  mean nothing to any other subcommand -- and nothing already in that struct
    //  means anything to `disk`. Every assertion here goes through `opts.disk`,
    //  which is the boundary being kept visible.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DiskSubcommandTests)
    {
    public:
        TEST_METHOD (Disk_SelectsTheSubcommand)
        {
            ArgVector           args = { "CassoCli", "disk", "list", "my.dsk" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.subcommand == CommandLineOptions::Subcommand::Disk);
            Assert::IsTrue (opts.disk.verb  == CommandLineOptions::DiskOptions::Verb::List);
            Assert::AreEqual (std::string ("my.dsk"), opts.disk.imagePath);
        }

        TEST_METHOD (Disk_TerseAliasesResolveToTheDescriptiveVerbs)
        {
            // `ls` and `rm` are what fingers type; the descriptive words are
            // what help displays. They must be the same verb, not two.
            ArgVector           lsArgs = { "CassoCli", "disk", "ls", "my.dsk" };
            ArgVector           rmArgs = { "CassoCli", "disk", "rm", "my.dsk", "PROG" };
            CommandLineOptions  ls     = CommandLineParser::Parse (lsArgs.Count(), lsArgs.Data(), NoProbe());
            CommandLineOptions  rm     = CommandLineParser::Parse (rmArgs.Count(), rmArgs.Data(), NoProbe());

            Assert::IsTrue (ls.disk.verb == CommandLineOptions::DiskOptions::Verb::List);
            Assert::IsTrue (rm.disk.verb == CommandLineOptions::DiskOptions::Verb::Delete);
        }

        TEST_METHOD (Disk_CatListsTheDisk_BecauseThatIsWhatCatDoesOnAnAppleII)
        {
            // This pins a reversal. `cat` was left out on the grounds that it
            // collides with the Unix meaning of printing a file, which weighed
            // a convention from another platform above the literal command of
            // the machine this tool exists to serve. On an Apple II, CAT lists
            // the disk -- and somebody who used one types it first.
            ArgVector           args = { "CassoCli", "disk", "cat", "my.dsk" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.disk.verb == CommandLineOptions::DiskOptions::Verb::List);
        }

        TEST_METHOD (Disk_AWordThatIsNoVerbAtAll_StillResolvesToNothing)
        {
            // The aliases widened the table; they must not have turned it into
            // one that accepts anything.
            ArgVector           args = { "CassoCli", "disk", "frobnicate", "my.dsk" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.disk.verb == CommandLineOptions::DiskOptions::Verb::None);
        }

        TEST_METHOD (Disk_GetTakesAnOnDiskPathAndAnOutputFile)
        {
            ArgVector           args = { "CassoCli", "disk", "get", "my.dsk", "PROG", "--out", "prog.bin" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.disk.verb == CommandLineOptions::DiskOptions::Verb::Get);
            Assert::AreEqual (std::string ("my.dsk"),   opts.disk.imagePath);
            Assert::AreEqual (std::string ("PROG"),     opts.disk.path);
            Assert::AreEqual (std::string ("prog.bin"), opts.disk.hostFile);
        }

        TEST_METHOD (Disk_PutsSecondOperandIsAHostFileNotAnOnDiskPath)
        {
            // The asymmetry is inherent: put is the only verb whose second
            // operand lives on the host. --as names the file on the disk.
            ArgVector           args = { "CassoCli", "disk", "put", "my.dsk", "prog.bin",
                                         "--as", "PROG", "--type", "B", "--addr", "$6000" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.disk.verb == CommandLineOptions::DiskOptions::Verb::Put);
            Assert::AreEqual (std::string ("prog.bin"), opts.disk.hostFile);
            Assert::AreEqual (std::string ("PROG"),     opts.disk.path);
            Assert::AreEqual (std::string ("B"),        opts.disk.typeName);
            Assert::IsTrue   (opts.disk.hasLoadAddress);
            Assert::AreEqual ((Word) 0x6000,            opts.disk.loadAddress);
        }

        TEST_METHOD (Disk_LoadAddressZeroIsDistinguishableFromUnspecified)
        {
            // $0000 is a legal load address, which is why the has-flag exists.
            ArgVector           given  = { "CassoCli", "disk", "put", "my.dsk", "p.bin", "--addr", "$0000" };
            ArgVector           absent = { "CassoCli", "disk", "put", "my.dsk", "p.bin" };
            CommandLineOptions  a      = CommandLineParser::Parse (given.Count(),  given.Data(),  NoProbe());
            CommandLineOptions  b      = CommandLineParser::Parse (absent.Count(), absent.Data(), NoProbe());

            Assert::IsTrue  (a.disk.hasLoadAddress);
            Assert::AreEqual ((Word) 0, a.disk.loadAddress);
            Assert::IsFalse (b.disk.hasLoadAddress);
            Assert::AreEqual ((Word) 0, b.disk.loadAddress);
        }

        TEST_METHOD (Disk_EncodingDefaultsToVerbatim)
        {
            // Verbatim means no CHARACTER conversion -- the lossless path. It
            // is reached by naming neither conversion, and by nothing else:
            // there is no flag that writes it.
            ArgVector           args = { "CassoCli", "disk", "get", "my.dsk", "PROG" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Verbatim);
            Assert::IsTrue (opts.parseVerdict  == CommandLineOptions::ParseVerdict::Clean);
        }

        TEST_METHOD (Disk_EncodingSelectorsAreRecognized)
        {
            ArgVector           textArgs  = { "CassoCli", "disk", "get", "my.dsk", "T", "--text" };
            ArgVector           basicArgs = { "CassoCli", "disk", "get", "my.dsk", "B", "--basic" };
            CommandLineOptions  text      = CommandLineParser::Parse (textArgs.Count(),  textArgs.Data(),  NoProbe());
            CommandLineOptions  basic     = CommandLineParser::Parse (basicArgs.Count(), basicArgs.Data(), NoProbe());

            Assert::IsTrue (text.disk.encoding  == CommandLineOptions::DiskOptions::Encoding::Text);
            Assert::IsTrue (basic.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Basic);
        }

        //  `--verbatim` and `--long` are GONE, and neither may come back as a
        //  silently swallowed operand.
        //
        //  `--verbatim` selected the default, so once verbatim was the default
        //  its only surviving effect was cancelling a `--text` or `--basic`
        //  earlier on the same line -- which nothing needs. `--long` withheld
        //  two ProDOS columns the volume fills whether or not anyone asks.
        TEST_METHOD (Disk_RetiredOptionsAreRefused_NotCountedAsOperands)
        {
            const char *  kRetired[] = { "--verbatim", "--long" };

            for (const char * flag : kRetired)
            {
                ArgVector           args = { "CassoCli", "disk", "list", "my.dsk", flag };
                CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                    (std::wstring (L"not refused: ") + Widen (flag)).c_str());

                //  The failure this forbids: an unrecognized flag counted as a
                //  positional, which for `list` means it lands nowhere and the
                //  command runs as though it were never typed.
                Assert::AreEqual (std::string ("my.dsk"), opts.disk.imagePath,
                    L"and it did not displace or follow the image path");
            }
        }

        //
        //  `-o` IS AS65'S FLAG AND STAYS THERE. `disk` takes `--out`, and the
        //  two are deliberately not unified -- as65 argument compatibility wins
        //  over uniformity by decision. What must not happen is the collision
        //  passing unremarked.
        //
        //  It did. `-o` and the filename after it fell into the positional
        //  block as operands three and four, which this grammar has none of, so
        //  both were dropped: the extracted file went to standard output, the
        //  name the caller gave was never opened, and the exit status said the
        //  command had worked.
        //
        TEST_METHOD (Disk_RefusesTheAssemblersDashO_RatherThanSwallowingItAndItsValue)
        {
            ArgVector           args = { "CassoCli", "disk", "get", "my.dsk", "PROG",
                                         "-o", "prog.bin" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                L"refused, rather than run with the flag and its value discarded");

            Assert::AreEqual (std::string(), opts.disk.hostFile,
                L"and `-o` did not quietly become --out either -- it is refused, not aliased");
        }

        //  A ProDOS path is written with a leading slash and is an operand, so
        //  the refusal above tests a DASH and not merely a flag-looking word.
        //  Refusing every `/...` would lose the path this grammar most needs.
        TEST_METHOD (Disk_RefusalDoesNotReachAProDosPath)
        {
            ArgVector           args = { "CassoCli", "disk", "get", "my.po", "/VOLUME/STARTUP" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean);
            Assert::AreEqual (std::string ("/VOLUME/STARTUP"), opts.disk.path);
        }

        TEST_METHOD (Disk_DoesNotDisturbTheAssemblerFields)
        {
            // The whole point of nesting. A disk invocation must leave the
            // assembler-shaped fields at their defaults, so a later reader
            // cannot mistake one subcommand's state for another's.
            ArgVector           args = { "CassoCli", "disk", "put", "my.dsk", "prog.bin", "--addr", "$6000" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue   (opts.inputFile.empty(),  L"disk does not set the assembler's input");
            Assert::IsTrue   (opts.outputFile.empty(), L"nor its output");
            Assert::IsFalse  (opts.hasLoadAddress,     L"--addr belongs to the disk options only");
            Assert::AreEqual ((Word) 0x8000, opts.loadAddress, L"the assembler default is untouched");
        }

        TEST_METHOD (Disk_AcceptsEveryAliasTheHelpOffers_ForTheVerbItAliases)
        {
            //  THREE HABITS, ALL OF THEM REAL. `catalog` and `cat` are the
            //  words the machines themselves answer to, so anyone who used one
            //  types them before anything else; `dir` and `del` are what the
            //  host shell trained them to type; `ls` and `rm` are what a Unix
            //  shell did. A sweep rather than a sample, because an alias in the
            //  table and not in the grammar is exactly what this catches.
            struct { const char * word; CommandLineOptions::DiskOptions::Verb verb; }
            kSpellings[] =
            {
                { "list",    CommandLineOptions::DiskOptions::Verb::List   },
                { "ls",      CommandLineOptions::DiskOptions::Verb::List   },
                { "dir",     CommandLineOptions::DiskOptions::Verb::List   },
                { "cat",     CommandLineOptions::DiskOptions::Verb::List   },
                { "catalog", CommandLineOptions::DiskOptions::Verb::List   },
                { "get",     CommandLineOptions::DiskOptions::Verb::Get    },
                { "read",    CommandLineOptions::DiskOptions::Verb::Get    },
                { "put",     CommandLineOptions::DiskOptions::Verb::Put    },
                { "write",   CommandLineOptions::DiskOptions::Verb::Put    },
                { "delete",  CommandLineOptions::DiskOptions::Verb::Delete },
                { "rm",      CommandLineOptions::DiskOptions::Verb::Delete },
                { "del",     CommandLineOptions::DiskOptions::Verb::Delete },
                { "boot",    CommandLineOptions::DiskOptions::Verb::Boot   },
            };

            for (const auto & form : kSpellings)
            {
                ArgVector           args = { "CassoCli", "disk", form.word, "my.dsk" };
                CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.disk.verb == form.verb,
                    (std::wstring (L"unrecognized form: ") +
                     std::wstring (form.word, form.word + strlen (form.word))).c_str());

                Assert::AreEqual (std::string ("my.dsk"), opts.disk.imagePath,
                    L"the image is still the first positional after any form");
            }

            Assert::AreEqual (size_t (13), CommandLineParser::GetAllDiskVerbs().size(),
                L"and the table holds exactly the forms swept above");
        }

        //  `disk --help` used to reach the verb table, be told `--help` is not
        //  a verb, and answer a question about the grammar with a complaint
        //  about the grammar. Help was recognized only as argv[1].
        //
        //  `-h` IS AMONG THEM AND IS SAFE HERE. The page height it collides
        //  with at the top level exists only inside the assembler's flag walk,
        //  and no argument of this grammar ever reaches that walk -- so the two
        //  characters a reader most likely types are free to mean help.
        TEST_METHOD (Disk_TakesAHelpRequestInEverySpelling_NotOnlyAsTheFirstArgument)
        {
            const char *  kSpellings[] = { "--help", "-help", "-?", "-h",
                                           "/help",  "/?",    "/h" };

            for (const char * form : kSpellings)
            {
                ArgVector           args = { "CassoCli", "disk", form };
                CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.disk.verb == CommandLineOptions::DiskOptions::Verb::Help,
                    (std::wstring (L"not read as a help request: ") +
                     std::wstring (form, form + strlen (form))).c_str());
            }
        }

        //  The prefix the reader typed reaches the disk help, which is what
        //  lets it write itself back the same way.
        TEST_METHOD (Disk_HelpRequestWithASlash_RecordsTheSlashPrefix)
        {
            ArgVector           args = { "CassoCli", "disk", "/?" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual ('/', opts.flagPrefix);
        }

        //  A verb standing before it does not make the request go away: a
        //  reader who has typed half a command and wants the grammar back adds
        //  --help to the end of what they have.
        TEST_METHOD (Disk_HelpRequestAfterAVerb_IsStillAHelpRequest)
        {
            ArgVector           args = { "CassoCli", "disk", "list", "my.dsk", "--help" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.disk.verb == CommandLineOptions::DiskOptions::Verb::Help);
        }

        //  THE ONE THING THE HELP FORMS MUST NOT SWALLOW. A ProDOS path
        //  begins with a slash, and `/HELP` is a legal volume.
        TEST_METHOD (Disk_ProDosPathNamedHelp_IsStillAPath)
        {
            ArgVector           args = { "CassoCli", "disk", "get", "d.po", "/HELP/STARTUP" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.disk.verb == CommandLineOptions::DiskOptions::Verb::Get);
            Assert::AreEqual (std::string ("/HELP/STARTUP"), opts.disk.path);
        }

        TEST_METHOD (Disk_TakesEitherPrefixOnEveryOption_SoTheHelpMaySpellEither)
        {
            const char *  kOptions[] = { "text", "basic" };

            for (const char * option : kOptions)
            {
                std::string         slash = std::string ("/") + option;
                std::string         dash  = std::string ("--") + option;
                ArgVector           slashArgs = { "CassoCli", "disk", "list", "my.dsk", slash.c_str() };
                ArgVector           dashArgs  = { "CassoCli", "disk", "list", "my.dsk", dash.c_str() };
                CommandLineOptions  slashed   = CommandLineParser::Parse (slashArgs.Count(), slashArgs.Data(), NoProbe());
                CommandLineOptions  dashed    = CommandLineParser::Parse (dashArgs.Count(),  dashArgs.Data(),  NoProbe());

                //  Neither form may be read as the image path, which is
                //  what an unrecognized flag would silently become.
                Assert::AreEqual (std::string ("my.dsk"), slashed.disk.imagePath,
                    L"the slash form is a flag, not a positional");
                Assert::IsTrue (slashed.disk.encoding == dashed.disk.encoding,
                    L"and it reaches the same arm as the dash form");
                Assert::IsTrue (slashed.parseVerdict == CommandLineOptions::ParseVerdict::Clean,
                    L"and neither prefix is refused");
            }
        }

        //  The value-taking options, which the loop above cannot cover: they
        //  consume the argument after them, so a `list` command line carrying
        //  one would be measuring something else.
        TEST_METHOD (Disk_TakesEitherPrefixOnTheValueTakingOptionsToo)
        {
            ArgVector           slashArgs = { "CassoCli", "disk", "put", "my.dsk", "p.bin",
                                              "/as", "PROG", "/type", "B", "/addr", "$6000" };
            ArgVector           dashArgs  = { "CassoCli", "disk", "put", "my.dsk", "p.bin",
                                              "--as", "PROG", "--type", "B", "--addr", "$6000" };
            CommandLineOptions  slashed   = CommandLineParser::Parse (slashArgs.Count(), slashArgs.Data(), NoProbe());
            CommandLineOptions  dashed    = CommandLineParser::Parse (dashArgs.Count(),  dashArgs.Data(),  NoProbe());

            Assert::AreEqual (dashed.disk.path,        slashed.disk.path);
            Assert::AreEqual (dashed.disk.typeName,    slashed.disk.typeName);
            Assert::AreEqual (dashed.disk.loadAddress, slashed.disk.loadAddress);
            Assert::AreEqual (std::string ("p.bin"),   slashed.disk.hostFile,
                L"and no value was left standing as an operand");
        }

        TEST_METHOD (As65_TakesEitherPrefixOnItsLongOptions_IncludingAnAttachedValue)
        {
            ArgVector           flat   = { "CassoCli", "as65", "prog.a65", "/flat" };
            ArgVector           dosBin = { "CassoCli", "as65", "prog.a65", "/dos-bin" };
            ArgVector           cpu    = { "CassoCli", "as65", "prog.a65", "/cpu", "65c02" };
            ArgVector           glued  = { "CassoCli", "as65", "prog.a65", "/cpu=65c02" };
            CommandLineOptions  cpuOpts   = CommandLineParser::Parse (cpu.Count(), cpu.Data(), NoProbe());
            CommandLineOptions  gluedOpts = CommandLineParser::Parse (glued.Count(), glued.Data(), NoProbe());

            //  `/flat` is the newest long option and would be read as the
            //  concatenated flags -f -l -a -t without the table -- which sets a
            //  listing file named `at` and complains about two flags that do
            //  not exist, all while writing the shape it was told not to.
            Assert::IsTrue (CommandLineParser::Parse (flat.Count(), flat.Data(), NoProbe()).outputFormat
                                == CommandLineOptions::OutputFormat::Binary, L"/flat");
            Assert::IsTrue (CommandLineParser::Parse (dosBin.Count(), dosBin.Data(), NoProbe()).outputFormat
                                == CommandLineOptions::OutputFormat::DosBinary, L"/dos-bin");
            //  `/cpu` IS WITHDRAWN WITH `--cpu`, and is matched by name in both
            //  prefixes rather than left to the concatenation walk -- which
            //  would read it as -c -p -u, a true reading of as65's grammar and
            //  a useless answer to somebody migrating off the flag.
            Assert::IsTrue (cpuOpts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                            L"/cpu 65c02");
            Assert::IsTrue (gluedOpts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                            L"/cpu=65c02");
            Assert::IsFalse (cpuOpts.cycleCounts,
                             L"and is not read as the concatenated -c -p -u");
        }

        TEST_METHOD (Run_TakesEitherPrefixOnItsLongOptions_TooBecauseTheHelpSpellsThem)
        {
            //  `/load` used to normalize to `-load`, match nothing, and be
            //  reported as an unknown option -- while the usage text under `/?`
            //  offered exactly that form.
            ArgVector           args = { "CassoCli", "run", "prog.bin",
                                         "/load", "$2000", "/reset-vector" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue   (opts.hasLoadAddress,  L"/load is recognized");
            Assert::AreEqual ((Word) 0x2000, opts.loadAddress);
            Assert::IsTrue   (opts.useResetVector,  L"and so is /reset-vector");
            Assert::AreEqual (std::string ("prog.bin"), opts.inputFile,
                L"and neither was mistaken for the input file");
        }

        TEST_METHOD (Disk_LeavesAProDosPathAlone_EvenThoughItBeginsWithASlash)
        {
            //  THIS IS WHY THE SLASH FORM IS A TABLE LOOKUP AND NOT A REWRITE.
            //  A ProDOS path is written with a leading slash; a parser that
            //  turned every one of them into a flag would lose the operand.
            ArgVector           args = { "CassoCli", "disk", "get", "my.po", "/VOLUME/STARTUP" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("/VOLUME/STARTUP"), opts.disk.path);
        }

        TEST_METHOD (BareWordThatIsNotASubcommand_StaysAs65)
        {
            // Adding a row must not turn an unrecognized first argument into an
            // error: it is a source filename, which is how as65 was invoked.
            ArgVector           args = { "CassoCli", "as65", "disky.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.subcommand == CommandLineOptions::Subcommand::As65);
        }
    };
}
