#include "Pch.h"

#include "Cli/CliMain.h"
#include "Cli/AssemblerMode.h"
#include "Cli/ArtifactWriter.h"
#include "Cli/CommandLine.h"
#include "AssemblerTypes.h"
#include "As65ExitStatus.h"
#include "CommandLineParser.h"
#include "CommandLineHelp.h"
#include "Dialect.h"

#include "CppUnitTest.h"




using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace CliSwitchCoverageTests
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




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SwitchCase
    //
    //  One switch, a command line that uses it, and what the parse should show
    //  for it afterwards.
    //
    ////////////////////////////////////////////////////////////////////////////////

    struct SwitchCase
    {
        const char *                                      mode;
        std::string                                       option;
        std::vector<std::string>                          argv;
        std::function<bool (const CommandLineOptions &)>  took;
        const char *                                      what;
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Cases
    //
    //  Every switch of every mode, with a command line that exercises it.
    //
    //  KEYED BY THE GRAMMAR'S OWN SPELLING of the option, without a prefix, so
    //  the two tests below can compare this list against the tables the parser
    //  walks. That comparison is the point: a hand-written list of switches
    //  goes stale the first time a row is added, and this one cannot, because
    //  a row with no case here fails and a case for a row that no longer
    //  exists fails too.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static std::vector<SwitchCase> Cases()
    {
        return
        {
            //
            //  as65
            //
            { "as65", "x", { "CassoCli", "as65", "p.a65", "-x" },
              [] (const CommandLineOptions & o) { return o.cpuTarget == CommandLineOptions::CpuTarget::M65C02; },
              "-x allows 65C02 instructions" },

            { "as65", "d", { "CassoCli", "as65", "p.a65", "-dFAST=2" },
              [] (const CommandLineOptions & o)
              {
                  auto  found = o.predefinedSymbols.find ("FAST");
                  return found != o.predefinedSymbols.end() && found->second == 2;
              },
              "-dNAME=VALUE predefines a symbol" },

            //  A no-op that must still be ACCEPTED: as65 takes it, so a command
            //  line carrying it cannot be refused.
            { "as65", "i", { "CassoCli", "as65", "p.a65", "-i" },
              [] (const CommandLineOptions & o) { return o.inputFile == "p.a65"; },
              "-i is accepted and changes nothing" },

            { "as65", "n", { "CassoCli", "as65", "p.a65", "-n" },
              [] (const CommandLineOptions & o) { return o.disableOpt; },
              "-n records the request even though nothing reads it yet" },

            { "as65", "o", { "CassoCli", "as65", "p.a65", "-o", "out.bin" },
              [] (const CommandLineOptions & o) { return o.outputFile == "out.bin"; },
              "-o names the output" },

            { "as65", "s2", { "CassoCli", "as65", "p.a65", "-s2" },
              [] (const CommandLineOptions & o)
              { return o.outputFormat == CommandLineOptions::OutputFormat::IntelHex; },
              "-s2 selects Intel HEX, and is one switch rather than -s and a 2" },

            { "as65", "s", { "CassoCli", "as65", "p.a65", "-s" },
              [] (const CommandLineOptions & o)
              { return o.outputFormat == CommandLineOptions::OutputFormat::SRecord; },
              "-s selects S-record" },

            { "as65", "z", { "CassoCli", "as65", "p.a65", "-z" },
              [] (const CommandLineOptions & o) { return o.fillZero; },
              "-z fills with $00" },

            { "as65", "l", { "CassoCli", "as65", "p.a65", "-lprog.lst" },
              [] (const CommandLineOptions & o)
              { return o.generateListing && o.listingFile == "prog.lst"; },
              "-l<file> writes the listing there" },

            { "as65", "p", { "CassoCli", "as65", "p.a65", "-p" },
              [] (const CommandLineOptions & o) { return o.pass1Listing; },
              "-p asks for the pass 1 listing" },

            { "as65", "c", { "CassoCli", "as65", "p.a65", "-c" },
              [] (const CommandLineOptions & o) { return o.cycleCounts; },
              "-c puts cycle counts in the listing" },

            { "as65", "m", { "CassoCli", "as65", "p.a65", "-m" },
              [] (const CommandLineOptions & o) { return o.macroExpansion; },
              "-m shows macro expansions" },

            { "as65", "h", { "CassoCli", "as65", "p.a65", "-h60" },
              [] (const CommandLineOptions & o) { return o.pageHeight == 60; },
              "-h<lines> sets the page height" },

            { "as65", "w", { "CassoCli", "as65", "p.a65", "-w133" },
              [] (const CommandLineOptions & o) { return o.pageWidth == 133; },
              "-w<width> sets the column width" },

            { "as65", "t", { "CassoCli", "as65", "p.a65", "-t" },
              [] (const CommandLineOptions & o) { return o.symbolTable; },
              "-t asks for the symbol table" },

            { "as65", "g", { "CassoCli", "as65", "p.a65", "-g" },
              [] (const CommandLineOptions & o) { return o.debugInfo; },
              "-g asks for debug information" },

            { "as65", "v", { "CassoCli", "as65", "p.a65", "-v" },
              [] (const CommandLineOptions & o) { return o.verbose; },
              "-v is verbose" },

            { "as65", "q", { "CassoCli", "as65", "p.a65", "-q" },
              [] (const CommandLineOptions & o) { return o.quiet; },
              "-q is quiet" },

            { "as65", "flat", { "CassoCli", "as65", "p.a65", "--flat" },
              [] (const CommandLineOptions & o)
              { return o.outputFormat == CommandLineOptions::OutputFormat::Binary; },
              "--flat writes the padded 64 KB image" },

            { "as65", "dos-bin", { "CassoCli", "as65", "p.a65", "--dos-bin" },
              [] (const CommandLineOptions & o)
              { return o.outputFormat == CommandLineOptions::OutputFormat::DosBinary; },
              "--dos-bin writes the DOS 3.3 header and the span" },

            { "as65", "disk", { "CassoCli", "as65", "p.a65", "--disk", "work.dsk" },
              [] (const CommandLineOptions & o) { return o.imagePath == "work.dsk"; },
              "--disk writes the object into that image" },

            { "as65", "as", { "CassoCli", "as65", "p.a65", "--disk", "work.dsk", "--as", "PROG" },
              [] (const CommandLineOptions & o) { return o.onDiskName == "PROG"; },
              "--as names the object on the volume" },

            { "as65", "type", { "CassoCli", "as65", "p.a65", "--disk", "work.dsk", "--type", "BIN" },
              [] (const CommandLineOptions & o) { return o.imageTypeName == "BIN"; },
              "--type sets the filesystem type" },

            { "as65", "startup", { "CassoCli", "as65", "p.a65", "--disk", "work.dsk", "--startup" },
              [] (const CommandLineOptions & o) { return o.setStartupProgram; },
              "--startup makes the object the volume's startup program" },

            //
            //  merlin
            //
            { "merlin", "o", { "CassoCli", "merlin", "p.s", "-o", "out.bin" },
              [] (const CommandLineOptions & o) { return o.outputFile == "out.bin"; },
              "-o names the output" },

            { "merlin", "l", { "CassoCli", "merlin", "p.s", "-lprog.lst" },
              [] (const CommandLineOptions & o)
              { return o.generateListing && o.listingFile == "prog.lst"; },
              "-l<file> writes the listing there" },

            { "merlin", "v", { "CassoCli", "merlin", "p.s", "-v" },
              [] (const CommandLineOptions & o) { return o.verbose; },
              "-v is verbose" },

            { "merlin", "d", { "CassoCli", "merlin", "p.s", "-d", "HOURS=12" },
              [] (const CommandLineOptions & o)
              {
                  auto  found = o.predefinedSymbols.find ("HOURS");
                  return found != o.predefinedSymbols.end() && found->second == 12;
              },
              "-d answers the question the source would have asked the operator" },

            { "merlin", "flat", { "CassoCli", "merlin", "p.s", "--flat" },
              [] (const CommandLineOptions & o)
              { return o.outputFormat == CommandLineOptions::OutputFormat::Binary; },
              "--flat writes the padded 64 KB image" },

            { "merlin", "dos-bin", { "CassoCli", "merlin", "p.s", "--dos-bin" },
              [] (const CommandLineOptions & o)
              { return o.outputFormat == CommandLineOptions::OutputFormat::DosBinary; },
              "--dos-bin writes the DOS 3.3 header and the span" },

            { "merlin", "disk", { "CassoCli", "merlin", "p.s", "--disk", "work.dsk" },
              [] (const CommandLineOptions & o) { return o.imagePath == "work.dsk"; },
              "--disk writes the object into that image" },

            { "merlin", "as", { "CassoCli", "merlin", "p.s", "--disk", "work.dsk", "--as", "PROG" },
              [] (const CommandLineOptions & o) { return o.onDiskName == "PROG"; },
              "--as beats the name the source gave" },

            { "merlin", "type", { "CassoCli", "merlin", "p.s", "--disk", "work.dsk", "--type", "BIN" },
              [] (const CommandLineOptions & o) { return o.imageTypeName == "BIN"; },
              "--type beats the type the source gave" },

            { "merlin", "startup", { "CassoCli", "merlin", "p.s", "--disk", "work.dsk", "--startup" },
              [] (const CommandLineOptions & o) { return o.setStartupProgram; },
              "--startup makes the object the volume's startup program" },

            //
            //  run
            //
            { "run", "load", { "CassoCli", "run", "p.bin", "--load", "$2000" },
              [] (const CommandLineOptions & o)
              { return o.hasLoadAddress && o.loadAddress == 0x2000; },
              "--load places the image" },

            { "run", "exec", { "CassoCli", "run", "p.bin", "--exec", "$2010" },
              [] (const CommandLineOptions & o)
              { return o.hasEntryAddress && o.entryAddress == 0x2010; },
              "--exec starts it somewhere other than the load address" },

            { "run", "stop", { "CassoCli", "run", "p.bin", "--stop", "$2100" },
              [] (const CommandLineOptions & o)
              { return o.hasStopAddress && o.stopAddress == 0x2100; },
              "--stop halts when the PC reaches an address" },

            { "run", "max-cycles", { "CassoCli", "run", "p.bin", "--max-cycles", "5000" },
              [] (const CommandLineOptions & o) { return o.maxCycles == 5000; },
              "--max-cycles bounds a program that never stops" },

            { "run", "reset-vector", { "CassoCli", "run", "p.bin", "--reset-vector" },
              [] (const CommandLineOptions & o) { return o.useResetVector; },
              "--reset-vector starts where the reset vector points" },

            { "run", "fill", { "CassoCli", "run", "p.bin", "--fill", "$EA" },
              [] (const CommandLineOptions & o) { return o.fillByte == 0xEA; },
              "--fill sets the byte unwritten memory holds" },

            { "run", "warn", { "CassoCli", "run", "p.a65", "--as65", "--warn" },
              [] (const CommandLineOptions & o)
              { return o.warningMode == WarningMode::Warn; },
              "--warn asks for warnings, which is also the default" },

            { "run", "no-warn", { "CassoCli", "run", "p.a65", "--as65", "--no-warn" },
              [] (const CommandLineOptions & o)
              { return o.warningMode == WarningMode::NoWarn; },
              "--no-warn silences them" },

            { "run", "fatal-warnings", { "CassoCli", "run", "p.a65", "--as65", "--fatal-warnings" },
              [] (const CommandLineOptions & o)
              { return o.warningMode == WarningMode::FatalWarnings; },
              "--fatal-warnings promotes them to errors" },

            //
            //  disk
            //
            { "disk", "out", { "CassoCli", "disk", "get", "d.dsk", "HELLO", "--out", "hello.bin" },
              [] (const CommandLineOptions & o) { return o.disk.hostFile == "hello.bin"; },
              "--out names where a fetched file lands" },

            { "disk", "as", { "CassoCli", "disk", "put", "d.dsk", "hello.bin", "--as", "HELLO" },
              [] (const CommandLineOptions & o) { return o.disk.path == "HELLO"; },
              "--as names the file on the disk" },

            { "disk", "type", { "CassoCli", "disk", "create", "d.dsk", "--type", "woz" },
              [] (const CommandLineOptions & o) { return o.disk.containerType == "woz"; },
              "--type picks the container" },

            { "disk", "load", { "CassoCli", "disk", "put", "d.dsk", "p.bin", "--load", "$0800" },
              [] (const CommandLineOptions & o)
              { return o.disk.hasLoadAddress && o.disk.loadAddress == 0x0800; },
              "--load gives a binary its load address" },

            { "disk", "text", { "CassoCli", "disk", "get", "d.dsk", "README", "--text" },
              [] (const CommandLineOptions & o)
              { return o.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Text; },
              "--text converts to host text on the way out" },

            { "disk", "basic", { "CassoCli", "disk", "get", "d.dsk", "HELLO", "--basic" },
              [] (const CommandLineOptions & o)
              { return o.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Basic; },
              "--basic detokenizes Applesoft on the way out" },

            { "disk", "format", { "CassoCli", "disk", "create", "d.dsk", "--format", "prodos" },
              [] (const CommandLineOptions & o) { return o.disk.formatName == "prodos"; },
              "--format picks the filesystem" },

            { "disk", "volume", { "CassoCli", "disk", "create", "d.dsk", "--volume", "42" },
              [] (const CommandLineOptions & o) { return o.disk.volumeName == "42"; },
              "--volume names or numbers the volume" },

            { "disk", "bootable", { "CassoCli", "disk", "create", "d.dsk", "--bootable" },
              [] (const CommandLineOptions & o)
              { return o.disk.bootable && o.disk.bootableFrom.empty(); },
              "--bootable bare finds the stock master itself" },

            { "disk", "boot", { "CassoCli", "disk", "create", "d.dsk", "--boot", "p.bin" },
              [] (const CommandLineOptions & o) { return o.disk.directBootFile == "p.bin"; },
              "--boot makes a disk that starts a binary with no operating system" },

            { "disk", "load", { "CassoCli", "disk", "create", "d.dsk", "--boot", "p.bin",
                                "--load", "$0900" },
              [] (const CommandLineOptions & o)
              { return o.disk.hasLoadAddress && o.disk.loadAddress == 0x0900; },
              "--load places a --boot binary in memory" },

            { "disk", "exec", { "CassoCli", "disk", "create", "d.dsk", "--boot", "p.bin",
                                "--exec", "$0910" },
              [] (const CommandLineOptions & o)
              { return o.disk.hasEntryAddress && o.disk.entryAddress == 0x0910; },
              "--exec starts the payload somewhere other than its first byte" },

            { "disk", "track", { "CassoCli", "disk", "sectorwrite", "d.dsk", "p.bin", "--track", "3" },
              [] (const CommandLineOptions & o) { return o.disk.track == 3; },
              "--track says which track to write at" },

            { "disk", "sector", { "CassoCli", "disk", "sectorwrite", "d.dsk", "p.bin", "--sector", "5" },
              [] (const CommandLineOptions & o) { return o.disk.sector == 5; },
              "--sector says which sector to start at" },

            { "disk", "count", { "CassoCli", "disk", "sectorread", "d.dsk", "--count", "4" },
              [] (const CommandLineOptions & o) { return o.disk.count == 4; },
              "--count is how many sectors a read takes, having no file to take a length from" },

            { "disk", "logical", { "CassoCli", "disk", "sectorread", "d.dsk", "--logical" },
              [] (const CommandLineOptions & o)
              { return o.disk.numbering == CommandLineOptions::DiskOptions::Numbering::Logical; },
              "--logical says the sector numbers are DOS logical" },

            { "disk", "physical", { "CassoCli", "disk", "sectorread", "d.dsk", "--physical" },
              [] (const CommandLineOptions & o)
              { return o.disk.numbering == CommandLineOptions::DiskOptions::Numbering::Physical; },
              "--physical says the sector numbers are the address-field order" },

            { "disk", "block", { "CassoCli", "disk", "blockread", "d.po", "--block", "6" },
              [] (const CommandLineOptions & o) { return o.disk.block == 6; },
              "--block says which ProDOS block to start at" },
        };
    }




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  GrammarSwitches
    //
    //  Every switch the parser will accept, gathered from the tables it walks.
    //
    ////////////////////////////////////////////////////////////////////////////////

    //  THE TABLES DISAGREE ABOUT THE DASHES. A dialect's output-format rows
    //  carry them ("--flat") and its long-option rows do not ("flat"), because
    //  one list is matched against a whole argument and the other feeds the
    //  normalization that keeps `/flat` from reading as -f -l -a -t. Keying on
    //  the bare name is what lets one case cover a flag that appears in both.
    static std::string Bare (const std::string & option)
    {
        size_t  start = option.find_first_not_of ('-');

        return (start == std::string::npos) ? option : option.substr (start);
    }




    static std::set<std::string> GrammarSwitches (const std::string & mode)
    {
        std::set<std::string>  names;

        if (mode == "as65" || mode == "merlin")
        {
            DialectId  dialect = (mode == "as65") ? DialectId::As65 : DialectId::Merlin;

            for (const CommandLineParser::DialectFlag & flag : CommandLineParser::GetFlags (dialect))
            {
                names.insert (Bare (flag.option));
            }

            for (const CommandLineParser::OutputFormatFlag & format :
                     CommandLineParser::GetOutputFormats (dialect))
            {
                names.insert (Bare (format.option));
            }

            if (mode == "as65")
            {
                for (const char * option : CommandLineParser::GetAs65LongOptions())
                {
                    names.insert (Bare (option));
                }
            }

            //  Both dialects, because sending the object onto a disk is the
            //  assembler's capability rather than one dialect's: a dialect is
            //  not required to have directives for a developer to reach it.
            for (const CommandLineParser::ImageTargetFlag & target : CommandLineParser::GetImageTargetFlags())
            {
                names.insert (Bare (target.option));
            }
        }
        else if (mode == "run")
        {
            for (const char * option : CommandLineParser::GetRunLongOptions())
            {
                names.insert (Bare (option));
            }
        }
        else if (mode == "disk")
        {
            for (const char * option : CommandLineParser::GetDiskOptionNames())
            {
                names.insert (Bare (option));
            }
        }

        return names;
    }




    static const char *  kModes[] = { "as65", "merlin", "run", "disk" };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SwitchCoverageTests
    //
    //  That every switch of every mode is exercised, asserted against the
    //  grammar rather than against a list somebody remembered to update.
    //
    //  TEN SWITCHES HAD NEVER APPEARED IN A TEST when this was written: as65's
    //  -n, -p, -c and -m, run's --warn, and disk's --format, --volume, --boot,
    //  --track and --sector. Nothing was failing, because nothing was looking.
    //  The two tests below look, and they read the parser's own tables to do
    //  it, so the next switch added without a case fails here rather than
    //  shipping unexercised.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SwitchCoverageTests)
    {
    public:
        //  A COVERAGE TEST THAT COVERS NOTHING PASSES BEAUTIFULLY. If
        //  GrammarSwitches ever returned an empty set (a renamed accessor, a
        //  mode string that stopped matching), the loop below would walk zero
        //  switches and report success. These floors are deliberately under
        //  the real counts, so adding a switch does not fail here; the test
        //  after this one is what notices that.
        TEST_METHOD (TheGrammarActuallyReportsSwitches_ForEveryMode)
        {
            Assert::IsTrue (GrammarSwitches ("as65").size()   >= 18, L"as65");
            Assert::IsTrue (GrammarSwitches ("merlin").size() >= 5,  L"merlin");
            Assert::IsTrue (GrammarSwitches ("run").size()    >= 8,  L"run");
            Assert::IsTrue (GrammarSwitches ("disk").size()   >= 12, L"disk");

            Assert::IsTrue (Cases().size() >= 45, L"and the case table is populated");
        }

        TEST_METHOD (EverySwitchTheGrammarTakes_IsExercisedByACase)
        {
            std::vector<SwitchCase>  cases = Cases();

            for (const char * mode : kModes)
            {
                std::set<std::string>  covered;

                for (const SwitchCase & one : cases)
                {
                    if (one.mode == std::string (mode))
                    {
                        covered.insert (Bare (one.option));
                    }
                }

                for (const std::string & option : GrammarSwitches (mode))
                {
                    Assert::IsTrue (covered.count (option) == 1,
                                    Widen (std::string (mode) + " takes '" + option
                                           + "' and no case here exercises it").c_str());
                }
            }
        }

        //  THE OTHER DIRECTION MATTERS TOO. A case for a switch the grammar no
        //  longer takes would go on passing forever, testing nothing, and
        //  reading like coverage.
        TEST_METHOD (NoCaseExercisesASwitchTheGrammarDoesNotTake)
        {
            for (const SwitchCase & one : Cases())
            {
                std::set<std::string>  grammar = GrammarSwitches (one.mode);

                Assert::IsTrue (grammar.count (Bare (one.option)) == 1,
                                Widen (std::string (one.mode) + " has no switch '"
                                       + one.option + "', but a case exercises it").c_str());
            }
        }

        //  The options that describe a placement on a volume, with no volume.
        //
        //  REFUSED RATHER THAN IGNORED, which is the same failure the pass
        //  below catches in its general form: a flag accepted and dropped on
        //  the floor tells a build script that a command line it got wrong had
        //  worked. Swept over both assembler grammars and both prefixes,
        //  because the options belong to the assembler rather than a dialect.
        TEST_METHOD (ImageOptionsWithoutAnImage_AreRefused)
        {
            const char *  kModes[]   = { "as65", "merlin" };
            const char *  kSources[] = { "p.a65", "p.s" };
            const char *  kOptions[] = { "--as", "--type", "--startup" };
            size_t        mode       = 0;
            size_t        option     = 0;

            for (mode = 0; mode < std::size (kModes); mode++)
            {
                for (option = 0; option < std::size (kOptions); option++)
                {
                    std::vector<std::string>  argv = { "CassoCli", kModes[mode], kSources[mode], kOptions[option] };
                    CommandLineOptions        options;

                    //  The two that take a value get one, so what is refused is
                    //  the missing image rather than a missing operand.
                    if (std::string (kOptions[option]) != "--startup")
                    {
                        argv.push_back ("VALUE");
                    }

                    ArgVector  args (argv);

                    options = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

                    Assert::IsTrue (options.parseVerdict == CommandLineOptions::ParseVerdict::Refused,
                                    Widen (std::string (kModes[mode]) + " " + kOptions[option]
                                           + " with no image should be refused").c_str());
                    Assert::IsTrue (options.refusalMessage.find ("no image was named") != std::string::npos,
                                    L"and the refusal should say why");
                }
            }
        }



        //  The same options WITH an image are ordinary, so the refusal above is
        //  about the missing image and not about the options themselves.
        TEST_METHOD (ImageOptionsWithAnImage_AreAccepted)
        {
            std::vector<std::string>  argv = { "CassoCli", "merlin", "p.s",
                                               "--disk", "d.dsk", "--as", "PROG",
                                               "--type", "BIN", "--startup" };
            ArgVector                 args (argv);
            CommandLineOptions        options = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::IsTrue (options.parseVerdict == CommandLineOptions::ParseVerdict::Clean,
                            L"an image was named, so nothing is stray");
            Assert::AreEqual (std::string ("PROG"), options.onDiskName, L"and the name took effect");
            Assert::IsTrue (options.setStartupProgram, L"as did the startup request");
        }



        //  Every switch parses without refusal AND is visible in the result.
        //  Accepting a flag and then dropping it on the floor is the failure
        //  this catches.
        TEST_METHOD (EverySwitchParsesCleanlyAndTakesEffect)
        {
            for (const SwitchCase & one : Cases())
            {
                Exercise (one, one.argv, "-");
            }
        }

        //  THE SAME SWITCHES IN THE OTHER PREFIX. Every flag is accepted as
        //  `/x` as readily as `-x`, and the two are meant to be the same
        //  switch. Eight of as65's and fifteen of the long options had never
        //  been typed that way in a test, so half the accepted grammar rested
        //  on the assumption that the prefixes could not diverge.
        //
        //  Driven from the same table as the pass above rather than a second
        //  list of its own, so a switch cannot be covered in one prefix and
        //  quietly missed in the other.
        TEST_METHOD (EverySwitchWorksTheSameInSlashes)
        {
            for (const SwitchCase & one : Cases())
            {
                Exercise (one, InSlashes (one.argv), "/");
            }
        }

    private:
        //  Every argument that opens with a dash, rewritten to open with a
        //  slash. A VALUE IS LEFT ALONE, which is what makes this safe: no
        //  value in the table above begins with a dash.
        static std::vector<std::string> InSlashes (const std::vector<std::string> & argv)
        {
            std::vector<std::string>  slashed;

            for (const std::string & arg : argv)
            {
                bool  isFlag = arg.size() > 1 && arg[0] == '-';

                slashed.push_back (isFlag ? "/" + arg.substr (arg.find_first_not_of ('-')) : arg);
            }

            return slashed;
        }

        static void Exercise (const SwitchCase & one, const std::vector<std::string> & argv,
                              const std::string & prefix)
        {
            ArgVector           args (argv);
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());
            std::string         what = std::string (one.what) + " [" + prefix + "]";

            Assert::IsTrue (opts.unrecognizedFlag.empty(),
                            Widen (what + ": rejected as unknown (" + opts.unrecognizedFlag + ")").c_str());
            Assert::IsTrue (opts.refusalMessage.empty(),
                            Widen (what + ": refused with " + opts.refusalMessage).c_str());
            Assert::IsTrue (opts.outputFormatConflict.empty(),
                            Widen (what + ": " + opts.outputFormatConflict).c_str());
            Assert::IsTrue (one.took (opts), Widen (what).c_str());
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  OneSource
    //
    //  A source file that never existed, handed to the assembler through the
    //  door AssemblerOptions has always described.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class OneSource : public FileReader
    {
    public:
        explicit OneSource (const std::string & text) : m_text (text), m_readable (true) {}

        //  The other outcome: a source the reader cannot produce at all, which
        //  is what an unreadable file looks like from here.
        static OneSource Unreadable()
        {
            OneSource  reader ("");

            reader.m_readable = false;

            return reader;
        }

        FileReadResult ReadFile (const std::string &, const std::string &) override
        {
            FileReadResult  result;

            result.success  = m_readable;
            result.contents = m_text;
            result.error    = m_readable ? "" : "no such file";

            return result;
        }

    private:
        std::string  m_text;
        bool         m_readable;
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MemorySink
    //
    //  The artifacts, kept in memory. Nothing here touches a disk, which is
    //  the only reason a SUCCESSFUL assembly can be asserted at all.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MemorySink : public ArtifactSink
    {
    public:
        HRESULT WriteBinary (const AssemblyResult & result, const CommandLineOptions & options) override
        {
            binaryPath  = options.outputFile;
            bytes       = result.bytes.size();
            wroteBinary = true;

            return binaryResult;
        }

        HRESULT WriteListing (const AssemblyResult &, const CommandLineOptions & options,
                              const std::vector<DialectReportLine> &) override
        {
            listingPath  = options.listingFile;
            wroteListing = true;

            return listingResult;
        }

        //  What the sink reports back, so the far side of a FAILED write is
        //  reachable without contriving an unwritable path.
        HRESULT      binaryResult  = S_OK;
        HRESULT      listingResult = S_OK;
        bool         wroteBinary   = false;
        bool         wroteListing  = false;
        size_t       bytes         = 0;
        std::string  binaryPath;
        std::string  listingPath;
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ExitStatusThroughTheAssemblerTests
    //
    //  The statuses an assembly earns, asserted against the assembler rather
    //  than against the function that maps them.
    //
    //  As65ExitStatus::ForAssembly is tested for all five values and always
    //  was. So was the mapper it replaced, which is the whole problem: two
    //  mappers, two green suites, and nothing showing which one the tool
    //  reached for. It reached for the wrong one, and every page of the help
    //  documented a numbering the tool did not use. These go through a real
    //  assembly of real source, so the mapper and the caller are asserted
    //  together.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ExitStatusThroughTheAssemblerTests)
    {
    public:
        static int StatusFor (const std::string & source)
        {
            OneSource  reader (source);

            return StatusUsing (reader);
        }

        static int StatusUsing (OneSource & reader)
        {
            MemorySink  sink;

            return StatusUsing (reader, sink);
        }

        static int StatusUsing (OneSource & reader, MemorySink & sink)
        {
            CommandLineOptions  options;

            options.inputFile = "in-memory.a65";

            return StatusFor (options, reader, sink);
        }

        static int StatusFor (const CommandLineOptions & options, OneSource & reader, MemorySink & sink)
        {
            std::unique_ptr<AssemblerMode>  mode     = AssemblerMode::CreateFor (DialectId::As65);
            HRESULT                         hr       = S_OK;
            int                             exitCode = -1;

            //  The HRESULT says whether the assembly went wrong; the exit code
            //  says what a shell is told. This asserts the second, so the
            //  first is captured and set aside rather than passed into the
            //  macro as a call.
            hr = mode->Run (options, exitCode, &reader, &sink);
            IGNORE_RETURN_VALUE (hr, S_OK);

            return exitCode;
        }

        //  ONE NAME CANNOT SERVE SEVERAL FILES. Applying it to each output in
        //  turn leaves each overwriting the last, and the tool would report
        //  success having written one file where the source asked for several.
        //
        //  Deliberately NOT the case two saves under one name make: there the
        //  SOURCE said so and the period assembler allows it, where here an
        //  option said it about outputs its author could not have seen.
        TEST_METHOD (ASingleNameForSeveralOutputs_IsRefused)
        {
            OneSource           reader (" ORG $300\n LDA #$11\n SAV ONE\n LDA #$22\n SAV TWO\n");
            MemorySink                      sink;
            CommandLineOptions              options;
            std::unique_ptr<AssemblerMode>  mode     = AssemblerMode::CreateFor (DialectId::Merlin);
            HRESULT                         hr       = S_OK;
            int                             exitCode = -1;

            options.dialect   = DialectId::Merlin;
            options.inputFile  = "in-memory.s";
            options.imagePath  = "work.dsk";
            options.onDiskName = "ONENAME";

            hr = mode->Run (options, exitCode, &reader, &sink);

            Assert::IsTrue (FAILED (hr), L"one name was given for two outputs");
            Assert::AreEqual (As65ExitStatus::kNoOutput, exitCode, L"and nothing was written");
        }



        //  The same source with no single name is fine, so the refusal above is
        //  about the name rather than about producing several outputs.
        TEST_METHOD (SeveralOutputsWithoutASingleName_IsAccepted)
        {
            OneSource           reader (" ORG $300\n LDA #$11\n SAV ONE\n LDA #$22\n SAV TWO\n");
            MemorySink                      sink;
            CommandLineOptions              options;
            std::unique_ptr<AssemblerMode>  mode     = AssemblerMode::CreateFor (DialectId::Merlin);
            HRESULT                         hr       = S_OK;
            int                             exitCode = -1;

            options.dialect   = DialectId::Merlin;
            options.inputFile = "in-memory.s";

            hr = mode->Run (options, exitCode, &reader, &sink);

            Assert::IsTrue (SUCCEEDED (hr), L"several outputs are ordinary");
        }



        //  as65: "3 - Errors during assembly."
        TEST_METHOD (AnAssemblyError_IsThree)
        {
            Assert::AreEqual (As65ExitStatus::kAssemblyErrors,
                              StatusFor (" ORG $0300\n NOSUCHOP\n"),
                              L"an opcode that does not exist");
        }

        //  as65: "2 - Unable to open input or output file."
        //
        //  THE DISTINCTION THIS PINS is the one the two statuses exist for. A
        //  source that could not be READ and a source that would not ASSEMBLE
        //  are different failures, and a script that reruns after fetching a
        //  missing file needs to tell them apart.
        TEST_METHOD (ASourceThatCannotBeRead_IsTwo_NotThree)
        {
            OneSource  missing = OneSource::Unreadable();

            Assert::AreEqual (As65ExitStatus::kNoOutput, StatusUsing (missing));
        }

        //  as65: "0 - No errors."
        TEST_METHOD (ACleanAssembly_IsZero_AndWritesItsObject)
        {
            OneSource   clean (" ORG $0300\n LDA #$01\n RTS\n");
            MemorySink  sink;

            Assert::AreEqual (0, StatusUsing (clean, sink));
            Assert::IsTrue (sink.wroteBinary, L"and the object was asked for");
            Assert::IsTrue (sink.bytes > 0,   L"with something in it");
        }

        //  as65: "5 - Warnings during assembly."
        //
        //  A WARNING SUCCEEDS, and the object is still written. That is why
        //  this status was unreachable before the sink, and why it must not be
        //  confused with the 3 an actual error earns.
        TEST_METHOD (AnAssemblyThatWarned_IsFive_AndStillWritesItsObject)
        {
            OneSource   warns (" ORG $0300\n ORG $0300\n RTS\n");
            MemorySink  sink;

            Assert::AreEqual (As65ExitStatus::kWarned, StatusUsing (warns, sink),
                              L"a redundant ORG warns");
            Assert::IsTrue (sink.wroteBinary, L"and the object is written anyway");
        }

        //  A WRITE THAT FAILED IS NO OUTPUT, whatever the assembly thought of
        //  the source. The question a script asks is whether it got a file.
        TEST_METHOD (AnObjectThatCouldNotBeWritten_IsNoOutput)
        {
            OneSource   clean (" ORG $0300\n RTS\n");
            MemorySink  sink;

            sink.binaryResult = HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED);

            Assert::AreEqual (As65ExitStatus::kNoOutput, StatusUsing (clean, sink));
        }

        //  The listing is written first, and failing it stops the object.
        TEST_METHOD (AListingThatCouldNotBeWritten_IsNoOutput)
        {
            OneSource           clean (" ORG $0300\n RTS\n");
            MemorySink          sink;
            CommandLineOptions  options;

            options.inputFile       = "in-memory.a65";
            options.generateListing = true;
            options.listingFile     = "prog.lst";
            sink.listingResult      = HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED);

            Assert::AreEqual (As65ExitStatus::kNoOutput, StatusFor (options, clean, sink));
            Assert::IsTrue  (sink.wroteListing, L"the listing was attempted");
            Assert::IsFalse (sink.wroteBinary,  L"and the object was not, after it failed");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TopLevelSwitchTests
    //
    //  The switches that belong to no mode: the seven ways to ask for help and
    //  the two ways to ask for the version.
    //
    //  THESE ARE NOT IN ANY GRAMMAR TABLE, so the coverage gate above cannot
    //  see them and they have to be listed. The list is asserted against
    //  IsHelpRequest rather than trusted, which is what stops a form from
    //  being dropped from one and not the other. `-version` had never been
    //  typed in a test at all.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (TopLevelSwitchTests)
    {
    public:
        static int Run (std::initializer_list<const char *> typed)
        {
            std::vector<std::string>  words;

            for (const char * word : typed)
            {
                words.push_back (std::string (word));
            }

            ArgVector  args (words);

            return CliMain (args.Count(), args.Data());
        }

        //  Asking how the tool works succeeds, whichever way it is asked.
        TEST_METHOD (EveryHelpForm_PrintsThePageAndSucceeds)
        {
            for (const char * form : { "--help", "-?", "-h", "/help", "/?", "/h" })
            {
                Assert::AreEqual (0, Run ({ "CassoCli", form }), Widen (form).c_str());
                Assert::IsTrue (CommandLineParser::IsHelpRequest (form),
                                Widen (std::string (form) + " is not recognized as a help request").c_str());
            }
        }

        //  And the list above is the whole of it: a near miss is an argument,
        //  not a request for the page.
        //
        //  `-help` IS ON THIS LIST DELIBERATELY. A single dash introduces
        //  concatenated single-letter switches, so `-help` is `-h -e -l -p`,
        //  and the `-e` is an option no grammar here has. It used to be
        //  accepted at the top level while the assembler's own flag walk
        //  refused it, which is one string with two answers.
        TEST_METHOD (AWordThatMerelyResemblesOne_IsNotAHelpRequest)
        {
            for (const char * form : { "help", "?", "--h", "-hh", "/hh", "--halp",
                                       "-help", "-version" })
            {
                Assert::IsFalse (CommandLineParser::IsHelpRequest (form), Widen (form).c_str());
            }
        }

        //  `?` ON ITS OWN IS AS65'S REQUEST and is answered, even though it
        //  carries no switch character and IsHelpRequest says no to it.
        TEST_METHOD (ABareQuestionMark_IsAs65sOwnRequest)
        {
            Assert::AreEqual (0, Run ({ "CassoCli", "?" }));
            Assert::AreEqual (0, Run ({ "CassoCli", "as65", "?" }));
        }

        //  The long word forms are legal behind `--` and behind `/`, and not
        //  behind a single dash, which introduces concatenated letters.
        TEST_METHOD (TheLongWordForms_TakeTwoDashesOrASlash)
        {
            Assert::AreEqual (0, Run ({ "CassoCli", "--version" }), L"--version");
            Assert::AreEqual (0, Run ({ "CassoCli", "/version" }),  L"/version");
            Assert::AreEqual (0, Run ({ "CassoCli", "--help" }),    L"--help");
            Assert::AreEqual (0, Run ({ "CassoCli", "/help" }),     L"/help");
        }

        //  AND A SINGLE DASH IN FRONT OF ONE IS REFUSED. It still prints the
        //  page, because an argument the grammar does not know always does;
        //  what it no longer does is exit 0 and call a misspelling a question.
        TEST_METHOD (ASingleDashLongWord_IsRefused_ThoughItStillPrintsThePage)
        {
            Assert::AreNotEqual (0, Run ({ "CassoCli", "-version" }), L"-version");
            Assert::AreNotEqual (0, Run ({ "CassoCli", "-help" }),    L"-help");

            //  Inside a mode it was always refused, and the two answers now
            //  agree with each other.
            Assert::AreEqual (As65ExitStatus::kBadCommandLine,
                              Run ({ "CassoCli", "as65", "p.a65", "-help" }),
                              L"as65 -help");
            Assert::AreEqual (As65ExitStatus::kBadCommandLine,
                              Run ({ "CassoCli", "as65", "p.a65", "-version" }),
                              L"as65 -version");
        }

        //
        //  EVERY MODE NAMED WITH NOTHING AFTER IT SAYS WHAT IS MISSING.
        //
        //  `disk cat` names the operand it wanted; for a while the three
        //  assembler-shaped modes printed their page and said nothing at all,
        //  leaving a reader to work out why a page had appeared. A mode with
        //  nothing after it has its command chosen and its operand missing,
        //  which is the same position `disk cat` is in.
        //
        //  THE OPERAND IS READ BACK OUT OF THE USAGE LINE the page prints, so
        //  a refusal cannot ask for something the usage does not show. This
        //  asserts the two agree rather than quoting either.
        //
        TEST_METHOD (EveryMode_NamesTheOperandItsOwnUsageLineShows)
        {
            const CommandLineOptions::Subcommand  kModes[] =
            {
                CommandLineOptions::Subcommand::As65,
                CommandLineOptions::Subcommand::Merlin,
                CommandLineOptions::Subcommand::Run,
            };

            for (CommandLineOptions::Subcommand mode : kModes)
            {
                std::string               usage    = CommandLineHelp::UsageLineFor (mode);
                std::vector<std::string>  required = CommandLineHelp::RequiredOperandsIn (usage);

                Assert::AreEqual (size_t (1), required.size(),
                                  Widen ("one required operand in: " + usage).c_str());

                //  And it is a real operand rather than an option's value or a
                //  bracketed extra.
                Assert::IsTrue (required[0].front() == '<' && required[0].back() == '>',
                                Widen ("an operand, in: " + usage).c_str());
                Assert::IsTrue (usage.find ("[" + required[0] + "]") == std::string::npos,
                                Widen ("not an optional one, in: " + usage).c_str());
            }
        }

        //  `[options]` is bracketed, so it is never mistaken for an operand,
        //  and disk's own line requires two.
        TEST_METHOD (RequiredOperands_SkipOptionalGroupsAndOptionValues)
        {
            std::vector<std::string>  disk = CommandLineHelp::RequiredOperandsIn (
                CommandLineHelp::UsageLineFor (CommandLineOptions::Subcommand::Disk));

            Assert::AreEqual (size_t (2), disk.size(), L"disk takes a command and an image");
            Assert::AreEqual (std::string ("<command>"), disk[0]);
            Assert::AreEqual (std::string ("<image>"),   disk[1]);

            //  An option's value is not an operand, and `<binary | source>` is
            //  ONE operand: the angle brackets bound it, not the spaces.
            std::vector<std::string>  contrived = CommandLineHelp::RequiredOperandsIn (
                "CassoCli x <binary | source> --track <n> [--out <file>]");

            Assert::AreEqual (size_t (1), contrived.size(),
                              L"the option's value and the bracketed one are both skipped");
            Assert::AreEqual (std::string ("<binary | source>"), contrived[0]);
        }

        //  A REFUSAL'S PAGE AND ITS REASON GO DOWN ONE STREAM.
        //
        //  They used to be split: the page to stdout, the reason to stderr,
        //  with stdout flushed between them so the order written would be the
        //  order read. It is not, and cannot be made to be. A terminal reads
        //  the two pipes on two threads, and the reason arrived spliced into
        //  the middle of the examples, four lines above where the page ended.
        //  Each half was correct in isolation, which is why redirecting either
        //  one to a file on its own showed nothing wrong.
        //
        //  THE MECHANISM IS WHAT IS ASSERTED HERE, not the bytes. Capturing
        //  the real streams means rebinding the process's stdout and stderr,
        //  which takes the test host's own reporting down with it: measured,
        //  the run lost its result summary entirely. The printers each open
        //  one of these guards as their first statement.
        TEST_METHOD (TheUsageStreamFollowsTheGuard_AndIsRestored)
        {
            Assert::IsTrue (CommandLine::UsageStream() == stdout,
                            L"usage is ordinary output by default");

            {
                CommandLine::UsageOnErrorStream  toTheErrorStream;

                Assert::IsTrue (CommandLine::UsageStream() == stderr,
                                L"and goes where the reason goes while a refusal is printing");
            }

            Assert::IsTrue (CommandLine::UsageStream() == stdout,
                            L"and is put back, so an asked-for page stays pipeable");
        }

        //  `-h` MEANS TWO THINGS AND THE POSITION DECIDES WHICH. On its own it
        //  asks for the page; after a source file it is as65's page height,
        //  which takes a number attached. Routing every `-h` to the help was a
        //  real bug: it made `-h60` unreachable behind the dialect's own flag.
        TEST_METHOD (DashH_IsHelpAlone_AndPageHeightInsideTheAssembler)
        {
            ArgVector           args ({ "CassoCli", "as65", "p.a65", "-h60" });
            CommandLineOptions  opts = CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());

            Assert::AreEqual (0, Run ({ "CassoCli", "-h" }), L"alone it is the page");
            Assert::AreEqual (60, opts.pageHeight,           L"and attached to a number it is the height");
            Assert::IsFalse (opts.showHelp,                  L"which is not a request for help");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  IllegalCombinationTests
    //
    //  Switch combinations the grammar refuses, and the status each earns.
    //
    //  A REFUSAL IS ONLY A REFUSAL IF THE TOOL EXITS NON-ZERO. These go
    //  through CliMain rather than Parse for that reason: the parse recording
    //  a complaint and the executable acting on it are two different things,
    //  and it was the second one that was wrong. Every case here asserts both
    //  that the command line is refused and what a shell sees when it is.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (IllegalCombinationTests)
    {
    public:
        static int Run (std::initializer_list<const char *> typed)
        {
            std::vector<std::string>  words;

            for (const char * word : typed)
            {
                words.push_back (std::string (word));
            }

            ArgVector  args (words);

            return CliMain (args.Count(), args.Data());
        }

        static CommandLineOptions ParseOf (std::initializer_list<const char *> typed)
        {
            std::vector<std::string>  words;

            for (const char * word : typed)
            {
                words.push_back (std::string (word));
            }

            ArgVector  args (words);

            return CommandLineParser::Parse (args.Count(), args.Data(), NoProbe());
        }

        //  Two output formats is a request for two files where one gets
        //  written, so it is refused rather than resolved by taking the last.
        TEST_METHOD (TwoOutputFormats_AreRefused_InEveryPairing)
        {
            const char *  kFlags[] = { "-s", "-s2", "--flat", "--dos-bin" };

            for (const char * first : kFlags)
            {
                for (const char * second : kFlags)
                {
                    if (std::string (first) == second)
                    {
                        continue;
                    }

                    CommandLineOptions  opts = ParseOf ({ "CassoCli", "as65", "p.a65", first, second });

                    Assert::IsFalse (opts.outputFormatConflict.empty(),
                                     Widen (std::string (first) + " with " + second
                                            + " was not refused").c_str());
                    Assert::AreEqual (As65ExitStatus::kBadCommandLine,
                                      Run ({ "CassoCli", "as65", "p.a65", first, second }),
                                      Widen (std::string (first) + " with " + second).c_str());
                }
            }
        }

        //  The SAME format twice asks for one thing, twice. That is not a
        //  conflict, and refusing it would break a script that builds its
        //  command line by appending.
        TEST_METHOD (TheSameFormatTwice_IsNotAConflict)
        {
            const char *  kFlags[] = { "-s", "-s2", "--flat", "--dos-bin" };

            for (const char * flag : kFlags)
            {
                CommandLineOptions  opts = ParseOf ({ "CassoCli", "as65", "p.a65", flag, flag });

                Assert::IsTrue (opts.outputFormatConflict.empty(),
                                Widen (std::string (flag) + " twice was refused").c_str());
            }
        }

        //  Merlin takes its processor from the source, with XC, so a CPU flag
        //  on the command line is refused by name rather than ignored.
        TEST_METHOD (MerlinRefusesACpuFlag_AndSaysWhatToWriteInstead)
        {
            CommandLineOptions  opts = ParseOf ({ "CassoCli", "merlin", "p.s", "-x" });

            Assert::IsFalse (opts.cpuFlagRefusal.empty(), L"-x must be refused, not ignored");
            Assert::IsTrue  (opts.cpuFlagRefusal.find ("XC") != std::string::npos,
                             Widen (opts.cpuFlagRefusal).c_str());
            Assert::AreEqual (As65ExitStatus::kBadCommandLine,
                              Run ({ "CassoCli", "merlin", "p.s", "-x" }));
        }

        //  Assembling takes one source file.
        TEST_METHOD (TwoSourceFiles_AreRefused)
        {
            for (const char * mode : { "as65", "merlin" })
            {
                CommandLineOptions  opts = ParseOf ({ "CassoCli", mode, "one.a65", "two.a65" });

                Assert::IsFalse (opts.refusalMessage.empty(),
                                 Widen (std::string (mode) + " took two sources").c_str());
                Assert::AreEqual (As65ExitStatus::kBadCommandLine,
                                  Run ({ "CassoCli", mode, "one.a65", "two.a65" }),
                                  Widen (mode).c_str());
            }
        }

        //  A switch that needs a value and did not get one.
        TEST_METHOD (AValueSwitchWithNothingAfterIt_IsRefused)
        {
            Assert::AreEqual (As65ExitStatus::kBadCommandLine,
                              Run ({ "CassoCli", "as65", "p.a65", "-o" }),        L"as65 -o");
            Assert::AreEqual (CommandLineParser::kNothingStarted,
                              Run ({ "CassoCli", "run", "p.bin", "--load" }),     L"run --load");
            Assert::AreEqual (CommandLineParser::kNothingStarted,
                              Run ({ "CassoCli", "disk", "create", "d.dsk", "--type" }),
                              L"disk --type");
        }

        //  A value the switch cannot read.
        TEST_METHOD (AValueTheSwitchCannotRead_IsRefused)
        {
            const char *  kBad[][5] =
            {
                { "CassoCli", "run",  "p.bin", "--load",  "nonsense" },
                { "CassoCli", "run",  "p.bin", "--exec", "nonsense" },
                { "CassoCli", "run",  "p.bin", "--stop",  "nonsense" },
                { "CassoCli", "run",  "p.bin", "--fill",  "nonsense" },
                { "CassoCli", "run",  "p.bin", "--max-cycles", "nonsense" },
            };

            for (const auto & words : kBad)
            {
                CommandLineOptions  opts = ParseOf ({ words[0], words[1], words[2], words[3], words[4] });

                Assert::IsFalse (opts.refusalMessage.empty(),
                                 Widen (std::string (words[3]) + " took a value it cannot read").c_str());
            }

            //  disk's track and sector are decimal, and say so by name.
            for (const char * option : { "--track", "--sector" })
            {
                CommandLineOptions  opts =
                    ParseOf ({ "CassoCli", "disk", "sectorwrite", "d.dsk", "p.bin", option, "$3" });

                Assert::IsFalse (opts.refusalMessage.empty(),
                                 Widen (std::string (option) + " took a hex value").c_str());
            }

            //  and its addresses are hex, written the way the message says.
            CommandLineOptions  addr =
                ParseOf ({ "CassoCli", "disk", "put", "d.dsk", "p.bin", "--load", "nonsense" });

            Assert::IsFalse (addr.refusalMessage.empty(), L"--addr took a value it cannot read");
        }

        //  A switch belonging to a different mode is not a switch here.
        TEST_METHOD (ASwitchFromAnotherMode_IsUnknownHere)
        {
            //  --track is disk's, not the assembler's.
            Assert::AreEqual (As65ExitStatus::kBadCommandLine,
                              Run ({ "CassoCli", "as65", "p.a65", "--track", "3" }),
                              L"as65 does not take --track");

            //  -x is as65's, and Merlin refuses it by name rather than as
            //  unknown, which is the more useful answer.
            Assert::AreEqual (CommandLineParser::kNothingStarted,
                              Run ({ "CassoCli", "disk", "list", "d.dsk", "--flat" }),
                              L"disk does not take --flat");
        }

        //  An unknown switch, in every mode, with the status that mode spends.
        TEST_METHOD (AnUnknownSwitch_IsRefusedByEveryMode)
        {
            Assert::AreEqual (As65ExitStatus::kBadCommandLine,
                              Run ({ "CassoCli", "as65", "p.a65", "-Z" }),            L"as65");
            Assert::AreEqual (As65ExitStatus::kBadCommandLine,
                              Run ({ "CassoCli", "merlin", "p.s", "-Z" }),            L"merlin");
            Assert::AreEqual (CommandLineParser::kNothingStarted,
                              Run ({ "CassoCli", "run", "p.bin", "--nonsense" }),     L"run");
            Assert::AreEqual (CommandLineParser::kNothingStarted,
                              Run ({ "CassoCli", "disk", "list", "d.dsk", "--nonsense" }), L"disk");
        }

        //  A surplus argument, in the modes that count their positionals.
        TEST_METHOD (ASurplusArgument_IsRefused)
        {
            Assert::AreEqual (CommandLineParser::kNothingStarted,
                              Run ({ "CassoCli", "disk", "list", "d.dsk", "surplus" }),
                              L"disk list takes the image and nothing else");
        }
    };
}
