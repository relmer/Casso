#pragma once

#include "Dialect.h"



class DialectProfile;





enum class WarningMode
{
    Warn,
    NoWarn,
    FatalWarnings,
};





////////////////////////////////////////////////////////////////////////////////
//
//  OperatorBinding
//
//  How an expression's binary operators bind to their operands. A DIALECT fact
//  carried into the shared evaluator, rather than a second evaluator.
//
//  Assemblers of the period commonly gave operators no precedence at all: the
//  expression is folded strictly left to right, and parentheses are the only way
//  to group. Merlin is one of them, and the vendor corpus proves it rather than
//  the manual -- LABELS.S ends with
//
//      ERR END-LABTBL-1/$700
//
//  bounding its own table at seven pages. Under ordinary precedence the division
//  binds first, the whole expression reduces to the table's length, and the
//  assertion fires on a file the vendor shipped an object for. Left to right it
//  is (END-LABTBL-1)/$700, which is 0 for any table inside seven pages.
//
//  It is a rule about BINDING, not about the operator set, so both dialects keep
//  the same operators and the same folds.
//
////////////////////////////////////////////////////////////////////////////////

enum class OperatorBinding
{
    ByPrecedence,   // the usual levels: multiplicative before additive, and so on
    LeftToRight,    // every operator binds equally; only parentheses group
};





////////////////////////////////////////////////////////////////////////////////
//
//  ArithmeticWidth
//
//  How wide the values an expression folds are.
//
//  Native is the evaluator's own 32-bit signed arithmetic, which is what every
//  caller had before dialect selection. Word16 makes every operand and every
//  result an unsigned 16-bit quantity, which is what a 6502-era assembler
//  actually computes in.
//
//  It is not cosmetic, and CLOCK.S is why. The file selects between a 12-hour
//  and a 24-hour build with
//
//      HOURS = VERSION-25/-1*12+12
//       ERR HOURS-VERSION
//
//  which is an equality test written as arithmetic: only when VERSION is 24 does
//  `VERSION-25` become $FFFF, which divided by $FFFF is 1. For 12 the numerator
//  is $FFF3, which is SMALLER than the divisor, so the quotient is 0 and the
//  expression folds to 12. In 32-bit signed arithmetic the same line gives
//  -13 / -1 = 13 and the assertion on the next line fails an assembly the vendor
//  shipped a working object for.
//
////////////////////////////////////////////////////////////////////////////////

enum class ArithmeticWidth
{
    Native,   // 32-bit signed, as every caller predating dialect selection had
    Word16,   // unsigned 16-bit, operands and results alike
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExprOperator
//
//  A bitwise operation an expression can name, independent of which character
//  names it. The shared evaluator already performs all three; what differs
//  between dialects is only the name, which is why this is a lookup on the
//  way IN to the tokenizer rather than a second set of folds.
//
//  Merlin needs it because two of its names collide with the shared
//  syntax outright: `!` reads as logical-not there and `.` is not punctuation
//  at all. CLOCK.S settles both from its two shipped objects -- `HOURS/24!1`
//  has to be exclusive-or (inclusive-or puts the edit cursor on the wrong digit
//  for the 24-hour build) and `HOURS/24+3."0"` has to be inclusive-or to make
//  the hour-rollover comparison read "24" and "13".
//
////////////////////////////////////////////////////////////////////////////////

enum class ExprOperator
{
    BitOr,
    BitXor,
    BitAnd,
};





////////////////////////////////////////////////////////////////////////////////
//
//  OperatorSpelling
//
//  One character a dialect uses for one bitwise operation. A profile supplies a
//  table of these and the tokenizer consults it before its own punctuation, so
//  a dialect can rename an operator without the evaluator gaining a dialect
//  branch.
//
////////////////////////////////////////////////////////////////////////////////

struct OperatorSpelling
{
    char          character;
    ExprOperator  operation;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DialectSelection
//
//  Whether the dialect below was CHOSEN or merely inherited.
//
//  The distinction is not decoration: it decides whether the active dialect is
//  worth telling anyone about. A caller that named one already knows -- saying
//  it back adds a line to every build and informs nobody. A caller that named
//  none is assembling under a dialect it never asked for, and that is the one
//  case where the answer is worth having.
//
//  It defaults to Defaulted for the same reason `dialect` defaults to AS65:
//  a caller written before dialect selection existed named nothing, and that is
//  exactly what this records. Setting the dialect without setting this leaves
//  the pair honest as well -- a stated dialect that forgot to say so is merely
//  over-reported, where the reverse would suppress the report that matters.
//
////////////////////////////////////////////////////////////////////////////////

enum class DialectSelection
{
    Stated,      // the caller named a dialect; the invocation itself is the record
    Defaulted,   // the caller named none and took whatever the default is
};





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolKind
//
////////////////////////////////////////////////////////////////////////////////

enum class SymbolKind
{
    Label,     // Defined by label declaration (immutable)
    Equ,       // Defined by equ (immutable)
    Set,       // Defined by = or set (reassignable)
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiagnosticKind
//
//  Whether a diagnostic says "your source is wrong" or "Casso does not do this".
//
//  A developer reading a failure has to be able to tell those apart, and the
//  message alone cannot carry the distinction to anything but a human -- a build
//  script, an editor, or a test asserting the difference all need a field. It is
//  additive, defaulting to the reading every diagnostic that predates it had, so
//  nothing already recorded changes shape or meaning.
//
////////////////////////////////////////////////////////////////////////////////

enum class DiagnosticKind
{
    SourceError,      // the source is wrong, or the assembler cannot make sense of it
    SubsetBoundary,   // the construct is understood and deliberately not supported
};





////////////////////////////////////////////////////////////////////////////////
//
//  AssemblyError
//
//  One diagnostic. Position is carried so an editor can jump to it.
//
//  `file` and `column` are DEFAULTED, which is what makes them additive: every
//  diagnostic that predates them keeps compiling and keeps its shape, and a
//  dialect that knows its column populates one without obliging every other
//  site to.
//
//  An empty `file` means the top-level input rather than "unknown". That is the
//  distinction that lets the reporting side print exactly what it always did
//  for a diagnostic with no file of its own, while an error raised inside an
//  included file names the file it actually came from.
//
//  A column of 0 means "no column known", since columns are 1-based when known.
//
////////////////////////////////////////////////////////////////////////////////

struct AssemblyError
{
    int             lineNumber;
    std::string     message;
    std::string     file;
    int             column = 0;
    DiagnosticKind  kind   = DiagnosticKind::SourceError;
};





struct AssemblyLine
{
    int                lineNumber;
    Word               address;
    std::vector<Byte>  bytes;
    std::string        sourceText;
    bool               hasAddress;
    bool               isMacroExpansion  = false;
    bool               isConditionalSkip = false;
    Byte               cycleCounts       = 0;
};





// Scalars carry defaults so a plain `AssemblyResult r;` means "failed, nothing
// assembled" rather than garbage. The containers default themselves; only the
// PODs need saying. Matches AssemblyLine above and ExprResult.
struct AssemblyResult
{
    bool                                        success      = false;
    std::vector<Byte>                           bytes;
    Word                                        startAddress = 0;
    Word                                        endAddress   = 0;
    std::unordered_map<std::string, Word>       symbols;
    std::unordered_map<std::string, SymbolKind> symbolKinds;
    std::vector<AssemblyError>                  errors;
    std::vector<AssemblyError>                  warnings;
    std::vector<AssemblyLine>                   listing;
    std::string                                 listingTitle;

    //  How many source lines pass 2 processed. Counted separately from
    //  `listing` because a listing is only BUILT when one was asked for, so
    //  its size is zero on an ordinary assembly and says nothing about the
    //  work done.
    size_t                                      linesAssembled = 0;

    //  What the assembly's object should be called, once the caller's answer
    //  and the source's have been reconciled. Empty when neither named one.
    //
    //  REPORTED rather than acted on. Nothing here writes a file, so this says
    //  what the name is and leaves the writing to whoever asked for the
    //  assembly -- which keeps the precedence rule in one place instead of
    //  repeated at every entry point that produces output.
    std::string                                 outputFileName;

    //  Whether the SOURCE selected the wider instruction set, through a dialect
    //  that has a directive for it. Reported because nothing outside the source
    //  can know: the directive may sit inside a conditional, so a caller that
    //  passed no CPU flag cannot otherwise tell "the dialect's default stood"
    //  from "the source chose the wider set" -- and telling those apart is the
    //  whole reason the CPU in effect is reported at all.
    bool                                        extendedSetSelectedInSource = false;
};





class FileReader;



struct AssemblerOptions
{
    // Which dialect's syntax the source is written in. Carried here rather than
    // in a command-line parser so EVERY entry point selects it the same way --
    // the CLI, the tests, and anything later that assembles source. Defaults to
    // AS65, so callers that predate dialect selection are unaffected.
    DialectId                                   dialect           = DialectId::As65;

    // Whether the field above was chosen or inherited. Carried beside the
    // dialect rather than derived from it, because the two are independent
    // facts: AS65 is both a dialect a caller can state and the value a caller
    // that stated nothing ends up with, so the dialect alone cannot say which
    // happened.
    DialectSelection                            dialectSelection  = DialectSelection::Defaulted;

    // A profile that is not in the registry, for a caller that has one. Null --
    // which is every production caller -- means the registry answers for
    // `dialect` as it always did.
    //
    // The injection point exists because the registry is a closed table, and a
    // mechanism that can only be exercised through its own table cannot be shown
    // to work for a dialect that is not in it yet. That is precisely the claim
    // SC-009 makes, so the claim needs a door. Same shape as `fileReader`: the
    // dependency is named here rather than reached for.
    const DialectProfile                      * dialectProfile    = nullptr;
    Byte                                        fillByte          = 0xFF;
    bool                                        generateListing   = false;
    WarningMode                                 warningMode       = WarningMode::Warn;
    FileReader                                * fileReader        = nullptr;
    std::string                                 baseDir;

    // What the CALLER wants the object called, which beats anything the source
    // names. Empty means the caller has no opinion and a dialect whose source
    // can name its own output gets to.
    //
    // Carried here rather than left to the command-line layer for the reason
    // `dialect` is: every entry point that assembles source has to resolve the
    // same precedence, and one that resolved it differently would be a
    // difference nobody would find until a build wrote the wrong file.
    std::string                                 outputFileName;
    bool                                        cycleCounts       = false;   // -c flag
    bool                                        macroExpansion    = false;   // -m flag (show macro expansion in listing)
    int                                         pageHeight        = 0;   // -h flag (0 = no pagination)
    int                                         pageWidth         = 79;   // -w flag (as65's default)
    bool                                        caseSensitive     = false;   // -i flag (we're case-insensitive by default)
    bool                                        pass1Listing      = false;   // -p flag
    bool                                        symbolTable       = false;   // -t flag
    bool                                        debugInfo         = false;   // -g flag
    bool                                        verbose           = false;   // -v flag
    bool                                        quiet             = false;   // -q flag
    bool                                        disableOpt        = false;   // -n flag
    std::unordered_map<std::string, int32_t>    predefinedSymbols;   // -d flag

    // Which prefix the invocation used for its flags, so a diagnostic that
    // tells the user to pass one uses their prefix. The assembler has no
    // command line of its own; this arrives from whoever built these options,
    // and the default suits every caller that never had one.
    char                                        flagPrefix        = '-';
};





struct OpcodeEntry
{
    Byte opcode;
    Byte operandSize;
    Byte cycleCounts;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MacroDefinition
//
////////////////////////////////////////////////////////////////////////////////

struct MacroDefinition
{
    std::string              name;
    std::vector<std::string> paramNames;    // Named parameters (optional)
    std::vector<std::string> body;          // Raw source lines between macro and endm
    std::vector<std::string> localLabels;   // Labels declared with local
    int                      lineNumber = 0; // Source line of macro keyword

    // The file the macro keyword appeared in. Same reasoning as
    // ConditionalState::openFile: a diagnostic about a definition is reported
    // long after the definition was read, so the file has to be captured where
    // the construct OPENED rather than taken from ambient state. Empty means
    // the top-level input.
    std::string              sourceFile;

    // And the column the keyword sat at, for the same reason once more. 0 where
    // the dialect records no columns.
    //
    // The three carriers above are what let this struct double as the record of
    // a definition still being COLLECTED, which is what the collector keeps a
    // stack of. A definition met while another is open shares its terminator,
    // so several can be in flight at once and each has to be able to report its
    // own opening position if the source ends before that terminator arrives.
    int                      openColumn = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ConditionalState
//
//  One frame of the conditional-assembly stack: an IF / IFDEF block and
//  whether its body is being assembled.
//
//  parentAssembling is separate from assembling because nesting is not a
//  simple AND at the point of use. An ELSE has to flip the block's own state
//  WITHOUT resurrecting a body whose enclosing block is skipped, so the two
//  facts must be tracked independently rather than folded into one flag.
//
//  seenElse makes a second ELSE in one block detectable, which is otherwise
//  indistinguishable from a legal one.
//
//  openLineNumber is carried purely for diagnostics. An unclosed block leaves
//  nothing behind at the point of failure, so without it the end-of-pass error
//  can only say how many levels are open and blame the end of the file --
//  which is never where the fix goes.
//
////////////////////////////////////////////////////////////////////////////////

struct ConditionalState
{
    bool assembling       = false;   // True if current block is being assembled
    bool seenElse         = false;   // True if else has been encountered
    bool parentAssembling = false;   // True if enclosing block is assembling

    // Source line the IF / IFDEF opened on. Carried purely for diagnostics:
    // a block that is never closed leaves nothing behind at the point of
    // failure, so without this the end-of-pass error can only say how many
    // levels are open and blame the end of the file -- which is never where
    // the fix goes.
    int  openLineNumber   = 0;

    // And the file it opened in, for the same reason one level up. This
    // diagnostic is DEFERRED to the end of the pass, by which time the
    // assembler's ambient notion of "the current file" is whatever was
    // processed last. An IF opened inside an included file would otherwise be
    // reported with the right line and the wrong file, which is a stronger
    // version of blaming the end of the file. Empty means the top-level input.
    std::string  openFile;

    // And the column it opened at, captured for the same reason again. 0 where
    // the dialect records no columns.
    int  openColumn       = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  StructMember
//
////////////////////////////////////////////////////////////////////////////////

struct StructMember
{
    std::string name;
    int32_t     offset;
    int32_t     size;
};





////////////////////////////////////////////////////////////////////////////////
//
//  StructDefinition
//
////////////////////////////////////////////////////////////////////////////////

struct StructDefinition
{
    std::string                name;
    int32_t                    startOffset   = 0;
    int32_t                    currentOffset = 0;
    std::vector<StructMember>  members;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CharacterMap
//
////////////////////////////////////////////////////////////////////////////////

struct CharacterMap
{
    Byte table[256];



    CharacterMap ()
    {
        for (int i = 0; i < 256; i++)
        {
            table[i] = (Byte) i;
        }
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  FileReader (interface for include file resolution)
//
////////////////////////////////////////////////////////////////////////////////

struct FileReadResult
{
    bool        success;
    std::string contents;
    std::string error;
};

class FileReader
{
public:
    virtual ~FileReader () = default;
    virtual FileReadResult ReadFile (const std::string & filename, const std::string & baseDir) = 0;
};

class DefaultFileReader : public FileReader
{
public:
    FileReadResult ReadFile (const std::string & filename, const std::string & baseDir) override;
};
