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
//  MnemonicAlias
//
//  One extra spelling a dialect accepts for an instruction the shared opcode
//  tables already hold. Merlin writes the carry branches BLT and BGE; the
//  machine has no such opcodes and never will, because they ARE BCC and BCS.
//
//  This is the instruction-layer twin of the directive spelling table, and it
//  exists for the same reason. A dialect is a second TABLE, not a second
//  assembler: the alias is resolved once, at parse time, and nothing downstream
//  ever learns that the source said something else. The alternative -- teaching
//  the lookup, the size estimator, the branch-range check and the encoder each
//  to consider a second name -- puts a per-dialect special case in five places
//  in shared mechanism, which is what the profile seam exists to prevent.
//
//  An alias must name an instruction the base table already carries. One that
//  does not is not an alias at all; it silently becomes an unknown mnemonic on
//  every line that uses it, so the registry sweep checks the whole table rather
//  than trusting the rows.
//
////////////////////////////////////////////////////////////////////////////////

struct MnemonicAlias
{
    const char *  spelling;      // what this dialect's source may write
    const char *  instruction;   // the opcode table's name for the same instruction
};





////////////////////////////////////////////////////////////////////////////////
//
//  MacroSyntax
//
//  Everything a dialect has to say about how macros are WRITTEN, in one value.
//
//  One virtual rather than six accessors, and that is the point. Every field
//  here is a spelling or a separator -- data the expander reads and never a
//  decision it delegates -- so the alternative was six more entries on a seam
//  the profile contract asks to keep narrow. Collapsing them also means a new
//  dialect answers the whole question in one place instead of discovering the
//  sixth accessor after its macros silently misbehave.
//
//  The defaults describe a dialect with keyword-terminated macro bodies and
//  comma-separated arguments, which is what a profile stating nothing gets.
//
////////////////////////////////////////////////////////////////////////////////

struct MacroSyntax
{
    // The bare keyword that closes a macro body, for a dialect whose directive
    // table carries no macro-end token. Empty when the token is the only route.
    //
    // Both routes exist because as65's `endm` is not in its spelling table at
    // all -- it is a macro-body keyword rather than a directive, and tokenizing
    // it would cost a lookup on every line of every file to serve lines that
    // appear only here.
    const char *  endKeyword            = "";

    // The keyword declaring macro-local labels inside a body, or empty for a
    // dialect with no such declaration.
    //
    // Data for the same reason the local-label prefix is: the expander has to
    // recognize the declaration and DROP it, so a hard-coded spelling deletes
    // any line another dialect's source happens to begin with that word. In a
    // field-based dialect the first word of a line is a label, so `LOCAL LDA #1`
    // is a labeled instruction and dropping it costs both the label and two
    // bytes with no diagnostic.
    const char *  localKeyword          = "";

    // The keyword that invokes a macro explicitly, with the macro's name first
    // in the operand. Empty for a dialect that invokes by bare name only.
    const char *  callKeyword           = "";

    // The character introducing a positional parameter inside a body -- `]1`
    // through `]9` in Merlin -- or 0 for a dialect with no positional form.
    //
    // Substitution is textual and deliberately ignores identifier boundaries,
    // because Merlin's own macro library splices a parameter INTO a name:
    // `LDX #A]1-ADRTBL` and `LDX #]1END-]1-1` are both on the distribution disk.
    char          parameterSigil        = 0;

    // What separates one argument from the next at an invocation.
    char          argumentSeparator     = ',';

    // Whether every label a macro body defines is made unique per expansion
    // without being declared. as65 requires the declaration; Merlin does not,
    // and its own sources prove it -- `MAKE DUMP.S` expands `INCD` twice and
    // `STORE` three times, each redefining a bare label, and ships a working
    // object.
    bool          labelsArePerExpansion = false;
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

    // Extra instruction spellings, as DATA rather than as behavior. Empty for a
    // dialect that spells every instruction the way the opcode table does, which
    // is why this is a default rather than a pure virtual.
    virtual std::span<const MnemonicAlias>  GetMnemonicAliases () const { return {}; }

    // How this dialect writes macros: terminator, parameter form, argument
    // separator, and whether body labels are unique per expansion. One value
    // rather than an accessor apiece -- see MacroSyntax above.
    virtual MacroSyntax         GetMacroSyntax () const { return {}; }
};
