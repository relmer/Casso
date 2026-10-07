#pragma once

#include "Pch.h"

#include "Debugger/IDebugExpressionContext.h"

class SymbolTable;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapSymbols
//
//  A copy of the symbols the heat map's ranges are read against, taken on
//  the CPU thread and handed to the window with a snapshot: every enabled
//  symbol by name, the size its file gave where it gave one, and the span
//  of the symbols loaded from files, which the "My program" range covers.
//
//  It is an expression context with no registers and no memory, so a range
//  is read by the console's own evaluator and means what it would mean in a
//  command, but can only ever hold numbers and symbols.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapSymbols : public IDebugExpressionContext
{
public:
    //  The enabled tables in their lookup order, so a name two tables hold
    //  means the address a command would take.
    static std::shared_ptr<const HeatMapSymbols>  Build (const SymbolTable & symbols);

    //  A name not yet held. isLoaded counts it toward the program's span.
    void  Add (const std::string & name, Word address, Word size = 0, bool isLoaded = false, bool isConstant = false);

    bool  TryGetRegister   (const std::string & name, Word & value) const override;
    bool  TryPeek          (Word address, Byte & value) const override;
    bool  TryResolveSymbol (const std::string & name, Word & address) const override;

    //  A symbol's size, where its file gave one.
    bool  TryGetSize       (const std::string & name, Word & size) const;

    //  From the lowest loaded label to the highest, through its size; false
    //  with no symbols loaded from a file.
    bool  TryGetProgramSpan (Word & first, Word & last) const;

    //  The first symbol added at an address, as given; a constant has no
    //  address and is never one.
    bool  TryGetNameAt      (Word address, std::string & name) const;

private:
    struct Entry
    {
        Word  address = 0;
        Word  size    = 0;
    };

    std::map<std::string, Entry>  m_symbols;
    std::map<Word, std::string>   m_names;
    std::optional<Word>           m_programFirst;
    uint32_t                      m_programLast = 0;
};
