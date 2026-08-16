#pragma once

#include "DialectProfile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDirectiveTable
//
//  Merlin's spelling -> token mapping. A second dialect is a second TABLE, not a
//  second assembler, which is what `Directive.h` says the token enum exists for.
//
//  Every spelling here was taken from the committed vendor sources rather than
//  transcribed from the manual, with one deliberate exception noted below. The
//  corpus supplies its own frequencies -- DCI appears 130 times, ASC 35, ERR 17,
//  HEX 13 -- so the table reflects what Merlin source actually contains.
//
//  DDB is the exception: it appears nowhere on the disk. It is included anyway,
//  because absence from one vendor's source is not absence from the language,
//  and DDB is the one data directive whose byte order the existing Word token
//  cannot express.
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
//  What this profile does NOT yet do: recognize Merlin's directive spellings.
//  `directiveToken` stays `Directive::None` until the Merlin spelling table
//  lands, so a directive currently parses as an ordinary mnemonic. The profile
//  is registered but is not reachable from the command line until the `merlin`
//  subcommand exists, so this is an incomplete profile rather than a broken
//  advertised feature.
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

private:
    static bool         IsFieldSpace       (char ch);
    static bool         TakesDelimitedText (const std::string & mnemonic);
    static void         SkipFieldSpace     (const std::string & line, size_t & pos);
    static std::string  ReadPlainField     (const std::string & line, size_t & pos);
    static std::string  ReadOperandField   (const std::string & line, size_t & pos, const std::string & mnemonic);
};
