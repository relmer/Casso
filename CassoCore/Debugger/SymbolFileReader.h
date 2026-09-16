#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileFormat / SymbolFileEntry
//
////////////////////////////////////////////////////////////////////////////////

enum class SymbolFileFormat
{
    Unknown,
    CassoDebug,       // NAME=$ADDR, with ; comments
    MerlinListing,    // the symbol table at the end of a Merlin listing
    AppleWinSym,      // ADDR NAME
    ViceLabels,       // al ADDR .NAME
};

struct SymbolFileEntry
{
    std::string  name;
    Word         address = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader
//
//  Reads a symbol file in any of the four formats, chosen from its content.
//  A name that appears more than once keeps its first address, so the two
//  orders a Casso debug file holds give one symbol each.
//
////////////////////////////////////////////////////////////////////////////////

class SymbolFileReader
{
public:
    static SymbolFileFormat  Detect (const std::string & content);
    static HRESULT           Read   (const std::string & content, std::vector<SymbolFileEntry> & symbols, SymbolFileFormat & format, std::string & error);

private:
    static constexpr size_t  kAddressDigits = 4;

    static void  SplitLines        (const std::string & content, std::vector<std::string> & lines);
    static bool  IsCassoLine       (const std::string & line);
    static bool  IsAppleWinLine    (const std::string & line);
    static bool  IsViceLine        (const std::string & line);
    static bool  HasMerlinHeading  (const std::string & line);
    static bool  TryParseHex       (const std::string & text, Word & value);
    static void  AddUnique         (std::vector<SymbolFileEntry> & symbols, const std::string & name, Word address);
    static void  ReadCasso         (const std::vector<std::string> & lines, std::vector<SymbolFileEntry> & symbols);
    static void  ReadMerlinListing (const std::vector<std::string> & lines, std::vector<SymbolFileEntry> & symbols);
    static void  ReadAppleWin      (const std::vector<std::string> & lines, std::vector<SymbolFileEntry> & symbols);
    static void  ReadVice          (const std::vector<std::string> & lines, std::vector<SymbolFileEntry> & symbols);
    static std::string  Trim       (const std::string & text);
};
