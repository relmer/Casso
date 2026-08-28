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

    // `endm` closes a macro body and `local a,b` declares the labels the
    // expander renames per invocation. Both are macro-body keywords rather than
    // directives, so neither is in the spelling table -- and stating them here
    // rather than comparing against literals in the expander keeps them special
    // only in the dialect that defines them. Everything else keeps the default:
    // arguments are comma-separated, parameters are named rather than
    // positional, and a body label is renamed only when it was declared.
    MacroSyntax         GetMacroSyntax () const override;

    // as65's own table, so a word another dialect rejects can be attributed
    // here. Every accepted spelling is in it, dotted and bare alike.
    Directive           GetDirectiveForSpelling (const std::string & upperSpelling) const override;

    // `RMB`, which is reserved storage given one operand and the Rockwell bit
    // instruction given two. Answered separately from the table above for the
    // reason stated on the seam: it is only sound where the context has ruled
    // the instruction out.
    Directive           GetAmbiguousDirectiveForSpelling (const std::string & upperSpelling) const override;

    // as65's own name for a token, dotted, so a line the assembler writes for
    // itself reads back through as65's own parser.
    const char *        GetSpellingForDirective (Directive token) const override;

    // `NOP <count>` emits that many, which is as65's alone. Merlin has no such
    // form, and a fixed spelling in the engine gave it one.
    const char *        GetMultiNopMnemonic () const override { return "NOP"; }

    // as65 rewrites a backward, in-range `JMP` as a `BRA` on the 65SC02, and
    // gives the source OPT / NOOPT to steer it. Merlin does neither, so this is
    // stated here rather than made the engine's ambient behavior.
    bool                HasJumpToBranchOptimization () const override { return true; }
};
