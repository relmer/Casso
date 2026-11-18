#include "Pch.h"

#include "Ui/Debugger/GoToAddressText.h"
#include "Ui/Debugger/DebuggerViewSnapshot.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GoToAddressText::TryParseGoTo
//
//  A hex address, with or without a $; PC; or a symbol's name, any case.
//
////////////////////////////////////////////////////////////////////////////////

bool GoToAddressText::TryParseGoTo (const std::wstring & text, const DebuggerViewSnapshot * snapshot, Word & outAddress)
{
    std::string             typed  = TextEncoding::WideToNarrow (text);
    std::string             digits;
    uint32_t                value  = 0;
    std::from_chars_result  parsed;



    typed.erase (0, typed.find_first_not_of (" \t"));
    typed.erase (typed.find_last_not_of (" \t") + 1);
    digits = typed.starts_with ('$') ? typed.substr (1) : typed;

    if (!digits.empty() && digits.size() <= 4)
    {
        parsed = std::from_chars (digits.data(), digits.data() + digits.size(), value, 16);

        if (parsed.ec == std::errc() && parsed.ptr == digits.data() + digits.size())
        {
            outAddress = (Word) value;
            return true;
        }
    }

    if (snapshot == nullptr)
    {
        return false;
    }

    if (_stricmp (typed.c_str(), "PC") == 0)
    {
        outAddress = snapshot->pc;
        return true;
    }

    for (const SymbolInfo & symbol : (snapshot->symbols != nullptr) ? snapshot->symbols->symbols : std::vector<SymbolInfo>())
    {
        if (_stricmp (symbol.name.c_str(), typed.c_str()) == 0)
        {
            outAddress = symbol.address;
            return true;
        }
    }

    return false;
}
