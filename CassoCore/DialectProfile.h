#pragma once

#include "Dialect.h"
#include "Parser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CpuSelectionSource
//
//  Where a dialect takes its CPU target from. Part of the PROFILE, not of the
//  mechanism -- as65 has no in-source CPU directive and so takes it from the
//  command line, while Merlin has one and takes it from there exclusively.
//
//  This is what lets the command-line parser refuse a CPU flag without knowing
//  which dialect it is refusing it for. A dialect-specific branch in the shared
//  parser would be a per-dialect special case in the very mechanism that exists
//  to avoid them.
//
////////////////////////////////////////////////////////////////////////////////

enum class CpuSelectionSource
{
    CommandLine,    // the CPU comes from a flag; no in-source directive exists
    InSource,       // the dialect's own directive decides; a flag is refused
};





////////////////////////////////////////////////////////////////////////////////
//
//  DialectProfile
//
//  The complete syntactic personality of one assembler: how source is READ.
//  What the machine then does with it -- the two-pass engine, the expression
//  evaluator, the opcode tables -- is shared and must stay that way. Three
//  front doors, one room.
//
//  The seam is deliberately NARROW. Line parsing is the one thing the dialects
//  genuinely disagree about, and everything downstream consumes the ParsedLine
//  it produces without caring which profile made it. Widening this interface is
//  how a mechanism quietly becomes hard-coded for the dialects that happen to
//  exist, so a profile that seems to need a new virtual is a signal to stop and
//  question the seam rather than to add one.
//
//  Adding a dialect must not require touching the engine, the evaluator, or the
//  opcode tables. That claim is not left as an assertion: a synthetic test-only
//  profile in the unit tests proves it, and `023-ca65-dialect` gates on it.
//
////////////////////////////////////////////////////////////////////////////////

class DialectProfile
{
public:
    virtual ~DialectProfile () = default;

    // Identity. `name` is the spelling used on the command line and in
    // diagnostics, so a message names the dialect the way the user selected it.
    virtual DialectId           GetId   () const = 0;
    virtual const char *        GetName () const = 0;

    // Whether a command-line CPU flag applies, or is refused in favor of this
    // dialect's own in-source directive.
    virtual CpuSelectionSource  GetCpuSelectionSource () const = 0;

    // The in-source directive to name when refusing a CPU flag. Empty when
    // GetCpuSelectionSource() is CommandLine, since there is nothing to name.
    virtual const char *        GetCpuDirectiveName () const = 0;

    // One source line into its parts. Purely syntactic: nothing is evaluated,
    // no symbol is looked up, no address assigned.
    virtual ParsedLine          ParseLine (const std::string & line, int lineNumber) const = 0;

    // Where assembly starts when the source names no origin. A real difference
    // between dialects rather than a convenience: as65 begins at 0 and Merlin at
    // $8000, and LABELS.S -- which contains no origin directive at all and ships
    // an object loading at $8000 -- is what settles Merlin's.
    //
    // A default rather than a pure virtual, because a dialect with no opinion
    // should not have to state one. This is the seam being EXTENDED, which
    // SC-009 expressly permits; what it forbids is a dialect reaching into how
    // the assembly runs.
    virtual Word                GetDefaultOrigin () const { return 0; }

    // How this dialect's expressions bind their operators. Assemblers of the
    // period commonly gave operators no precedence at all, and the shared
    // evaluator honors the answer rather than each profile carrying a private
    // expression parser -- the operator set and the folds stay one
    // implementation.
    virtual OperatorBinding     GetOperatorBinding () const { return OperatorBinding::ByPrecedence; }

    // The character marking a label as local to the enclosing global one, or 0
    // for a dialect with no such concept.
    //
    // A character rather than a predicate because the engine has to do two
    // different things with it -- recognize a definition, and recognize a
    // reference inside an operand -- and a dialect that answered only the first
    // would leave every use of a local label unresolvable. Naming the character
    // once keeps those two answers from disagreeing.
    virtual char                GetLocalLabelPrefix () const { return 0; }
};
