#include "Pch.h"

#include "Debugger/AppleWinArgumentForms.h"

#include "Debugger/AppleWinBreakpointParser.h"
#include "Debugger/AppleWinText.h"
#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugExpressionEvaluator.h"
#include "Debugger/IDebugExpressionContext.h"
#include "Debugger/AppleWinCommandTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinArgumentForms::GetTextAfterFirstWord
//
//  The rest of the line after its first argument, as typed, for a file name
//  that may hold runs of spaces or tabs.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleWinArgumentForms::GetTextAfterFirstWord (const AppleWinArguments & args)
{
    size_t  from = args.rest.find_first_of (" \t");



    from = args.rest.find_first_not_of (" \t", from);

    return (from == std::string::npos) ? std::string() : args.rest.substr (from);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinArgumentForms::TryParseCount
//
//  A decimal count or id that fits the command's 32-bit field. A larger
//  number is a parse error, not an exception out of the parser.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinArgumentForms::TryParseCount (const std::string & text, uint32_t & value, NumberSyntax syntax)
{
    uint64_t  wide = 0;



    if (!TryParseDecimal (text, wide, syntax) || wide > UINT32_MAX)
    {
        return false;
    }

    value = (uint32_t) wide;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinArgumentForms::TryParseDecimal
//
//  Digits, with or without the # an id is listed with, or the 0n that marks a
//  decimal number in an expression.
//  In WinDbg's syntax 0x or $ marks hex.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinArgumentForms::TryParseDecimal (const std::string & text, uint64_t & value, NumberSyntax syntax)
{
    static constexpr size_t  kMaxDigits    = 18;
    static constexpr size_t  kMaxHexDigits = 15;
    static constexpr int     kHex          = 16;
    std::string              upper         = AppleWinText::ToUpper (text);
    std::string              digits        = text.starts_with ('#') ? text.substr (1) : text;
    int                      radix         = 10;



    if (upper.starts_with ("0N"))
    {
        digits = text.substr (2);
    }
    else if (syntax == NumberSyntax::WinDbg)
    {
        if      (upper.starts_with ("0X")) { digits = text.substr (2); radix = kHex; }
        else if (upper.starts_with ("$"))  { digits = text.substr (1); radix = kHex; }
    }

    if (digits.empty() || digits.size() > (radix == kHex ? kMaxHexDigits : kMaxDigits) ||
        digits.find_first_not_of (radix == kHex ? "0123456789ABCDEFabcdef" : "0123456789") != std::string::npos)
    {
        return false;
    }

    value = std::stoull (digits, nullptr, radix);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinArgumentForms::IsNumberOrRange
//
//  Text made of digits, $ or 0n prefixes and a range's period, rather than a
//  name; in WinDbg's syntax each end may also carry 0x.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinArgumentForms::IsNumberOrRange (const std::string & text, NumberSyntax syntax)
{
    static constexpr const char * kNumberChars = "0123456789ABCDEFabcdef$.";
    std::string                   upper        = AppleWinText::ToUpper (text);
    size_t                        prefix       = std::string::npos;



    for (;;)
    {
        prefix = (syntax == NumberSyntax::WinDbg) ? upper.find ("0X") : std::string::npos;
        prefix = (prefix == std::string::npos) ? upper.find ("0N") : prefix;

        if (prefix == std::string::npos || (prefix > 0 && upper[prefix - 1] != '.'))
        {
            break;
        }

        upper.erase (prefix, 2);
    }

    return upper.find_first_not_of (kNumberChars) == std::string::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinArgumentForms::TryEvaluate
//
//  A value that does not fit in 16 bits is an error.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinArgumentForms::TryEvaluate (
    const std::string              & text,
    const IDebugExpressionContext  & context,
    Word                           & value,
    std::string                    & error)
{
    static constexpr int32_t  kMaxWord = 0xFFFF;
    int32_t                   result   = 0;
    HRESULT                   hr       = DebugExpressionEvaluator::ParseAndEvaluate (text, context, result, error);
    size_t                    first    = text.find_first_not_of (" \t");
    size_t                    last     = text.find_last_not_of (" \t");



    if (FAILED (hr))
    {
        return false;
    }

    //  The value as typed, without the separator a caller left in front of it.
    if (result < 0 || result > kMaxWord)
    {
        error = std::format ("{} is outside $0000-$FFFF.", first == std::string::npos ? text : text.substr (first, last - first + 1));
        return false;
    }

    value = (Word) result;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinArgumentForms::TryParseSourceLine
//
//  `file:line`, for a breakpoint on a source line. Told apart from AppleWin's
//  `addr:addr` range by its left side, which does not evaluate as an address,
//  and its right, which is a decimal line number. The session finds the
//  line's address.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinArgumentForms::TryParseSourceLine (const std::string & text, const IDebugExpressionContext & context, DebugCommand & command)
{
    size_t       colon  = text.rfind (':');
    std::string  file;
    std::string  line;
    std::string  error;
    Word         unused = 0;



    if (colon == std::string::npos || colon == 0 || colon + 1 >= text.size())
    {
        return false;
    }

    file = text.substr (0, colon);
    line = text.substr (colon + 1);

    if (!TryParseCount (line, command.count, context.GetNumberSyntax()) || TryEvaluate (file, context, unused, error))
    {
        return false;
    }

    command.verb  = DebugVerb::SetSourceBreakpoint;
    command.text  = file;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinArgumentForms::TryParseRange
//
//  `addr`, `addr,len` or `addr:last`, into a1 and a2. A range that runs
//  past $FFFF or ends before it starts is an error.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinArgumentForms::TryParseRange (
    const std::string              & text,
    const IDebugExpressionContext  & context,
    DebugCommand                   & command,
    std::string                    & error)
{
    static constexpr uint32_t  kLastAddress = 0xFFFF;
    size_t                     separator    = text.find_first_of (",:");
    Word                       second       = 0;



    command.hasA1 = true;
    command.hasA2 = true;

    if (separator == std::string::npos)
    {
        command.hasA2 = false;
        return TryEvaluate (text, context, command.a1, error);
    }

    if (!TryEvaluate (text.substr (0, separator), context, command.a1, error) ||
        !TryEvaluate (text.substr (separator + 1), context, second, error))
    {
        return false;
    }

    if (text[separator] == ':')
    {
        command.a2 = second;
    }
    else if (second == 0)
    {
        error = "A range length must be at least 1.";
        return false;
    }
    else if ((uint32_t) command.a1 + second - 1 > kLastAddress)
    {
        error = std::format ("The range runs past $FFFF from ${:04X}.", command.a1);
        return false;
    }
    else
    {
        command.a2 = (Word) (command.a1 + second - 1);
    }

    if (command.a2 < command.a1)
    {
        error = std::format ("The range ends at ${:04X}, before its start at ${:04X}.", command.a2, command.a1);
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinArgumentForms::TryParseValues
//
//  Bytes, or words stored low byte first.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinArgumentForms::TryParseValues (
    const Tokens                   & tokens,
    size_t                           first,
    ValueWidth                       width,
    const IDebugExpressionContext  & context,
    DebugCommand                   & command,
    std::string                    & error)
{
    static constexpr Word  kMaxByte = 0xFF;
    Word                   value    = 0;
    bool                   isWord   = false;



    for (size_t i = first; i < tokens.size(); ++i)
    {
        if (!TryEvaluate (tokens[i], context, value, error))
        {
            return false;
        }

        if (width == ValueWidth::Bytes && value > kMaxByte)
        {
            error = std::format ("{} is not a byte.", tokens[i]);
            return false;
        }

        isWord = width == ValueWidth::Words || (width == ValueWidth::BytesOrWords && value > kMaxByte);

        command.values.push_back ((Byte) value);
        command.mask.push_back (kMaxByte);

        if (isWord)
        {
            command.values.push_back ((Byte) (value >> 8));
            command.mask.push_back (kMaxByte);
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinArgumentForms::TryParseSearchItems
//
//  The S and SH item syntax: "text" with the high bit clear, 'text' with it
//  set, a byte, a 16-bit value matched low byte first, and the wildcards ?
//  (any byte), ?n (any high nibble) and n? (any low nibble). ?? is one
//  wildcard. Each byte gets a mask of the bits that must match.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinArgumentForms::TryParseSearchItems (
    const std::string              & items,
    const IDebugExpressionContext  & context,
    DebugCommand                   & command,
    std::string                    & error)
{
    static constexpr Byte  kHighBit = 0x80;
    static constexpr Byte  kAllBits = 0xFF;
    size_t                 pos      = 0;



    while (pos < items.size())
    {
        char    quote = items[pos];
        size_t  close = 0;
        size_t  end   = 0;



        if (isspace ((unsigned char) quote))
        {
            ++pos;
            continue;
        }

        if (quote == '"' || quote == '\'')
        {
            close = items.find (quote, pos + 1);

            if (close == std::string::npos)
            {
                error = "A search string needs a closing quote.";
                return false;
            }

            for (size_t i = pos + 1; i < close; ++i)
            {
                command.values.push_back ((Byte) ((items[i] & ~kHighBit) | (quote == '\'' ? kHighBit : 0)));
                command.mask.push_back (kAllBits);
            }

            pos = close + 1;
            continue;
        }

        end = items.find_first_of (" 	", pos);
        end = (end == std::string::npos) ? items.size() : end;

        if (!TryParseSearchWord (items.substr (pos, end - pos), context, command, error))
        {
            return false;
        }

        pos = end;
    }

    if (command.values.empty())
    {
        error = "Give something to search for.";
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinArgumentForms::TryParseSearchWord
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinArgumentForms::TryParseSearchWord (
    const std::string              & word,
    const IDebugExpressionContext  & context,
    DebugCommand                   & command,
    std::string                    & error)
{
    static constexpr Byte    kAllBits   = 0xFF;
    static constexpr Byte    kLowBits   = 0x0F;
    static constexpr Byte    kHighBits  = 0xF0;
    static constexpr int     kNibble    = 4;
    static constexpr size_t  kByteWidth = 2;
    Word                     value      = 0;
    bool                     isHexish   = word.size() <= kByteWidth && word.find_first_not_of ("0123456789ABCDEFabcdef?") == std::string::npos;



    if (word == "?" || word == "??")
    {
        command.values.push_back (0);
        command.mask.push_back (0);
        return true;
    }

    if (isHexish && word.size() == kByteWidth && word[0] == '?')
    {
        command.values.push_back ((Byte) std::stoul (word.substr (1), nullptr, 16));
        command.mask.push_back (kLowBits);
        return true;
    }

    if (isHexish && word.size() == kByteWidth && word[1] == '?')
    {
        command.values.push_back ((Byte) (std::stoul (word.substr (0, 1), nullptr, 16) << kNibble));
        command.mask.push_back (kHighBits);
        return true;
    }

    if (!TryEvaluate (word, context, value, error))
    {
        return false;
    }

    command.values.push_back ((Byte) value);
    command.mask.push_back (kAllBits);

    if (value > kAllBits)
    {
        command.values.push_back ((Byte) (value >> 8));
        command.mask.push_back (kAllBits);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinArgumentForms::TryParseIdOrAll
//
//  An id in decimal as AppleWin lists them, or * for every entry, which is
//  carried as text.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleWinArgumentForms::TryParseIdOrAll (const Tokens & tokens, DebugCommand & command, std::string & error, NumberSyntax syntax)
{
    if (tokens.empty())
    {
        error = "Give an id, or * for all.";
        return false;
    }

    if (tokens[0] == "*")
    {
        command.text = "*";
        return true;
    }

    if (!TryParseCount (tokens[0], command.count, syntax))
    {
        error = std::format ("{} is not an id.", tokens[0]);
        return false;
    }

    return true;
}
