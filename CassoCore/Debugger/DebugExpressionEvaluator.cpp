#include "Pch.h"

#include "Debugger/DebugExpressionEvaluator.h"

#include "Debugger/IDebugExpressionContext.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Operator tables
//
//  Longer spellings come first so the scan is longest-match: `<=` is never
//  read as `<` followed by `=`. Higher precedence binds tighter; every unary
//  operator binds tighter than any binary one.
//
////////////////////////////////////////////////////////////////////////////////

const DebugExpressionEvaluator::OperatorSpelling DebugExpressionEvaluator::s_kBinaryOperators[16] =
{
    { "//", ExpressionOperator::Divide,         7 },
    { "<=", ExpressionOperator::LessOrEqual,    5 },
    { ">=", ExpressionOperator::GreaterOrEqual, 5 },
    { "==", ExpressionOperator::Equal,          4 },
    { "!=", ExpressionOperator::NotEqual,       4 },
    { "*",  ExpressionOperator::Multiply,       7 },
    { "/",  ExpressionOperator::Divide,         7 },
    { "%",  ExpressionOperator::Modulo,         7 },
    { "+",  ExpressionOperator::Add,            6 },
    { "-",  ExpressionOperator::Subtract,       6 },
    { "<",  ExpressionOperator::Less,           5 },
    { ">",  ExpressionOperator::Greater,        5 },
    { "=",  ExpressionOperator::Equal,          4 },
    { "&",  ExpressionOperator::And,            3 },
    { "^",  ExpressionOperator::Xor,            2 },
    { "|",  ExpressionOperator::Or,             1 },
};

const DebugExpressionEvaluator::OperatorSpelling DebugExpressionEvaluator::s_kUnaryOperators[6] =
{
    { "-",  ExpressionOperator::Negate,         8 },
    { "+",  ExpressionOperator::Identity,       8 },
    { "!",  ExpressionOperator::Not,            8 },
    { "<",  ExpressionOperator::LowByte,        8 },
    { ">",  ExpressionOperator::HighByte,       8 },
    { "*",  ExpressionOperator::Dereference,    8 },
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluator::Parse
//
//  Converts infix text to postfix. A bare name that matches a register is the
//  register, a bare name made only of hex digits is a number, and any other
//  name is a symbol, resolved when the expression is evaluated. $ forces hex
//  and # forces decimal.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugExpressionEvaluator::Parse (
    const std::string  & text,
    Expression         & expression,
    std::string        & error)
{
    HRESULT        hr            = S_OK;
    OperatorStack  stack;
    size_t         pos           = 0;
    bool           expectOperand = true;
    bool           isValid       = true;
    bool           isBalanced    = true;



    expression      = Expression();
    expression.text = text;
    error.clear();

    while (pos < text.size())
    {
        if (isspace ((unsigned char) text[pos]))
        {
            ++pos;
            continue;
        }

        isValid = expectOperand ? TryParseOperandPosition  (text, pos, expectOperand, stack, expression, error)
                                : TryParseOperatorPosition (text, pos, expectOperand, stack, expression, error);
        CBR (isValid);
    }

    CBRF (!expectOperand, error = "The expression is incomplete.");

    while (!stack.empty())
    {
        isBalanced = stack.back() != ExpressionOperator::OpenParen;
        CBRF (isBalanced, error = "The parentheses do not balance.");

        EmitOperator (stack.back(), expression);
        stack.pop_back();
    }

Error:
    if (FAILED (hr))
    {
        expression.postfix.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluator::Evaluate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugExpressionEvaluator::Evaluate (
    const Expression               & expression,
    const IDebugExpressionContext  & context,
    int32_t                        & value,
    std::string                    & error)
{
    HRESULT               hr        = S_OK;
    std::vector<int32_t>  values;
    bool                  isApplied = true;
    bool                  isSingle  = false;



    value = 0;
    error.clear();

    for (const ExpressionToken & token : expression.postfix)
    {
        isApplied = TryApplyToken (token, context, values, error);
        CBR (isApplied);
    }

    isSingle = values.size() == 1;
    CBRF (isSingle, error = "The expression is incomplete.");

    value = values.back();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluator::ParseAndEvaluate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DebugExpressionEvaluator::ParseAndEvaluate (
    const std::string              & text,
    const IDebugExpressionContext  & context,
    int32_t                        & value,
    std::string                    & error)
{
    HRESULT     hr = S_OK;
    Expression  expression;



    value = 0;

    hr = Parse (text, expression, error);
    CHR (hr);

    hr = Evaluate (expression, context, value, error);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluator::TryParseOperandPosition
//
//  Where a value is expected: an opening parenthesis, a prefix operator, or
//  the value itself.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugExpressionEvaluator::TryParseOperandPosition (
    const std::string  & text,
    size_t             & pos,
    bool               & expectOperand,
    OperatorStack      & stack,
    Expression         & expression,
    std::string        & error)
{
    ExpressionToken     token;
    ExpressionOperator  op        = ExpressionOperator::Add;
    bool                isRead    = false;



    if (text[pos] == '(')
    {
        stack.push_back (ExpressionOperator::OpenParen);
        ++pos;
        return true;
    }

    if (TryReadOperator (text, pos, s_kUnaryOperators, op))
    {
        stack.push_back (op);
        return true;
    }

    isRead = TryReadOperand (text, pos, token, error);

    if (isRead)
    {
        expression.postfix.push_back (token);
        expectOperand = false;
    }
    else if (error.empty())
    {
        error = std::format ("A value was expected at column {}.", pos + 1);
    }

    return isRead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluator::TryParseOperatorPosition
//
//  Where an operator is expected: a closing parenthesis or a binary
//  operator. Binary operators are left-associative.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugExpressionEvaluator::TryParseOperatorPosition (
    const std::string  & text,
    size_t             & pos,
    bool               & expectOperand,
    OperatorStack      & stack,
    Expression         & expression,
    std::string        & error)
{
    ExpressionOperator  op = ExpressionOperator::Add;



    if (text[pos] == ')')
    {
        while (!stack.empty() && stack.back() != ExpressionOperator::OpenParen)
        {
            EmitOperator (stack.back(), expression);
            stack.pop_back();
        }

        if (stack.empty())
        {
            error = "The parentheses do not balance.";
            return false;
        }

        stack.pop_back();
        ++pos;
        return true;
    }

    if (!TryReadOperator (text, pos, s_kBinaryOperators, op))
    {
        error = std::format ("An operator was expected at column {}.", pos + 1);
        return false;
    }

    while (!stack.empty() && stack.back() != ExpressionOperator::OpenParen && GetPrecedence (stack.back()) >= GetPrecedence (op))
    {
        EmitOperator (stack.back(), expression);
        stack.pop_back();
    }

    stack.push_back (op);
    expectOperand = true;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluator::TryReadOperand
//
//  A character literal, $hex, #decimal, or a name. Leaves error empty when
//  nothing at pos can start a value.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugExpressionEvaluator::TryReadOperand (
    const std::string  & text,
    size_t             & pos,
    ExpressionToken    & token,
    std::string        & error)
{
    static constexpr size_t  kLiteralLength = 3;
    size_t                   start          = pos;
    std::string              word;
    std::string              upper;
    char                     lead           = text[pos];
    bool                     isNumber       = false;



    if (lead == '\'')
    {
        if (pos + kLiteralLength > text.size() || text[pos + 2] != '\'')
        {
            error = "A character value is written as 'c'.";
            return false;
        }

        token.value = (unsigned char) text[pos + 1];
        pos        += kLiteralLength;
        return true;
    }

    // @n is search result n, resolved by the session like a symbol.
    if (lead == '@')
    {
        ++pos;

        while (pos < text.size() && isdigit ((unsigned char) text[pos]))
        {
            word += text[pos++];
        }

        if (word.empty())
        {
            error = "A search result is written @n.";
            return false;
        }

        token.kind = ExpressionToken::Kind::Symbol;
        token.name = "@" + word;
        return true;
    }

    if (lead == '$' || lead == '#')
    {
        ++pos;
    }

    while (pos < text.size() && (isalnum ((unsigned char) text[pos]) || text[pos] == '_' || text[pos] == '.'))
    {
        word  += text[pos];
        upper += (char) toupper ((unsigned char) text[pos]);
        ++pos;
    }

    if (word.empty())
    {
        pos = start;
        return false;
    }

    if (lead == '#' || lead == '$')
    {
        isNumber = TryParseNumber (word, lead == '#' ? 10 : 16, token.value);

        if (!isNumber)
        {
            error = std::format ("{} is not a {} number.", text.substr (start, pos - start), lead == '#' ? "decimal" : "hex");
        }

        return isNumber;
    }

    if (IsRegisterName (upper))
    {
        token.kind = ExpressionToken::Kind::Register;
        token.name = upper;
        return true;
    }

    if (TryParseNumber (word, 16, token.value))
    {
        return true;
    }

    if (isdigit ((unsigned char) lead))
    {
        error = std::format ("{} is not a hex number.", word);
        return false;
    }

    token.kind = ExpressionToken::Kind::Symbol;
    token.name = word;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluator::TryReadOperator
//
////////////////////////////////////////////////////////////////////////////////

bool DebugExpressionEvaluator::TryReadOperator (
    const std::string                  & text,
    size_t                             & pos,
    std::span<const OperatorSpelling>    table,
    ExpressionOperator                 & op)
{
    for (const OperatorSpelling & entry : table)
    {
        if (text.compare (pos, strlen (entry.spelling), entry.spelling) == 0)
        {
            op   = entry.op;
            pos += strlen (entry.spelling);
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluator::TryParseNumber
//
//  Digits only, in the given base, up to 32 bits.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugExpressionEvaluator::TryParseNumber (
    const std::string  & digits,
    int                  base,
    int32_t            & value)
{
    static constexpr int64_t  kMaxValue = 0xFFFFFFFF;
    int64_t                   total     = 0;



    for (char ch : digits)
    {
        int digit = isdigit ((unsigned char) ch) ? ch - '0' : (isxdigit ((unsigned char) ch) ? toupper ((unsigned char) ch) - 'A' + 10 : base);

        if (digit >= base)
        {
            return false;
        }

        total = total * base + digit;

        if (total > kMaxValue)
        {
            return false;
        }
    }

    value = (int32_t) total;
    return !digits.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluator::IsRegisterName
//
////////////////////////////////////////////////////////////////////////////////

bool DebugExpressionEvaluator::IsRegisterName (const std::string & upperName)
{
    return upperName == "A" || upperName == "X" || upperName == "Y" ||
           upperName == "P" || upperName == "S" || upperName == "PC";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluator::IsUnary
//
////////////////////////////////////////////////////////////////////////////////

bool DebugExpressionEvaluator::IsUnary (ExpressionOperator op)
{
    for (const OperatorSpelling & entry : s_kUnaryOperators)
    {
        if (entry.op == op)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluator::GetPrecedence
//
////////////////////////////////////////////////////////////////////////////////

int DebugExpressionEvaluator::GetPrecedence (ExpressionOperator op)
{
    for (const OperatorSpelling & entry : s_kUnaryOperators)
    {
        if (entry.op == op)
        {
            return entry.precedence;
        }
    }

    for (const OperatorSpelling & entry : s_kBinaryOperators)
    {
        if (entry.op == op)
        {
            return entry.precedence;
        }
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluator::EmitOperator
//
////////////////////////////////////////////////////////////////////////////////

void DebugExpressionEvaluator::EmitOperator (ExpressionOperator op, Expression & expression)
{
    ExpressionToken  token;



    token.kind = ExpressionToken::Kind::Operator;
    token.op   = op;
    expression.postfix.push_back (token);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluator::TryApplyToken
//
////////////////////////////////////////////////////////////////////////////////

bool DebugExpressionEvaluator::TryApplyToken (
    const ExpressionToken          & token,
    const IDebugExpressionContext  & context,
    std::vector<int32_t>           & values,
    std::string                    & error)
{
    int32_t  lhs    = 0;
    int32_t  rhs    = 0;
    int32_t  result = 0;
    bool     isDone = false;
    size_t   needed = 0;



    if (token.kind != ExpressionToken::Kind::Operator)
    {
        isDone = TryResolveOperand (token, context, result, error);
    }
    else
    {
        needed = IsUnary (token.op) ? 1 : 2;

        if (values.size() < needed)
        {
            error = "The expression is incomplete.";
            return false;
        }

        rhs = values.back();
        values.pop_back();

        if (needed == 1)
        {
            isDone = TryApplyUnary (token.op, rhs, context, result, error);
        }
        else
        {
            lhs = values.back();
            values.pop_back();
            isDone = TryApplyBinary (token.op, lhs, rhs, result, error);
        }
    }

    if (isDone)
    {
        values.push_back (result);
    }

    return isDone;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluator::TryResolveOperand
//
////////////////////////////////////////////////////////////////////////////////

bool DebugExpressionEvaluator::TryResolveOperand (
    const ExpressionToken          & token,
    const IDebugExpressionContext  & context,
    int32_t                        & value,
    std::string                    & error)
{
    Word  resolved = 0;
    bool  isFound  = true;



    switch (token.kind)
    {
    case ExpressionToken::Kind::Register:
        isFound = context.TryGetRegister (token.name, resolved);
        break;

    case ExpressionToken::Kind::Symbol:
        isFound = context.TryResolveSymbol (token.name, resolved);
        break;

    default:
        value = token.value;
        return true;
    }

    if (!isFound)
    {
        error = std::format ("{} is not a symbol, register, or hex number.", token.name);
        return false;
    }

    value = resolved;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluator::TryApplyUnary
//
//  ! complements within 16 bits, since values are addresses and bytes.
//  < and > take the low and high byte. * reads one byte through the context.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugExpressionEvaluator::TryApplyUnary (
    ExpressionOperator               op,
    int32_t                          operand,
    const IDebugExpressionContext  & context,
    int32_t                        & result,
    std::string                    & error)
{
    static constexpr int32_t  kWordMask = 0xFFFF;
    static constexpr int32_t  kByteMask = 0xFF;
    static constexpr int      kByteBits = 8;
    Byte                      byte      = 0;
    bool                      isRead    = true;



    switch (op)
    {
    case ExpressionOperator::Negate:   result = -operand;                            break;
    case ExpressionOperator::Not:      result = ~operand & kWordMask;                break;
    case ExpressionOperator::LowByte:  result = operand & kByteMask;                 break;
    case ExpressionOperator::HighByte: result = (operand >> kByteBits) & kByteMask;  break;

    case ExpressionOperator::Dereference:
        isRead = context.TryPeek ((Word) operand, byte);
        result = byte;
        break;

    default:
        result = operand;
        break;
    }

    if (!isRead)
    {
        error = std::format ("The byte at ${:04X} cannot be read.", (Word) operand);
    }

    return isRead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluator::TryApplyBinary
//
//  Comparisons produce 1 or 0.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugExpressionEvaluator::TryApplyBinary (
    ExpressionOperator   op,
    int32_t              lhs,
    int32_t              rhs,
    int32_t            & result,
    std::string        & error)
{
    bool  isDivision = op == ExpressionOperator::Divide || op == ExpressionOperator::Modulo;



    if (isDivision && rhs == 0)
    {
        error = "The expression divides by zero.";
        return false;
    }

    switch (op)
    {
    case ExpressionOperator::Add:            result = lhs + rhs;   break;
    case ExpressionOperator::Subtract:       result = lhs - rhs;   break;
    case ExpressionOperator::Multiply:       result = lhs * rhs;   break;
    case ExpressionOperator::Divide:         result = lhs / rhs;   break;
    case ExpressionOperator::Modulo:         result = lhs % rhs;   break;
    case ExpressionOperator::And:            result = lhs & rhs;   break;
    case ExpressionOperator::Or:             result = lhs | rhs;   break;
    case ExpressionOperator::Xor:            result = lhs ^ rhs;   break;
    case ExpressionOperator::Less:           result = lhs <  rhs;  break;
    case ExpressionOperator::Greater:        result = lhs >  rhs;  break;
    case ExpressionOperator::LessOrEqual:    result = lhs <= rhs;  break;
    case ExpressionOperator::GreaterOrEqual: result = lhs >= rhs;  break;
    case ExpressionOperator::Equal:          result = lhs == rhs;  break;
    case ExpressionOperator::NotEqual:       result = lhs != rhs;  break;
    default:                                 result = 0;           break;
    }

    return true;
}
