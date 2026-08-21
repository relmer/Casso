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

        //  Deliberately unlike the as65 form, which also accepts a separated
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
        //  NOT differ is what an answer MEANS, so the forms that carry a
        //  value, omit one, or write one unreadably are swept through both and
        //  required to land on the same map.
        TEST_METHOD (BothGrammarsReadAnAnswerTheSameWay)
        {
            const char *  forms[] = { "SYM=7", "SYM", "SYM=0", "SYM=0x10", "SYM=-3" };

            for (const char * form : forms)
            {
                std::string         typed  = std::string ("-d") + form;
                CommandLineOptions  merlin = Fixture::Parse ({ "CassoCli", "merlin", "demo.s",   typed });
                CommandLineOptions  as65   = Fixture::Parse ({ "CassoCli", "as65",   "demo.a65", typed });

                Assert::AreEqual (Fixture::Definitions (as65), Fixture::Definitions (merlin),
                                  Fixture::Widen (typed).c_str());
                Assert::IsFalse  (Fixture::Definitions (merlin).empty(),
                                  Fixture::Widen (typed + " reached neither grammar").c_str());
            }
        }

        //  A VALUE NEITHER GRAMMAR CAN READ IS REFUSED BY BOTH, which is the
        //  same equivalence claim from the other side.
        //
        //  It used to be defined as 1 in silence, on the reasoning that
        //  inventing zero from a typo would assemble a different object. True,
        //  and 1 is just as invented: `-dADDR=$6000` defined ADDR as 1 and the
        //  source took a branch nobody chose. Refusing says so instead, and
        //  `$6000` is worth the sentence -- it is the assembler's own hex
        //  syntax, correct inside a source file and not a number this flag
        //  knows.
        TEST_METHOD (AValueNeitherGrammarCanRead_IsRefusedByBoth)
        {
            const char *  unreadable[] = { "-dSYM=zzz", "-dSYM=1.0", "-dSYM=$6000", "-dSYM=" };

            for (const char * typed : unreadable)
            {
                CommandLineOptions  merlin = Fixture::Parse ({ "CassoCli", "merlin", "demo.s",   typed });
                CommandLineOptions  as65   = Fixture::Parse ({ "CassoCli", "as65",   "demo.a65", typed });

                Assert::IsTrue (merlin.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                                Fixture::Widen (std::string (typed) + " under merlin").c_str());
                Assert::IsTrue (as65.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                                Fixture::Widen (std::string (typed) + " under as65").c_str());
                Assert::IsTrue (Fixture::Definitions (merlin).empty(),
                                L"and nothing is defined from a value that was not read");
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

            //  BOTH DEFAULT TO THE ASSEMBLED BYTES NOW. Merlin always did. as65
            //  wrote a padded 64 KB image, which is right for a ROM burner and
            //  useless for loading a 2 KB routine, so `--flat` asks for that and
            //  the bytes are what you get for asking for nothing. The two
            //  dialects agreeing here is a consequence of that, not a decision
            //  taken about Merlin.
            Assert::IsTrue (merlin.outputFormat == CommandLineOptions::OutputFormat::Raw);
            Assert::IsTrue (as65.outputFormat   == CommandLineOptions::OutputFormat::Raw,
                            L"and as65 writes the assembled bytes unless --flat asks otherwise");
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
        TEST_METHOD (EveryOutputFormatFlagIsAcceptedAndSelectsTheFormatItDeclares)
        {
            Assert::IsFalse (CommandLineParser::GetOutputFormats (DialectId::Merlin).empty(),
                             L"nothing to sweep");

            for (const CommandLineParser::OutputFormatFlag & shape : CommandLineParser::GetOutputFormats (DialectId::Merlin))
            {
                CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", shape.option });

                Assert::IsTrue (opts.outputFormat == shape.format,
                                Fixture::Widen (std::string (shape.option) + " did not select its declared format").c_str());
            }
        }

        //  The same sweep in the Windows form, and it is a REGRESSION test
        //  rather than a completeness one.
        //
        //  `/dos-bin` used to reach the letter loop as `-dos-bin`, where `-d`
        //  takes a required value and swallowed `os-bin` as a symbol definition.
        //  No warning, no header, exit 0 -- the file was simply the wrong shape.
        //  `/raw` on the as65 side did the same thing and wrote 65536 bytes where
        //  27 were asked for. A flag the help advertises has to be a flag the
        //  parser accepts, in both forms the help is willing to print.
        TEST_METHOD (EveryOutputFormatFlagIsAcceptedInTheSlashSpellingToo)
        {
            for (const CommandLineParser::OutputFormatFlag & shape : CommandLineParser::GetOutputFormats (DialectId::Merlin))
            {
                std::string         slashed = CommandLineParser::FormatLongOption (shape.option, '/');
                CommandLineOptions  opts    = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", slashed });

                Assert::IsTrue (opts.outputFormat == shape.format,
                                Fixture::Widen (slashed + " did not select its declared format").c_str());
                Assert::AreEqual ('/', opts.flagPrefix,
                                  Fixture::Widen (slashed + " did not record the prefix it was typed with").c_str());
            }
        }

        //  The as65 half of that regression, which is where it was found.
        TEST_METHOD (As65AcceptsTheSlashSpellingOfItsOutputFormatFlags)
        {
            CommandLineOptions  raw = Fixture::Parse ({ "CassoCli", "as65", "demo.a65", "/raw" });
            CommandLineOptions  dos = Fixture::Parse ({ "CassoCli", "as65", "demo.a65", "/dos-bin" });

            Assert::IsTrue (raw.outputFormat == CommandLineOptions::OutputFormat::Raw,
                            L"/raw must write the assembled span, not the 64 KB image it replaces");
            Assert::IsTrue (dos.outputFormat == CommandLineOptions::OutputFormat::DosBinary,
                            L"/dos-bin must write the DOS 3.3 header");
        }

        //  A mixed command line is a typo, and the only wrong answer is to echo
        //  back a prefix the user never typed. First one wins, so the answer
        //  depends on how the invocation opens rather than on which flag happens
        //  to sit last.
        TEST_METHOD (TheFirstPrefixTypedIsTheOneEchoedBack)
        {
            CommandLineOptions  slashFirst = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", "/v", "-o", "out.bin" });
            CommandLineOptions  dashFirst  = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", "-v", "/o", "out.bin" });

            Assert::AreEqual ('/', slashFirst.flagPrefix, L"opened with a slash, so slashes come back");
            Assert::AreEqual ('-', dashFirst.flagPrefix,  L"opened with a dash, so dashes come back");
        }

        //  An invocation that names no flag at all still has to answer in
        //  something. The dash is the documented default, and it is asserted
        //  rather than assumed because every message that writes a flag reads
        //  this field.
        TEST_METHOD (AnInvocationWithNoFlagsAnswersInDashes)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s" });

            Assert::AreEqual ('-', opts.flagPrefix, L"the default prefix is the dash");
            Assert::IsFalse (opts.flagPrefixSeen,   L"and nothing claimed to have seen one");
        }

        //  A whole-word flag must not fall into the letter loop. `--flat` read a
        //  character at a time is -f -l -a -t, and the `l` arm GENERATES A
        //  LISTING -- so the failure is not a warning about unknown flags, it is
        //  an output file nobody asked for, produced by an invocation that
        //  otherwise looks like it worked.
        TEST_METHOD (AnOutputFormatFlagIsNotReadAsAStringOfLetterFlags)
        {
            std::string  baseline = Fixture::Fingerprint (Fixture::Parse ({ "CassoCli", "merlin", "demo.s" }));

            for (const CommandLineParser::OutputFormatFlag & shape : CommandLineParser::GetOutputFormats (DialectId::Merlin))
            {
                CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", shape.option });

                Assert::AreEqual (baseline, Fixture::Fingerprint (opts),
                                  Fixture::Widen (std::string (shape.option) + " disturbed something other than the output format").c_str());
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
                std::string  written  = std::string ("-") + flag.option;
                std::string  typed;

                if (flag.value != CommandLineParser::ValueKind::None)
                {
                    written += "value";
                }

                typed = Fixture::Fingerprint (Fixture::Parse ({ "CassoCli", "merlin", "demo.s", written }));

                Assert::AreNotEqual (baseline, typed, Fixture::Widen (written).c_str());
            }
        }

        //  `run` given a SOURCE has to assemble it, and until now it assembled
        //  as65 and nothing else -- so a Merlin source could be assembled or
        //  run, but not both in one step.
        //
        //  The provenance travels with it for the same reason the subcommands
        //  carry it: a dialect the caller named needs no report, one it
        //  inherited does.
        TEST_METHOD (RunTakesTheAssemblerItIsToldToUse)
        {
            CommandLineOptions  merlin    = Fixture::Parse ({ "CassoCli", "run", "demo.s", "--merlin" });
            CommandLineOptions  as65      = Fixture::Parse ({ "CassoCli", "run", "demo.a65", "--as65" });
            CommandLineOptions  unstated  = Fixture::Parse ({ "CassoCli", "run", "demo.a65" });
            CommandLineOptions  slashed   = Fixture::Parse ({ "CassoCli", "run", "demo.s", "/merlin" });

            Assert::IsTrue (merlin.dialect == DialectId::Merlin, L"--merlin selects the Merlin assembler");
            Assert::IsTrue (merlin.dialectSelection == DialectSelection::Stated,
                            L"and an invocation that named one must not be told it back");

            Assert::IsTrue (as65.dialect == DialectId::As65, L"--as65 selects as65");
            Assert::IsTrue (as65.dialectSelection == DialectSelection::Stated);

            Assert::IsTrue (slashed.dialect == DialectId::Merlin, L"the slash form selects it too");

            //  The default is unchanged, so every `run` written before this
            //  keeps assembling what it always did.
            Assert::IsTrue (unstated.dialect == DialectId::As65,
                            L"naming no assembler still means as65");
            Assert::IsTrue (unstated.dialectSelection == DialectSelection::Defaulted,
                            L"and that is reportable, because nobody asked for it");
        }

        //  `run` also takes the CPU, in both spellings the assembler subcommands
        //  take.
        //
        //  Without it there was a source Casso could assemble and could not
        //  run: `run` refused the flag as unknown and then reported every 65C02
        //  instruction in the file as invalid. It remains the ONLY assembler
        //  option `run` accepts -- the rest have no meaning when no file is
        //  written.
        TEST_METHOD (RunTakesTheCpuTheSourceNeeds)
        {
            CommandLineOptions  viaX    = Fixture::Parse ({ "CassoCli", "run", "demo.a65", "-x" });
            CommandLineOptions  viaSlash = Fixture::Parse ({ "CassoCli", "run", "demo.a65", "/x" });
            CommandLineOptions  plain   = Fixture::Parse ({ "CassoCli", "run", "demo.a65" });

            Assert::IsTrue (viaX.cpuTarget   == CommandLineOptions::CpuTarget::M65C02,
                            L"-x must reach the assembler run uses");
            Assert::IsTrue (viaSlash.cpuTarget == CommandLineOptions::CpuTarget::M65C02,
                            L"and so must its slash form");
            Assert::IsTrue (plain.cpuTarget  == CommandLineOptions::CpuTarget::M6502,
                            L"with the strict 6502 still the default");
        }

        //  And `run` refuses it for a dialect whose source selects its own CPU,
        //  which `merlin` already did. A flag refused in one subcommand and
        //  honored in another is worse than one refused in both: it teaches
        //  that Merlin takes a CPU flag after all.
        //
        //  Both orders, because the dialect and the flag arrive in either one
        //  and a check made as each argument is read would refuse only the
        //  order it happened to be written for.
        TEST_METHOD (RunRefusesTheCpuFlagForADialectThatSelectsItsOwn)
        {
            CommandLineOptions  flagLast  = Fixture::Parse ({ "CassoCli", "run", "demo.s", "--merlin", "-x" });
            CommandLineOptions  flagFirst = Fixture::Parse ({ "CassoCli", "run", "demo.s", "-x", "--merlin" });

            Assert::IsFalse (flagLast.cpuFlagRefusal.empty(),
                             L"--merlin then -x must be refused");
            Assert::IsFalse (flagFirst.cpuFlagRefusal.empty(),
                             L"and so must -x then --merlin");

            Assert::IsTrue (flagLast.cpuFlagRefusal.find ("XC") != std::string::npos,
                            L"and the refusal names the directive to write instead");
        }

        //  BOTH DIALECTS STATE A TABLE NOW, and as65's was the harder one to
        //  win. Its grammar resisted a table only while a row was a single
        //  `char`: `-s2` is not `-s` followed by `2`, a bare `-w` means 133
        //  where a bare `-h` means nothing, and `-h80t` continues the group
        //  where `-lfile` cannot. Those are columns -- an option string matched
        //  longest-first, a value kind, an attachment rule, a bare default --
        //  and once they were columns the parser could walk the same rows the
        //  help is generated from, which is the whole point of having them.
        TEST_METHOD (EveryDialectStatesItsOwnFlagTable)
        {
            Assert::IsFalse (CommandLineParser::GetFlags (DialectId::As65).empty(),
                             L"as65's switches are data, not a hand-rolled walk");
            Assert::IsFalse (CommandLineParser::GetFlags (DialectId::Merlin).empty());
        }

        //  The row that only exists because the key is a string. `-s` and `-s2`
        //  are both options, and `-s` takes an optional attached filename, so
        //  `-s2out.hex` parses two ways; longest match settles it the way as65
        //  settles it, at the documented cost that `-s` cannot name a file
        //  beginning with `2`.
        TEST_METHOD (LongestMatchWins_SoMinusS2IsItsOwnOption)
        {
            size_t                              matched = 0;
            const CommandLineParser::DialectFlag *  flag =
                CommandLineParser::MatchFlag (DialectId::As65, "-s2out.hex", 1, matched);

            Assert::IsTrue (flag != nullptr, L"-s2 is matched");
            Assert::AreEqual (std::string ("s2"), std::string (flag->option));
            Assert::AreEqual (size_t (2), matched, L"and it consumed both characters, not one");
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
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", "-x" });

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
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "merlin", "demo.s", "/x" });

            Assert::IsFalse (opts.cpuFlagRefusal.empty());
        }

        TEST_METHOD (As65AcceptsTheCpuFlag)
        {
            CommandLineOptions  opts = Fixture::Parse ({ "CassoCli", "as65", "demo.a65", "-x" });

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
                CommandLineOptions  opts       = Fixture::Parse ({ "CassoCli", entry.name, "source.s", "-x" });
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
        TEST_METHOD (EveryOutputFormatFlagReachesTheHelpWithItsDescription)
        {
            std::string  help = DialectHelp::GetAllDialects ('-');

            Assert::IsFalse (CommandLineParser::GetOutputFormats (DialectId::Merlin).empty(), L"nothing to sweep");

            for (const CommandLineParser::OutputFormatFlag & shape : CommandLineParser::GetOutputFormats (DialectId::Merlin))
            {
                Assert::IsTrue (help.find (shape.option)   != std::string::npos,
                                Fixture::Widen (shape.option).c_str());
                Assert::IsTrue (help.find (shape.description) != std::string::npos,
                                Fixture::Widen (shape.description).c_str());
            }
        }

        //  Help asked for in slashes answers in slashes, all the way down.
        //
        //  The flag table already honored the prefix; the CPU line and the shape
        //  lines did not, so a `/?` invocation used to print `/o` and `/l` beside
        //  `--cpu` and `--dos-bin` -- two conventions mixed in one block, half of
        //  which the parser then refused. The sweep asserts the absence of the
        //  dashed form too, because a line printing BOTH forms would satisfy
        //  a find() for the slash one.
        TEST_METHOD (HelpAskedForInSlashesNamesEveryLongOptionInSlashes)
        {
            std::string  help = DialectHelp::GetAllDialects ('/');

            for (const CommandLineParser::OutputFormatFlag & shape : CommandLineParser::GetOutputFormats (DialectId::Merlin))
            {
                std::string  slashed = CommandLineParser::FormatLongOption (shape.option, '/');

                Assert::IsTrue  (help.find (slashed)        != std::string::npos,
                                 Fixture::Widen (slashed + " is missing from slash-prefixed help").c_str());
                Assert::IsFalse (help.find (shape.option) != std::string::npos,
                                 Fixture::Widen (std::string (shape.option) + " leaked into slash-prefixed help").c_str());
            }
        }

        //  One long option, both forms, from the one function that renders
        //  them. A second slash would be nobody's convention.
        TEST_METHOD (FormatLongOptionRendersOneSlashAndKeepsTheDashedFormIntact)
        {
            Assert::AreEqual (std::string ("/dos-bin"),  CommandLineParser::FormatLongOption ("--dos-bin", '/'));
            Assert::AreEqual (std::string ("--dos-bin"), CommandLineParser::FormatLongOption ("--dos-bin", '-'));
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
                std::string  written  = std::string ("-") + flag.option;

                Assert::IsTrue (help.find (written)          != std::string::npos,
                                Fixture::Widen (written).c_str());
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

        //  THE SUBSET BOUNDARY IS NOT IN --help, AND THAT IS THE ASSERTION.
        //
        //  Six rows, each a sentence of what the construct is, why it is refused
        //  and what would widen it, is a page of prose in front of a reader who
        //  typed --help wanting the flags. It moved to docs/Assembler.md.
        //
        //  The table is unchanged and still composes that text -- the sweep below
        //  proves every row reaches it -- so what this pins is only WHERE it is
        //  printed. Without the second half, deleting the boundary table
        //  altogether would satisfy the first.
        TEST_METHOD (TheSubsetBoundaryIsDocumentedRatherThanPrintedInHelp)
        {
            std::string  help    = DialectHelp::GetAllDialects ('-');
            std::string  written = MerlinSubsetBoundary::GetHelpText();

            Assert::IsFalse (MerlinSubsetBoundary::GetAll().empty(), L"nothing to sweep");
            Assert::IsFalse (help.find ("support ends") != std::string::npos,
                             L"the boundary heading must not be in the flag help");

            for (const SubsetBoundaryRow & row : MerlinSubsetBoundary::GetAll())
            {
                Assert::IsFalse (help.find (row.construct) != std::string::npos,
                                 Fixture::Widen (std::string (row.construct) + " is still in the flag help").c_str());

                Assert::IsTrue (written.find (row.spelling)  != std::string::npos,
                                Fixture::Widen (row.spelling).c_str());
                Assert::IsTrue (written.find (row.construct) != std::string::npos,
                                Fixture::Widen (row.construct).c_str());
            }
        }

        //  NOR IS THE CPU FLAG, for the dialect that refuses it.
        //
        //  Documenting a flag in order to say it is unavailable spends a line of
        //  help telling the reader not to type something they had no reason to
        //  type. Merlin takes its CPU from the source; the flag is still refused
        //  by name if passed, which is where that belongs.
        TEST_METHOD (TheRefusedCpuFlagIsNotAdvertisedInHelp)
        {
            std::string  help = DialectHelp::GetAllDialects ('-');

            Assert::IsFalse (help.find ("Refused") != std::string::npos,
                             L"help must not advertise a flag in order to refuse it");
            Assert::IsFalse (help.find ("--cpu")   != std::string::npos,
                             L"and the dialect block names no CPU flag at all");
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
