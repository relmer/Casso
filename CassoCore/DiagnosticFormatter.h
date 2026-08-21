#pragma once

#include "AssemblerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiagnosticSeverity
//
////////////////////////////////////////////////////////////////////////////////

enum class DiagnosticSeverity
{
    Warning,
    Error,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiagnosticFormatter
//
//  Turns one diagnostic into the line an editor can parse to jump to it.
//
//  In core rather than beside the printing, because choosing which file to name
//  and whether a column belongs in the output is a DECISION, and decisions have
//  to be reachable from the tests. The executable's share is calling print.
//
//  Two facts drive the shape. An empty `file` means the top-level input rather
//  than "unknown", so the caller's own path is the fallback and a diagnostic
//  that carries no file of its own formats exactly as it always did. And a
//  column of 0 means "no column known", so it is omitted entirely rather than
//  printed as a zero an editor would jump to.
//
////////////////////////////////////////////////////////////////////////////////

class DiagnosticFormatter
{
public:
    static std::string Format (const AssemblyError    & diagnostic,
                               const std::string      & fallbackFile,
                               DiagnosticSeverity       severity);
};
