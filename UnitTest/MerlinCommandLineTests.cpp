#include "Pch.h"

#include "TestHelpers.h"
#include "TestCpu65C02.h"
#include "Assembler.h"
#include "CommandLineParser.h"
#include "DialectHelp.h"
#include "DialectProfile.h"
#include "DialectRegistry.h"
#include "MerlinSubsetBoundary.h"

#include "CppUnitTest.h"




using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace MerlinCommandLineTests
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
        ArgVector (std::initializer_list<std::string> args)
        {
            for (const std::string & arg : args)
            {
                m_storage.push_back (arg);
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
    //  Fixture
    //
    //  Parsing an argv, and assembling under whatever dialect the parse landed
    //  on. The second half is the point of several tests below: a subcommand
    //  that selects a dialect nothing downstream reads would satisfy every
    //  assertion about the parse and none about the tool.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class Fixture
    {
    public:
        static CommandLineOptions Parse (std::initializer_list<std::string> args)
        {
            ArgVector  argv (args);

            return CommandLineParser::Parse (argv.Count(), argv.Data(),
                                             [] (const std::string &) { return false; });
        }



        //  A parse whose one named file is reported as existing, for the
        //  extension the source path leaves off.
        static CommandLineOptions ParseWithExisting (std::initializer_list<std::string> args,
                                                     const std::string                & existing)
        {
            ArgVector  argv (args);

            return CommandLineParser::Parse (argv.Count(), argv.Data(),
                                             [existing] (const std::string & path) { return path == existing; });
        }



        //  Assembled under the dialect the COMMAND LINE selected, rather than
        //  under one named here. Two instruction tables, because a Merlin source
        //  may select the wider one.
        static AssemblyResult AssembleAsParsed (const CommandLineOptions & options, const std::string & source)
        {
            TestCpu           cpu;
            TestCpu65C02      cmos;
            AssemblerOptions  asmOptions = {};

            cpu.InitForTest();
            asmOptions.dialect          = options.dialect;
            asmOptions.dialectSelection = options.dialectSelection;

            Assembler  assembler (cpu.GetInstructionSet(), cmos.GetInstructionSet(), asmOptions);

            return assembler.Assemble (source);
        }



        //  Everything the Merlin grammar is able to record, as one string. A
        //  flag that parses and then does nothing leaves this unchanged, which is
        //  what makes the table sweep below an assertion rather than a listing.
        static std::string Fingerprint (const CommandLineOptions & options)
        {
            return options.outputFile + "|" + options.listingFile + "|"
                 + (options.generateListing ? "L" : "-")
                 + (options.listingToStdout ? "S" : "-")
                 + (options.verbose         ? "V" : "-")
                 + "|" + Definitions (options);
        }



        //  The answers map rendered in a fixed order, because the map itself has
        //  none: a fingerprint carrying the hash order would differ from itself
        //  between runs, and the sweep that compares two fingerprints would be
        //  reporting the container rather than the parse.
        static std::string Definitions (const CommandLineOptions & options)
        {
            std::vector<std::string>  pairs;
            std::string               text;

            for (const auto & definition : options.predefinedSymbols)
            {
                pairs.push_back (definition.first + "=" + std::to_string (definition.second));
            }

            std::sort (pairs.begin(), pairs.end());

            for (const std::string & pair : pairs)
            {
                text += pair + ",";
            }

            return text;
        }



        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }



        static std::string FirstError (const AssemblyResult & result)
        {
            return result.errors.empty() ? std::string ("no error was reported") : result.errors[0].message;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinGrammarTests
    //
    //  The `merlin` subcommand and its flags.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinGrammarTests)
    {
    public:
        TEST_METHOD (MerlinSubcommand_SelectsTheDialectAndSaysTheInvocationNamedIt)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s" });

            Assert::IsTrue   (opts.subcommand == CommandLineOptions::Subcommand::Merlin);
            Assert::IsTrue   (opts.dialect    == DialectId::Merlin,
                              L"the subcommand selects the dialect, not merely a code path");
            Assert::IsTrue   (opts.dialectSelection == DialectSelection::Stated,
                              L"an invocation that named a dialect must not be reported one back");
            Assert::AreEqual (std::string ("demo.s"), opts.inputFile);
            Assert::IsTrue   (opts.unrecognizedArgument.empty());
        }

        //  The other half of the provenance pair. Without this, "Stated" above is
        //  satisfied by a parser that says Stated always, and the reporting rule
        //  that turns on the difference is never exercised.
        TEST_METHOD (RunSubcommand_StatesNoDialectAndLeavesTheDefault)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "run", "demo.a65" });

            Assert::IsTrue (opts.dialect          == DialectId::As65);
            Assert::IsTrue (opts.dialectSelection == DialectSelection::Defaulted,
                            L"running a source names no dialect, and that is what makes it reportable");
        }

        TEST_METHOD (OutputFlag_TakesAnAttachedName)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", "-oout.bin" });

            Assert::AreEqual (std::string ("out.bin"), opts.outputFile);
        }

        TEST_METHOD (OutputFlag_TakesASeparatedName)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", "-o", "out.bin" });

            Assert::AreEqual (std::string ("out.bin"), opts.outputFile);
            Assert::AreEqual (std::string ("demo.s"),  opts.inputFile,
                              L"the name after -o must not also be taken for the source");
        }

        TEST_METHOD (ListingFlagAlone_GoesToStdout)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", "-l" });

            Assert::IsTrue (opts.generateListing);
            Assert::IsTrue (opts.listingToStdout);
            Assert::IsTrue (opts.listingFile.empty());
        }

        TEST_METHOD (ListingFlag_TakesAnAttachedName)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", "-lout.lst" });

            Assert::AreEqual (std::string ("out.lst"), opts.listingFile);
            Assert::IsFalse  (opts.listingToStdout);
        }

        //  Deliberately unlike the as65 spelling, which also accepts a separated
        //  filename. Merlin source names its own object, so the bare word after a
        //  flag is far more likely to be the source -- and swallowing it leaves an
        //  assembly with no input at all.
        TEST_METHOD (ListingFlag_DoesNotSwallowTheSourceAfterIt)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "-l", "demo.s" });

            Assert::AreEqual (std::string ("demo.s"), opts.inputFile,
                              L"the word after -l is the source, not the listing");
            Assert::IsTrue   (opts.listingToStdout);
            Assert::IsTrue   (opts.listingFile.empty());
        }

        TEST_METHOD (VerboseFlag_IsSeen)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", "-v" });

            Assert::IsTrue (opts.verbose);
        }

        //  Merlin stops and asks the operator for a keyboard-input symbol. A
        //  batch assembly has nobody to ask, so the answer arrives here or the
        //  source cannot be assembled at all -- which is what the three vendor
        //  sources that ask questions were, before this flag existed.
        TEST_METHOD (DefineFlag_TakesAnAttachedAnswer)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", "-dVERSION=24" });

            Assert::AreEqual (std::string ("VERSION=24,"), Fixture::Definitions (opts));
        }

        TEST_METHOD (DefineFlag_TakesASeparatedAnswer)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "-d", "VERSION=24", "demo.s" });

            Assert::AreEqual (std::string ("VERSION=24,"), Fixture::Definitions (opts));
            Assert::AreEqual (std::string ("demo.s"), opts.inputFile,
                              L"the answer after -d must not also be taken for the source");
        }

        //  Zero is an ANSWER in these sources, not an absence, so it has to
        //  survive the parse as itself. A parser treating an unset value and a
        //  zero value alike assembles the other build of the file.
        TEST_METHOD (DefineFlag_KeepsZeroApartFromNoAnswerAtAll)
        {
            CommandLineOptions  answered   = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", "-dSAVOBJ=0" });
            CommandLineOptions  unanswered = Fixture::Parse ({ "CassoCli", "merlin", "demo.s" });

            Assert::AreEqual (std::string ("SAVOBJ=0,"), Fixture::Definitions (answered));
            Assert::AreEqual (std::string(),             Fixture::Definitions (unanswered));
        }

        TEST_METHOD (DefineFlag_AnswersABareSymbolWithOne)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", "-dSAVOBJ" });

            Assert::AreEqual (std::string ("SAVOBJ=1,"), Fixture::Definitions (opts));
        }

        //  Sources ask more than one question. A flag that kept only the last
        //  answer would satisfy every single-answer test above and still fail
        //  every source that asks twice.
        TEST_METHOD (DefineFlag_KeepsEveryAnswerGiven)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s",
                                                         "-dSAVOBJ=0", "-dVERSION=24" });

            Assert::AreEqual (std::string ("SAVOBJ=0,VERSION=24,"), Fixture::Definitions (opts));
        }

        //  The two grammars parse an answer with separate code, deliberately --
        //  they are separate walks over incompatible command lines. What must
        //  NOT differ is what an answer MEANS, so the spellings that carry a
        //  value, omit one, or spell one unreadably are swept through both and
        //  required to land on the same map.
        TEST_METHOD (BothGrammarsReadAnAnswerTheSameWay)
        {
            const char *  spellings[] = { "SYM=7", "SYM", "SYM=0", "SYM=0x10", "SYM=-3", "SYM=zzz" };

            for (const char * spelling : spellings)
            {
                std::string         typed  = std::string ("-d") + spelling;
                CommandLineOptions  merlin = Fixture::Parse ({ "CassoCli", "merlin", "demo.s",   typed });
                CommandLineOptions  as65   = Fixture::Parse ({ "CassoCli", "as65",   "demo.a65", typed });

                Assert::AreEqual (Fixture::Definitions (as65), Fixture::Definitions (merlin),
                                  Fixture::Widen (typed).c_str());
                Assert::IsFalse  (Fixture::Definitions (merlin).empty(),
                                  Fixture::Widen (typed + " reached neither grammar").c_str());
            }
        }

        TEST_METHOD (ConcatenatedFlags_SplitIntoIndividualFlags)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", "-voout.bin" });

            Assert::IsTrue   (opts.verbose, L"-v must be seen inside -voout.bin");
            Assert::AreEqual (std::string ("out.bin"), opts.outputFile);
        }

        TEST_METHOD (SlashPrefix_IsAcceptedAndRemembered)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", "/v" });

            Assert::IsTrue   (opts.verbose, L"/v must mean what -v means");
            Assert::AreEqual ('/', opts.flagPrefix,
                              L"the prefix the user typed comes back in usage text");
        }

        TEST_METHOD (ExtensionlessSource_IsResolved)
        {
            CommandLineOptions  opts = Fixture::ParseWithExisting ({ "CassoCli", "merlin", "build" }, "build.s");

            Assert::AreEqual (std::string ("build.s"), opts.inputFile);
        }

        //  The object IS the assembled stream. Merlin's origin relocates rather
        //  than seeks, so the as65 default of a full address-indexed image would
        //  scatter one contiguous object across the address space -- which is a
        //  wrong FILE, not a wrong flag.
        TEST_METHOD (TheObjectShapeIsTheAssembledStreamAndNotAnImage)
        {
            CommandLineOptions  merlin = Fixture::Parse ({ "CassoCli", "merlin", "demo.s" });
            CommandLineOptions  as65   = Fixture::Parse ({ "CassoCli", "as65",   "demo.a65" });

            Assert::IsTrue (merlin.outputFormat == CommandLineOptions::OutputFormat::Raw);
            Assert::IsTrue (as65.outputFormat   == CommandLineOptions::OutputFormat::Binary,
                            L"the as65 default is unchanged");
        }

        //  No output name is invented here. The source may name its own object,
        //  and a name defaulted at parse time would be the caller's answer --
        //  which beats the source's, so every source directive would be dead.
        TEST_METHOD (NoOutputFlag_LeavesTheNameForTheSourceToGive)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s" });

            Assert::IsTrue (opts.outputFile.empty(),
                            L"a defaulted output name would silently outrank the source's own");
        }

        //  The default is the assembled bytes and nothing around them, which is
        //  what this subcommand has always written. Pinned as its own test
        //  because the two shape flags are a DEPARTURE from it: a default that
        //  drifted would make both of them mean something else while their own
        //  tests carried on passing.
        TEST_METHOD (MerlinWritesRawBytesWhenNoShapeIsNamed)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s" });

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::Raw,
                            L"the merlin default is the object and nothing around it");
        }

        //  Sweeps the shape table for the reason the flag sweep exists one class
        //  over: a row the help advertises and the parser never matches is a
        //  documented flag that quietly does nothing.
        TEST_METHOD (EveryOutputShapeIsAcceptedAndSelectsTheFormatItDeclares)
        {
            Assert::IsFalse (CommandLineParser::GetOutputShapes (DialectId::Merlin).empty(),
                             L"nothing to sweep");

            for (const CommandLineParser::OutputShape & shape : CommandLineParser::GetOutputShapes (DialectId::Merlin))
            {
                CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", shape.spelling });

                Assert::IsTrue (opts.outputFormat == shape.format,
                                Fixture::Widen (std::string (shape.spelling) + " did not select its declared format").c_str());
            }
        }

        //  A whole-word flag must not fall into the letter loop. `--flat` read a
        //  character at a time is -f -l -a -t, and the `l` arm GENERATES A
        //  LISTING -- so the failure is not a warning about unknown flags, it is
        //  an output file nobody asked for, produced by an invocation that
        //  otherwise looks like it worked.
        TEST_METHOD (AnOutputShapeIsNotReadAsAStringOfLetterFlags)
        {
            std::string  baseline = Fixture::Fingerprint (Fixture::Parse ({ "CassoCli", "merlin", "demo.s" }));

            for (const CommandLineParser::OutputShape & shape : CommandLineParser::GetOutputShapes (DialectId::Merlin))
            {
                CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", shape.spelling });

                Assert::AreEqual (baseline, Fixture::Fingerprint (opts),
                                  Fixture::Widen (std::string (shape.spelling) + " disturbed something other than the output shape").c_str());
            }
        }

        //  Sweeps the flag table rather than a hand-picked sample, and asserts
        //  each row DID something: a row added to the table with no arm behind it
        //  parses quietly and changes nothing, which is exactly the drift the
        //  shared table exists to prevent.
        TEST_METHOD (EveryFlagInTheTableIsAcceptedAndHasAnEffect)
        {
            std::string  baseline = Fixture::Fingerprint (Fixture::Parse ({ "CassoCli", "merlin", "demo.s" }));

            Assert::IsFalse (CommandLineParser::GetFlags (DialectId::Merlin).empty(),
                             L"nothing to sweep");

            for (const CommandLineParser::DialectFlag & flag : CommandLineParser::GetFlags (DialectId::Merlin))
            {
                std::string  spelling = std::string ("-") + flag.letter;
                std::string  typed;

                if (flag.argument == CommandLineParser::FlagArgument::Required)
                {
                    spelling += "value";
                }

                typed = Fixture::Fingerprint (Fixture::Parse ({ "CassoCli", "merlin", "demo.s", spelling }));

                Assert::AreNotEqual (baseline, typed, Fixture::Widen (spelling).c_str());
            }
        }

        //  as65 states no table, and the absence is the point: its grammar is a
        //  hand-rolled walk over a historical command line, and a table it does
        //  not walk would be a second description of the parser.
        TEST_METHOD (As65StatesNoFlagTableOfItsOwn)
        {
            Assert::IsTrue (CommandLineParser::GetFlags (DialectId::As65).empty());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DialectSelectionTests
    //
    //  That every dialect is reachable, and reachable BY NAME.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DialectSelectionTests)
    {
    public:
        //  Sweeps the registry, so a dialect added without a subcommand fails
        //  here rather than becoming a dialect the tool supports and nobody can
        //  ask for.
        TEST_METHOD (EveryDialectIsSelectableUnderItsOwnName)
        {
            Assert::IsFalse (DialectRegistry::GetAllDialects().empty(), L"nothing to sweep");

            for (const DialectRegistry::Entry & entry : DialectRegistry::GetAllDialects())
            {
                CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", entry.name, "source.s" });

                Assert::IsTrue (opts.unrecognizedArgument.empty(),
                                Fixture::Widen (std::string (entry.name) + " is not a subcommand").c_str());
                Assert::IsTrue (opts.dialect == entry.id,
                                Fixture::Widen (std::string (entry.name) + " selected another dialect").c_str());
                Assert::IsTrue (opts.dialectSelection == DialectSelection::Stated,
                                Fixture::Widen (std::string (entry.name) + " did not record that it was named").c_str());
                Assert::AreEqual (std::string ("source.s"), opts.inputFile,
                                  Fixture::Widen (std::string (entry.name) + " lost its source").c_str());
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CpuFlagTests
    //
    //  Whether a command-line CPU flag applies is the PROFILE's answer. Nothing
    //  in the parser knows which dialects have an in-source directive, and these
    //  sweep the registry rather than naming one, so a parser that special-cased
    //  a dialect would pass the first two and fail the third.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CpuFlagTests)
    {
    public:
        TEST_METHOD (MerlinRefusesTheCpuFlagAndNamesTheDirectiveThatReplacesIt)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", "--cpu", "65c02" });

            Assert::IsFalse (opts.cpuFlagRefusal.empty(), L"the flag must be refused, not ignored");
            Assert::IsTrue  (opts.cpuFlagRefusal.find ("XC") != std::string::npos,
                             Fixture::Widen (opts.cpuFlagRefusal).c_str());
            Assert::IsTrue  (opts.cpuFlagRefusal.find ("merlin") != std::string::npos,
                             Fixture::Widen (opts.cpuFlagRefusal).c_str());
            Assert::IsFalse (opts.hasCpuTarget,
                             L"a refused flag must not also select a target");
        }

        //  The attached spelling reaches the same refusal. A flag refused in one
        //  spelling and honored in the other is worse than one honored in both.
        TEST_METHOD (TheAttachedSpellingIsRefusedToo)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", "--cpu=65c02" });

            Assert::IsFalse (opts.cpuFlagRefusal.empty());
        }

        TEST_METHOD (As65AcceptsTheCpuFlag)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "as65", "demo.a65", "--cpu", "65c02" });

            Assert::IsTrue (opts.cpuFlagRefusal.empty(), L"as65 takes its CPU from the command line");
            Assert::IsTrue (opts.cpuTarget == CommandLineOptions::CpuTarget::M65C02);
            Assert::IsTrue (opts.hasCpuTarget);
        }

        //  A CPU flag nobody typed is not a stated one, which is the distinction
        //  the CPU report turns on.
        TEST_METHOD (NoCpuFlag_LeavesTheTargetUnstated)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "as65", "demo.a65" });

            Assert::IsFalse (opts.hasCpuTarget);
            Assert::IsTrue  (opts.cpuTarget == CommandLineOptions::CpuTarget::M6502,
                             L"the default target itself is unchanged");
        }

        //  The one that would catch a dialect-specific arm in the parser: every
        //  dialect is asked, and what happens is compared against what its own
        //  profile says -- so flipping a profile's answer flips this test, and a
        //  hard-coded name does not.
        TEST_METHOD (EveryDialectAnswersTheCpuFlagAsItsProfileSaysItShould)
        {
            Assert::IsFalse (DialectRegistry::GetAllDialects().empty(), L"nothing to sweep");

            for (const DialectRegistry::Entry & entry : DialectRegistry::GetAllDialects())
            {
                CommandLineOptions  opts       = Fixture::Parse ({ "CassoCli", entry.name, "source.s",
                                                                   "--cpu", "65c02" });
                bool                isInSource = entry.profile->GetCpuSelectionSource() ==
                                                     CpuSelectionSource::InSource;
                bool                wasRefused = !opts.cpuFlagRefusal.empty();

                Assert::AreEqual (isInSource, wasRefused,
                                  Fixture::Widen (std::string (entry.name) +
                                                  " did not answer the CPU flag the way its profile does").c_str());

                if (isInSource)
                {
                    Assert::IsTrue (opts.cpuFlagRefusal.find (entry.profile->GetCpuDirectiveName()) !=
                                        std::string::npos,
                                    Fixture::Widen (std::string ("the refusal must name ") +
                                                    entry.profile->GetCpuDirectiveName()).c_str());
                }
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DialectHelpTests
    //
    //  Help is GENERATED, from the registry, the parser's own flag table and the
    //  boundary table. These sweep those three so a row added anywhere is
    //  documented without anybody remembering to document it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DialectHelpTests)
    {
    public:
        TEST_METHOD (EveryDialectIsNamedInTheHelp)
        {
            std::string  help = DialectHelp::GetAllDialects ('-');

            for (const DialectRegistry::Entry & entry : DialectRegistry::GetAllDialects())
            {
                Assert::IsTrue (help.find (entry.profile->GetName()) != std::string::npos,
                                Fixture::Widen (std::string ("undocumented dialect: ") + entry.name).c_str());
            }
        }

        //  The same rule for the shape rows, and for the same reason: a shape
        //  the parser accepts and the help never mentions is a flag a developer
        //  can only find by reading the source.
        TEST_METHOD (EveryOutputShapeReachesTheHelpWithItsDescription)
        {
            std::string  help = DialectHelp::GetAllDialects ('-');

            Assert::IsFalse (CommandLineParser::GetOutputShapes (DialectId::Merlin).empty(), L"nothing to sweep");

            for (const CommandLineParser::OutputShape & shape : CommandLineParser::GetOutputShapes (DialectId::Merlin))
            {
                Assert::IsTrue (help.find (shape.spelling)   != std::string::npos,
                                Fixture::Widen (shape.spelling).c_str());
                Assert::IsTrue (help.find (shape.description) != std::string::npos,
                                Fixture::Widen (shape.description).c_str());
            }
        }

        //  Both halves of every row, because a help line carrying the spelling
        //  and not the description tells a reader a flag exists and nothing about
        //  what it does.
        TEST_METHOD (EveryMerlinFlagReachesTheHelpWithItsDescription)
        {
            std::string  help = DialectHelp::GetAllDialects ('-');

            //  A table that came back empty would satisfy every assertion below
            //  by never reaching one, which is how a sweep quietly stops being a
            //  test. Mutating the table's dialect away is what found this.
            Assert::IsFalse (CommandLineParser::GetFlags (DialectId::Merlin).empty(), L"nothing to sweep");

            for (const CommandLineParser::DialectFlag & flag : CommandLineParser::GetFlags (DialectId::Merlin))
            {
                std::string  spelling = std::string ("-") + flag.letter;

                Assert::IsTrue (help.find (spelling)         != std::string::npos,
                                Fixture::Widen (spelling).c_str());
                Assert::IsTrue (help.find (flag.description) != std::string::npos,
                                Fixture::Widen (flag.description).c_str());
            }
        }

        TEST_METHOD (FlagsAreSpelledWithThePrefixTheUserTyped)
        {
            std::string  slashed = DialectHelp::GetAllDialects ('/');

            Assert::IsTrue  (slashed.find ("/o") != std::string::npos,
                             L"help must come back spelled the way the tool was invoked");
            Assert::IsFalse (slashed.find ("-o <file>") != std::string::npos);
        }

        //  The CPU sentence is the profile's answer rather than a fixed line, so
        //  each dialect is checked against what its own profile says.
        TEST_METHOD (TheCpuLineSaysWhatEachProfileSaysAboutItsCpu)
        {
            std::string  help = DialectHelp::GetAllDialects ('-');

            for (const DialectRegistry::Entry & entry : DialectRegistry::GetAllDialects())
            {
                bool  isInSource = entry.profile->GetCpuSelectionSource() == CpuSelectionSource::InSource;

                if (isInSource)
                {
                    Assert::IsTrue (help.find (entry.profile->GetCpuDirectiveName()) != std::string::npos,
                                    L"a dialect selecting its CPU in source must name the directive");
                }
            }

            Assert::IsTrue (help.find ("--cpu <6502|65c02>") != std::string::npos,
                            L"a dialect taking its CPU from the command line must show the flag");
            Assert::IsTrue (help.find ("Refused") != std::string::npos,
                            L"and one that does not must say the flag is refused");
        }

        //  Where the supported subset ends, swept from the boundary table itself.
        TEST_METHOD (EveryBoundaryRowReachesTheHelp)
        {
            std::string  help = DialectHelp::GetAllDialects ('-');

            Assert::IsFalse (MerlinSubsetBoundary::GetAll().empty(), L"nothing to sweep");

            for (const SubsetBoundaryRow & row : MerlinSubsetBoundary::GetAll())
            {
                Assert::IsTrue (help.find (row.spelling)  != std::string::npos,
                                Fixture::Widen (row.spelling).c_str());
                Assert::IsTrue (help.find (row.construct) != std::string::npos,
                                Fixture::Widen (row.construct).c_str());
            }
        }

        //  A dialect that refuses nothing gets no boundary heading, rather than
        //  an empty one. An empty heading reads as "the list is coming".
        TEST_METHOD (ADialectWithNoBoundaryGetsNoBoundaryHeading)
        {
            std::string  help = DialectHelp::GetDialect (DialectRegistry::Get (DialectId::As65), '-');

            Assert::IsTrue  (help.find ("as65")               != std::string::npos);
            Assert::IsFalse (help.find ("support ends")       != std::string::npos,
                             L"as65 states no subset boundary");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CrossDialectStrictnessTests
    //
    //  Each dialect is applied strictly and authentically: neither accepts the
    //  other's constructs, and a rejection says which dialect the word belongs to
    //  and which one is running.
    //
    //  Both directions and both signs. A rejection test alone is satisfied by an
    //  assembler that rejects the construct under EVERY dialect, which is not
    //  strictness but breakage -- so each construct is also assembled under the
    //  dialect that owns it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CrossDialectStrictnessTests)
    {
    public:
        TEST_METHOD (AMerlinConstructUnderAs65IsRejectedNamingBothDialects)
        {
            CommandLineOptions  opts   = Fixture::Parse ({ "CassoCli", "as65", "demo.a65" });
            AssemblyResult      result = Fixture::AssembleAsParsed (opts, "  .org $800\n  FIN\n");

            Assert::IsFalse (result.errors.empty(), L"as65 must not accept Merlin's conditional terminator");
            Assert::IsTrue  (result.errors[0].message.find ("FIN") != std::string::npos,
                             Fixture::Widen (Fixture::FirstError (result)).c_str());
            Assert::IsTrue  (result.errors[0].message.find ("belonging to the merlin dialect") != std::string::npos,
                             Fixture::Widen (Fixture::FirstError (result)).c_str());
            Assert::IsTrue  (result.errors[0].message.find ("not to as65") != std::string::npos,
                             Fixture::Widen (Fixture::FirstError (result)).c_str());
        }

        TEST_METHOD (TheSameMerlinConstructUnderMerlinAssembles)
        {
            CommandLineOptions  opts   = Fixture::Parse ({ "CassoCli", "merlin", "demo.s" });
            AssemblyResult      result = Fixture::AssembleAsParsed (opts,
                                             "          DO 1\n"
                                             "          LDA #$01\n"
                                             "          FIN\n");
            std::vector<Byte>   expected = { 0xA9, 0x01 };

            Assert::IsTrue (result.errors.empty(), Fixture::Widen (Fixture::FirstError (result)).c_str());
            Assert::IsTrue (result.bytes == expected,
                            L"the conditional body assembles, so the terminator was understood");
        }

        TEST_METHOD (AnAs65ConstructUnderMerlinIsRejectedNamingBothDialects)
        {
            CommandLineOptions  opts   = Fixture::Parse ({ "CassoCli", "merlin", "demo.s" });
            AssemblyResult      result = Fixture::AssembleAsParsed (opts, "          .struct FOO\n");

            Assert::IsFalse (result.errors.empty(), L"merlin must not accept as65's structure directive");
            Assert::IsTrue  (result.errors[0].message.find (".struct") != std::string::npos,
                             Fixture::Widen (Fixture::FirstError (result)).c_str());
            Assert::IsTrue  (result.errors[0].message.find ("belonging to the as65 dialect") != std::string::npos,
                             Fixture::Widen (Fixture::FirstError (result)).c_str());
            Assert::IsTrue  (result.errors[0].message.find ("not to merlin") != std::string::npos,
                             Fixture::Widen (Fixture::FirstError (result)).c_str());
        }

        TEST_METHOD (TheSameAs65ConstructUnderAs65Assembles)
        {
            CommandLineOptions  opts   = Fixture::Parse ({ "CassoCli", "as65", "demo.a65" });
            AssemblyResult      result = Fixture::AssembleAsParsed (opts,
                                             "  .org $800\n"
                                             "  .struct Obj\n"
                                             "x ds 1\n"
                                             "  end struct\n"
                                             "  lda #Obj\n");
            std::vector<Byte>   expected = { 0xA9, 0x01 };

            Assert::IsTrue (result.errors.empty(), Fixture::Widen (Fixture::FirstError (result)).c_str());
            Assert::IsTrue (result.bytes == expected,
                            L"the declaration reserves no space, and its size answers as a symbol");
        }
    };
}
