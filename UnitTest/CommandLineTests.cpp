#include "Pch.h"

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

        //  The fallback is gone: a first word naming no subcommand used to be
        //  ASSUMED to be an as65 source file, and now is not. Asserting the
        //  subcommand alone would be satisfied by a parser that simply left it
        //  at None for some other reason, so the word itself must come back.
        TEST_METHOD (UnrecognizedFirstArgument_IsReportedRatherThanAssumedToBeSource)
        {
            ArgVector           args = { "CassoCli", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.subcommand != CommandLineOptions::Subcommand::As65,
                            L"a bare source filename must no longer select the assembler");
            Assert::AreEqual (std::string ("demo.a65"), opts.unrecognizedArgument,
                              L"the rejected word comes back so the caller can name the replacement");
        }

        //  A recognized word leaves it clear, or every successful parse would
        //  look like a rejected one to a caller checking the field.
        TEST_METHOD (ARecognizedSubcommand_LeavesNoUnrecognizedArgument)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue   (opts.subcommand == CommandLineOptions::Subcommand::As65);
            Assert::AreEqual (std::string ("demo.a65"), opts.inputFile,
                              L"the source is the argument AFTER the subcommand");
            Assert::IsTrue   (opts.unrecognizedArgument.empty());
        }

        //  A flag first is the other half of the same removal: it reached the
        //  assembler through the fallback too.
        TEST_METHOD (AFlagAsTheFirstArgument_IsAlsoUnrecognized)
        {
            ArgVector           args = { "CassoCli", "-t" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("-t"), opts.unrecognizedArgument);
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

        TEST_METHOD (ListingFlag_TakesSeparatedFilename)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-l", "out.lst" };
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

        //  A bare -d NAME defines the symbol as 1, so `ifdef` works without the
        //  caller inventing a value.
        TEST_METHOD (PredefineWithoutValue_DefaultsToOne)
        {
            ArgVector           args  = { "CassoCli", "as65", "demo.a65", "-d", "DEBUG" };
            CommandLineOptions  opts  = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());
            auto                found = opts.predefinedSymbols.find ("DEBUG");
            bool                isSet = found != opts.predefinedSymbols.end();

            Assert::IsTrue (isSet);
            Assert::AreEqual ((int32_t) 1, found->second);
        }

        TEST_METHOD (PageWidthFlag_TakesAttachedValue)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-w133" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (133, opts.pageWidth);
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

    TEST_CLASS (CpuTargetTests)
    {
    public:
        TEST_METHOD (Default_IsStrict6502)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.cpuTarget == CommandLineOptions::CpuTarget::M6502);
        }

        TEST_METHOD (SeparatedCpuValue_Selects65C02)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "--cpu", "65c02" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.cpuTarget == CommandLineOptions::CpuTarget::M65C02);
        }

        TEST_METHOD (AttachedCpuValue_Selects65C02)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "--cpu=65C02" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.cpuTarget == CommandLineOptions::CpuTarget::M65C02,
                            L"the value is matched case-insensitively");
        }

        TEST_METHOD (UnknownCpuTarget_RequestsHelp)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "--cpu", "6809" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.showHelp, L"a bad target must stop parsing and print usage");
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
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "-o", "custom.out" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (std::string ("custom.out"), opts.outputFile);
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
            ArgVector           args = { "CassoCli", "as65", "-t" };
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
    //  Selecting a binary shape. The default must stay the as65 full image, or
    //  every existing invocation quietly changes what it produces.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (OutputShapeTests)
    {
    public:
        TEST_METHOD (Default_IsTheFullImage)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::Binary);
        }

        TEST_METHOD (RawFlag_SelectsRaw)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "--raw" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::Raw);
            Assert::AreEqual (std::string ("demo.bin"), opts.outputFile);
        }

        TEST_METHOD (DosBinFlag_SelectsDosBinary)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "--dos-bin" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::DosBinary);
        }

        TEST_METHOD (ShapeFlag_ComposesWithOtherFlags)
        {
            ArgVector           args = { "CassoCli", "as65", "demo.a65", "--raw", "-t", "-o", "out.obj" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::Raw);
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
}
