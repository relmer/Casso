#pragma once

#include "AssemblerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReportSink
//
//  Where a report about the active dialect and CPU is allowed to go.
//
//  StandardOutput is listed and is NEVER chosen. It is here so the guarantee is
//  testable rather than merely intended: when no listing file is named the
//  LISTING goes to stdout, and build scripts pipe it, so a banner printed there
//  is not a stray line -- it is corruption of the artifact the caller asked
//  for. A sweep asserting that no report ever claims this sink is an assertion
//  that cannot be written unless the value exists to be compared against.
//
//  ListingHeader is not stdout, even when the listing itself ends up on stdout.
//  The difference is that the header is part of the listing rather than a line
//  beside it, so anything reading the listing finds it where a listing header
//  belongs instead of ahead of the first record.
//
////////////////////////////////////////////////////////////////////////////////

enum class ReportSink
{
    StandardOutput,   // never chosen; see above
    StandardError,
    ListingHeader,
};





////////////////////////////////////////////////////////////////////////////////
//
//  CpuSelection
//
//  How the active CPU target came to be active.
//
//  Three states rather than two, and the third is the one that earns its keep.
//  A target that was stated on the command line needs no report, for the same
//  reason a stated dialect does not. A target chosen by a directive in the
//  source is worth reporting, since the invocation does not record it. And a
//  target that nothing chose is worth reporting TOO -- otherwise a developer
//  who passed no flag and wrote no directive cannot tell "nothing selected a
//  CPU, so the dialect's default stands" from "the thing I asked for was
//  quietly dropped", and those two look identical in an empty report.
//
////////////////////////////////////////////////////////////////////////////////

enum class CpuSelection
{
    StatedOnCommandLine,   // a flag named it; the invocation is the record
    SelectedInSource,      // the dialect's own directive named it
    DialectDefault,        // nothing named it, so the dialect's default stands
};





////////////////////////////////////////////////////////////////////////////////
//
//  CpuReport
//
//  What is known about the active CPU target: what to call it, and how it was
//  chosen.
//
//  The name is supplied by the caller rather than derived here. Core's
//  assembler has no CPU-target vocabulary of its own -- instruction sets arrive
//  as raw tables, base and extended, with no names attached -- and inventing an
//  enumeration here to serve one report would be a second list of CPUs to keep
//  in step with the first, for no reader's benefit.
//
////////////////////////////////////////////////////////////////////////////////

struct CpuReport
{
    std::string   name;
    CpuSelection  selection = CpuSelection::DialectDefault;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DialectReportLine
//
//  One report, and where it goes. A report can be due in more than one place at
//  once -- a verbose run that also writes a listing wants it in both -- so this
//  is returned as a list rather than as a single destination.
//
////////////////////////////////////////////////////////////////////////////////

struct DialectReportLine
{
    ReportSink   sink = ReportSink::StandardError;
    std::string  text;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DialectReporting
//
//  Decides WHAT to say about the active dialect and CPU, and WHERE it may be
//  said. Callers print what comes back and choose nothing themselves.
//
//  Both halves of that live here for the same reason. The rule is not "print a
//  banner": it is a set of cases, several of which report nothing at all, and a
//  caller reimplementing it is a caller that will eventually print
//  unconditionally because that is the shorter program. Keeping the decision in
//  core also keeps it reachable from a test, where an executable's console
//  output is reachable only by running the executable.
//
////////////////////////////////////////////////////////////////////////////////

class DialectReporting
{
public:
    // Every report due for this run, which is frequently none.
    static std::vector<DialectReportLine>  BuildReport (const AssemblerOptions & options,
                                                        const CpuReport        & cpu);

private:
    static std::string  ComposeText (const AssemblerOptions & options,
                                     const CpuReport        & cpu,
                                     bool                     includeDialect,
                                     bool                     includeCpu);
};
