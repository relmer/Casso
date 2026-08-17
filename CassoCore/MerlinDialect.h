#pragma once

#include "DialectProfile.h"
#include "StringEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDirectiveTable
//
//  Merlin's spelling -> token mapping. A second dialect is a second TABLE, not a
//  second assembler, which is what `Directive.h` says the token enum exists for.
//
//  Every spelling here was taken from the committed vendor sources rather than
//  transcribed from the manual, with two deliberate exceptions noted below. The
//  corpus supplies its own frequencies -- DCI appears 130 times, ASC 35, ERR 17,
//  HEX 13 -- so the table reflects what Merlin source actually contains.
//
//  DDB is the first exception: it appears nowhere on the disk. It is included
//  anyway, because absence from one vendor's source is not absence from the
//  language, and DDB is the one data directive whose byte order the existing
//  Word token cannot express.
//
//  IF is the second, and it is the sharper lesson. It appears in none of the
//  committed sources either -- and the distribution disk's own macro library
//  uses it a dozen times over, because it is how a Merlin macro dispatches on
//  addressing mode. A vocabulary derived from the files that happened to be
//  committed is a vocabulary with holes in it.
//
//  This hole did not read as one, which is the part worth remembering. The
//  conditional dispatcher falls back to the OTHER dialect's table for a bare
//  mnemonic, so the missing spelling resolved there instead and every use was
//  reported as an expression that would not evaluate -- a message about the
//  operand, on a directive this table did not claim.
//
//  Merlin has NO dotted spellings, so these tokens have no canonical name in
//  DirectiveTable, which is as65's. Diagnostics quoting a Merlin directive must
//  come through GetCanonicalName here.
//
////////////////////////////////////////////////////////////////////////////////

class MerlinDirectiveTable
{
public:
    struct Spelling
    {
        const char *  name;
        Directive     token;
    };

    // `word` is expected upper-cased. Returns Directive::None when the spelling
    // is not a Merlin directive.
    static Directive           FromSpelling (const std::string & word);

    // Merlin's own spelling for a token, for diagnostics and listings. Empty for
    // a token Merlin does not have.
    static const char *        GetCanonicalName (Directive directive);

    // Every accepted spelling, so a test can sweep the vocabulary rather than a
    // hand-picked sample -- matching DirectiveTable::GetAllSpellings.
    static std::span<const Spelling>  GetAllSpellings();

    // Which encoding one of the six string spellings selects. The spellings are
    // Merlin's, so the mapping lives here; what each mode DOES with the text is
    // dialect-independent and lives in StringEncoding.
    //
    // Returns Plain for a spelling that is not a string directive, which callers
    // must not reach -- resolve the token first.
    static StringEncodingMode  EncodingModeForSpelling (const std::string & spelling);
};





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect
//
//  Glen Bredon's Merlin, in the absolute subset this feature supports.
//
//  The line model is FIELD-BASED, not column-based. Runs of whitespace separate
//  the label, opcode, operand, and comment fields, and the only significant
//  column is the first: a line beginning with whitespace has no label. Tabs are
//  ordinary whitespace and are never expanded, because tab stops affect display
//  and nothing else. The tidy columns in a Merlin listing are the EDITOR's
//  formatting, and a parser demanding an opcode at a fixed column would reject
//  source Merlin accepts.
//
//  A semicolon is NOT a comment introducer "anywhere". It introduces a comment
//  only where it begins a field; inside the operand it is data, and it is how
//  Merlin separates macro arguments -- `ADD SUMSTR;DEFLEN;PL` passes three. A
//  parser stripping from the first semicolon would silently truncate every macro
//  call on the vendor disk to its first argument, which is the class of bug that
//  produces plausible-looking wrong bytes rather than an error.
//
//  Whole-line comments follow from the same rule rather than being special
//  cases. `*` in column 1 is one. So is `;` in column 1 -- with no label present,
//  column 1 IS the first field boundary, so a semicolon there is simply a
//  semicolon beginning a field. Eight such lines appear across three vendor
//  sources.
//
//  THE OPERAND SCANNER MUST KNOW THE MNEMONIC. Merlin's string directives take
//  ANY character as their delimiter, not a fixed quote set, and the vendor
//  source relies on it precisely where a fixed set would fail:
//
//      ASC !7" "&$9F!          delimiter is !, because the string contains "
//      ASC !" ASC ""!          likewise, and would be shredded by a " scanner
//
//  So the operand of a string directive runs from its opening delimiter to the
//  next occurrence of that same character, and whitespace inside it is payload.
//  164 of the 166 string lines on the disk use `"`; the two that do not are the
//  reason this is a rule about delimiters and not about quotes.
//
//  THE SIGIL IS TWO THINGS. `]COUNT` is a variable symbol -- reassignable,
//  where an ordinary label is not -- and `]1` through `]9` are positional macro
//  parameters. The digit is the whole distinction, and it has to be, because
//  the two live in the same source: a macro body may reference a variable and a
//  variable's expression may hold a parameter. Parameters are substituted
//  textually at expansion, before any symbol exists; variables are rewritten
//  here, into names the shared expression tokenizer can lex.
//
//  What this profile does NOT yet do: reach a user. Every directive it claims
//  is now either carried out or refused by name, but the profile is not
//  selectable from the command line until the `merlin` subcommand exists -- so
//  this is a dialect nothing outside the tests can ask for yet, rather than a
//  broken advertised feature.
//
////////////////////////////////////////////////////////////////////////////////

class MerlinDialect : public DialectProfile
{
public:
    DialectId           GetId   () const override { return DialectId::Merlin; }
    const char *        GetName () const override { return "merlin"; }

    // Merlin selects its CPU with XC in the source, so a command-line CPU flag
    // is refused rather than ignored -- accepting extended opcodes without the
    // directive would assemble source real Merlin rejects.
    CpuSelectionSource  GetCpuSelectionSource () const override { return CpuSelectionSource::InSource; }
    const char *        GetCpuDirectiveName   () const override { return "XC"; }

    ParsedLine          ParseLine (const std::string & line, int lineNumber) const override;

    // Merlin assembles to $8000 when the source names no origin. LABELS.S proves
    // it: the file contains no ORG and no object-file directive, and its shipped
    // object loads at $8000. as65 defaults to 0, so this cannot be one shared
    // default -- and getting it wrong yields 984 byte-perfect bytes at the wrong
    // address, which reads as a far deeper problem than it is.
    Word                GetDefaultOrigin () const override { return 0x8000; }

    // Merlin has no operator precedence: an expression folds strictly left to
    // right and parentheses are the only grouping. LABELS.S is the oracle --
    // `ERR END-LABTBL-1/$700` bounds its table at seven pages, and under
    // ordinary precedence the division binds first, the expression collapses to
    // the table's own length, and the assertion fires on a file the vendor
    // shipped a working object for.
    OperatorBinding     GetOperatorBinding () const override { return OperatorBinding::LeftToRight; }

    // ORG relocates and does not seek. Merlin's output is one contiguous stream
    // whatever the origin says, and `MAKE DUMP` proves it: three sections at
    // $9000, $0300 and $0900 in a single 589-byte object loading at $9000, with
    // the $0300 section beginning at file offset 89 directly after the loader.
    // An operandless ORG then means "resync the program counter to where output
    // has actually reached", which the vendor writes twice and comments as
    // recalling the real load address.
    OriginSemantic      GetOriginSemantic () const override { return OriginSemantic::ProgramCounterOnly; }

    // A shift or rotate is written with no operand at all for accumulator mode.
    // `MAKE DUMP.S` has five bare LSRs and never writes the accumulator by name.
    OperandlessForm     GetOperandlessForm () const override { return OperandlessForm::ImpliedOrAccumulator; }

    // A double-quoted character constant is HIGH ASCII, matching the high-bit
    // convention Merlin's string directives take from their delimiter. The
    // apostrophe form stays low, and is what the shared tokenizer already had.
    char                GetHighAsciiCharDelimiter () const override { return '"'; }

    // A leading colon makes a label local to the global label above it, so the
    // same short name can be reused throughout a file. The vendor sources lean
    // on it hard: CLOCK.S defines `:OK` twice under different globals, and
    // `:RET`, `:EXIT` and `:GK` recur across files. Without scoping, every one
    // of those is a duplicate-label error.
    char                GetLocalLabelPrefix () const override { return ':'; }

    // BLT and BGE, Merlin's names for BCC and BCS. Supplied as data so the
    // resolution happens once at parse time rather than as a Merlin branch in
    // the opcode lookup, the size estimator, the branch-range check and the
    // encoder -- four places in shared mechanism, for two spellings.
    std::span<const MnemonicAlias>  GetMnemonicAliases() const override;

    // Merlin spells exclusive-or `!` and inclusive-or `.`, where the shared
    // punctuation reads the first as logical-not and does not treat the second
    // as punctuation at all. Both are settled by CLOCK.S against its two
    // shipped objects rather than taken from the manual.
    std::span<const OperatorSpelling>  GetOperatorSpellings() const override;

    // Merlin computes in unsigned 16-bit quantities, operands and intermediates
    // alike. CLOCK.S turns that into a build switch -- `VERSION-25/-1` is 1 only
    // because $FFFF divided by $FFFF is, and 0 for any other version because the
    // numerator is smaller than the divisor. Signed 32-bit arithmetic reads the
    // same line as -13 / -1 and fails an assembly the vendor shipped.
    ArithmeticWidth     GetArithmeticWidth () const override { return ArithmeticWidth::Word16; }

    // `?` is legal inside a Merlin symbol. Four occur in the vendor sources --
    // `CMD?` in KEYMAC.S, and `CORR?`, `ISY?` and `RNGOK?` in the linker demo --
    // and only the first sits in a file that ships an object, which is what
    // makes it a byte-comparison failure rather than a matter of taste.
    const char *        GetExtraSymbolCharacters () const override { return "?"; }

    // Merlin's macro syntax: a triple-angle terminator carried as a token,
    // positional parameters written with the variable sigil, semicolon-separated
    // arguments, and body labels made unique per expansion without any
    // declaration to say so.
    MacroSyntax         GetMacroSyntax () const override;

    // Where Merlin support ends: relocatable mode and its entry and external
    // declarations, a second CPU selection, the output file-type directive and
    // the save-object directive. Supplied as the profile's own data so the
    // assembler refuses them without ever naming this dialect.
    std::span<const SubsetBoundaryRow>  GetSubsetBoundary () const override;

    // Merlin's own table, so a word as65 rejects can be attributed here rather
    // than reported as an unknown instruction.
    Directive           GetDirectiveForSpelling (const std::string & upperSpelling) const override;

    // Merlin's own name for a token, so a closer the assembler writes for itself
    // is `FIN` and not another dialect's word. Merlin has no dotted spellings,
    // so this is the only place such a line can come from.
    const char *        GetSpellingForDirective (Directive token) const override;

    // The mistake a developer arriving from another assembler makes first: a
    // label written anywhere but column 1. Merlin's line model has no other way
    // to read an indented word than as the opcode field, so the label becomes an
    // operation nobody defined and the instruction beside it becomes its
    // operand -- and "that is not an instruction" describes the symptom while
    // saying nothing about the cause.
    std::string         ExplainUnknownOperation (const ParsedLine & parsed,
                                                 bool               operandNamesAnOperation) const override;

    // What one positional parameter binds under, so the directive that assigns
    // them outside a macro call defines exactly the symbols a reference in the
    // source resolves to.
    std::string         GetPositionalParameterSymbol (int index) const override;

    // The stored name a variable symbol binds under. Public so a test can state
    // the expectation in the same terms the profile does rather than repeating
    // the marker as a literal.
    static std::string  QualifyVariableName (const std::string & spelling);

private:
    static bool         IsFieldSpace            (char ch);
    static bool         IsVariableNameStart     (char ch);
    static bool         IsParameterIndex        (char ch);
    static bool         IsCharConstantDelimiter (char ch);
    static bool         IsExplicitCallSpelling  (const std::string & opcode);
    static bool         TakesDelimitedText      (const std::string & mnemonic);
    static bool         TakesFileName           (const std::string & mnemonic);
    static void         SkipCharConstant        (const std::string & line, size_t & pos);
    static void         SkipFieldSpace          (const std::string & line, size_t & pos);
    static std::string  ReadPlainField          (const std::string & line, size_t & pos);
    static std::string  ReadOperandField        (const std::string & line, size_t & pos, const std::string & mnemonic);
    static std::string  QualifyVariableRefs     (const std::string & text);
    static std::string  FoldFirstCharacterTest  (const std::string & operand);
    static std::string  RewriteAddressCheck     (const std::string & operand);
    static std::string  RewriteByteSelector     (const std::string & operand);
    static std::string  ResolveIncludeName      (const std::string & operand);
};
