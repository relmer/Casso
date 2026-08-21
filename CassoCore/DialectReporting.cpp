#include "Pch.h"

#include "DialectReporting.h"

#include "DialectProfile.h"
#include "DialectRegistry.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DialectReporting::BuildReport
//
//  Answers both halves of the question at once: whether there is anything worth
//  saying, and where saying it is permitted.
//
//  The two halves are separate and neither implies the other. What is worth
//  saying depends on how the dialect and the CPU were chosen; where it may be
//  said depends on what the run was asked to produce. A run with nothing worth
//  saying reports nothing however verbose it is, and a run with something worth
//  saying still reports nothing if it was asked for neither a verbose account
//  nor a listing -- discoverable is not the same as always printed.
//
//  The dialect and the CPU are decided INDEPENDENTLY, which is the part that is
//  easy to get wrong. A stated dialect suppresses only the dialect: source that
//  selects its own CPU is worth reporting whether or not the dialect was named,
//  because naming a dialect on the command line says nothing about which
//  instruction set a directive inside the file went on to pick.
//
//  Nothing here can produce a report on stdout. That is not an omission to be
//  restored later: stdout carries the listing when no listing file is named,
//  and a line printed there ends up inside a piped artifact.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DialectReportLine> DialectReporting::BuildReport (
    const AssemblerOptions  & options,
    const CpuReport         & cpu)
{
    HRESULT                         hr                  = S_OK;
    std::vector<DialectReportLine>  lines;
    std::string                     text;
    bool                            includeDialect      = options.dialectSelection == DialectSelection::Defaulted;
    bool                            includeCpu          = cpu.selection != CpuSelection::StatedOnCommandLine;
    bool                            hasCpuName          = !cpu.name.empty();
    bool                            hasAnythingToReport = false;
    bool                            cpuIsNamedIfShown   = false;



    hasAnythingToReport = includeDialect || includeCpu;
    cpuIsNamedIfShown   = !includeCpu || hasCpuName;

    BAIL_OUT_IF (!hasAnythingToReport, S_OK);

    // A caller asking for the CPU to be reported and supplying no name for it
    // has a bug. Reporting an unnamed CPU would print a blank where the answer
    // goes, which reads exactly like a CPU whose name happens to be empty.
    CBRAEx (cpuIsNamedIfShown, E_INVALIDARG);

    text = ComposeText (options, cpu, includeDialect, includeCpu);

    if (options.verbose)
    {
        lines.push_back ({ ReportSink::StandardError, text });
    }

    if (options.generateListing)
    {
        lines.push_back ({ ReportSink::ListingHeader, text });
    }

Error:
    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DialectReporting::ComposeText
//
//  Builds the one line both sinks receive.
//
//  One text rather than one per sink, deliberately. The two destinations differ
//  in who reads them, not in what they need to say, and a listing header that
//  worded the same fact differently from the verbose account would give two
//  answers to one question.
//
//  Each half states not just the value but how it was arrived at, because the
//  value alone is the ambiguous part. "6502" answers "which CPU"; it does not
//  answer "did anything choose this", and that is what a developer reads such a
//  line to find out.
//
////////////////////////////////////////////////////////////////////////////////

std::string DialectReporting::ComposeText (
    const AssemblerOptions  & options,
    const CpuReport         & cpu,
    bool                      includeDialect,
    bool                      includeCpu)
{
    const DialectProfile  & profile = (options.dialectProfile != nullptr)
                                          ? *options.dialectProfile
                                          : DialectRegistry::Get (options.dialect);
    std::string             text;



    if (includeDialect)
    {
        text = std::string ("dialect: ") + profile.GetName() + " (default)";
    }

    if (includeCpu)
    {
        if (!text.empty())
        {
            text += "; ";
        }

        text += "cpu: " + cpu.name;
        text += (cpu.selection == CpuSelection::SelectedInSource) ? " (selected in source)"
                                                                  : " (dialect default)";
    }

    return text;
}
