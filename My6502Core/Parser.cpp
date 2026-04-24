#include "Pch.h"

#include "Parser.h"



std::vector<std::string> Parser::SplitLines (const std::string & source)
{
    std::vector<std::string> lines;
    std::string              line;
    std::istringstream       stream (source);

    while (std::getline (stream, line))
    {
        // Strip trailing \r if present (handles \r\n line endings)
        if (!line.empty () && line.back () == '\r')
        {
            line.pop_back ();
        }

        lines.push_back (line);
    }

    if (lines.empty ())
    {
        lines.push_back ("");
    }

    return lines;
}



static std::string StripComments (const std::string & line)
{
    size_t pos = line.find (';');

    if (pos == std::string::npos)
    {
        return line;
    }

    return line.substr (0, pos);
}



static std::string Trim (const std::string & s)
{
    size_t start = s.find_first_not_of (" \t");

    if (start == std::string::npos)
    {
        return "";
    }

    size_t end = s.find_last_not_of (" \t");
    return s.substr (start, end - start + 1);
}



static std::string ToUpper (const std::string & s)
{
    std::string result = s;

    for (char & c : result)
    {
        c = (char) toupper ((unsigned char) c);
    }

    return result;
}



ParsedLine Parser::ParseLine (const std::string & line, int lineNumber)
{
    ParsedLine result = {};
    result.lineNumber = lineNumber;
    result.isEmpty    = true;

    // Strip comments first
    std::string stripped = StripComments (line);
    std::string trimmed  = Trim (stripped);

    if (trimmed.empty ())
    {
        return result;
    }

    result.isEmpty = false;
    std::string remainder = trimmed;

    // Check for label (contains ':')
    size_t colonPos = remainder.find (':');

    if (colonPos != std::string::npos)
    {
        result.label = Trim (remainder.substr (0, colonPos));
        remainder    = Trim (remainder.substr (colonPos + 1));

        if (remainder.empty ())
        {
            return result;
        }
    }

    // Extract mnemonic (first word)
    size_t spacePos = remainder.find_first_of (" \t");

    if (spacePos == std::string::npos)
    {
        result.mnemonic = ToUpper (remainder);
        return result;
    }

    result.mnemonic = ToUpper (remainder.substr (0, spacePos));
    result.operand  = Trim (remainder.substr (spacePos + 1));

    return result;
}



ClassifiedOperand Parser::ClassifyOperand (const std::string & operand, const std::string & mnemonic)
{
    (void) operand;
    (void) mnemonic;

    ClassifiedOperand result = {};
    result.mode    = GlobalAddressingMode::SingleByteNoOperand;
    result.value   = 0;
    result.isLabel = false;
    return result;
}



bool Parser::ParseValue (const std::string & text, int & value)
{
    if (text.empty ())
    {
        return false;
    }

    // Hex: $FF
    if (text[0] == '$')
    {
        std::string hex = text.substr (1);

        if (hex.empty ())
        {
            return false;
        }

        char * endPtr = nullptr;
        long   parsed = strtol (hex.c_str (), &endPtr, 16);

        if (*endPtr != '\0')
        {
            return false;
        }

        value = (int) parsed;
        return true;
    }

    // Decimal
    char * endPtr = nullptr;
    long   parsed = strtol (text.c_str (), &endPtr, 10);

    if (*endPtr != '\0')
    {
        return false;
    }

    value = (int) parsed;
    return true;
}
