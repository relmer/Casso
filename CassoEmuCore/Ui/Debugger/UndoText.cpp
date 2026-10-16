#include "Pch.h"

#include "Ui/Debugger/UndoText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UndoText::Split
//
//  An empty text has no fields.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> UndoText::Split (const std::string & text, char separator)
{
    std::vector<std::string>  fields;
    size_t                    at = 0;



    if (text.empty())
    {
        return fields;
    }

    for (size_t next = text.find (separator); ; next = text.find (separator, at))
    {
        fields.push_back (text.substr (at, (next == std::string::npos) ? std::string::npos : next - at));

        if (next == std::string::npos)
        {
            break;
        }

        at = next + 1;
    }

    return fields;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UndoText::TryParseHex
//
////////////////////////////////////////////////////////////////////////////////

bool UndoText::TryParseHex (const std::string & text, uint32_t & value)
{
    constexpr size_t  kMaxDigits = 8;



    if (text.empty() || text.size() > kMaxDigits || !std::ranges::all_of (text, [] (char ch) { return isxdigit ((unsigned char) ch) != 0; }))
    {
        return false;
    }

    value = (uint32_t) std::stoul (text, nullptr, 16);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UndoText::FormatBytes
//
////////////////////////////////////////////////////////////////////////////////

std::string UndoText::FormatBytes (std::span<const Byte> bytes)
{
    std::string  text;



    for (Byte each : bytes)
    {
        text += std::format ("{:02X}", each);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UndoText::TryParseBytes
//
////////////////////////////////////////////////////////////////////////////////

bool UndoText::TryParseBytes (const std::string & text, std::vector<Byte> & bytes)
{
    uint32_t  value = 0;



    bytes.clear();

    if (text.size() % 2 != 0)
    {
        return false;
    }

    for (size_t i = 0; i < text.size(); i += 2)
    {
        if (!TryParseHex (text.substr (i, 2), value))
        {
            return false;
        }

        bytes.push_back ((Byte) value);
    }

    return true;
}
