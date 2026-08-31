#include "Pch.h"

#include "MerlinDialect.h"

#include "MerlinSubsetBoundary.h"



//  Merlin's directive vocabulary. Frequencies in the comments are counts across
//  the nine committed vendor sources, so a reader can tell a workhorse from a
//  directive that appears twice.
static constexpr MerlinDirectiveTable::Spelling  s_kMerlinSpellings[] =
{
    //  The string family: ONE operation with six encodings, not six operations.
    //  They differ in high-bit handling, inversion, and terminator convention --
    //  parameters rather than different work. Which encoding applies is resolved
    //  from the spelling later; the token only says "string data".
    { "ASC",  Directive::StringData       },   //  35
    { "DCI",  Directive::StringData       },   // 130 -- the workhorse, and the corpus's sharpest probe
    { "INV",  Directive::StringData       },   //   1
    { "FLS",  Directive::StringData       },
    { "STR",  Directive::StringData       },
    { "REV",  Directive::StringData       },   //   3 -- found in the sources, absent from the spec's list

    //  Data.
    { "DFB",  Directive::Byte             },   //  14
    { "DB",   Directive::Byte             },
    { "DA",   Directive::Word             },   //   8
    { "DW",   Directive::Word             },
    { "DDB",  Directive::WordHighFirst    },   //   0 -- vocabulary, not idiom; see the header
    { "HEX",  Directive::HexData          },   //  13
    { "DS",   Directive::Ds               },   //   2

    //  Location and output.
    { "ORG",  Directive::Org              },   //  16
    { "DSK",  Directive::ObjectFile       },   //   2
    { "END",  Directive::End              },

    //  Inclusion. Two spellings, one operation -- the difference between them is
    //  which filesystem convention the name follows, not what the assembler does.
    { "PUT",  Directive::Include          },   //   1
    { "USE",  Directive::Include          },   //   2

    //  Conditional assembly. CLOCK.S turns one source into two objects with
    //  these, which is why it is the corpus's best single specimen.
    { "DO",   Directive::If               },   //   5
    { "ELSE", Directive::Else             },   //   1
    { "FIN",  Directive::Endif            },   //   5

    //  The first-character conditional, which is how a macro dispatches on
    //  addressing mode. It opens and closes the same block the expression form
    //  does and shares its token, because it is a second way of writing the
    //  CONDITION and not a second operation -- the assembler can already
    //  assemble a block when a value is non-zero, and a token exists only for
    //  work it cannot already do. What differs is settled where every other
    //  dialect spelling is: at parse time, in this profile.
    //
    //  Absent from the nine committed vendor sources and emphatically not
    //  absent from the language: the distribution disk's own macro library
    //  writes it a dozen times over, in MOVD, LDHI, ADD, SUB and PRINT, and
    //  every macro of consequence dispatches with it.
    { "IF",   Directive::If               },   //   0

    //  Macros. `<<<` is the TERMINATOR of a definition, not an invocation --
    //  every macro in the vendor library ends with it.
    { "MAC",  Directive::MacroDef         },   //  18
    { "<<<",  Directive::MacroEnd         },   //  18

    //  Structure.
    { "LUP",  Directive::Loop             },
    { "--^",  Directive::LoopEnd          },
    { "DUM",  Directive::DummySection     },
    { "DEND", Directive::DummySectionEnd  },

    //  Assembly-time assertion. LABELS.S depends on it.
    { "ERR",  Directive::ErrorIf          },   //  17

    //  Listing control, which as65 already has tokens for.
    //
    //  TR, EXP and AST steer what the LISTING looks like -- truncated lines,
    //  whether macro expansions are shown, a rule of asterisks -- and change no
    //  object byte. That is precisely what the option token means, so they reuse
    //  it rather than each bringing a token whose handler would do nothing. A
    //  token exists for an operation the assembler cannot already perform, and
    //  "recognized, affects no output" is an operation it can.
    { "PAG",  Directive::Page             },   //   1
    { "TR",   Directive::OptNoop          },   //   1
    { "EXP",  Directive::OptNoop          },   //   4
    { "AST",  Directive::OptNoop          },   //   4

    //  The CPU selector. Merlin takes its target from here and nowhere else.
    { "XC",   Directive::CpuSelect        },

    //  Binds the symbol in the label field to an answer supplied to the
    //  assembly. Merlin asks the operator at the keyboard; a batch assembler
    //  takes the answer from its predefined symbols and refuses to guess. Seven
    //  lines across the vendor sources, and three of the five oracle programs
    //  are unreachable without it.
    { "KBD",  Directive::KeyboardInput    },   //   7

    //  Binds the positional parameters without a macro call, so a fragment
    //  pulled in with an inclusion directive can be parameterized the way a
    //  macro body is. `PI.ADD.S` writes `VAR MSGPNT;OUTPUT` immediately before
    //  `PUT SENDMSG`, and the eight parameter references in the included
    //  fragment resolve to it. The operand separator is the macro argument
    //  separator, which is why the field scanner already keeps a semicolon
    //  inside the operand out of the comment field.
    { "VAR",  Directive::ParameterBinding },   //   1

    //  Outside the supported subset. Present so they are refused BY NAME.
    { "REL",  Directive::Relocatable      },   //   2
    { "ENT",  Directive::EntrySymbol      },   //   7
    { "EXT",  Directive::ExternalSymbol   },   //   3
    { "TYP",  Directive::FileType         },
    { "SAV",  Directive::SaveObject       },   //   2
};



//  Merlin's own names for the two carry branches. They are MNEMONICS, not
//  directives: the machine has one opcode each and Merlin simply spells it
//  differently, so they resolve to BCC and BCS at parse time and nothing
//  downstream ever sees the alternate name.
//
//  Not optional flavor. Three of the five oracle sources -- PRINTFILER, MAKE
//  DUMP and CLOCK -- use them, so four of the six shipped objects are
//  unreachable without this table. as65 must NOT gain these spellings: admitting
//  one dialect's constructs into another is exactly what the strictness rule
//  forbids.
static constexpr MnemonicAlias  s_kMerlinMnemonicAliases[] =
{
    { "BLT", "BCC" },   //  branch if less than -- unsigned, so carry clear
    { "BGE", "BCS" },   //  branch if greater or equal -- carry set
};



//  Merlin's own spellings for two bitwise operations. The operations are the
//  evaluator's, unchanged; only the characters differ, which is why this is a
//  table consulted by the tokenizer rather than a second set of folds.
//
//  Settled from CLOCK.S and its two shipped objects, not from the manual. The
//  file computes `LDX #HOURS/24!1` to place the time editor's cursor: for the
//  24-hour build that is 1 against 1, which must give 0 -- the tens-of-hours
//  digit -- and inclusive-or gives 1. The rollover comparison
//  `CMP #HOURS/24+3."0"` runs the other way: it must read the character "4" for
//  the 24-hour build and "3" for the 12-hour one, which only inclusive-or with
//  the high-ASCII zero produces.
//
//  A CAVEAT that belongs at the code rather than in a note nobody rereads. The
//  inclusive-or character is also what joins a local label to its scope, and the
//  expression tokenizer reads an identifier greedily -- so `LABEL.OTHER` lexes
//  as one symbol where Merlin would read an operation. Every use on the vendor
//  disk follows a digit, where no identifier is being scanned, so the corpus
//  cannot force the other reading; a source that needs it would.
static constexpr OperatorSpelling  s_kMerlinOperatorSpellings[] =
{
    { '!', ExprOperator::BitXor },
    { '.', ExprOperator::BitOr  },
};



//  Introduces a comment when it BEGINS a field. Inside the operand it is data --
//  Merlin's macro-argument separator.
static const char  s_kCommentIntroducer = ';';

//  Introduces a whole-line comment in column 1.
static const char  s_kLineCommentIntroducer = '*';

//  The two spellings that make a line an equate. `=` is the only one on the
//  disk -- 128 uses, and no EQU anywhere -- but EQU is in the language, and a
//  dialect is judged by what it accepts rather than by what one vendor wrote.
static const char *  s_kpszEquateSign    = "=";
static const char *  s_kpszEquateKeyword = "EQU";

//  Merlin writes a variable symbol and a positional macro parameter with the
//  same character: `]COUNT` is a reassignable symbol, `]1` a parameter. The
//  digit is what tells them apart, and nothing else has to.
static const char  s_kVariableSigil = ']';

//  Separates one macro argument from the next. NOT a comment introducer inside
//  the operand field, which is the whole reason the field scanner exists.
static const char  s_kMacroArgumentSeparator = ';';

//  The two spellings that invoke a macro explicitly. Both put the macro's name
//  in the OPERAND field and its arguments in the field after it, so the name is
//  separated from the arguments by a space and only the arguments are separated
//  from each other by the macro separator.
//
//  MEASURED, against Merlin Pro 2.23, one form per assembly because the first
//  diagnostic ends the run. `>>> NOPS` is accepted and `PMC NOPS` behaves
//  identically; `>>>NOPS` written flush against the name is refused, as is
//  `PMC ADDA;$30` with the name joined to its argument by the separator. All
//  three of those facts contradict what this file assumed while the answer was
//  unverifiable, and the assumption was wrong in every direction it could be:
//  it accepted the flush form, it took the name up to the separator, and it
//  left the word form out on the grounds that it doubled an unverified surface.
static constexpr const char *  s_kExplicitCallSpellings[] = { ">>>", "PMC" };

//  Merlin's first-character conditional, which compares the leading character
//  of a macro argument against a literal.
static const char *  s_kpszFirstCharConditional = "IF";

//  What an inclusion directive's operand becomes on the way to a filename.
//  Merlin's own sources name a shorter file than the one on the disk -- `USE
//  PI.MACS` reaches `T.PI.MACS` -- so the prefix is part of resolving the name
//  rather than part of what the author typed.
static const char *  s_kpszIncludeNamePrefix = "T.";

//  What a variable symbol binds under, once the sigil is gone.
//
//  Two characters of it are load-bearing. The name has to start with a letter
//  so the expression tokenizer will lex it, and it has to hold TWO periods so
//  no name a source can spell may collide with it: an ordinary label may hold
//  none -- ValidateLabel rejects the character outright -- and a scoped local
//  binds as one label joined to another, so it holds exactly one.
static const char *  s_kpszVariableNamePrefix = "var..";





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::IsFieldSpace
//
//  Tabs separate fields exactly as spaces do, and are never expanded: tab stops
//  change where text appears on a screen and nothing about what it means.
//
////////////////////////////////////////////////////////////////////////////////

bool MerlinDialect::IsFieldSpace (char ch)
{
    return (ch == ' ') || (ch == '\t');
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDirectiveTable::FromSpelling
//
////////////////////////////////////////////////////////////////////////////////

Directive MerlinDirectiveTable::FromSpelling (const std::string & word)
{
    Directive  token = Directive::None;



    for (const Spelling & entry : s_kMerlinSpellings)
    {
        if (word == entry.name)
        {
            token = entry.token;
            break;
        }
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDirectiveTable::GetCanonicalName
//
//  The first spelling mapping to the token, which for the string family means
//  ASC rather than the far more common DCI. That is deliberate: the canonical
//  name answers "what operation is this", and every encoding of it is a string.
//
////////////////////////////////////////////////////////////////////////////////

const char * MerlinDirectiveTable::GetCanonicalName (Directive directive)
{
    const char *  name = "";



    for (const Spelling & entry : s_kMerlinSpellings)
    {
        if (entry.token == directive)
        {
            name = entry.name;
            break;
        }
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDirectiveTable::GetAllSpellings
//
////////////////////////////////////////////////////////////////////////////////

std::span<const MerlinDirectiveTable::Spelling> MerlinDirectiveTable::GetAllSpellings()
{
    return std::span<const Spelling> (s_kMerlinSpellings);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDirectiveTable::GetEncodingModeForSpelling
//
//  Which of the six encodings a string spelling selects.
//
//  A table rather than a chain of comparisons, and separate from the spelling
//  table above because the two answer different questions: that one says "this
//  is string data", this one says "encoded how". Collapsing them would put an
//  encoding column on every row of a table that is mostly not strings.
//
////////////////////////////////////////////////////////////////////////////////

StringEncodingMode MerlinDirectiveTable::GetEncodingModeForSpelling (const std::string & spelling)
{
    struct ModeRow
    {
        const char *        name;
        StringEncodingMode  mode;
    };

    static constexpr ModeRow  s_kModes[] =
    {
        { "ASC", StringEncodingMode::Plain          },
        { "DCI", StringEncodingMode::DciTerminated  },
        { "INV", StringEncodingMode::Inverse        },
        { "FLS", StringEncodingMode::Flashing       },
        { "STR", StringEncodingMode::LengthPrefixed },
        { "REV", StringEncodingMode::Reversed       },
    };

    StringEncodingMode  mode = StringEncodingMode::Plain;



    for (const ModeRow & row : s_kModes)
    {
        if (spelling == row.name)
        {
            mode = row.mode;
            break;
        }
    }

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::GetMnemonicAliases
//
////////////////////////////////////////////////////////////////////////////////

std::span<const MnemonicAlias> MerlinDialect::GetMnemonicAliases() const
{
    return std::span<const MnemonicAlias> (s_kMerlinMnemonicAliases);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::GetOperatorSpellings
//
////////////////////////////////////////////////////////////////////////////////

std::span<const OperatorSpelling> MerlinDialect::GetOperatorSpellings() const
{
    return std::span<const OperatorSpelling> (s_kMerlinOperatorSpellings);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::GetSubsetBoundary
//
//  Handed straight through from the boundary table, which is the whole point:
//  the profile names WHAT it refuses and nothing else. What each refusal says,
//  and when a construct accepted once becomes one refused twice, are the
//  mechanism's to decide from the rows.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const SubsetBoundaryRow> MerlinDialect::GetSubsetBoundary() const
{
    return MerlinSubsetBoundary::GetAll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::GetMacroSyntax
//
//  Merlin's macros in one value.
//
//  No end keyword, no local-label keyword and no explicit-call keyword: the
//  terminator is a real token in the spelling table, there is nothing to
//  declare because every label a body defines is made unique per expansion
//  whether the author asked or not, and an explicit invocation is resolved into
//  an ordinary one while the line is parsed.
//  That last one is the corpus's finding rather than the manual's. `MAKE
//  DUMP.S` expands `INCD` twice and `STORE` three times; each expansion
//  redefines a bare label, and the vendor shipped a working object. A dialect
//  that reused those labels would resolve every branch to the first expansion.
//
////////////////////////////////////////////////////////////////////////////////

MacroSyntax MerlinDialect::GetMacroSyntax() const
{
    MacroSyntax  syntax = {};



    syntax.parameterSigil        = s_kVariableSigil;
    syntax.argumentSeparator     = s_kMacroArgumentSeparator;
    syntax.labelsArePerExpansion = true;

    return syntax;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::IsVariableNameStart
//
//  What may follow the sigil in a NAMED variable, as opposed to in a positional
//  parameter. The digit is the whole distinction, and it is what decides which
//  of the two a label field spells: `]LOOP` is a program-counter symbol the
//  source may redefine, and `]1` is never a label at all.
//
////////////////////////////////////////////////////////////////////////////////

bool MerlinDialect::IsVariableNameStart (char ch)
{
    return isalpha ((unsigned char) ch) || (ch == '_');
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::IsParameterIndex
//
//  What may follow the sigil in a POSITIONAL parameter. One digit is the whole
//  form -- `]12` is parameter 1 beside a literal 2 -- and zero is excluded,
//  because the argument-count form asks a question rather than naming a slot.
//
////////////////////////////////////////////////////////////////////////////////

bool MerlinDialect::IsParameterIndex (char ch)
{
    return (ch >= '1') && (ch <= '9');
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::GetPositionalParameterSymbol
//
//  The symbol one positional parameter binds under, so the directive that
//  assigns them outside a macro call defines exactly the names a reference in
//  the source resolves to.
//
//  It goes through the same qualification a named variable does, which is what
//  keeps the two answers from drifting: the binding and the reference are one
//  rule spelled once, rather than a prefix written out twice.
//
//  A PARAMETER BOUND THIS WAY HOLDS A VALUE, not the text of its expression.
//  That is what the directive assigns -- Merlin documents it as setting the
//  variables to expressions -- and it is what lets the shared symbol table hold
//  the result. The divergence it implies is worth stating: a reference pasted
//  into a longer identifier, which textual substitution inside a macro body
//  would splice, lexes here as one symbol instead. No vendor line does it, so
//  the corpus cannot settle which reading Merlin takes outside a macro.
//
////////////////////////////////////////////////////////////////////////////////

std::string MerlinDialect::GetPositionalParameterSymbol (int index) const
{
    return QualifyVariableName (std::to_string (index));
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::QualifyVariableName
//
//  The stored name for one variable symbol, sigil already removed or not.
//
//  Variables are a namespace of their own -- `]COUNT` and `COUNT` are two
//  symbols and either may exist without the other -- so the stored name cannot
//  simply drop the sigil. It cannot keep it either: the shared expression
//  tokenizer lexes identifiers and the sigil is not one of the characters it
//  accepts. The prefix is the answer to both at once.
//
////////////////////////////////////////////////////////////////////////////////

std::string MerlinDialect::QualifyVariableName (const std::string & spelling)
{
    bool  hasSigil = !spelling.empty() && (spelling[0] == s_kVariableSigil);



    return s_kpszVariableNamePrefix + (hasSigil ? spelling.substr (1) : spelling);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::QualifyVariableRefs
//
//  Every variable REFERENCE in one field, rewritten to the name the symbol
//  binds under.
//
//  The reference half is not optional. A dialect answering only "is this a
//  variable definition" leaves every use of one unresolvable, which is the same
//  trap the local-label prefix had.
//
//  A POSITIONAL PARAMETER IS REWRITTEN TOO, and it has to be. Inside a macro
//  body the expander replaces it textually long before the line is parsed, so
//  one arriving here is a reference outside any expansion -- which is exactly
//  the form the parameter-binding directive exists to serve. Leaving it alone
//  hands the sigil to an expression tokenizer that cannot lex it, so the
//  fragment the directive parameterizes fails at every reference in it.
//
////////////////////////////////////////////////////////////////////////////////

std::string MerlinDialect::QualifyVariableRefs (const std::string & text)
{
    std::string  result;
    size_t       i      = 0;



    while (i < text.size())
    {
        bool  startsName = (text[i] == s_kVariableSigil) &&
                           (i + 1 < text.size()) &&
                           (IsVariableNameStart (text[i + 1]) || IsParameterIndex (text[i + 1]));

        if (startsName)
        {
            result += s_kpszVariableNamePrefix;
        }
        else
        {
            result += text[i];
        }

        i++;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::IsExplicitCallSpelling
//
//  Whether this opcode field is one of the two explicit macro-invocation
//  prefixes. Matched WHOLE, which is the flush form's answer: `>>>NOPS` is one
//  word, not a prefix wearing a name, and the real assembler refuses it.
//
////////////////////////////////////////////////////////////////////////////////

bool MerlinDialect::IsExplicitCallSpelling (const std::string & opcode)
{
    bool  isCall = false;



    for (const char * spelling : s_kExplicitCallSpellings)
    {
        if (opcode == spelling)
        {
            isCall = true;
            break;
        }
    }

    return isCall;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::FoldFirstCharacterTest
//
//  The first-character conditional's operand, folded to the value the ordinary
//  conditional evaluates.
//
//  The rule is POSITIONAL and the manual states it exactly: the first and third
//  characters of the operand are compared, and the character between them is
//  never examined -- `IF (=]1` and `IF (,]1` are the same test, and the vendor
//  library writes both. A parameter reference is one character wide in that
//  count because the argument has already been substituted for it by the time
//  the line is read, so `IF (=]1` invoked with `(ADR),Y` arrives here as
//  `(=(ADR),Y` and the third character is the argument's first.
//
//  Folded here rather than given a token of its own, for the same reason the
//  address-check assertion is rewritten into an ordinary comparison: the
//  assembler can already assemble a block when a value is non-zero, and both
//  characters are known the moment the line is read. Nothing downstream learns
//  that the condition was written this way.
//
//  UNVERIFIED, and the corpus structurally cannot settle it: the spelling that
//  puts the PARAMETER first and the literal after it -- the vendor's PRINT
//  macro writes one -- cannot be told apart here from an argument that happens
//  to begin with the same character, because substitution is textual and has
//  already erased which position held the reference. Every other use on the
//  distribution disk writes the literal first.
//
////////////////////////////////////////////////////////////////////////////////

std::string MerlinDialect::FoldFirstCharacterTest (const std::string & operand)
{
    constexpr size_t  kComparedCharacter = 2;
    bool              matched            = (operand.size() > kComparedCharacter) &&
                                           (operand[0] == operand[kComparedCharacter]);



    return matched ? "1" : "0";
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::RewriteByteSelector
//
//  In Merlin the byte selector written straight after the immediate sigil picks
//  a byte out of the WHOLE expression, where the shared evaluator's `<` and `>`
//  are prefix operators binding to the term beside them. Parenthesizing the
//  remainder makes the two agree.
//
//  Settled by the object rather than by the manual. `MAKE DUMP`'s loader stores
//  the address of a section it is about to move with `LDA #>HEREMAIN-1`, and
//  the shipped bytes hold $90 where the prefix reading gives $8F -- the high
//  byte of a value one less, taken before the subtraction instead of after.
//  Both readings agree on the low byte, so half of every such pair matches
//  either way, which is what makes this the kind of defect that survives a
//  casual comparison.
//
//  Confined to the immediate form, because that is where Merlin documents the
//  selector and where the corpus exercises it. A `>` elsewhere in an expression
//  is left to bind as it always did.
//
////////////////////////////////////////////////////////////////////////////////

std::string MerlinDialect::RewriteByteSelector (const std::string & operand)
{
    constexpr char  kImmediateSigil = '#';
    constexpr char  kLowByte        = '<';
    constexpr char  kHighByte       = '>';
    std::string     rewritten       = operand;
    bool            isSelected      = (operand.size() > 2) &&
                                      (operand[0] == kImmediateSigil) &&
                                      ((operand[1] == kLowByte) || (operand[1] == kHighByte));



    if (isSelected)
    {
        rewritten = operand.substr (0, 2) + "(" + operand.substr (2) + ")";
    }

    return rewritten;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::RewriteAddressCheck
//
//  Merlin's second form of the assembly-time assertion. `ERR expr` fails when
//  the expression is non-zero; `ERR \expr` fails when the assembly has grown
//  past `expr` -- "does this still fit below here". `MAKE DUMP.S` bounds all
//  three of its sections that way.
//
//  Rewritten into the ordinary form rather than given a token or a flag,
//  because the assembler can already compute it: the program counter is `*`
//  and the comparison is `>`. That is the same treatment BLT and BGE get -- a
//  dialect spelling resolved once at parse time, with nothing downstream
//  learning that the source said something else.
//
//  The expression is PARENTHESIZED, which is not decoration. Merlin folds
//  strictly left to right, so `ERR \$3D0+1` would otherwise become
//  `(* > $3D0) + 1` -- a comparison whose result is then added to, which is
//  neither what was written nor an error. No vendor line has a compound
//  ceiling, so the corpus cannot report this and a synthetic test carries it.
//
//  UNVERIFIED: whether the boundary is exclusive. Merlin documents the check as
//  firing when the address EXCEEDS the ceiling, and none of the three vendor
//  uses lands on its own limit -- the closest is $034E against $03D0 -- so the
//  corpus cannot tell `>` from `>=`.
//
////////////////////////////////////////////////////////////////////////////////

std::string MerlinDialect::RewriteAddressCheck (const std::string & operand)
{
    constexpr char  kAddressCheckPrefix = '\\';
    std::string     rewritten           = operand;



    if (!operand.empty() && (operand[0] == kAddressCheckPrefix))
    {
        rewritten = "*>(" + operand.substr (1) + ")";
    }

    return rewritten;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::ResolveIncludeName
//
//  The filename an inclusion directive's operand names.
//
//  IT IS NOT THE OPERAND. Merlin prepends a fixed prefix when it turns the
//  operand into a filename, and the vendor disk proves it twice: `PI.START.S`
//  says `USE PI.MACS` and `PI.ADD.S` says `PUT SENDMSG`, while the files on the
//  disk are `T.PI.MACS` and `T.SENDMSG`. Both are committed under their on-disk
//  names for exactly this reason, so the rule is measured rather than assumed.
//
//  Applied UNCONDITIONALLY, including to an operand that already begins with the
//  prefix. That is the rule as the sources state it, and softening it -- "prefix
//  unless it looks prefixed" -- would be inventing a second rule the corpus says
//  nothing about, on the strength of a case that appears in it nowhere.
//
//  Done here rather than in the assembler because it is a fact about how ONE
//  dialect spells a filename. The engine resolves whatever name it is handed,
//  the same way it does for every other dialect, and never learns that this one
//  writes them short.
//
////////////////////////////////////////////////////////////////////////////////

std::string MerlinDialect::ResolveIncludeName (const std::string & operand)
{
    size_t  start = operand.find_first_not_of (" \t");
    size_t  end   = operand.find_last_not_of (" \t");



    if (start == std::string::npos)
    {
        return operand;
    }

    return s_kpszIncludeNamePrefix + operand.substr (start, end - start + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::TakesDelimitedText
//
//  Whether this mnemonic's operand is delimiter-quoted rather than
//  whitespace-delimited.
//
//  Asks the real directive table rather than carrying its own list of string
//  spellings. It did carry one while the table did not exist, which meant the
//  operand scanner -- whose correctness the `!` delimiter rule turns on -- was
//  resting on a private copy that could disagree with the vocabulary. Adding
//  REV to the table now reaches the scanner by construction instead of needing
//  a second edit that could be forgotten.
//
//  The prompt on a keyboard-input line is delimited for the same reason a
//  string is: it is a sentence, and every one of them on the vendor disk
//  contains spaces. A whitespace-delimited read would keep the first word and
//  hand the rest to the comment field.
//
////////////////////////////////////////////////////////////////////////////////

bool MerlinDialect::TakesDelimitedText (const std::string & mnemonic)
{
    Directive  token = MerlinDirectiveTable::FromSpelling (Parser::ToUpper (mnemonic));



    return (token == Directive::StringData) || (token == Directive::KeyboardInput);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::TakesFileName
//
//  Whether this mnemonic's operand is a FILENAME, which runs to the comment
//  rather than to the next space.
//
//  DOS 3.3 names may contain spaces and the distribution disk's own macro
//  library is called `MACRO LIBRARY`, so ` USE MACRO LIBRARY` asks for one file
//  and not for `MACRO` with a comment after it. Measured against Merlin Pro
//  2.23 rather than inferred: that line assembles and defines every macro in
//  the library, and the same line with ` ;NOTE` appended assembles identically
//  -- so the space does not end the name and the comment introducer does.
//
//  The vendor sources could never have settled this. Both of their inclusions
//  name a file with no space in it and neither carries a trailing comment, so
//  a scanner that stops at the first space reproduces every byte on the disk.
//
////////////////////////////////////////////////////////////////////////////////

bool MerlinDialect::TakesFileName (const std::string & mnemonic)
{
    return MerlinDirectiveTable::FromSpelling (Parser::ToUpper (mnemonic)) == Directive::Include;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::SkipFieldSpace
//
////////////////////////////////////////////////////////////////////////////////

void MerlinDialect::SkipFieldSpace (const std::string & line, size_t & pos)
{
    while ((pos < line.size()) && IsFieldSpace (line[pos]))
    {
        pos++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::ReadPlainField
//
//  One whitespace-delimited field.
//
////////////////////////////////////////////////////////////////////////////////

std::string MerlinDialect::ReadPlainField (const std::string & line, size_t & pos)
{
    size_t  start = pos;



    while ((pos < line.size()) && !IsFieldSpace (line[pos]))
    {
        pos++;
    }

    return line.substr (start, pos - start);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::IsCharConstantDelimiter
//
//  The two characters that open a character constant inside an ordinary
//  operand. The apostrophe gives the plain character and the quotation mark the
//  same character with bit 7 set, which is the convention Merlin's string
//  directives take from their own delimiter.
//
////////////////////////////////////////////////////////////////////////////////

bool MerlinDialect::IsCharConstantDelimiter (char ch)
{
    return (ch == '\'') || (ch == '"');
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::SkipCharConstant
//
//  Steps over one character constant, leaving `pos` on whatever follows it.
//
//  The form is a delimiter, ONE character, and a closing delimiter. Consuming
//  exactly one character rather than running to the next matching delimiter is
//  what keeps an unclosed constant from swallowing the comment field.
//
//  THAT LAST PART IS UNCOVERED and cannot be covered today, which is worth
//  saying here rather than leaving to be rediscovered. Merlin also accepts the
//  closing delimiter being absent -- `LDA #'A` -- but the shared expression
//  tokenizer requires it, so a line exercising the difference fails one step
//  later whichever way this function scans. No vendor line writes the open form.
//  The rule stays because it is what keeps a comment out of the operand by
//  construction rather than by every author remembering to close a quote.
//
////////////////////////////////////////////////////////////////////////////////

void MerlinDialect::SkipCharConstant (const std::string & line, size_t & pos)
{
    char  delimiter = line[pos];



    pos++;

    if (pos < line.size())
    {
        pos++;
    }

    if ((pos < line.size()) && (line[pos] == delimiter))
    {
        pos++;
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::ReadOperandField
//
//  The operand, which is whitespace-delimited EXCEPT where a string directive
//  opens delimited text.
//
//  The delimiter is whatever character opens the text -- any character, not a
//  fixed quote set. `ASC !" ASC ""!` on the vendor disk chooses `!` precisely
//  because the text contains quotes, and a scanner hard-coded to `"` would end
//  the operand in the middle of the data.
//
//  Scanning continues past the closing delimiter to the next whitespace, because
//  Merlin allows a trailing byte after the text (`ASC "ABC",8D`), and that byte
//  is part of the operand rather than the beginning of a comment.
//
//  An unterminated delimiter runs to end of line rather than erroring here.
//  Parsing is purely syntactic; deciding that a string was never closed is a
//  diagnostic belonging to the pass that knows what the directive means.
//
//  An ORDINARY operand is whitespace-delimited, with one exception that is not
//  optional: a character constant may hold a space. `LDA #" "` blanks the
//  leading zero of the hour in CLOCK.S, and a scanner breaking on the first
//  space keeps `#"` and hands ` " ;Blank leading "0"` to the comment field --
//  an expression error on a line the vendor shipped an object for.
//
////////////////////////////////////////////////////////////////////////////////

std::string MerlinDialect::ReadOperandField (const std::string & line, size_t & pos, const std::string & mnemonic)
{
    size_t  start     = pos;
    char    delimiter = 0;



    // A filename runs to the comment, because a space inside one is part of the
    // name. Trailing field space is left for the caller to trim, so the operand
    // is the name and nothing else.
    if (TakesFileName (mnemonic))
    {
        while ((pos < line.size()) && (line[pos] != s_kCommentIntroducer))
        {
            pos++;
        }

        return line.substr (start, pos - start);
    }

    if (!TakesDelimitedText (mnemonic))
    {
        while ((pos < line.size()) && !IsFieldSpace (line[pos]))
        {
            if (IsCharConstantDelimiter (line[pos]))
            {
                SkipCharConstant (line, pos);
            }
            else
            {
                pos++;
            }
        }

        return line.substr (start, pos - start);
    }

    delimiter = line[pos];
    pos++;

    while ((pos < line.size()) && (line[pos] != delimiter))
    {
        pos++;
    }

    // Step over the closing delimiter when there was one.
    if (pos < line.size())
    {
        pos++;
    }

    while ((pos < line.size()) && !IsFieldSpace (line[pos]))
    {
        pos++;
    }

    return line.substr (start, pos - start);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::ParseLine
//
//  One Merlin source line into its fields.
//
//  Field ORDER carries the meaning, and the only significant column is the
//  first. A line starting in column 0 opens with a label; a line starting with
//  whitespace has none and begins at the opcode.
//
//  Every field's starting column is recorded as it is read, which is the only
//  moment it is knowable -- the fields become strings and the line's own
//  geometry is gone. The columns are 1-based, so a diagnostic can hand one to an
//  editor, and they are recorded even where the field's text is later cleared:
//  an equate keeps the columns its name and sign were written at.
//
////////////////////////////////////////////////////////////////////////////////

ParsedLine MerlinDialect::ParseLine (const std::string & line, int lineNumber) const
{
    ParsedLine   result   = {};
    std::string  opcode;
    size_t       pos      = 0;
    bool         isEquate = false;



    result.lineNumber      = lineNumber;
    result.isEmpty         = true;
    result.startsAtColumn0 = (!line.empty() && !IsFieldSpace (line[0]));

    if (line.empty())
    {
        return result;
    }

    // A whole-line comment. The semicolon case is NOT a separate rule: with no
    // label present, column 1 is the first field boundary, so a semicolon there
    // is a semicolon beginning a field.
    if ((line[0] == s_kLineCommentIntroducer) || (line[0] == s_kCommentIntroducer))
    {
        return result;
    }

    if (result.startsAtColumn0)
    {
        result.labelColumn = (int) pos + 1;
        result.label       = ReadPlainField (line, pos);
    }

    SkipFieldSpace (line, pos);

    if ((pos < line.size()) && (line[pos] != s_kCommentIntroducer))
    {
        result.mnemonicColumn = (int) pos + 1;
        result.mnemonic       = ReadPlainField (line, pos);

        SkipFieldSpace (line, pos);

        if ((pos < line.size()) && (line[pos] != s_kCommentIntroducer))
        {
            result.operandColumn = (int) pos + 1;
            result.operand       = ReadOperandField (line, pos, result.mnemonic);
        }
    }

    opcode = Parser::ToUpper (result.mnemonic);

    // An explicit macro invocation names the macro in the OPERAND field and
    // carries its arguments in the field after it. Resolved into the ordinary
    // invocation the two fields already describe -- the name becomes the opcode
    // and the arguments become the operand -- which is the same treatment the
    // alternate branch mnemonics get, and it is what makes the three refusals
    // fall out rather than needing to be written: the flush spelling is one
    // word and matches no prefix, a name joined to its argument by the macro
    // separator is one word that names no macro, and the word form differs from
    // the punctuation form in nothing but its spelling.
    //
    // A prefix with an empty operand field is left exactly as it was read, so
    // it fails as the unknown operation it is instead of vanishing into a line
    // with no opcode at all.
    if (IsExplicitCallSpelling (opcode) && !result.operand.empty())
    {
        result.mnemonic       = result.operand;
        result.mnemonicColumn = result.operandColumn;
        result.operand.clear();
        result.operandColumn  = 0;

        SkipFieldSpace (line, pos);

        if ((pos < line.size()) && (line[pos] != s_kCommentIntroducer))
        {
            result.operandColumn = (int) pos + 1;
            result.operand       = ReadOperandField (line, pos, result.mnemonic);
        }

        opcode = Parser::ToUpper (result.mnemonic);
    }

    // An equate puts its sign in the OPCODE field, with the name beside it in
    // the label field. It is a field-model fact rather than an expression one:
    // nothing in `LOADADR = $9000` is an operand containing an operator, so a
    // parser looking for `=` inside the text would find it in the wrong place
    // and leave the line looking like an instruction called `=`.
    isEquate = !result.label.empty() &&
               ((opcode == s_kpszEquateSign) || (opcode == s_kpszEquateKeyword));

    if (isEquate)
    {
        // A variable is REASSIGNABLE and an ordinary equate is not, which is the
        // entire difference between the two and the only thing the sigil says.
        // Recording it as the mutable kind is what lets a later definition
        // rebind it instead of being reported as a duplicate.
        bool  isVariable = (result.label[0] == s_kVariableSigil);

        result.isConstant   = true;
        result.constantName = isVariable ? QualifyVariableName (result.label) : result.label;
        result.constantExpr = QualifyVariableRefs (result.operand);
        result.constantKind = isVariable ? SymbolKind::Set : SymbolKind::Equ;
        result.isEmpty      = false;

        // The name is the CONSTANT's, not a label binding to the current
        // address. Leaving it in the label field would bind it twice -- once
        // here at the program counter and once as the constant -- and the second
        // definition would be reported as a duplicate of the first.
        result.label.clear();
        result.mnemonic.clear();
        result.operand.clear();

        return result;
    }

    // A variable symbol may also stand where an ordinary label would, taking the
    // program counter as its value. The whole point is that it may do so REPEATEDLY:
    // CLOCK.S names eight separate loop targets `]LOOP`, and each branch means
    // the definition immediately above it. The name is qualified exactly as an
    // assigned variable is, so the two spellings of one symbol cannot diverge.
    if ((result.label.size() > 1) &&
        (result.label[0] == s_kVariableSigil) &&
        IsVariableNameStart (result.label[1]))
    {
        result.label     = QualifyVariableName (result.label);
        result.labelKind = SymbolKind::Set;
    }

    // Resolve the opcode field against Merlin's vocabulary. A word that is not a
    // directive stays a mnemonic -- it is an instruction or a macro invocation,
    // and telling those two apart needs the macro table rather than the parser.
    //
    // THE DIRECTIVE TABLE IS ASKED FIRST, and that must not be what decides the
    // answer. It cannot be, because the table holds no spelling the instruction
    // tables answer to: with the two disjoint, either order gives the same
    // result. That is measured rather than assumed -- every mnemonic in both
    // instruction sets is asked of every dialect -- so a spelling added here
    // that would shadow an instruction fails before it can ship.
    //
    // A spelling that genuinely is BOTH does not go in this table at all. It
    // goes where as65's `RMB` does, in the ambiguous table, and is resolved from
    // the operand: `DirectiveTable::FromAmbiguousSpelling`. Merlin has no such
    // spelling today, which is why it has no ambiguous table of its own -- an
    // always-empty lookup would be an answer nothing could ever exercise.
    result.directiveToken = MerlinDirectiveTable::FromSpelling (opcode);
    result.isDirective    = (result.directiveToken != Directive::None);

    // The first-character conditional becomes the value the ordinary
    // conditional tests. Folded BEFORE the variable rewriting below, because a
    // reference that reached this line unsubstituted is not an argument's first
    // character and must not be turned into a symbol that would be evaluated as
    // one.
    if (opcode == s_kpszFirstCharConditional)
    {
        result.operand = FoldFirstCharacterTest (result.operand);
    }

    // Variable references become the names their symbols bind under, before
    // anything downstream tries to resolve them. Literal text is left alone:
    // the sigil is an ordinary character inside a message, and rewriting there
    // would change emitted bytes -- or garble a prompt -- rather than resolve a
    // symbol.
    if (!TakesDelimitedText (result.mnemonic))
    {
        result.operand = QualifyVariableRefs (result.operand);
        result.operand = RewriteByteSelector (result.operand);
    }

    if (result.isDirective)
    {
        result.directive    = MerlinDirectiveTable::GetCanonicalName (result.directiveToken);
        result.directiveArg = result.operand;
    }

    // The assertion's address-check spelling becomes the expression it stands
    // for, so the directive itself has one form downstream.
    if (result.directiveToken == Directive::ErrorIf)
    {
        result.directiveArg = RewriteAddressCheck (result.directiveArg);
    }

    // An inclusion operand becomes the filename it stands for, so the assembler
    // resolves an ordinary name and never learns this dialect writes them short.
    if (result.directiveToken == Directive::Include)
    {
        result.directiveArg = ResolveIncludeName (result.directiveArg);
    }

    // Which of the six encodings the spelling selected. Resolved here because
    // the spellings are Merlin's; what each mode DOES with the text is
    // dialect-independent and belongs to StringEncoding.
    if (result.directiveToken == Directive::StringData)
    {
        result.stringMode = MerlinDirectiveTable::GetEncodingModeForSpelling (Parser::ToUpper (result.mnemonic));
    }

    // Everything from here is the comment field, and is discarded: nothing
    // downstream needs it, and a line that is only a comment must look empty.
    result.isEmpty = result.label.empty() && result.mnemonic.empty();

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::GetDirectiveForSpelling
//
//  Whether Merlin claims a word, for the benefit of a diagnostic raised by some
//  OTHER dialect.
//
//  The same table ParseLine resolves against, so the two cannot disagree about
//  what Merlin's vocabulary is. That includes the refused constructs: `REL`
//  belongs to Merlin whether or not Casso will assemble it, and a source
//  reaching as65 with one in it is in the wrong dialect rather than misspelled.
//
////////////////////////////////////////////////////////////////////////////////

Directive MerlinDialect::GetDirectiveForSpelling (const std::string & upperSpelling) const
{
    return MerlinDirectiveTable::FromSpelling (upperSpelling);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::GetSpellingForDirective
//
//  The same table again, read the other way. Merlin has no dotted forms, so a
//  synthesized line reading `.ENDIF` is not a stricter spelling of `FIN` -- it
//  is an unknown operation, on a line the source never wrote and cannot see.
//
////////////////////////////////////////////////////////////////////////////////

const char * MerlinDialect::GetSpellingForDirective (Directive token) const
{
    return MerlinDirectiveTable::GetCanonicalName (token);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::ExplainUnknownOperation
//
//  The indented label, which is the first thing a developer coming from a
//  colon-terminated assembler gets wrong.
//
//  Merlin's line model puts the label in the FIRST field and nowhere else, so an
//  indented one is read as the opcode. The line then fails as an unknown
//  operation, which is true and useless: the word is not an operation because it
//  was never meant to be one, and the instruction the developer wrote has
//  quietly become its operand.
//
//  Two conditions, and both are necessary. The line must have begun with
//  whitespace -- a word in column 1 IS the label, so there is nothing to
//  explain. And the next field must name something the assembler could execute,
//  which is what separates a misplaced label from an ordinary misspelling: a
//  line whose second field is an expression or nothing at all is a bad opcode
//  and gets the plain answer.
//
//  The engine supplies that second fact rather than this profile digging for it.
//  The instruction tables are shared and unnamed, and a dialect reaching into
//  them to compose a sentence is the seam leaking in the direction the profile
//  contract spends most of its words on.
//
////////////////////////////////////////////////////////////////////////////////

std::string MerlinDialect::ExplainUnknownOperation (const ParsedLine & parsed,
                                                    bool               operandNamesAnOperation) const
{
    std::string  explanation;
    bool         isIndentedLabel = !parsed.startsAtColumn0 && operandNamesAnOperation;



    if (isIndentedLabel)
    {
        explanation = parsed.mnemonic + " is not an instruction, a directive or a macro. A Merlin label must"
                      " begin in column 1; indented, it is read as the opcode field and " + parsed.operand +
                      " is read as its operand. Move " + parsed.mnemonic + " to the start of the line.";
    }

    return explanation;
}
