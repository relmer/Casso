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

        TEST_METHOD (PageWidthFlag_TakesAttachedValue)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "-w133" };
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
    //  Selecting a binary shape. The default must stay the as65 full image, or
    //  every existing invocation quietly changes what it produces.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (OutputShapeTests)
    {
    public:
        TEST_METHOD (Default_IsTheFullImage)
        {
            ArgVector           args = { "CassoCli", "demo.a65" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::Binary);
        }

        TEST_METHOD (RawFlag_SelectsRaw)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "--raw" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::Raw);
            Assert::AreEqual (std::string ("demo.bin"), opts.outputFile);
        }

        TEST_METHOD (DosBinFlag_SelectsDosBinary)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "--dos-bin" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::DosBinary);
        }

        TEST_METHOD (ShapeFlag_ComposesWithOtherFlags)
        {
            ArgVector           args = { "CassoCli", "demo.a65", "--raw", "-t", "-o", "out.obj" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.outputFormat == CommandLineOptions::OutputFormat::Raw);
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
            // Verbatim means no CHARACTER conversion -- the lossless path.
            ArgVector           args = { "CassoCli", "disk", "get", "my.dsk", "PROG" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Verbatim);
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

        TEST_METHOD (Disk_VerbatimIsASelectorOfItsOwn_NotJustTheAbsenceOfTheOthers)
        {
            // Verbatim being the default makes it easy to leave --verbatim
            // parsed-but-inert, which nothing would notice until somebody used
            // it to override an earlier selector. It is spelled that way rather
            // than --raw or --binary because both of those already name
            // assembler output shapes in this same parser.
            ArgVector           args = { "CassoCli", "disk", "put", "my.dsk", "p.bin",
                                         "--text", "--verbatim" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Verbatim,
                L"the last selector on the line wins, including the default one");
        }

        TEST_METHOD (Disk_LongListingFlag)
        {
            ArgVector           args = { "CassoCli", "disk", "list", "my.dsk", "--long" };
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (opts.disk.longListing);
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

        TEST_METHOD (Disk_TakesEitherPrefixOnEveryOption_SoTheHelpMaySpellEither)
        {
            const char *  kOptions[] = { "long", "text", "basic", "verbatim" };

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
                Assert::IsTrue (slashed.disk.longListing == dashed.disk.longListing);
            }
        }

        TEST_METHOD (As65_TakesEitherPrefixOnItsLongOptions_IncludingAnAttachedValue)
        {
            //  `/raw` used to be read as the concatenated flags -r -a -w, which
            //  silently set the column width and never selected a shape.
            ArgVector           raw    = { "CassoCli", "prog.a65", "/raw" };
            ArgVector           dosBin = { "CassoCli", "prog.a65", "/dos-bin" };
            ArgVector           cpu    = { "CassoCli", "prog.a65", "/cpu", "65c02" };
            ArgVector           glued  = { "CassoCli", "prog.a65", "/cpu=65c02" };

            Assert::IsTrue (CommandLineParser::Parse (raw.Count(), raw.Data(), NoProbe()).outputFormat
                                == CommandLineOptions::OutputFormat::Raw, L"/raw");
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
