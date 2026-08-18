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
    //  spelling ever is.
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
        //  question mark". The prefixed spellings were already accepted; the
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
            Assert::AreEqual (std::string ("?"), opts.inputFile);
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
            ArgVector           args = { "CassoCli", "demo.a65" };
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
    //  BOTH PREFIXES ARE SWEPT EVERYWHERE, because a page spells itself with
    //  the prefix the reader typed: a route accepted in only one of them offers
    //  the other back in text and then refuses it on the next command line.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (HelpRoutingTests)
    {
    public:
        //  Every spelling of "help" that both grammars beneath the top level
        //  answer to. `-h` is among them and is safe there: the page height it
        //  collides with lives in the assembler's own flag walk, which no
        //  argument of `run` or `disk` ever reaches.
        static std::vector<std::string> Spellings()
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
            for (const std::string & spelling : Spellings())
            {
                CommandLineOptions  opts = ParseTyped ({ "CassoCli", spelling.c_str() });

                Assert::IsTrue (opts.showHelp,
                    (L"not read as a help request: " + Widen (spelling)).c_str());
                Assert::IsTrue (opts.helpPage == CommandLineOptions::HelpPage::General,
                    (L"did not open the general page: " + Widen (spelling)).c_str());
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

            for (const char * spelling : kSlashed)
            {
                CommandLineOptions  opts = ParseTyped ({ "CassoCli", spelling });

                Assert::AreEqual ('/', opts.flagPrefix,
                    (L"the prefix was not remembered: " + Widen (spelling)).c_str());
            }
        }

        //  A LONE `?` IS THE ONLY ROUTE TO THE ASSEMBLER'S PAGE, so every other
        //  spelling has to land somewhere else even when a source file is
        //  standing on the command line beside it.
        TEST_METHOD (HelpBesideASourceFile_StillOpensTheGeneralPage)
        {
            CommandLineOptions  opts = ParseTyped ({ "CassoCli", "prog.a65", "--help" });

            Assert::IsTrue (opts.showHelp);
            Assert::IsTrue (opts.helpPage == CommandLineOptions::HelpPage::General,
                            L"only a lone ? reaches the assembler's page");
        }

        //  `run --help` was an option this grammar does not have: a diagnostic,
        //  a refusal, and exit 2 -- answering a question the tool knows the
        //  answer to by complaining about being asked.
        TEST_METHOD (RunTakesAHelpRequestInEverySpelling_AndOpensTheRunPage)
        {
            for (const std::string & spelling : Spellings())
            {
                CommandLineOptions  opts = ParseTyped ({ "CassoCli", "run", spelling.c_str() });

                Assert::IsTrue (opts.showHelp,
                    (L"not read as a help request: " + Widen (spelling)).c_str());
                Assert::IsTrue (opts.helpPage == CommandLineOptions::HelpPage::Run,
                    (L"did not open the run page: " + Widen (spelling)).c_str());
                Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean,
                    (L"asking for help is not a mistake: " + Widen (spelling)).c_str());
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
            for (const std::string & spelling : Spellings())
            {
                CommandLineOptions  opts = ParseTyped ({ "CassoCli", "disk", spelling.c_str() });

                Assert::IsTrue (opts.subcommand == CommandLineOptions::Subcommand::Disk,
                    (L"left the disk grammar: " + Widen (spelling)).c_str());
                Assert::IsTrue (opts.disk.verb == CommandLineOptions::DiskOptions::Verb::Help,
                    (L"not read as a help request: " + Widen (spelling)).c_str());
                Assert::IsFalse (opts.showHelp,
                    (L"the general page would be printed over it: " + Widen (spelling)).c_str());
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
            //  and as65 spells it bare.
            Assert::IsTrue (slashed.find ("CassoCli ?")          != std::string::npos);
            Assert::IsTrue (slashed.find ("CassoCli run /help")  != std::string::npos);
            Assert::IsTrue (slashed.find ("CassoCli disk /help") != std::string::npos);
            Assert::IsTrue (slashed.find ("CassoCli /version")   != std::string::npos);
            Assert::IsTrue (slashed.find ("--help")              == std::string::npos,
                            L"and never the spelling the reader did not type");
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
            ArgVector           args = { "CassoCli", "demo.a65", "-tlfile.lst" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.symbolTable,     L"-t must be seen inside -tlfile.lst");
            Assert::IsTrue (opts.generateListing, L"-l must be seen inside -tlfile.lst");
            Assert::AreEqual (std::string ("file.lst"), opts.listingFile);
        }

        TEST_METHOD (ListingFlagAlone_GoesToStdout)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-l" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.generateListing);
            Assert::IsTrue (opts.listingToStdout);
            Assert::IsTrue (opts.listingFile.empty());
        }

        TEST_METHOD (ListingFlag_TakesSeparatedFilename)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-l", "out.lst" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("out.lst"), opts.listingFile);
            Assert::IsFalse (opts.listingToStdout);
        }

        TEST_METHOD (SlashPrefix_IsAcceptedAndRemembered)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "/t" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.symbolTable, L"/t must mean what -t means");
        }

        //  -i IS STILL A NO-OP, and this test is about its NAME rather than its
        //  effect. as65's -i means "Ignore case in opcodes"; the field it sets
        //  was called caseSensitive and was set to TRUE, so the record of the
        //  flag stated the opposite of the flag. Whoever implements it reads
        //  this field first, and an inverted name is how a no-op becomes a bug.
        TEST_METHOD (IgnoreCaseFlag_RecordsThatCaseIsToBeIgnored)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-i" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.ignoreOpcodeCase);
        }

        TEST_METHOD (WithoutTheIgnoreCaseFlag_CaseIsNotToBeIgnored)
        {
            ArgVector           args = { "CassoCli", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsFalse (opts.ignoreOpcodeCase);
        }

        TEST_METHOD (FillZeroFlag_SetsZeroFill)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-z" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.fillZero);
            Assert::AreEqual ((int) 0x00, (int) opts.fillByte);
        }

        TEST_METHOD (DefaultFill_IsFF)
        {
            ArgVector           args = { "CassoCli", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual ((int) 0xFF, (int) opts.fillByte);
        }

        TEST_METHOD (PredefineWithValue_IsRecorded)
        {
            ArgVector           args  = { "CassoCli", "demo.a65", "-dDEBUG=5" };
            CommandLineOptions  opts  = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());
            auto                found = opts.predefinedSymbols.find ("DEBUG");
            bool                isSet = found != opts.predefinedSymbols.end();

            Assert::IsTrue (isSet);
            Assert::AreEqual ((int32_t) 5, found->second);
        }

        //  A bare -d NAME defines the symbol as 1, so `ifdef` works without the
        //  caller inventing a value.
        TEST_METHOD (PredefineWithoutValue_DefaultsToOne)
        {
            ArgVector           args  = { "CassoCli", "demo.a65", "-d", "DEBUG" };
            CommandLineOptions  opts  = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());
            auto                found = opts.predefinedSymbols.find ("DEBUG");
            bool                isSet = found != opts.predefinedSymbols.end();

            Assert::IsTrue (isSet);
            Assert::AreEqual ((int32_t) 1, found->second);
        }

        //  as65: "If no name is specified, DEBUG is defined. The label is
        //  EQUated to be 1." A bare -d used to define nothing at all when it
        //  ended the command line, which is the one place the default was
        //  unambiguously being asked for.
        TEST_METHOD (PredefineWithNoNameAtAll_DefinesDebugAsOne)
        {
            ArgVector           args  = { "CassoCli", "demo.a65", "-d" };
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
        //  had eaten.
        TEST_METHOD (PredefineBeforeTheSourceFile_LeavesTheSourceFileAlone)
        {
            ArgVector           args  = { "CassoCli", "-d", "demo.a65" };
            CommandLineOptions  opts  = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());
            bool                named = opts.predefinedSymbols.find ("demo.a65") != opts.predefinedSymbols.end();

            Assert::AreEqual (std::string ("demo.a65"), opts.inputFile, L"the source is still the source");
            Assert::IsFalse (named, L"and is not a symbol name");
            Assert::IsTrue (opts.predefinedSymbols.find ("DEBUG") != opts.predefinedSymbols.end(),
                            L"the bare flag still means DEBUG");
        }

        //  The other thing that follows a bare -d is the next flag, and eating
        //  one costs the argument BEHIND it too: `-d -o out.bin` defined a
        //  label called `-o`, so `out.bin` was never read as an output name and
        //  the derived name was written instead -- a file the caller did not
        //  ask for, under a name they did not choose, reported as success.
        TEST_METHOD (PredefineBeforeAnotherFlag_LeavesThatFlagAlone)
        {
            ArgVector           args  = { "CassoCli", "demo.a65", "-d", "-o", "out.bin" };
            CommandLineOptions  opts  = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("out.bin"), opts.outputFile, L"-o kept its value");
            Assert::IsTrue (opts.predefinedSymbols.find ("DEBUG") != opts.predefinedSymbols.end());
            Assert::IsTrue (opts.predefinedSymbols.find ("-o") == opts.predefinedSymbols.end(),
                            L"a flag is not a symbol name");
        }

        TEST_METHOD (PageWidthFlag_TakesAttachedValue)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-w133" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (133, opts.pageWidth);
        }

        //  The separated spelling is the one the usage text has always shown --
        //  `-h <lines>` -- and it was the one being thrown away. Only the glued
        //  form was read, so `-h 10` set nothing and said nothing.
        TEST_METHOD (PageHeightFlag_TakesASeparatedValue_TheFormTheHelpDocuments)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-h", "10" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (10, opts.pageHeight);
        }

        TEST_METHOD (PageHeightFlag_TakesAnAttachedValue)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-h10" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (10, opts.pageHeight);
        }

        //  The separated form must not eat whatever happens to follow. A count
        //  is a number, and the input file is what a bare -h is usually in
        //  front of.
        TEST_METHOD (PageHeightFlag_LeavesANonNumericNeighborAlone)
        {
            //  -q leads only because a bare -h in the first position is the
            //  top-level help spelling and never reaches this grammar at all.
            ArgVector           args = { "CassoCli", "-q", "-h", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("demo.a65"), opts.inputFile,
                              L"the source file is not the page height");
            Assert::AreEqual (0, opts.pageHeight, L"and no pagination was asked for");
        }

        TEST_METHOD (PageWidthFlag_TakesASeparatedValue)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-w", "100" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (100, opts.pageWidth);
        }

        //  A bare -w means the wide listing, which is what the help says it
        //  means. It used to mean nothing at all and leave the default width.
        TEST_METHOD (PageWidthFlagAlone_SelectsTheWideListing)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-w" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (CommandLineParser::kWideListingColumns, opts.pageWidth);
        }

        //  -g took its filename glued on and silently dropped a separated one,
        //  writing the derived name instead. A file the user named and never
        //  got is worse than a refusal.
        TEST_METHOD (DebugFlag_TakesASeparatedFileName)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-g", "out.dbg" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.debugInfo);
            Assert::AreEqual (std::string ("out.dbg"), opts.debugFile);
        }

        TEST_METHOD (DebugFlag_TakesAnAttachedFileName)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-gout.dbg" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("out.dbg"), opts.debugFile);
        }

        //  A bare -g still derives its name from the source, so the flag on its
        //  own keeps working -- and the flag before a source file must not
        //  swallow the source file.
        TEST_METHOD (DebugFlagAlone_StillDerivesTheNameFromTheSource)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-g" };
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
            ArgVector           args = { "CassoCli", "demo.a65", "-t" };
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

        //  An as65 flag nobody knows is a COMPLAINT, not a refusal: the flag is
        //  dropped and the assembly still produces its output, which is exactly
        //  what "succeeded with complaints" describes.
        TEST_METHOD (UnknownAs65Flag_IsAComplaint_BecauseTheAssemblyStillRuns)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-Y" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Complaint);
        }

        //  A --cpu target that does not exist stops parsing outright and asks
        //  for the usage text. Printing usage is the ANSWER to the mistake, not
        //  evidence there was none, so it is still a refusal.
        TEST_METHOD (UnknownCpuTarget_IsRefused_EvenThoughItAsksForUsage)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "--cpu", "6510" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.showHelp, L"the user is shown the grammar");
            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                            L"and the script is told it was wrong");
        }

        //  Asking for help is not a complaint about anything.
        TEST_METHOD (HelpRequest_LeavesNoVerdict)
        {
            ArgVector           args = { "CassoCli", "--help" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean);
        }
    };





    TEST_CLASS (CpuTargetTests)
    {
    public:
        TEST_METHOD (Default_IsStrict6502)
        {
            ArgVector           args = { "CassoCli", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.cpuTarget == CommandLineOptions::CpuTarget::M6502);
        }

        TEST_METHOD (SeparatedCpuValue_Selects65C02)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "--cpu", "65c02" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.cpuTarget == CommandLineOptions::CpuTarget::M65C02);
        }

        TEST_METHOD (AttachedCpuValue_Selects65C02)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "--cpu=65C02" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.cpuTarget == CommandLineOptions::CpuTarget::M65C02,
                            L"the value is matched case-insensitively");
        }

        TEST_METHOD (UnknownCpuTarget_RequestsHelp)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "--cpu", "6809" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.showHelp, L"a bad target must stop parsing and print usage");
        }

        //  as65: "Use 65SC02 extensions. This CPU has several additional
        //  instructions." It was not accepted at all -- the flag fell through
        //  to the unknown-flag warning, was dropped, and the source then failed
        //  to assemble on a strict 6502 with a diagnostic about the opcode
        //  rather than about the flag.
        TEST_METHOD (As65ExtensionFlag_Selects65C02)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-x" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.cpuTarget == CommandLineOptions::CpuTarget::M65C02);
            Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Clean,
                            L"and is no longer complained about as unknown");
        }

        //  It is an as65 flag, so it packs with the others the way they do.
        TEST_METHOD (As65ExtensionFlag_Concatenates)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-xq" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.cpuTarget == CommandLineOptions::CpuTarget::M65C02);
            Assert::IsTrue (opts.quiet, L"the flag after it is still read");
        }

        TEST_METHOD (As65ExtensionFlag_AcceptsTheSlashPrefix)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "/x" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.cpuTarget == CommandLineOptions::CpuTarget::M65C02);
        }

        //  BOTH NAMES REACH THE SAME TIER, and both keep working. -x is as65's
        //  name for it and --cpu is this tool's; retiring either would break a
        //  caller for no capability gained.
        TEST_METHOD (BothSpellingsOfTheCmosTier_SelectTheSameTarget)
        {
            ArgVector           as65   = { "CassoCli", "demo.a65", "-x" };
            ArgVector           modern = { "CassoCli", "demo.a65", "--cpu", "65c02" };
            CommandLineOptions  a      = CommandLineParser::Parse (as65.Count(), as65.Data(), NoProbe());
            CommandLineOptions  m      = CommandLineParser::Parse (modern.Count(), modern.Data(), NoProbe());

            Assert::IsTrue (a.cpuTarget == m.cpuTarget);
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
            ArgVector           args = { "CassoCli", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("demo.bin"), opts.outputFile);
        }

        TEST_METHOD (ExplicitOutputName_Wins)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-o", "custom.out" };
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
        TEST_METHOD (TrailingOutputFlag_WithNothingAfterIt_ReturnsInsteadOfSpinning)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-o" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("demo.a65"), opts.inputFile,
                L"the input survives");
            Assert::AreEqual (std::string ("demo.bin"), opts.outputFile,
                L"and the name falls back to the inferred one, as though -o were absent");
        }

        TEST_METHOD (SRecordFlag_InfersS19Extension)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-s" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::SRecord);
            Assert::AreEqual (std::string ("demo.s19"), opts.outputFile);
        }

        TEST_METHOD (IntelHexFlag_InfersHexExtension)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-s2" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::IntelHex);
            Assert::AreEqual (std::string ("demo.hex"), opts.outputFile);
        }

        TEST_METHOD (DebugFlag_InfersDbgExtension)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-g" };
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
            ArgVector           args = { "CassoCli", "build" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), ProbeFor ("build.a65"));

            Assert::AreEqual (std::string ("build.a65"), opts.inputFile);
            Assert::AreEqual (std::string ("build.bin"), opts.outputFile,
                              L"the derived name follows the RESOLVED input");
        }

        TEST_METHOD (ExtensionlessInput_FallsThroughToAsm)
        {
            ArgVector           args = { "CassoCli", "build" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), ProbeFor ("build.asm"));

            Assert::AreEqual (std::string ("build.asm"), opts.inputFile);
        }

        TEST_METHOD (ExtensionlessInput_WithNoMatch_IsLeftAsTyped)
        {
            ArgVector           args = { "CassoCli", "build" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("build"), opts.inputFile,
                              L"reported against the name the user actually typed");
        }

        TEST_METHOD (InputWithExtension_IsNeverProbed)
        {
            ArgVector           args = { "CassoCli", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), ProbeFor ("demo.a65.a65"));

            Assert::AreEqual (std::string ("demo.a65"), opts.inputFile);
        }

        //  A dot in a DIRECTORY name is not an extension, so this path is still
        //  a candidate for auto-extension.
        TEST_METHOD (DotInDirectoryName_DoesNotCountAsExtension)
        {
            ArgVector           args = { "CassoCli", "src/v1.2/build" };
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
    //  NOTHING SPELLS THE DEFAULT. `--raw` did for one revision, on the
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
            ArgVector           args = { "CassoCli", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::Raw,
                L"naming no shape writes only what was assembled");
            Assert::IsFalse (opts.outputFormatNamed,
                L"and nothing was named, which is a separate fact from which shape it is");
        }

        TEST_METHOD (FlatFlag_SelectsTheFullPaddedImage)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "--flat" };
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
            ArgVector           args      = { "CassoCli", "demo.a65", "--raw" };
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
            ArgVector           silent = { "CassoCli", "demo.a65", "-o", "out.s19" };
            ArgVector           spoken = { "CassoCli", "demo.a65", "--flat", "-o", "out.s19" };
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
            ArgVector           args = { "CassoCli", "demo.a65", "--dos-bin" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::DosBinary);
        }

        TEST_METHOD (ShapeFlag_ComposesWithOtherFlags)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "--flat", "-t", "-o", "out.obj" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::Binary);
            Assert::IsTrue (opts.symbolTable);
            Assert::AreEqual (std::string ("out.obj"), opts.outputFile);
        }

        TEST_METHOD (ShapeFlag_DoesNotConsumeTheInputFile)
        {
            ArgVector           args = { "CassoCli", "--dos-bin", "demo.a65" };
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
            ArgVector           args = { "CassoCli", "demo.a65", "--out", "demo.bin" };
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
                ArgVector           args = { "CassoCli", "demo.a65", flag };
                CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                    (std::wstring (L"not refused: ") + Widen (flag)).c_str());
            }
        }

        //  And the three it DOES have still work, which is what keeps the
        //  refusal above from being a blanket ban on the `--` prefix.
        TEST_METHOD (AssemblyMode_StillTakesTheLongOptionsItDoesHave)
        {
            ArgVector           cpu  = { "CassoCli", "demo.a65", "--cpu", "65c02" };
            ArgVector           flat = { "CassoCli", "demo.a65", "--flat" };
            ArgVector           dos  = { "CassoCli", "demo.a65", "--dos-bin" };

            Assert::IsTrue (CommandLineParser::Parse (cpu.Count(), cpu.Data(), NoProbe()).cpuTarget
                                == CommandLineOptions::CpuTarget::M65C02, L"--cpu");
            Assert::IsTrue (CommandLineParser::Parse (flat.Count(), flat.Data(), NoProbe()).outputFormat
                                == CommandLineOptions::OutputFormat::Binary, L"--flat");
            Assert::IsTrue (CommandLineParser::Parse (dos.Count(), dos.Data(), NoProbe()).outputFormat
                                == CommandLineOptions::OutputFormat::DosBinary, L"--dos-bin");
        }

        //  The `/` forms deliberately do NOT get the same refusal. `/oFILE` is
        //  the glued spelling as65 itself documents, so `/out` genuinely means
        //  `-o ut` in the grammar this mode exists to be compatible with --
        //  and as65 compatibility outranks uniformity here by decision.
        TEST_METHOD (AssemblyMode_LeavesTheSlashFormAlone_BecauseAs65GluesValuesToFlags)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "/oout.bin" };
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
            // there is no flag that spells it.
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

        //  A ProDOS path is spelled with a leading slash and is an operand, so
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

            for (const auto & spelling : kSpellings)
            {
                ArgVector           args = { "CassoCli", "disk", spelling.word, "my.dsk" };
                CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.disk.verb == spelling.verb,
                    (std::wstring (L"unrecognized spelling: ") +
                     std::wstring (spelling.word, spelling.word + strlen (spelling.word))).c_str());

                Assert::AreEqual (std::string ("my.dsk"), opts.disk.imagePath,
                    L"the image is still the first positional after any spelling");
            }

            Assert::AreEqual (size_t (13), CommandLineParser::GetAllDiskVerbs().size(),
                L"and the table holds exactly the spellings swept above");
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

            for (const char * spelling : kSpellings)
            {
                ArgVector           args = { "CassoCli", "disk", spelling };
                CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                Assert::IsTrue (opts.disk.verb == CommandLineOptions::DiskOptions::Verb::Help,
                    (std::wstring (L"not read as a help request: ") +
                     std::wstring (spelling, spelling + strlen (spelling))).c_str());
            }
        }

        //  The prefix the reader typed reaches the disk help, which is what
        //  lets it spell itself back the same way.
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

        //  THE ONE THING THE HELP SPELLINGS MUST NOT SWALLOW. A ProDOS path
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

                //  Neither spelling may be read as the image path, which is
                //  what an unrecognized flag would silently become.
                Assert::AreEqual (std::string ("my.dsk"), slashed.disk.imagePath,
                    L"the slash spelling is a flag, not a positional");
                Assert::IsTrue (slashed.disk.encoding == dashed.disk.encoding,
                    L"and it reaches the same arm as the dash spelling");
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
            ArgVector           flat   = { "CassoCli", "prog.a65", "/flat" };
            ArgVector           dosBin = { "CassoCli", "prog.a65", "/dos-bin" };
            ArgVector           cpu    = { "CassoCli", "prog.a65", "/cpu", "65c02" };
            ArgVector           glued  = { "CassoCli", "prog.a65", "/cpu=65c02" };

            //  `/flat` is the newest long option and would be read as the
            //  concatenated flags -f -l -a -t without the table -- which sets a
            //  listing file named `at` and complains about two flags that do
            //  not exist, all while writing the shape it was told not to.
            Assert::IsTrue (CommandLineParser::Parse (flat.Count(), flat.Data(), NoProbe()).outputFormat
                                == CommandLineOptions::OutputFormat::Binary, L"/flat");
            Assert::IsTrue (CommandLineParser::Parse (dosBin.Count(), dosBin.Data(), NoProbe()).outputFormat
                                == CommandLineOptions::OutputFormat::DosBinary, L"/dos-bin");
            Assert::IsTrue (CommandLineParser::Parse (cpu.Count(), cpu.Data(), NoProbe()).cpuTarget
                                == CommandLineOptions::CpuTarget::M65C02, L"/cpu 65c02");
            Assert::IsTrue (CommandLineParser::Parse (glued.Count(), glued.Data(), NoProbe()).cpuTarget
                                == CommandLineOptions::CpuTarget::M65C02, L"/cpu=65c02");
        }

        TEST_METHOD (Run_TakesEitherPrefixOnItsLongOptions_TooBecauseTheHelpSpellsThem)
        {
            //  `/load` used to normalize to `-load`, match nothing, and be
            //  reported as an unknown option -- while the usage text under `/?`
            //  offered exactly that spelling.
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
            //  A ProDOS path is spelled with a leading slash; a parser that
            //  turned every one of them into a flag would lose the operand.
            ArgVector           args = { "CassoCli", "disk", "get", "my.po", "/VOLUME/STARTUP" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("/VOLUME/STARTUP"), opts.disk.path);
        }

        TEST_METHOD (BareWordThatIsNotASubcommand_StaysAs65)
        {
            // Adding a row must not turn an unrecognized first argument into an
            // error: it is a source filename, which is how as65 was invoked.
            ArgVector           args = { "CassoCli", "disky.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.subcommand == CommandLineOptions::Subcommand::As65);
        }
    };
}
