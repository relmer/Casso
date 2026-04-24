#pragma once



enum class WarningMode
{
    Warn,
    NoWarn,
    FatalWarnings,
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
};



struct AssemblyResult
{
    bool                                       success;
    std::vector<Byte>                          bytes;
    Word                                       startAddress;
    Word                                       endAddress;
    std::unordered_map<std::string, Word>      symbols;
    std::vector<AssemblyError>                 errors;
    std::vector<AssemblyError>                 warnings;
    std::vector<AssemblyLine>                  listing;
};



struct AssemblerOptions
{
    Byte        fillByte        = 0xFF;
    bool        generateListing = false;
    WarningMode warningMode     = WarningMode::Warn;
};



struct OpcodeEntry
{
    Byte opcode;
    Byte operandSize;
};
