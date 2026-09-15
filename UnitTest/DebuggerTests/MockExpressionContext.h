#pragma once

#include "Debugger/IDebugExpressionContext.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MockExpressionContext
//
//  Registers, 64 KB of memory and a symbol map, all set directly by tests.
//  Addresses listed in unreadable fail TryPeek, as I/O does.
//
////////////////////////////////////////////////////////////////////////////////

class MockExpressionContext : public IDebugExpressionContext
{
public:
    std::map<std::string, Word>  registers  = { { "A", 0x41 }, { "X", 0x01 }, { "Y", 0x02 }, { "P", 0x30 }, { "S", 0xFF }, { "PC", 0x0300 } };
    std::map<std::string, Word>  symbols    = { { "HOME", 0xFC58 }, { "COUT", 0xFDED } };
    std::vector<Byte>            memory     = std::vector<Byte> (0x10000, 0);
    std::set<Word>               unreadable = { 0xC000 };

    bool TryGetRegister (const std::string & name, Word & value) const override
    {
        auto it = registers.find (name);

        if (it == registers.end())
        {
            return false;
        }

        value = it->second;
        return true;
    }

    bool TryPeek (Word address, Byte & value) const override
    {
        if (unreadable.contains (address))
        {
            return false;
        }

        value = memory[address];
        return true;
    }

    bool TryResolveSymbol (const std::string & name, Word & address) const override
    {
        std::string upper (name);



        for (char & ch : upper)
        {
            ch = (char) toupper ((unsigned char) ch);
        }

        auto it = symbols.find (upper);

        if (it == symbols.end())
        {
            return false;
        }

        address = it->second;
        return true;
    }
};
