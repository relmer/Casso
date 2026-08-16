#pragma once

#include "DialectProfile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  As65Dialect
//
//  Casso's original dialect, and the one every existing caller gets by default.
//
//  Its grammar was the assembler's only grammar, living directly in
//  Parser::ParseLine. Moving it behind the profile seam changed nothing about
//  it -- the body here is the same body, in the same order, because the ORDER of
//  its tests IS the grammar and reshuffling them breaks things quietly.
//
////////////////////////////////////////////////////////////////////////////////

class As65Dialect : public DialectProfile
{
public:
    DialectId           GetId   () const override { return DialectId::As65; }
    const char *        GetName () const override { return "as65"; }

    // as65 has no in-source CPU directive, so the command line is the only
    // place a target can come from.
    CpuSelectionSource  GetCpuSelectionSource () const override { return CpuSelectionSource::CommandLine; }
    const char *        GetCpuDirectiveName   () const override { return ""; }

    ParsedLine          ParseLine (const std::string & line, int lineNumber) const override;
};
