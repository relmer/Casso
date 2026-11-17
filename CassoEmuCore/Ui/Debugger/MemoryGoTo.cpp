#include "Pch.h"

#include "Ui/Debugger/MemoryGoTo.h"

#include "Debugger/DebugSession.h"
#include "Debugger/AppleWinParser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryGoTo::TryResolveGoTo
//
//  The address is read by the console's own evaluator, so a symbol, a
//  register, decimal or arithmetic works here as it does in any command.
//  Around it may sit a 6502 operand form -- e,X, e,Y, (e,X), (e),Y or (e) --
//  resolved against the registers and the bytes `peek` returns. A value
//  under $100 is a zero-page operand unless it was written as three or four
//  hex digits. S on its own is the stack address, as the registers pane
//  shows it.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryGoTo::TryResolveGoTo (
    const std::string              & text,
    const IDebugExpressionContext  & context,
    const GoToPeek                 & peek,
    Word                           & address,
    std::string                    & error)
{
    static constexpr Word  kStackPage    = 0x0100;
    static constexpr Word  kZeroPageMask = 0x00FF;
    GoToOperand            operand       = SplitGoToOperand (text);
    std::string            upper         = SymbolTable::ToUpper (operand.core);
    Word                   value         = 0;
    Word                   index         = 0;
    bool                   isZeroPage    = false;



    if (!operand.isIndirect && operand.index == 0 && (upper == "S" || upper == "SP"))
    {
        context.TryGetRegister ("S", value);
        address = (Word) (kStackPage + (value & kZeroPageMask));
        return true;
    }

    if (!AppleWinParser::TryEvaluate (operand.core, context, value, error))
    {
        return false;
    }

    isZeroPage = value <= kZeroPageMask && !IsWrittenAbsolute (operand.core);

    if (operand.index != 0)
    {
        context.TryGetRegister (std::string (1, operand.index), index);
    }

    if (operand.isIndirect)
    {
        return TryReadGoToPointer (operand, value, index, isZeroPage, peek, address, error);
    }

    address = (Word) (value + index);
    address = (isZeroPage && operand.index != 0) ? (Word) (address & kZeroPageMask) : address;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryGoTo::TryReadGoToPointer
//
//  (e,X) and (e),Y take a zero-page pointer, which wraps within the zero
//  page; (e) takes any address.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryGoTo::TryReadGoToPointer (
    const GoToOperand  & operand,
    Word                 value,
    Word                 index,
    bool                 isZeroPage,
    const GoToPeek     & peek,
    Word               & address,
    std::string        & error)
{
    static constexpr Word  kZeroPageMask = 0x00FF;
    static constexpr int   kByteBits     = 8;
    Word                   pointer       = value;
    std::optional<Byte>    lo;
    std::optional<Byte>    hi;



    if (operand.index != 0 && !isZeroPage)
    {
        error = std::format ("(zp,X) and (zp),Y take a zero-page pointer, and ${:04X} is not on the zero page.", value);
        return false;
    }

    pointer = operand.isIndexInside ? (Word) ((value + index) & kZeroPageMask) : value;
    lo      = peek (pointer);
    hi      = peek (isZeroPage ? (Word) ((pointer + 1) & kZeroPageMask) : (Word) (pointer + 1));

    if (!lo.has_value() || !hi.has_value())
    {
        error = std::format ("The pointer at ${:04X} cannot be read.", pointer);
        return false;
    }

    address = (Word) (*lo | (*hi << kByteBits));
    address = operand.isIndexInside ? address : (Word) (address + index);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryGoTo::SplitGoToOperand
//
//  The 6502 operand form around the expression. Spaces go first, except
//  inside a 'c' character value. Parentheses form an indirect operand only
//  when the first one closes at the form's own end, so (1+2)*3 and (1)+(2)
//  stay expressions.
//
////////////////////////////////////////////////////////////////////////////////

MemoryGoTo::GoToOperand MemoryGoTo::SplitGoToOperand (const std::string & text)
{
    static constexpr size_t  kLiteralLength = 3;
    static constexpr size_t  kSuffixLength  = 2;
    static constexpr size_t  kFormLength    = 3;
    GoToOperand              operand;
    std::string              s;
    std::string              upper;
    size_t                   closing        = std::string::npos;



    for (size_t i = 0; i < text.size(); i++)
    {
        if (text[i] == '\'' && i + kLiteralLength <= text.size())
        {
            s += text.substr (i, kLiteralLength);
            i += kLiteralLength - 1;
        }
        else if (!isspace ((unsigned char) text[i]))
        {
            s += text[i];
        }
    }

    upper           = SymbolTable::ToUpper (s);
    closing         = FindClosingParen (s);
    operand.core    = s;

    if (closing != std::string::npos && upper.ends_with (",X)") && closing == s.size() - 1)
    {
        operand       = { s.substr (1, s.size() - 1 - kFormLength), 'X', true, true };
    }
    else if (closing != std::string::npos && upper.ends_with ("),Y") && closing == s.size() - kFormLength)
    {
        operand       = { s.substr (1, closing - 1), 'Y', true, false };
    }
    else if (closing != std::string::npos && closing == s.size() - 1)
    {
        operand       = { s.substr (1, closing - 1), 0, true, false };
    }
    else if (upper.ends_with (",X") || upper.ends_with (",Y"))
    {
        operand       = { s.substr (0, s.size() - kSuffixLength), upper.back(), false, false };
    }

    return operand;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryGoTo::FindClosingParen
//
//  Where the parenthesis that opens the text closes, or npos when the text
//  does not open with one or it never closes.
//
////////////////////////////////////////////////////////////////////////////////

size_t MemoryGoTo::FindClosingParen (const std::string & text)
{
    int  depth = 0;



    if (!text.starts_with ('('))
    {
        return std::string::npos;
    }

    for (size_t i = 0; i < text.size(); i++)
    {
        depth += (text[i] == '(') ? 1 : 0;
        depth -= (text[i] == ')') ? 1 : 0;

        if (depth == 0)
        {
            return i;
        }
    }

    return std::string::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryGoTo::IsWrittenAbsolute
//
//  A bare hex number of three or four digits, as $0010 or 0010, is an
//  absolute operand whatever its value, as an assembler reads it.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryGoTo::IsWrittenAbsolute (const std::string & core)
{
    static constexpr size_t  kZeroPageDigits = 2;
    std::string              digits          = core.starts_with ('$') ? core.substr (1) : core;



    return digits.size() > kZeroPageDigits && digits.find_first_not_of ("0123456789ABCDEFabcdef") == std::string::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryGoTo::TryResolveMemoryAddress
//
//  On the CPU thread, where the registers and memory are; a pointer in I/O
//  is not read, since reading one changes the machine.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryGoTo::TryResolveMemoryAddress (DebugSession & session, const std::string & text, Word & address, std::string & error)
{
    IDebugTarget  & target = session.GetTarget();



    return TryResolveGoTo (text, session, [&session, &target] (Word at) -> std::optional<Byte>
    {
        Byte  value = 0;

        if (target.GetRegion (at) == MemoryRegion::Io || !session.TryPeek (at, value))
        {
            return std::nullopt;
        }

        return value;
    }, address, error);
}
