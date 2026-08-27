#pragma once

#include "Dialect.h"





class DialectProfile;





////////////////////////////////////////////////////////////////////////////////
//
//  DialectHelp
//
//  The tool's help text about dialects: which ones can be selected, what flags
//  each one takes, where its CPU target comes from, and where its supported
//  subset ends.
//
//  Every line is DERIVED. The selectable dialects come from the registry, the
//  flags from the same table the parser walks, the CPU sentence from the
//  profile's own answer about where its CPU comes from, and the boundary from
//  the boundary table. Nothing here is a second account of any of those, which
//  is what keeps help from describing a tool that no longer exists.
//
//  In core rather than beside the executable's printing for the reason the
//  reporting decision is: text composed in an executable is reachable only by
//  running it, so "help cannot disagree with the parser" would be a claim
//  nothing checks. The executable prints what comes back.
//
////////////////////////////////////////////////////////////////////////////////

class DialectHelp
{
public:
    // Every selectable dialect and everything the tool has to say about it.
    // `flagPrefix` is the one the user typed, so flags are written back the way
    // they invoked the tool.
    static std::string  GetAllDialects (char flagPrefix);

    // One dialect's own section.
    static std::string  GetDialect (const DialectProfile & profile, char flagPrefix);

    // A dialect's flag lines alone, for a caller printing its own heading. The
    // lines still come from the parser's tables; only the invocation line above
    // them is left out.
    static std::string  GetDialectFlags (const DialectProfile & profile, char flagPrefix);

    //  One dialect's flags, from the same table its parser walks. Public
    //  because a tiered help page asks for exactly one dialect's block, and
    //  generating it here is what keeps the page and the parser in step.
    static std::string  ComposeFlagLines (DialectId dialect, char flagPrefix);

private:
    static std::string  PadTo            (const std::string & text, size_t width);
};
