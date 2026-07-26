#include "Pch.h"

#include "ExpressionEvaluator.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Token types for expression lexer
//
////////////////////////////////////////////////////////////////////////////////

namespace
{

enum class TokType
{
    Number, Ident,
    Plus, Minus, Star, Slash, Percent,
    Amp, Pipe, Caret, Tilde, Bang,
    AmpAmp, PipePipe,
    LShift, RShift,
    Lt, Le, Gt, Ge, Eq, Ne,
    LParen, RParen, LBracket, RBracket,
    PlusPlus, MinusMinus,
    End, Error
};

struct Token
{
    TokType     type   = TokType::End;
    int32_t     numVal = 0;
    std::string strVal;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Tokenizer
//
////////////////////////////////////////////////////////////////////////////////

class Tokenizer
{
public:
    Tokenizer (const std::string & text) : m_text (text), m_pos (0), m_hasPeeked (false), m_lastWasValue (false) { }

    Token Next()
    {
        Token t;

        if (m_hasPeeked)
        {
            m_hasPeeked = false;
            t = m_peeked;
        }
        else
        {
            t = ReadNext();
        }

        m_lastWasValue = (t.type == TokType::Number || t.type == TokType::Ident || t.type == TokType::RParen || t.type == TokType::RBracket);
        return t;
    }

    Token Peek()
    {
        if (!m_hasPeeked)
        {
            m_peeked    = ReadNext();
            m_hasPeeked = true;
        }

        return m_peeked;
    }

private:
    void SkipSpaces()
    {
        while (m_pos < m_text.size() && (m_text[m_pos] == ' ' || m_text[m_pos] == '\t'))
            m_pos++;
    }



    Token ReadNext();
    Token ReadHexNumber();
    Token ReadBinaryNumber();
    Token ReadCharConstant();
    Token ReadDecimalNumber();
    Token ReadIdentifier();

    const std::string & m_text;
    size_t              m_pos;
    Token               m_peeked;
    bool                m_hasPeeked;
    bool                m_lastWasValue;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReadHexNumber — after consuming '$'
//
////////////////////////////////////////////////////////////////////////////////

Token Tokenizer::ReadHexNumber()
{
    size_t start = m_pos;

    while (m_pos < m_text.size() && isxdigit ((unsigned char) m_text[m_pos]))
        m_pos++;

    if (m_pos == start)
        return { TokType::Error, 0, "Expected hex digit after $" };

    int32_t val = (int32_t) strtoul (m_text.substr (start, m_pos - start).c_str(), nullptr, 16);
    return { TokType::Number, val, "" };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadBinaryNumber — after consuming '%'
//
////////////////////////////////////////////////////////////////////////////////

Token Tokenizer::ReadBinaryNumber()
{
    size_t start = m_pos;

    while (m_pos < m_text.size() && (m_text[m_pos] == '0' || m_text[m_pos] == '1'))
        m_pos++;

    if (m_pos == start)
        return { TokType::Error, 0, "Expected binary digit after %" };

    int32_t val = (int32_t) strtoul (m_text.substr (start, m_pos - start).c_str(), nullptr, 2);
    return { TokType::Number, val, "" };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadCharConstant — after consuming opening quote
//
////////////////////////////////////////////////////////////////////////////////

Token Tokenizer::ReadCharConstant()
{
    if (m_pos >= m_text.size())
        return { TokType::Error, 0, "Unterminated character constant" };

    char ch = m_text[m_pos++];

    // Handle escape sequences
    if (ch == '\\' && m_pos < m_text.size())
    {
        char esc = m_text[m_pos++];

        switch (esc)
        {
        case 'n':  ch = '\n'; break;
        case 'r':  ch = '\r'; break;
        case 't':  ch = '\t'; break;
        case '0':  ch = '\0'; break;
        case '\\': ch = '\\'; break;
        case '\'': ch = '\''; break;
        default:   ch = esc;  break;
        }
    }

    if (m_pos >= m_text.size() || m_text[m_pos] != '\'')
        return { TokType::Error, 0, "Unterminated character constant" };

    m_pos++;
    return { TokType::Number, (int32_t) (unsigned char) ch, "" };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadDecimalNumber
//
////////////////////////////////////////////////////////////////////////////////

Token Tokenizer::ReadDecimalNumber()
{
    size_t start = m_pos;

    // Check for 0x (hex) or 0b (binary) prefix
    if (m_text[m_pos] == '0' && m_pos + 1 < m_text.size())
    {
        char next = (char) tolower ((unsigned char) m_text[m_pos + 1]);

        if (next == 'x')
        {
            m_pos += 2;
            size_t hexStart = m_pos;

            while (m_pos < m_text.size() && isxdigit ((unsigned char) m_text[m_pos]))
                m_pos++;

            if (m_pos == hexStart)
                return { TokType::Error, 0, "Expected hex digit after 0x" };

            int32_t val = (int32_t) strtoul (m_text.substr (hexStart, m_pos - hexStart).c_str(), nullptr, 16);
            return { TokType::Number, val, "" };
        }

        if (next == 'b' && m_pos + 2 < m_text.size() && (m_text[m_pos + 2] == '0' || m_text[m_pos + 2] == '1'))
        {
            m_pos += 2;
            size_t binStart = m_pos;

            while (m_pos < m_text.size() && (m_text[m_pos] == '0' || m_text[m_pos] == '1'))
                m_pos++;

            int32_t val = (int32_t) strtoul (m_text.substr (binStart, m_pos - binStart).c_str(), nullptr, 2);
            return { TokType::Number, val, "" };
        }
    }

    while (m_pos < m_text.size() && isdigit ((unsigned char) m_text[m_pos]))
        m_pos++;

    // Check for base#value format: digits followed by #
    if (m_pos < m_text.size() && m_text[m_pos] == '#')
    {
        std::string baseStr = m_text.substr (start, m_pos - start);
        int base = (int) strtol (baseStr.c_str(), nullptr, 10);

        if (base >= 2 && base <= 36)
        {
            m_pos++;  // skip '#'
            size_t valStart = m_pos;

            while (m_pos < m_text.size() && isalnum ((unsigned char) m_text[m_pos]))
                m_pos++;

            if (m_pos == valStart)
                return { TokType::Error, 0, "Expected value after base#" };

            int32_t val = (int32_t) strtoul (m_text.substr (valStart, m_pos - valStart).c_str(), nullptr, base);
            return { TokType::Number, val, "" };
        }
    }

    int32_t val = (int32_t) strtol (m_text.substr (start, m_pos - start).c_str(), nullptr, 10);
    return { TokType::Number, val, "" };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadIdentifier
//
////////////////////////////////////////////////////////////////////////////////

Token Tokenizer::ReadIdentifier()
{
    size_t start = m_pos;

    while (m_pos < m_text.size() && (isalnum ((unsigned char) m_text[m_pos]) || m_text[m_pos] == '_' || m_text[m_pos] == '.'))
        m_pos++;

    std::string name = m_text.substr (start, m_pos - start);
    return { TokType::Ident, 0, name };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadNext — main tokenizer dispatch
//
////////////////////////////////////////////////////////////////////////////////

Token Tokenizer::ReadNext()
{
    SkipSpaces();

    if (m_pos >= m_text.size())
        return { TokType::End, 0, "" };

    char c = m_text[m_pos];

    if (c == '\'') { m_pos++; return ReadCharConstant(); }

    if (c == '$')
    {
        m_pos++;

        if (m_pos < m_text.size() && isxdigit ((unsigned char) m_text[m_pos]))
            return ReadHexNumber();

        // Bare $ = current PC (returned as Star, handled like * in primary)
        return { TokType::Star, 0, "" };
    }

    if (c == '%')
    {
        if (!m_lastWasValue && m_pos + 1 < m_text.size() && (m_text[m_pos + 1] == '0' || m_text[m_pos + 1] == '1'))
        {
            m_pos++;
            return ReadBinaryNumber();
        }

        m_pos++;
        return { TokType::Percent, 0, "" };
    }

    if (c == '@')
    {
        // @octal prefix
        m_pos++;
        size_t start = m_pos;

        while (m_pos < m_text.size() && m_text[m_pos] >= '0' && m_text[m_pos] <= '7')
            m_pos++;

        if (m_pos == start)
            return { TokType::Error, 0, "Expected octal digit after @" };

        int32_t val = (int32_t) strtoul (m_text.substr (start, m_pos - start).c_str(), nullptr, 8);
        return { TokType::Number, val, "" };
    }

    if (isdigit ((unsigned char) c))
        return ReadDecimalNumber();

    if (isalpha ((unsigned char) c) || c == '_')
        return ReadIdentifier();

    m_pos++;

    switch (c)
    {
    case '+':
        if (m_pos < m_text.size() && m_text[m_pos] == '+') { m_pos++; return { TokType::PlusPlus, 0, "" }; }
        return { TokType::Plus, 0, "" };

    case '-':
        if (m_pos < m_text.size() && m_text[m_pos] == '-') { m_pos++; return { TokType::MinusMinus, 0, "" }; }
        return { TokType::Minus, 0, "" };

    case '*':  return { TokType::Star,     0, "" };
    case '/':  return { TokType::Slash,    0, "" };
    case '&':
        if (m_pos < m_text.size() && m_text[m_pos] == '&') { m_pos++; return { TokType::AmpAmp, 0, "" }; }
        return { TokType::Amp, 0, "" };

    case '|':
        if (m_pos < m_text.size() && m_text[m_pos] == '|') { m_pos++; return { TokType::PipePipe, 0, "" }; }
        return { TokType::Pipe, 0, "" };

    case '^':  return { TokType::Caret,    0, "" };
    case '~':  return { TokType::Tilde,    0, "" };
    case '!':
        if (m_pos < m_text.size() && m_text[m_pos] == '=') { m_pos++; return { TokType::Ne, 0, "" }; }
        return { TokType::Bang, 0, "" };

    case '(':  return { TokType::LParen,   0, "" };
    case ')':  return { TokType::RParen,   0, "" };
    case '[':  return { TokType::LBracket, 0, "" };
    case ']':  return { TokType::RBracket, 0, "" };

    case '<':
        if (m_pos < m_text.size() && m_text[m_pos] == '<') { m_pos++; return { TokType::LShift, 0, "" }; }
        if (m_pos < m_text.size() && m_text[m_pos] == '=') { m_pos++; return { TokType::Le,     0, "" }; }
        if (m_pos < m_text.size() && m_text[m_pos] == '>') { m_pos++; return { TokType::Ne,     0, "" }; }
        return { TokType::Lt, 0, "" };

    case '>':
        if (m_pos < m_text.size() && m_text[m_pos] == '>') { m_pos++; return { TokType::RShift, 0, "" }; }
        if (m_pos < m_text.size() && m_text[m_pos] == '=') { m_pos++; return { TokType::Ge,     0, "" }; }
        return { TokType::Gt, 0, "" };

    case '=':
        if (m_pos < m_text.size() && m_text[m_pos] == '=') m_pos++;
        return { TokType::Eq, 0, "" };

    default:
        return { TokType::Error, 0, std::string ("Unexpected character: ") + c };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Forward declarations for recursive descent
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseLogOr      (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
static bool ParseLogAnd     (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
static bool ParseBitOr      (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
static bool ParseBitXor     (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
static bool ParseBitAnd     (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
static bool ParseEquality   (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
static bool ParseComparison (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
static bool ParseShift      (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
static bool ParseAddSub     (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
static bool ParseMulDiv     (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
static bool ParseUnary      (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
static bool ParsePrimary    (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);

static std::string ToUpperIdent (const std::string & s)
{
    std::string r = s;

    for (char & c : r)
        c = (char) toupper ((unsigned char) c);

    return r;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParsePrimary — numbers, identifiers, *, (expr), [expr]
//
////////////////////////////////////////////////////////////////////////////////

static bool ParsePrimary (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error)
{
    Token t = tok.Peek();

    if (t.type == TokType::Number) { tok.Next(); result = t.numVal; return true; }

    if (t.type == TokType::Ident)
    {
        tok.Next();

        if (ctx.symbols)
        {
            auto it = ctx.symbols->find (t.strVal);

            if (it != ctx.symbols->end())
            {
                result = it->second;
                return true;
            }
        }

        error = "Undefined symbol: " + t.strVal;
        return false;
    }

    if (t.type == TokType::Star)
    {
        tok.Next();
        result = ctx.currentPC;
        return true;
    }

    if (t.type == TokType::LParen)
    {
        tok.Next();

        if (!ParseLogOr (tok, ctx, result, error))
            return false;

        if (tok.Next().type != TokType::RParen)
        {
            error = "Expected closing parenthesis";
            return false;
        }

        return true;
    }

    if (t.type == TokType::LBracket)
    {
        tok.Next();

        if (!ParseLogOr (tok, ctx, result, error))
            return false;

        if (tok.Next().type != TokType::RBracket)
        {
            error = "Expected closing bracket";
            return false;
        }

        return true;
    }

    if (t.type == TokType::End) { error = "Unexpected end of expression"; return false; }

    error = "Unexpected token in expression";
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseUnary — -, +, ~, !, <, >, ++, --, lo, hi
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseUnary (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error)
{
    Token t = tok.Peek();

    if (t.type == TokType::Minus)     { tok.Next(); if (!ParseUnary (tok, ctx, result, error)) return false; result = -result; return true; }
    if (t.type == TokType::Plus)      { tok.Next(); return ParseUnary (tok, ctx, result, error); }
    if (t.type == TokType::Tilde)     { tok.Next(); if (!ParseUnary (tok, ctx, result, error)) return false; result = ~result; return true; }
    if (t.type == TokType::Bang)      { tok.Next(); if (!ParseUnary (tok, ctx, result, error)) return false; result = (result == 0) ? 1 : 0; return true; }
    if (t.type == TokType::PlusPlus)  { tok.Next(); if (!ParseUnary (tok, ctx, result, error)) return false; result = result + 1; return true; }
    if (t.type == TokType::MinusMinus){ tok.Next(); if (!ParseUnary (tok, ctx, result, error)) return false; result = result - 1; return true; }
    if (t.type == TokType::Lt)        { tok.Next(); if (!ParseUnary (tok, ctx, result, error)) return false; result = result & 0xFF; return true; }
    if (t.type == TokType::Gt)        { tok.Next(); if (!ParseUnary (tok, ctx, result, error)) return false; result = (result >> 8) & 0xFF; return true; }

    if (t.type == TokType::Ident)
    {
        std::string upper = ToUpperIdent (t.strVal);

        if (upper == "LO") { tok.Next(); if (!ParseUnary (tok, ctx, result, error)) return false; result = result & 0xFF; return true; }
        if (upper == "HI") { tok.Next(); if (!ParseUnary (tok, ctx, result, error)) return false; result = (result >> 8) & 0xFF; return true; }
    }

    return ParsePrimary (tok, ctx, result, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseMulDiv — *, /, %
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseMulDiv (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error)
{
    if (!ParseUnary (tok, ctx, result, error))
        return false;

    for (;;)
    {
        Token t = tok.Peek();

        if (t.type == TokType::Star)
        {
            tok.Next();
            int32_t right = 0;

            if (!ParseUnary (tok, ctx, right, error)) return false;
            result = result * right;
        }
        else if (t.type == TokType::Slash)
        {
            tok.Next();
            int32_t right = 0;

            if (!ParseUnary (tok, ctx, right, error)) return false;

            if (right == 0) { error = "Division by zero"; return false; }
            result = result / right;
        }
        else if (t.type == TokType::Percent)
        {
            tok.Next();
            int32_t right = 0;

            if (!ParseUnary (tok, ctx, right, error)) return false;

            if (right == 0) { error = "Division by zero"; return false; }
            result = result % right;
        }
        else break;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseAddSub — +, -
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseAddSub (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error)
{
    if (!ParseMulDiv (tok, ctx, result, error))
        return false;

    for (;;)
    {
        Token t = tok.Peek();

        if (t.type == TokType::Plus)       { tok.Next(); int32_t r = 0; if (!ParseMulDiv (tok, ctx, r, error)) return false; result += r; }
        else if (t.type == TokType::Minus) { tok.Next(); int32_t r = 0; if (!ParseMulDiv (tok, ctx, r, error)) return false; result -= r; }
        else break;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseShift — <<, >>
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseShift (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error)
{
    if (!ParseAddSub (tok, ctx, result, error))
        return false;

    for (;;)
    {
        Token t = tok.Peek();

        if (t.type == TokType::LShift)      { tok.Next(); int32_t r = 0; if (!ParseAddSub (tok, ctx, r, error)) return false; result = result << r; }
        else if (t.type == TokType::RShift)  { tok.Next(); int32_t r = 0; if (!ParseAddSub (tok, ctx, r, error)) return false; result = (int32_t) ((uint32_t) result >> r); }
        else break;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseComparison — <, >, <=, >=
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseComparison (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error)
{
    if (!ParseShift (tok, ctx, result, error))
        return false;

    for (;;)
    {
        Token t = tok.Peek();

        if (t.type == TokType::Lt)      { tok.Next(); int32_t r = 0; if (!ParseShift (tok, ctx, r, error)) return false; result = (result < r) ? 1 : 0; }
        else if (t.type == TokType::Gt) { tok.Next(); int32_t r = 0; if (!ParseShift (tok, ctx, r, error)) return false; result = (result > r) ? 1 : 0; }
        else if (t.type == TokType::Le) { tok.Next(); int32_t r = 0; if (!ParseShift (tok, ctx, r, error)) return false; result = (result <= r) ? 1 : 0; }
        else if (t.type == TokType::Ge) { tok.Next(); int32_t r = 0; if (!ParseShift (tok, ctx, r, error)) return false; result = (result >= r) ? 1 : 0; }
        else break;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseEquality — =, ==, !=
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseEquality (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error)
{
    if (!ParseComparison (tok, ctx, result, error))
        return false;

    for (;;)
    {
        Token t = tok.Peek();

        if (t.type == TokType::Eq)      { tok.Next(); int32_t r = 0; if (!ParseComparison (tok, ctx, r, error)) return false; result = (result == r) ? 1 : 0; }
        else if (t.type == TokType::Ne) { tok.Next(); int32_t r = 0; if (!ParseComparison (tok, ctx, r, error)) return false; result = (result != r) ? 1 : 0; }
        else break;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseBitAnd — &
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseBitAnd (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error)
{
    if (!ParseEquality (tok, ctx, result, error))
        return false;

    while (tok.Peek().type == TokType::Amp)
    {
        tok.Next();
        int32_t r = 0;

        if (!ParseEquality (tok, ctx, r, error)) return false;
        result = result & r;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseBitXor — ^
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseBitXor (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error)
{
    if (!ParseBitAnd (tok, ctx, result, error))
        return false;

    while (tok.Peek().type == TokType::Caret)
    {
        tok.Next();
        int32_t r = 0;

        if (!ParseBitAnd (tok, ctx, r, error)) return false;
        result = result ^ r;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseBitOr — |
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseBitOr (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error)
{
    if (!ParseBitXor (tok, ctx, result, error))
        return false;

    while (tok.Peek().type == TokType::Pipe)
    {
        tok.Next();
        int32_t r = 0;

        if (!ParseBitXor (tok, ctx, r, error)) return false;
        result = result | r;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseLogAnd — &&
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseLogAnd (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error)
{
    if (!ParseBitOr (tok, ctx, result, error))
        return false;

    while (tok.Peek().type == TokType::AmpAmp)
    {
        tok.Next();
        int32_t r = 0;

        if (!ParseBitOr (tok, ctx, r, error)) return false;
        result = (result != 0 && r != 0) ? 1 : 0;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseLogOr — ||
//
////////////////////////////////////////////////////////////////////////////////

static bool ParseLogOr (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error)
{
    if (!ParseLogAnd (tok, ctx, result, error))
        return false;

    while (tok.Peek().type == TokType::PipePipe)
    {
        tok.Next();
        int32_t r = 0;

        if (!ParseLogAnd (tok, ctx, r, error)) return false;
        result = (result != 0 || r != 0) ? 1 : 0;
    }

    return true;
}

}  // anonymous namespace





////////////////////////////////////////////////////////////////////////////////
//
//  ExpressionEvaluator::Evaluate
//
////////////////////////////////////////////////////////////////////////////////

ExprResult ExpressionEvaluator::Evaluate (const std::string & expr, const ExprContext & ctx)
{
    ExprResult res = {};
    res.success       = false;
    res.value         = 0;
    res.hasUnresolved = false;

    std::string trimmed = expr;

    size_t start = trimmed.find_first_not_of (" \t");

    if (start == std::string::npos)
    {
        res.error = "Empty expression";
        return res;
    }

    size_t end = trimmed.find_last_not_of (" \t");
    trimmed = trimmed.substr (start, end - start + 1);

    Tokenizer   tok (trimmed);
    std::string error;
    int32_t     value = 0;

    if (!ParseLogOr (tok, ctx, value, error))
    {
        res.error         = error;
        res.hasUnresolved = (error.find ("Undefined symbol") == 0);
        return res;
    }

    Token remaining = tok.Peek();

    if (remaining.type != TokType::End)
    {
        res.error = "Unexpected content after expression";
        return res;
    }

    res.success = true;
    res.value   = value;
    return res;
}
