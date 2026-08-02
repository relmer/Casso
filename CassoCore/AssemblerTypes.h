#pragma once





enum class WarningMode
{
    Warn,
    NoWarn,
    FatalWarnings,
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





struct AssemblyError
{
    int         lineNumber;
    std::string message;
};





struct AssemblyLine
{
    int               lineNumber;
    Word              address;
    std::vector<Byte> bytes;
    std::string       sourceText;
    bool              hasAddress;
    bool              isMacroExpansion = false;
    bool              isConditionalSkip = false;
    Byte              cycleCounts = 0;
};





struct AssemblyResult
{
    bool                                        success;
    std::vector<Byte>                           bytes;
    Word                                        startAddress;
    Word                                        endAddress;
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
    Byte        fillByte        = 0xFF;
    bool        generateListing = false;
    WarningMode warningMode     = WarningMode::Warn;
    FileReader * fileReader     = nullptr;
    std::string  baseDir;
    bool        cycleCounts     = false;     // -c flag
    bool        macroExpansion  = false;     // -m flag (show macro expansion in listing)
    int         pageHeight      = 0;         // -h flag (0 = no pagination)
    int         pageWidth       = 80;        // -w flag
    bool        caseSensitive   = false;     // -i flag (we're case-insensitive by default)
    bool        pass1Listing    = false;     // -p flag
    bool        symbolTable     = false;     // -t flag
    bool        debugInfo       = false;     // -g flag
    bool        verbose         = false;     // -v flag
    bool        quiet           = false;     // -q flag
    bool        disableOpt      = false;     // -n flag
    std::unordered_map<std::string, int32_t> predefinedSymbols; // -d flag
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
};





////////////////////////////////////////////////////////////////////////////////
//
//  ConditionalState
//
////////////////////////////////////////////////////////////////////////////////

struct ConditionalState
{
    bool assembling;         // True if current block is being assembled
    bool seenElse;           // True if else has been encountered
    bool parentAssembling;   // True if enclosing block is assembling
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
    std::string              name;
    int32_t                  startOffset   = 0;
    int32_t                  currentOffset = 0;
    std::vector<StructMember> members;
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
