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
    int          lineNumber;
    std::string  message;
    std::string  file;
    int          column = 0;
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
};





class FileReader;



struct AssemblerOptions
{
    // Which dialect's syntax the source is written in. Carried here rather than
    // in a command-line parser so EVERY entry point selects it the same way --
    // the CLI, the tests, and anything later that assembles source. Defaults to
    // AS65, so callers that predate dialect selection are unaffected.
    DialectId                                   dialect           = DialectId::As65;

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
    bool                                        cycleCounts       = false;   // -c flag
    bool                                        macroExpansion    = false;   // -m flag (show macro expansion in listing)
    int                                         pageHeight        = 0;   // -h flag (0 = no pagination)
    int                                         pageWidth         = 80;   // -w flag
    bool                                        caseSensitive     = false;   // -i flag (we're case-insensitive by default)
    bool                                        pass1Listing      = false;   // -p flag
    bool                                        symbolTable       = false;   // -t flag
    bool                                        debugInfo         = false;   // -g flag
    bool                                        verbose           = false;   // -v flag
    bool                                        quiet             = false;   // -q flag
    bool                                        disableOpt        = false;   // -n flag
    std::unordered_map<std::string, int32_t>    predefinedSymbols;   // -d flag
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
