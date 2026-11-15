#include "Pch.h"

#include "Machines/Apple2/Common/ProDosTypeNames.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosTypeNames::Find
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ProDosTypeNames::Find (Byte type)
{
    for (const Row & row : kRows)
    {
        if (row.type == type)
        {
            return row.abbreviation;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosTypeNames::Format
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ProDosTypeNames::Format (Byte type)
{
    const wchar_t  * abbreviation = Find (type);



    return (abbreviation != nullptr) ? std::wstring (abbreviation) : std::format (L"${:02X}", (unsigned) type);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosTypeNames::TryParse
//
////////////////////////////////////////////////////////////////////////////////

bool ProDosTypeNames::TryParse (const std::string & text, Byte & outType)
{
    size_t        prefix = 0;
    unsigned      value  = 0;
    std::wstring  upper;
    bool          found  = false;



    if (text.size() > 1 && text[0] == '$')
    {
        prefix = 1;
    }
    else if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        prefix = 2;
    }

    if (prefix != 0)
    {
        auto  [end, error] = std::from_chars (text.data() + prefix, text.data() + text.size(), value, 16);

        found = error == std::errc() && end == text.data() + text.size() && value <= 0xFF;

        if (found)
        {
            outType = (Byte) value;
        }
    }
    else
    {
        for (char c : text)
        {
            upper += (wchar_t) ((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c);
        }

        for (const Row & row : kRows)
        {
            std::wstring  abbreviation = row.abbreviation;

            while (!abbreviation.empty() && abbreviation.back() == L' ')
            {
                abbreviation.pop_back();
            }

            if (!found && abbreviation == upper)
            {
                outType = row.type;
                found   = true;
            }
        }
    }

    return found;
}
