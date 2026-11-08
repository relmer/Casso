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
    Word         address       = 0;
    bool         isRelocatable = true;    // moved by a load offset; a cc65 equate is not
};





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReader
//
//  Reads a symbol file in any of the five formats, chosen from its content.
//  A cc65 debug file gives the symbols in its top-level scopes; local labels,
//  the labels macro expansions made, imports and the symbols the assembler
//  defines itself stay out of the table.
//  A name that appears more than once, in any case, keeps its first address,
//  so the two orders a Casso debug file holds give one symbol each.
//
////////////////////////////////////////////////////////////////////////////////

class SymbolFileReader
{
public:
    static SymbolFileFormat  Detect (const std::string & content);
    static HRESULT           Read   (const std::string & content, std::vector<SymbolFileEntry> & symbols, SymbolFileFormat & format, std::string & error);

private:
    static constexpr size_t  kAddressDigits = 4;

    static void         SplitLines            (const std::string & content, std::vector<std::string> & lines);
    static bool         IsCassoLine           (const std::string & line);
    static bool         IsAppleWinLine        (const std::string & line);
    static bool         IsViceLine            (const std::string & line);
    static bool         TryParseViceAddress   (const std::string & text, Word & value);
    static bool         HasMerlinHeading      (const std::string & line);
    static bool         TryParseHex           (const std::string & text, Word & value);
    static void         AddUnique             (std::vector<SymbolFileEntry> & symbols, const std::string & name, Word address, bool isRelocatable = true);
    static void         ReadCasso             (const std::vector<std::string> & lines, std::vector<SymbolFileEntry> & symbols);
    static void         ReadCc65              (const std::string & content, const DebugFile & file, std::vector<SymbolFileEntry> & symbols);
    static void         FindPredefinedSymbols (const std::string & content, std::set<int> & predefined);
    static bool         TryGetRecordField     (const std::string & record, const std::string & key, int & value);
    static void         ReadMerlinListing     (const std::vector<std::string> & lines, std::vector<SymbolFileEntry> & symbols);
    static void         StripMerlinFlags      (std::string & token);
    static void         ReadAppleWin          (const std::vector<std::string> & lines, std::vector<SymbolFileEntry> & symbols);
    static void         ReadVice              (const std::vector<std::string> & lines, std::vector<SymbolFileEntry> & symbols);
    static std::string  Trim                  (const std::string & text);
};
