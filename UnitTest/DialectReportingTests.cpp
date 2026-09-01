#include "Pch.h"

#include "AssemblerTypes.h"
#include "Dialect.h"
#include "DialectProfile.h"
#include "DialectRegistry.h"
#include "DialectReporting.h"
#include "EhmTestHelper.h"
#include "Parser.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DialectReportingTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  NamedProfile
    //
    //  A profile that is in no registry, existing only to prove the report names
    //  the profile it was HANDED rather than the one the enumerator would have
    //  found. Its name is deliberately not a name any shipped dialect answers to,
    //  so a report built from the registry instead cannot accidentally match.
    //
    //  It parses nothing, because reporting never parses. Everything else is the
    //  seam's own defaults.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class NamedProfile : public DialectProfile
    {
    public:

        DialectId           GetId() const override { return DialectId::Count; }
        const char *        GetName() const override { return "ledger"; }

        CpuSelectionSource  GetCpuSelectionSource() const override { return CpuSelectionSource::CommandLine; }
        const char *        GetCpuDirectiveName() const override { return ""; }

        ParsedLine ParseLine (const std::string & line, int lineNumber) const override
        {
            ParsedLine  result = {};

            UNREFERENCED_PARAMETER (line);
            result.lineNumber = lineNumber;

            return result;
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Fixture
    //
    //  Case construction and the two questions every row of the reporting table
    //  asks: which sinks received a report, and what it said.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class Fixture
    {
    public:

        static AssemblerOptions MakeOptions (
            DialectId         dialect,
            DialectSelection  selection,
            bool              verbose,
            bool              listing)
        {
            AssemblerOptions  options;

            options.dialect          = dialect;
            options.dialectSelection = selection;
            options.verbose          = verbose;
            options.generateListing  = listing;

            return options;
        }



        static CpuReport MakeCpu (const char * name, CpuSelection selection)
        {
            CpuReport  cpu;

            cpu.name      = name;
            cpu.selection = selection;

            return cpu;
        }



        //  What a sink was told, or the empty string when it was told nothing.
        //  Fails outright if one sink received two reports, since a caller
        //  printing both would say the same thing twice.
        static std::string TextForSink (const std::vector<DialectReportLine> & lines, ReportSink sink)
        {
            std::string  text;
            int          matches = 0;

            for (const DialectReportLine & line : lines)
            {
                if (line.sink == sink)
                {
                    text = line.text;
                    matches++;
                }
            }

            Assert::IsTrue (matches <= 1, L"one sink received more than one report");

            return text;
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReportingTableRows
    //
    //  One test per row of the reporting table, INCLUDING the rows whose answer
    //  is "nowhere". Those are the half worth having: an implementation that
    //  reports on every run satisfies every positive row and still breaks the
    //  guarantee the whole design exists for.
    //
    //  The dialect rows hold the CPU at "stated on the command line" and the CPU
    //  rows hold the dialect at "stated", so each row is measured with the other
    //  axis silent. A test that let both speak would pass on either half.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ReportingTableRows)
    {
    public:

        //  Row: dialect stated by subcommand -> nowhere. Both sinks are open and
        //  the CPU is stated too, so there is nothing at all to say.
        TEST_METHOD (DialectStatedAndCpuStated_ReportsNothingAnywhere)
        {
            AssemblerOptions               options = Fixture::MakeOptions (DialectId::Merlin, DialectSelection::Stated, true, true);
            CpuReport                      cpu     = Fixture::MakeCpu ("6502", CpuSelection::StatedOnCommandLine);
            std::vector<DialectReportLine> lines   = DialectReporting::BuildReport (options, cpu);

            Assert::AreEqual ((size_t) 0, lines.size(), L"a stated dialect and a stated CPU are reported nowhere");
        }



        //  ...and it suppresses ONLY the dialect. A CPU the source picked is
        //  still worth reporting, because naming a dialect on the command line
        //  says nothing about which instruction set a directive went on to pick.
        TEST_METHOD (DialectStated_SuppressesTheDialectAndNotTheCpu)
        {
            AssemblerOptions               options = Fixture::MakeOptions (DialectId::Merlin, DialectSelection::Stated, true, false);
            CpuReport                      cpu     = Fixture::MakeCpu ("65C02", CpuSelection::SelectedInSource);
            std::vector<DialectReportLine> lines   = DialectReporting::BuildReport (options, cpu);

            Assert::AreEqual (std::string ("cpu: 65C02 (selected in source)"),
                              Fixture::TextForSink (lines, ReportSink::StandardError));
        }



        //  Row: dialect defaulted, verbose given -> standard error.
        TEST_METHOD (DialectDefaultedAndVerbose_GoesToStandardError)
        {
            AssemblerOptions               options = Fixture::MakeOptions (DialectId::As65, DialectSelection::Defaulted, true, false);
            CpuReport                      cpu     = Fixture::MakeCpu ("6502", CpuSelection::StatedOnCommandLine);
            std::vector<DialectReportLine> lines   = DialectReporting::BuildReport (options, cpu);

            Assert::AreEqual ((size_t) 1, lines.size(), L"verbose alone opens exactly one sink");
            Assert::AreEqual (std::string ("dialect: as65 (default)"),
                              Fixture::TextForSink (lines, ReportSink::StandardError));
        }



        //  Row: dialect defaulted, listing produced -> the listing header.
        TEST_METHOD (DialectDefaultedAndListing_GoesToTheListingHeader)
        {
            AssemblerOptions               options = Fixture::MakeOptions (DialectId::As65, DialectSelection::Defaulted, false, true);
            CpuReport                      cpu     = Fixture::MakeCpu ("6502", CpuSelection::StatedOnCommandLine);
            std::vector<DialectReportLine> lines   = DialectReporting::BuildReport (options, cpu);

            Assert::AreEqual ((size_t) 1, lines.size(), L"a listing alone opens exactly one sink");
            Assert::AreEqual (std::string ("dialect: as65 (default)"),
                              Fixture::TextForSink (lines, ReportSink::ListingHeader));
        }



        //  Row: dialect defaulted, neither verbose nor a listing -> not reported.
        //  Discoverable is not the same as always printed, and this is the row
        //  that says so.
        TEST_METHOD (DialectDefaultedWithNoSinkOpen_ReportsNothing)
        {
            AssemblerOptions               options = Fixture::MakeOptions (DialectId::As65, DialectSelection::Defaulted, false, false);
            CpuReport                      cpu     = Fixture::MakeCpu ("6502", CpuSelection::StatedOnCommandLine);
            std::vector<DialectReportLine> lines   = DialectReporting::BuildReport (options, cpu);

            Assert::AreEqual ((size_t) 0, lines.size(), L"nothing was requested, so nothing is reported");
        }



        //  ...and a caller that set nothing at all is one of those rows, which
        //  is the case the three "defaulted" rows exist for now that the command
        //  line always states a dialect. The options are left exactly as they
        //  arrive so the DEFAULT is what is under test: a default of "stated"
        //  would compile, would pass every case above, and would silently stop
        //  reporting for the only callers that still need it.
        TEST_METHOD (OptionsNobodyTouched_AreTreatedAsDefaulted)
        {
            AssemblerOptions                options;
            CpuReport                       cpu = Fixture::MakeCpu ("6502", CpuSelection::StatedOnCommandLine);
            std::vector<DialectReportLine>  lines;

            options.verbose = true;
            lines           = DialectReporting::BuildReport (options, cpu);

            Assert::AreEqual (std::string ("dialect: as65 (default)"),
                              Fixture::TextForSink (lines, ReportSink::StandardError));
        }



        //  Row: CPU stated by the flag -> nowhere. The dialect is defaulted here
        //  so a report IS produced; the CPU simply is not in it. A test that let
        //  the report be empty could not tell suppression from silence.
        TEST_METHOD (CpuStatedOnCommandLine_IsAbsentFromAReportThatIsProduced)
        {
            AssemblerOptions               options = Fixture::MakeOptions (DialectId::As65, DialectSelection::Defaulted, true, false);
            CpuReport                      cpu     = Fixture::MakeCpu ("65C02", CpuSelection::StatedOnCommandLine);
            std::vector<DialectReportLine> lines   = DialectReporting::BuildReport (options, cpu);

            Assert::AreEqual (std::string ("dialect: as65 (default)"),
                              Fixture::TextForSink (lines, ReportSink::StandardError));
        }



        //  Row: CPU selected in source, verbose given -> standard error.
        TEST_METHOD (CpuSelectedInSourceAndVerbose_GoesToStandardError)
        {
            AssemblerOptions               options = Fixture::MakeOptions (DialectId::Merlin, DialectSelection::Stated, true, false);
            CpuReport                      cpu     = Fixture::MakeCpu ("65C02", CpuSelection::SelectedInSource);
            std::vector<DialectReportLine> lines   = DialectReporting::BuildReport (options, cpu);

            Assert::AreEqual ((size_t) 1, lines.size(), L"verbose alone opens exactly one sink");
            Assert::AreEqual (std::string ("cpu: 65C02 (selected in source)"),
                              Fixture::TextForSink (lines, ReportSink::StandardError));
        }



        //  Row: CPU selected in source, listing produced -> the listing header.
        TEST_METHOD (CpuSelectedInSourceAndListing_GoesToTheListingHeader)
        {
            AssemblerOptions               options = Fixture::MakeOptions (DialectId::Merlin, DialectSelection::Stated, false, true);
            CpuReport                      cpu     = Fixture::MakeCpu ("65C02", CpuSelection::SelectedInSource);
            std::vector<DialectReportLine> lines   = DialectReporting::BuildReport (options, cpu);

            Assert::AreEqual ((size_t) 1, lines.size(), L"a listing alone opens exactly one sink");
            Assert::AreEqual (std::string ("cpu: 65C02 (selected in source)"),
                              Fixture::TextForSink (lines, ReportSink::ListingHeader));
        }



        //  Row: CPU left at the dialect's default -> reported wherever the
        //  dialect is. Both sinks open, and both are told the same thing.
        TEST_METHOD (CpuAtDialectDefault_IsReportedWhereverTheDialectIs)
        {
            AssemblerOptions               options  = Fixture::MakeOptions (DialectId::As65, DialectSelection::Defaulted, true, true);
            CpuReport                      cpu      = Fixture::MakeCpu ("6502", CpuSelection::DialectDefault);
            std::vector<DialectReportLine> lines    = DialectReporting::BuildReport (options, cpu);
            std::string                    expected = "dialect: as65 (default); cpu: 6502 (dialect default)";

            Assert::AreEqual ((size_t) 2, lines.size(), L"both sinks are open");
            Assert::AreEqual (expected, Fixture::TextForSink (lines, ReportSink::StandardError));
            Assert::AreEqual (expected, Fixture::TextForSink (lines, ReportSink::ListingHeader));
        }



        //  ...and it is reported even when the dialect was stated, which is the
        //  whole reason the row exists. Without it a developer who passed no
        //  flag and wrote no directive cannot tell "nothing selected a CPU" from
        //  "what I asked for was dropped".
        TEST_METHOD (CpuAtDialectDefault_IsReportedEvenWhenTheDialectWasStated)
        {
            AssemblerOptions               options = Fixture::MakeOptions (DialectId::Merlin, DialectSelection::Stated, true, false);
            CpuReport                      cpu     = Fixture::MakeCpu ("6502", CpuSelection::DialectDefault);
            std::vector<DialectReportLine> lines   = DialectReporting::BuildReport (options, cpu);

            Assert::AreEqual ((size_t) 1, lines.size(), L"the CPU alone still earns a report");
            Assert::AreEqual (std::string ("cpu: 6502 (dialect default)"),
                              Fixture::TextForSink (lines, ReportSink::StandardError));
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReportingSinks
    //
    //  The guarantee the reporting design exists to keep: standard output is
    //  never a destination, whatever the run looks like. Stdout carries the
    //  listing when no listing file is named, and build scripts pipe it, so a
    //  line printed there is corruption of the artifact rather than noise.
    //
    //  Swept over every combination rather than sampled, and the sweep asserts
    //  its own size and its own yield first. A sweep that visited nothing, or
    //  that produced no reports to inspect, would pass while checking nothing --
    //  which is indistinguishable in the output from a full run.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ReportingSinks)
    {
    public:

        TEST_METHOD (NoCombinationEverTargetsStandardOutput)
        {
            //  2 dialect selections x 3 CPU selections x verbose on/off x
            //  listing on/off.
            constexpr size_t  kCasesPerDialect = 24;

            const DialectSelection  kSelections[]    = { DialectSelection::Stated, DialectSelection::Defaulted };
            const CpuSelection      kCpuSelections[] = { CpuSelection::StatedOnCommandLine,
                                                         CpuSelection::SelectedInSource,
                                                         CpuSelection::DialectDefault };
            const bool              kFlagStates[]    = { false, true };

            std::span<const DialectRegistry::Entry>  dialects        = DialectRegistry::GetAllDialects();
            size_t                                   cases           = 0;
            size_t                                   reportsSeen     = 0;
            size_t                                   standardErrors  = 0;
            size_t                                   listingHeaders  = 0;

            Assert::IsTrue (!dialects.empty(), L"the dialect registry is empty, so this sweep checks nothing");

            for (const DialectRegistry::Entry & entry : dialects)
            {
                for (DialectSelection selection : kSelections)
                {
                    for (CpuSelection cpuSelection : kCpuSelections)
                    {
                        for (bool verbose : kFlagStates)
                        {
                            for (bool listing : kFlagStates)
                            {
                                AssemblerOptions                options = Fixture::MakeOptions (entry.id, selection, verbose, listing);
                                CpuReport                       cpu     = Fixture::MakeCpu ("6502", cpuSelection);
                                std::vector<DialectReportLine>  lines   = DialectReporting::BuildReport (options, cpu);

                                cases++;

                                for (const DialectReportLine & line : lines)
                                {
                                    reportsSeen++;

                                    Assert::IsTrue (line.sink != ReportSink::StandardOutput,
                                                    L"a report was routed to standard output, which carries the listing");

                                    standardErrors += (line.sink == ReportSink::StandardError) ? 1 : 0;
                                    listingHeaders += (line.sink == ReportSink::ListingHeader) ? 1 : 0;
                                }
                            }
                        }
                    }
                }
            }

            Assert::AreEqual (dialects.size() * kCasesPerDialect, cases, L"the sweep did not visit every combination");
            Assert::IsTrue (reportsSeen > 0, L"the sweep produced no reports, so it inspected no sinks");
            Assert::IsTrue (standardErrors > 0, L"no combination reached standard error");
            Assert::IsTrue (listingHeaders > 0, L"no combination reached the listing header");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReportingNames
    //
    //  Which dialect the report names. Swept over the registry rather than
    //  spelled out, so a dialect added to the table is covered without anyone
    //  editing this file -- and so a report that named one dialect for all of
    //  them cannot pass.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ReportingNames)
    {
    public:

        TEST_METHOD (EveryDialect_IsNamedByItsOwnRegistryName)
        {
            std::span<const DialectRegistry::Entry>  dialects = DialectRegistry::GetAllDialects();
            size_t                                   checked  = 0;

            Assert::IsTrue (!dialects.empty(), L"the dialect registry is empty, so this sweep checks nothing");

            for (const DialectRegistry::Entry & entry : dialects)
            {
                AssemblerOptions                options  = Fixture::MakeOptions (entry.id, DialectSelection::Defaulted, true, false);
                CpuReport                       cpu      = Fixture::MakeCpu ("6502", CpuSelection::StatedOnCommandLine);
                std::vector<DialectReportLine>  lines    = DialectReporting::BuildReport (options, cpu);
                std::string                     expected = std::string ("dialect: ") + entry.name + " (default)";

                Assert::AreEqual (expected, Fixture::TextForSink (lines, ReportSink::StandardError));

                checked++;
            }

            Assert::AreEqual (dialects.size(), checked);
        }



        //  A profile handed to the assembler directly is named by the profile,
        //  not by the enumerator beside it. The options below claim AS65 and
        //  supply a profile calling itself something else; a report built from
        //  the registry would say "as65" and this would catch it.
        TEST_METHOD (AnInjectedProfile_IsNamedRatherThanTheEnumerator)
        {
            NamedProfile                    profile;
            AssemblerOptions                options = Fixture::MakeOptions (DialectId::As65, DialectSelection::Defaulted, true, false);
            CpuReport                       cpu     = Fixture::MakeCpu ("6502", CpuSelection::StatedOnCommandLine);
            std::vector<DialectReportLine>  lines;

            options.dialectProfile = &profile;
            lines                  = DialectReporting::BuildReport (options, cpu);

            Assert::AreEqual (std::string ("dialect: ledger (default)"),
                              Fixture::TextForSink (lines, ReportSink::StandardError));
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ReportingArgumentValidation
    //
    //  Asking for the CPU to be reported without saying what it is called is a
    //  caller bug, and it is rejected as one. Printing the blank would produce a
    //  line indistinguishable from a CPU whose name is genuinely empty, which is
    //  the failure mode this whole feature keeps running into.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ReportingArgumentValidation)
    {
    public:

        TEST_METHOD (ReportingAnUnnamedCpu_IsRejected)
        {
            AssemblerOptions                options = Fixture::MakeOptions (DialectId::Merlin, DialectSelection::Stated, true, true);
            CpuReport                       cpu     = Fixture::MakeCpu ("", CpuSelection::DialectDefault);
            std::vector<DialectReportLine>  lines;

            {
                UnitTestHelpers::ExpectedEhmAssert  expect;

                lines = DialectReporting::BuildReport (options, cpu);

                expect.RequireCount (1);
            }

            Assert::AreEqual ((size_t) 0, lines.size(), L"a rejected request must report nothing at all");
        }



        //  ...and the same missing name is fine when the CPU is not being
        //  reported, since nothing is going to print it. Without this the
        //  validation could be tightened to "always require a name" and no test
        //  would notice.
        TEST_METHOD (AnUnnamedCpuThatIsNotReported_IsAccepted)
        {
            AssemblerOptions                options = Fixture::MakeOptions (DialectId::As65, DialectSelection::Defaulted, true, false);
            CpuReport                       cpu     = Fixture::MakeCpu ("", CpuSelection::StatedOnCommandLine);
            std::vector<DialectReportLine>  lines   = DialectReporting::BuildReport (options, cpu);

            Assert::AreEqual (std::string ("dialect: as65 (default)"),
                              Fixture::TextForSink (lines, ReportSink::StandardError));
        }
    };
}
