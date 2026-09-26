#pragma once

#include "Debugger/DebugFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileFormat / SymbolFileEntry
//
////////////////////////////////////////////////////////////////////////////////

enum class SymbolFileFormat
{
    Unknown,
    CassoDebug,       // NAME=$ADDR, with ; comments
    Cc65Debug,        // cc65's debug-info format, version 2
    MerlinListing,    // the symbol table at the end of a Merlin listing
    AppleWinSym,      // ADDR NAME
    ViceLabels,       // al ADDR .NAME
};

struct SymbolFileEntry
{
    std::string  name;
    Word         address    = 0;
    bool         isConstant = false;   // an equate: a value, not an address
};





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader
//
//  Reads a symbol file in any of the five formats, chosen from its content.
//  A cc65 debug file gives the symbols in its top-level scopes; local labels
//  and the labels macro expansions made stay out of the table.
//  Two formats tell an equate from a label, and give it as a constant: a cc65
//  debug file by its `type=equ`, and a Casso debug file by the `; constants`
//  section a SYM SAVE writes. The other three carry no such distinction.
//  A name that appears more than once keeps its first address, so the two
//  orders a Casso debug file holds give one symbol each.
//
////////////////////////////////////////////////////////////////////////////////

class SymbolFileReader
{
public:
    // A Casso debug file's heading for its constants, after the `;`.
    static constexpr const char  * kConstantsHeading = "constants";

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
    static void  AddUnique         (std::vector<SymbolFileEntry> & symbols, const std::string & name, Word address, bool isConstant = false);
    static void  ReadCasso         (const std::vector<std::string> & lines, std::vector<SymbolFileEntry> & symbols);
    static void  ReadCc65          (const DebugFile & file, std::vector<SymbolFileEntry> & symbols);
    static void  ReadMerlinListing (const std::vector<std::string> & lines, std::vector<SymbolFileEntry> & symbols);
    static void  ReadAppleWin      (const std::vector<std::string> & lines, std::vector<SymbolFileEntry> & symbols);
    static void  ReadVice          (const std::vector<std::string> & lines, std::vector<SymbolFileEntry> & symbols);
    static std::string  Trim       (const std::string & text);
};
