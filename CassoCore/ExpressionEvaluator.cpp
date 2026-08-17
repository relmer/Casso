#include "Pch.h"

#include "ExpressionEvaluator.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Token types for expression lexer
//
////////////////////////////////////////////////////////////////////////////////



struct ExpressionEvaluator::Token
{
    TokType     type   = TokType::End;
    int32_t     numVal = 0;
    std::string strVal;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Tokenizer
//
//  One-token lookahead scanner over an expression string.
//
//  m_lastWasValue is the interesting piece of state. Several characters are
//  ambiguous in 6502 assembler syntax and can only be resolved by what came
//  before -- `*` is multiplication after a value but the PROGRAM COUNTER at
//  the start of an expression, and `<` / `>` are comparisons after a value but
//  low-byte / high-byte selectors otherwise. Tracking whether the previous
//  token could END a value is exactly the context needed to decide, and it is
//  updated in Next rather than in ReadNext so a Peek cannot corrupt it.
//
//  Peek caches one token, so the parser can branch on what is coming without
//  the scanner re-reading -- which matters because reading is not idempotent
//  once numeric bases and character constants are involved.
//
//  The text is held by REFERENCE, so a Tokenizer must not outlive the string
//  it scans. Every use is a local within a single evaluation.
//
////////////////////////////////////////////////////////////////////////////////

class ExpressionEvaluator::Tokenizer
{
public:
    Tokenizer (const std::string & text, char highAsciiCharDelimiter)
        : m_text (text), m_pos (0), m_hasPeeked (false), m_lastWasValue (false),
          m_highAsciiCharDelimiter (highAsciiCharDelimiter) { }

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
    Token ReadOctalNumber();
    Token ReadCharConstant (char delimiter, bool setsHighBit);
    Token ReadDecimalNumber();
    Token ReadIdentifier();
    Token ReadOperator();
    Token ScanDigits (int base, const char * emptyError);

    const std::string & m_text;
    size_t              m_pos;
    Token               m_peeked;
    bool                m_hasPeeked;
    bool                m_lastWasValue;
    char                m_highAsciiCharDelimiter;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IsDigitInBase
//
//  Whether `c` is a legal digit in radix `base` (2..36). Letters count
//  from a=10, case-insensitively, so this covers every radix the
//  tokenizer accepts with one rule instead of a predicate per prefix.
//
////////////////////////////////////////////////////////////////////////////////

static bool IsDigitInBase (char c, int base)
{
    int  value = -1;



    if (isdigit ((unsigned char) c))
    {
        value = c - '0';
    }
    else if (isalpha ((unsigned char) c))
    {
        value = tolower ((unsigned char) c) - 'a' + 10;
    }

    return (value >= 0 && value < base);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ScanDigits
//
//  Consume the run of base-`base` digits at m_pos and build a Number
//  token from it. An empty run yields an Error token carrying
//  `emptyError`, which is what every radix prefix here wants: a lone '$'
//  or '@' with nothing after it is a malformed literal, not a zero.
//
////////////////////////////////////////////////////////////////////////////////

ExpressionEvaluator::Token ExpressionEvaluator::Tokenizer::ScanDigits (int base, const char * emptyError)
{
    size_t  start = m_pos;
    Token   token = { TokType::Error, 0, emptyError };



    while (m_pos < m_text.size() && IsDigitInBase (m_text[m_pos], base))
        m_pos++;

    if (m_pos > start)
    {
        token = { TokType::Number,
                  (int32_t) strtoul (m_text.substr (start, m_pos - start).c_str(), nullptr, base),
                  "" };
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadHexNumber — after consuming '$'
//
////////////////////////////////////////////////////////////////////////////////

ExpressionEvaluator::Token ExpressionEvaluator::Tokenizer::ReadHexNumber()
{
    return ScanDigits (16, "Expected hex digit after $");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadBinaryNumber — after consuming '%'
//
////////////////////////////////////////////////////////////////////////////////

ExpressionEvaluator::Token ExpressionEvaluator::Tokenizer::ReadBinaryNumber()
{
    return ScanDigits (2, "Expected binary digit after %");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadOctalNumber — after consuming '@'
//
//  Was inlined into ReadNext while its hex and binary siblings each had
//  a function; it is one now so the three radix prefixes read alike.
//
////////////////////////////////////////////////////////////////////////////////

ExpressionEvaluator::Token ExpressionEvaluator::Tokenizer::ReadOctalNumber()
{
    return ScanDigits (8, "Expected octal digit after @");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadCharConstant
//
//  Reads a delimited character constant. The opening delimiter has already
//  been consumed by the caller, which also says which character closes it and
//  whether this spelling carries the high bit.
//
//  `setsHighBit` is what makes the second delimiter worth having rather than a
//  synonym. A dialect that offers two spellings offers them because they mean
//  different bytes -- the apostrophe form is the plain character and the other
//  is the same character in high ASCII, which is the encoding Apple II text is
//  stored in.
//
//  An UNKNOWN escape yields the literal character rather than an error, so
//  `'\q'` is `q`. That is the period assembler behavior, and rejecting it
//  would break sources that rely on it to escape characters no formal list
//  covers.
//
//  The token starts as the error case and is overwritten only on success,
//  which lets running off the end and a missing closing quote share one
//  complaint -- they are the same mistake from the author's point of view.
//
//  The result is cast through unsigned char so a high-bit character (common in
//  Apple II sources, where inverse and flashing text set bit 7) produces
//  128..255 rather than a negative value.
//
////////////////////////////////////////////////////////////////////////////////

ExpressionEvaluator::Token ExpressionEvaluator::Tokenizer::ReadCharConstant (char delimiter, bool setsHighBit)
{
    constexpr int32_t  kHighBit = 0x80;
    char               ch       = 0;
    char               esc      = 0;
    int32_t            value    = 0;



    Token  token = { TokType::Error, 0, "Unterminated character constant" };
    ch = '\0';
    esc = '\0';



    // Running off the end and missing the closing quote are the same
    // complaint, so the error token above serves both and only the
    // success path overwrites it.
    if (m_pos < m_text.size())
    {
        ch = m_text[m_pos++];

        if (ch == '\\' && m_pos < m_text.size())
        {
            esc = m_text[m_pos++];

            switch (esc)
            {
            case 'n':  ch = '\n'; break;
            case 'r':  ch = '\r'; break;
            case 't':  ch = '\t'; break;
            case '0':  ch = '\0'; break;
            case '\\': ch = '\\'; break;
            case '\'': ch = '\''; break;
            default:   ch = esc;  break;   // unknown escape is the literal char
            }
        }

        if (m_pos < m_text.size() && m_text[m_pos] == delimiter)
        {
            m_pos++;
            value = (int32_t) (unsigned char) ch;

            if (setsHighBit)
            {
                value |= kHighBit;
            }

            token = { TokType::Number, value, "" };
        }
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadDecimalNumber
//
//  Reads a number that starts with a digit -- which, given how many notations
//  the assembler accepts, is three different syntaxes:
//
//    0x / 0b  C-style radix prefixes
//    base#    an explicit radix, e.g. 16#FF
//    plain    decimal
//
//  They are tried in that order because each is a prefix of the next: `0x1F`
//  begins like the decimal `0`, and `16#FF` begins like the decimal `16`. Only
//  after ruling out the specific forms can the digits be read as decimal.
//
//  A radix outside 2..36 is not treated as an error but as NOT A RADIX AT ALL:
//  the position is left on the `#` and the leading digits read as plain
//  decimal. That keeps a stray `#` -- which is also the immediate-addressing
//  sigil -- from turning a valid line into a diagnostic.
//
//  The base# value scan deliberately consumes every ALPHANUMERIC rather than
//  only the digits legal in that base, unlike ScanDigits. strtoul then stops
//  at the first illegal character, so `2#9` reads as 0 instead of erroring.
//  That is long-standing behavior and sources depend on it.
//
//  The 0b form additionally requires a binary digit in the lookahead, so `0b`
//  followed by anything else falls through to decimal rather than erroring --
//  which is what lets a symbol named `0b`-something still parse.
//
////////////////////////////////////////////////////////////////////////////////

ExpressionEvaluator::Token ExpressionEvaluator::Tokenizer::ReadDecimalNumber()
{
    size_t  start    = m_pos;
    size_t  valStart = 0;
    Token   token;
    bool    complete = false;
    char    next     = '\0';
    int     base     = 0;



    // C-style 0x and 0b radix prefixes.
    if (m_text[m_pos] == '0' && m_pos + 1 < m_text.size())
    {
        next = (char) tolower ((unsigned char) m_text[m_pos + 1]);

        if (next == 'x')
        {
            m_pos   += 2;
            token    = ScanDigits (16, "Expected hex digit after 0x");
            complete = true;
        }
        else if (next == 'b' && m_pos + 2 < m_text.size() && (m_text[m_pos + 2] == '0' || m_text[m_pos + 2] == '1'))
        {
            // The lookahead already proved a binary digit follows, so
            // ScanDigits cannot take its empty-run path here.
            m_pos   += 2;
            token    = ScanDigits (2, "Expected binary digit after 0b");
            complete = true;
        }
    }

    if (!complete)
    {
        while (m_pos < m_text.size() && isdigit ((unsigned char) m_text[m_pos]))
            m_pos++;

        // The base#value form, e.g. 16#FF.
        if (m_pos < m_text.size() && m_text[m_pos] == '#')
        {
            base = (int) strtol (m_text.substr (start, m_pos - start).c_str(), nullptr, 10);

            // A radix outside 2..36 is not a radix at all: leave m_pos on the
            // '#' and let the digits before it read as plain decimal.
            if (base >= 2 && base <= 36)
            {
                m_pos++;
                valStart = m_pos;

                // Deliberately consumes every alphanumeric rather than only
                // the digits legal in `base`, unlike ScanDigits. strtoul then
                // stops at the first illegal one, so "2#9" reads as 0 instead
                // of erroring. Long-standing behavior; sources depend on it.
                while (m_pos < m_text.size() && isalnum ((unsigned char) m_text[m_pos]))
                    m_pos++;

                if (m_pos == valStart)
                {
                    token = { TokType::Error, 0, "Expected value after base#" };
                }
                else
                {
                    token = { TokType::Number,
                              (int32_t) strtoul (m_text.substr (valStart, m_pos - valStart).c_str(), nullptr, base),
                              "" };
                }

                complete = true;
            }
        }
    }

    if (!complete)
    {
        token = { TokType::Number,
                  (int32_t) strtol (m_text.substr (start, m_pos - start).c_str(), nullptr, 10),
                  "" };
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadIdentifier
//
////////////////////////////////////////////////////////////////////////////////

ExpressionEvaluator::Token ExpressionEvaluator::Tokenizer::ReadIdentifier()
{
    size_t       start = m_pos;
    std::string  name;



    while (m_pos < m_text.size() && (isalnum ((unsigned char) m_text[m_pos]) || m_text[m_pos] == '_' || m_text[m_pos] == '.'))
        m_pos++;

    name = m_text.substr (start, m_pos - start);
    return { TokType::Ident, 0, name };
}





////////////////////////////////////////////////////////////////////////////////
//
//  Punctuation operator tables
//
//  Two-character operators are tried first so the lexer is longest-match:
//  "<=" must not read as "<" then "=". Everything else falls back to the
//  one-character table. A lead character that appears in both tables is
//  the normal case -- '<' is LShift/Le/Ne when paired and Lt when bare.
//
//  '=' is deliberately in both with the same token: this assembler accepts
//  a single '=' as equality, so "a = b" and "a == b" mean the same thing.
//
//  '%' and '$' are absent because they are radix prefixes as often as they
//  are operators, and ReadNext resolves that ambiguity before it gets here.
//
////////////////////////////////////////////////////////////////////////////////

const ExpressionEvaluator::Digraph  ExpressionEvaluator::s_kDigraphs[11] =
{
    { '+', '+', TokType::PlusPlus   },
    { '-', '-', TokType::MinusMinus },
    { '&', '&', TokType::AmpAmp     },
    { '|', '|', TokType::PipePipe   },
    { '!', '=', TokType::Ne         },
    { '<', '<', TokType::LShift     },
    { '<', '=', TokType::Le         },
    { '<', '>', TokType::Ne         },   // Pascal-style inequality
    { '>', '>', TokType::RShift     },
    { '>', '=', TokType::Ge         },
    { '=', '=', TokType::Eq         },
};

const ExpressionEvaluator::Monograph  ExpressionEvaluator::s_kMonographs[16] =
{
    { '+', TokType::Plus     },
    { '-', TokType::Minus    },
    { '*', TokType::Star     },
    { '/', TokType::Slash    },
    { '&', TokType::Amp      },
    { '|', TokType::Pipe     },
    { '^', TokType::Caret    },
    { '~', TokType::Tilde    },
    { '!', TokType::Bang     },
    { '(', TokType::LParen   },
    { ')', TokType::RParen   },
    { '[', TokType::LBracket },
    { ']', TokType::RBracket },
    { '<', TokType::Lt       },
    { '>', TokType::Gt       },
    { '=', TokType::Eq       },
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExpressionEvaluator::FindDigraph
//
//  The two-character operator starting with `lead` and continuing with
//  `follow`, or null. Callers pass '\0' for `follow` at end of input,
//  which no row can match.
//
////////////////////////////////////////////////////////////////////////////////

const ExpressionEvaluator::Digraph * ExpressionEvaluator::FindDigraph (char lead, char follow)
{
    const Digraph *  found = nullptr;



    for (const Digraph & d : s_kDigraphs)
    {
        if (d.lead == lead && d.follow == follow && found == nullptr)
        {
            found = &d;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExpressionEvaluator::FindMonograph
//
//  The one-character operator `lead`, or null when it is not punctuation
//  this expression syntax knows. Only consulted after FindDigraph misses.
//
////////////////////////////////////////////////////////////////////////////////

const ExpressionEvaluator::Monograph * ExpressionEvaluator::FindMonograph (char lead)
{
    const Monograph *  found = nullptr;



    for (const Monograph & m : s_kMonographs)
    {
        if (m.lead == lead && found == nullptr)
        {
            found = &m;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadOperator — punctuation, with m_pos on the lead character
//
//  Consumes one or two characters and yields the matching token, or an
//  Error token naming the character when it is not punctuation we know.
//
////////////////////////////////////////////////////////////////////////////////

ExpressionEvaluator::Token ExpressionEvaluator::Tokenizer::ReadOperator()
{
    char               next = 0;
    const Digraph    * two  = nullptr;
    const Monograph  * one  = nullptr;



    char               lead  = m_text[m_pos++];
    next = (m_pos < m_text.size()) ? m_text[m_pos] : '\0';
    two = FindDigraph (lead, next);
    one = (two == nullptr) ? FindMonograph (lead) : nullptr;
    Token              token = { TokType::Error, 0, std::string ("Unexpected character: ") + lead };

    if (two != nullptr)
    {
        m_pos++;                       // consume the second character
        token = { two->type, 0, "" };
    }
    else if (one != nullptr)
    {
        token = { one->type, 0, "" };
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadNext — main tokenizer dispatch
//
//  Classifies by the first character: quote, radix prefix, digit,
//  identifier start, or punctuation. Each arm leaves m_pos just past the
//  token it produced.
//
////////////////////////////////////////////////////////////////////////////////

ExpressionEvaluator::Token ExpressionEvaluator::Tokenizer::ReadNext()
{
    char  c = 0;



    Token  token = { TokType::End, 0, "" };
    c = '\0';



    SkipSpaces();

    if (m_pos < m_text.size())
    {
        c = m_text[m_pos];

        if (c == '\'')
        {
            m_pos++;
            token = ReadCharConstant ('\'', false);
        }
        else if ((m_highAsciiCharDelimiter != 0) && (c == m_highAsciiCharDelimiter))
        {
            // The dialect's high-ASCII spelling of the same thing. Absent
            // unless a profile named a delimiter, so a dialect without one
            // still gets "unexpected token" here rather than a second syntax it
            // does not have.
            m_pos++;
            token = ReadCharConstant (m_highAsciiCharDelimiter, true);
        }
        else if (c == '$')
        {
            m_pos++;

            // A bare '$' is the current PC. It rides TokType::Star because
            // TryParsePrimary already resolves Star to ctx.currentPC.
            if (m_pos < m_text.size() && isxdigit ((unsigned char) m_text[m_pos]))
            {
                token = ReadHexNumber();
            }
            else
            {
                token = { TokType::Star, 0, "" };
            }
        }
        else if (c == '%')
        {
            // '%' is modulo after a value and a binary prefix before one, so
            // "a%10" is a remainder while "lda #%1010" is a literal. The
            // digit lookahead keeps a trailing '%' from swallowing nothing.
            m_pos++;

            if (!m_lastWasValue && m_pos < m_text.size() && (m_text[m_pos] == '0' || m_text[m_pos] == '1'))
            {
                token = ReadBinaryNumber();
            }
            else
            {
                token = { TokType::Percent, 0, "" };
            }
        }
        else if (c == '@')
        {
            m_pos++;
            token = ReadOctalNumber();
        }
        else if (isdigit ((unsigned char) c))
        {
            token = ReadDecimalNumber();
        }
        else if (isalpha ((unsigned char) c) || c == '_')
        {
            token = ReadIdentifier();
        }
        else
        {
            token = ReadOperator();
        }
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Forward declarations for recursive descent
//
////////////////////////////////////////////////////////////////////////////////


std::string ExpressionEvaluator::ToUpperIdent (const std::string & s)
{
    std::string r = s;



    for (char & c : r)
        c = (char) toupper ((unsigned char) c);

    return r;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryParsePrimary
//
//  The bottom of the recursive-descent parser: a number, a symbol, the program
//  counter, or a parenthesized subexpression.
//
//  `*` is the program counter here, not multiplication. The tokenizer already
//  resolved that ambiguity from context (see Tokenizer's m_lastWasValue), so
//  by the time a Star token reaches primary position its meaning is settled.
//
//  Brackets group exactly like parentheses but do NOT pair with them -- `(1]`
//  is an error. Both forms exist because `(` is also the indirect-addressing
//  sigil in 6502 syntax, so `[` gives an author an unambiguous grouping
//  character; letting them cross-pair would hide a real typo.
//
//  A missing symbol table is treated the same as an empty one: either way the
//  name does not resolve, and the caller distinguishes a forward reference
//  from a genuine error by whether the symbol turns up in a later pass, not by
//  which of those two shapes it hit here.
//
//  Peek-then-Next rather than Next-then-classify, so the token stays in the
//  stream for the branch that actually consumes it and no path has to push one
//  back.
//
////////////////////////////////////////////////////////////////////////////////

bool ExpressionEvaluator::TryParsePrimary (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error)
{
    Token    t         = tok.Peek();
    bool     ok        = false;
    bool     isBracket = false;
    TokType  closer    = TokType::RParen;



    if (t.type == TokType::Number)
    {
        tok.Next();
        result = t.numVal;
        ok     = true;
    }
    else if (t.type == TokType::Ident)
    {
        tok.Next();

        if (ctx.symbols != nullptr)
        {
            auto  it = ctx.symbols->find (t.strVal);

            if (it != ctx.symbols->end())
            {
                result = it->second;
                ok     = true;
            }
        }

        // A missing symbol table is indistinguishable from an empty one:
        // either way the name does not resolve.
        if (!ok)
        {
            error = "Undefined symbol: " + t.strVal;
        }
    }
    else if (t.type == TokType::Star)
    {
        tok.Next();
        result = ctx.currentPC;
        ok     = true;
    }
    else if (t.type == TokType::LParen || t.type == TokType::LBracket)
    {
        // Parentheses and brackets group identically, but they do not pair
        // with each other -- "(1]" is an error, not a group.
        isBracket = (t.type == TokType::LBracket);
        closer    = isBracket ? TokType::RBracket : TokType::RParen;

        tok.Next();

        ok = TryParseBinary (tok, ctx, s_kLoosestBinaryLevel, result, error);

        if (ok && tok.Next().type != closer)
        {
            error = isBracket ? "Expected closing bracket" : "Expected closing parenthesis";
            ok    = false;
        }
    }
    else if (t.type == TokType::End)
    {
        error = "Unexpected end of expression";
    }
    else
    {
        error = "Unexpected token in expression";
    }

    return ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Prefix operator table
//
//  Eight operators that were eight copies of the same four lines: consume
//  the token, recurse, fold the result. Only the fold differed.
//
////////////////////////////////////////////////////////////////////////////////

int32_t ExpressionEvaluator::ApplyNegate    (int32_t v) { return -v; }
int32_t ExpressionEvaluator::ApplyIdentity  (int32_t v) { return v; }
int32_t ExpressionEvaluator::ApplyBitNot    (int32_t v) { return ~v; }
int32_t ExpressionEvaluator::ApplyLogNot    (int32_t v) { return (v == 0) ? 1 : 0; }
int32_t ExpressionEvaluator::ApplyIncrement (int32_t v) { return v + 1; }
int32_t ExpressionEvaluator::ApplyDecrement (int32_t v) { return v - 1; }
int32_t ExpressionEvaluator::ApplyLowByte   (int32_t v) { return v & 0xFF; }
int32_t ExpressionEvaluator::ApplyHighByte  (int32_t v) { return (v >> 8) & 0xFF; }

const ExpressionEvaluator::UnaryOp  ExpressionEvaluator::s_kUnaryOps[8] =
{
    { TokType::Minus,       ApplyNegate    },
    { TokType::Plus,        ApplyIdentity  },
    { TokType::Tilde,       ApplyBitNot    },
    { TokType::Bang,        ApplyLogNot    },
    { TokType::PlusPlus,    ApplyIncrement },
    { TokType::MinusMinus,  ApplyDecrement },
    { TokType::Lt,          ApplyLowByte   },   // < selects the low byte
    { TokType::Gt,          ApplyHighByte  },   // > selects the high byte
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExpressionEvaluator::FindUnaryOp
//
//  Null when the token does not introduce a prefix operator, which is how
//  TryParseUnary decides to hand off to TryParsePrimary instead.
//
////////////////////////////////////////////////////////////////////////////////

const ExpressionEvaluator::UnaryOp * ExpressionEvaluator::FindUnaryOp (TokType token)
{
    const UnaryOp *  found = nullptr;



    for (const UnaryOp & op : s_kUnaryOps)
    {
        if (op.token == token && found == nullptr)
        {
            found = &op;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryParseUnary — -, +, ~, !, <, >, ++, --, lo, hi
//
//  Right-associative by construction: each operator recurses into
//  TryParseUnary before folding, so "--5" is -(-5) and "<>addr" is the low
//  byte of the high byte.
//
////////////////////////////////////////////////////////////////////////////////

bool ExpressionEvaluator::TryParseUnary (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error)
{
    Token             t     = tok.Peek();
    const UnaryOp *   op    = FindUnaryOp (t.type);
    bool              ok    = false;
    std::string       upper;



    // "lo" and "hi" are word spellings of the < and > byte selectors, so
    // they reuse those rows rather than carrying duplicate folds.
    if (op == nullptr && t.type == TokType::Ident)
    {
        upper = ToUpperIdent (t.strVal);

        if (upper == "LO")
        {
            op = FindUnaryOp (TokType::Lt);
        }
        else if (upper == "HI")
        {
            op = FindUnaryOp (TokType::Gt);
        }
    }

    if (op == nullptr)
    {
        ok = TryParsePrimary (tok, ctx, result, error);
    }
    else
    {
        tok.Next();
        ok = TryParseUnary (tok, ctx, result, error);

        if (ok)
        {
            result = op->Apply (result);
        }
    }

    return ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Binary operator table
//
//  Levels run loosest (1) to tightest (9), matching the order the old
//  ParseLogOr -> ParseMulDiv chain called through. Unary sits above level 9
//  and is still a separate function, because it is prefix rather than infix.
//
//  Every operator here is left-associative, which TryParseBinary gets by
//  recursing at level + 1.
//
////////////////////////////////////////////////////////////////////////////////

bool ExpressionEvaluator::TryApplyLogOr  (int32_t a, int32_t b, int32_t & o, std::string &) { o = (a != 0 || b != 0) ? 1 : 0; return true; }
bool ExpressionEvaluator::TryApplyLogAnd (int32_t a, int32_t b, int32_t & o, std::string &) { o = (a != 0 && b != 0) ? 1 : 0; return true; }
bool ExpressionEvaluator::TryApplyBitOr  (int32_t a, int32_t b, int32_t & o, std::string &) { o = a | b;  return true; }
bool ExpressionEvaluator::TryApplyBitXor (int32_t a, int32_t b, int32_t & o, std::string &) { o = a ^ b;  return true; }
bool ExpressionEvaluator::TryApplyBitAnd (int32_t a, int32_t b, int32_t & o, std::string &) { o = a & b;  return true; }
bool ExpressionEvaluator::TryApplyEq     (int32_t a, int32_t b, int32_t & o, std::string &) { o = (a == b) ? 1 : 0; return true; }
bool ExpressionEvaluator::TryApplyNe     (int32_t a, int32_t b, int32_t & o, std::string &) { o = (a != b) ? 1 : 0; return true; }
bool ExpressionEvaluator::TryApplyLt     (int32_t a, int32_t b, int32_t & o, std::string &) { o = (a <  b) ? 1 : 0; return true; }
bool ExpressionEvaluator::TryApplyGt     (int32_t a, int32_t b, int32_t & o, std::string &) { o = (a >  b) ? 1 : 0; return true; }
bool ExpressionEvaluator::TryApplyLe     (int32_t a, int32_t b, int32_t & o, std::string &) { o = (a <= b) ? 1 : 0; return true; }
bool ExpressionEvaluator::TryApplyGe     (int32_t a, int32_t b, int32_t & o, std::string &) { o = (a >= b) ? 1 : 0; return true; }
bool ExpressionEvaluator::TryApplyShl    (int32_t a, int32_t b, int32_t & o, std::string &) { o = a << b; return true; }
bool ExpressionEvaluator::TryApplyAdd    (int32_t a, int32_t b, int32_t & o, std::string &) { o = a + b;  return true; }
bool ExpressionEvaluator::TryApplySub    (int32_t a, int32_t b, int32_t & o, std::string &) { o = a - b;  return true; }
bool ExpressionEvaluator::TryApplyMul    (int32_t a, int32_t b, int32_t & o, std::string &) { o = a * b;  return true; }





////////////////////////////////////////////////////////////////////////////////
//
//  ExpressionEvaluator::TryApplyShr
//
//  Logical shift right, matching the original cast through uint32_t: a
//  negative left operand shifts in zeros rather than sign bits.
//
////////////////////////////////////////////////////////////////////////////////

bool ExpressionEvaluator::TryApplyShr (int32_t a, int32_t b, int32_t & o, std::string &)
{
    o = (int32_t) ((uint32_t) a >> b);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExpressionEvaluator::TryApplyDiv
//
//  One of only two operators that can fail, and the reason BinaryOp::Apply
//  returns bool at all.
//
////////////////////////////////////////////////////////////////////////////////

bool ExpressionEvaluator::TryApplyDiv (int32_t a, int32_t b, int32_t & o, std::string & error)
{
    bool  ok = (b != 0);



    if (ok) { o = a / b; }
    else    { error = "Division by zero"; }

    return ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExpressionEvaluator::TryApplyMod
//
//  Shares TryApplyDiv's zero-divisor rule and its diagnostic wording.
//
////////////////////////////////////////////////////////////////////////////////

bool ExpressionEvaluator::TryApplyMod (int32_t a, int32_t b, int32_t & o, std::string & error)
{
    bool  ok = (b != 0);



    if (ok) { o = a % b; }
    else    { error = "Division by zero"; }

    return ok;
}


const ExpressionEvaluator::BinaryOp  ExpressionEvaluator::s_kBinaryOps[18] =
{
    { TokType::PipePipe, 1, TryApplyLogOr  },

    { TokType::AmpAmp,   2, TryApplyLogAnd },

    { TokType::Pipe,     3, TryApplyBitOr  },

    { TokType::Caret,    4, TryApplyBitXor },

    { TokType::Amp,      5, TryApplyBitAnd },

    { TokType::Eq,       6, TryApplyEq     },
    { TokType::Ne,       6, TryApplyNe     },

    { TokType::Lt,       7, TryApplyLt     },
    { TokType::Gt,       7, TryApplyGt     },
    { TokType::Le,       7, TryApplyLe     },
    { TokType::Ge,       7, TryApplyGe     },

    { TokType::LShift,   8, TryApplyShl    },
    { TokType::RShift,   8, TryApplyShr    },

    { TokType::Plus,     9, TryApplyAdd    },
    { TokType::Minus,    9, TryApplySub    },

    { TokType::Star,    10, TryApplyMul    },
    { TokType::Slash,   10, TryApplyDiv    },
    { TokType::Percent, 10, TryApplyMod    },
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExpressionEvaluator::FindBinaryOp
//
//  Null when the token is not a binary operator, which is also how TryParseBinary
//  detects the end of an expression.
//
////////////////////////////////////////////////////////////////////////////////

const ExpressionEvaluator::BinaryOp * ExpressionEvaluator::FindBinaryOp (TokType token)
{
    const BinaryOp *  found = nullptr;



    for (const BinaryOp & op : s_kBinaryOps)
    {
        if (op.token == token && found == nullptr)
        {
            found = &op;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExpressionEvaluator::TryParseBinary
//
//  Precedence climbing over s_kBinaryOps. Absorbs every operator binding at
//  `minLevel` or tighter, recursing at level + 1 for the right operand so the
//  fold stays left-associative.
//
//  A dialect binding LEFT TO RIGHT flattens every operator to the loosest level
//  instead of reading its row. That is the whole of the difference: with one
//  level, the recursion for the right operand can absorb nothing, so the fold
//  runs strictly left to right and parentheses become the only way to group.
//  The operator set, the folds, and the diagnostics are untouched.
//
////////////////////////////////////////////////////////////////////////////////

bool ExpressionEvaluator::TryParseBinary (
    Tokenizer          &  tok,
    const ExprContext  &  ctx,
    int                   minLevel,
    int32_t            &  result,
    std::string        &  error)
{
    const BinaryOp *  op    = nullptr;
    int32_t           right = 0;
    int               level = 0;
    bool              flat  = (ctx.binding == OperatorBinding::LeftToRight);
    bool              ok    = false;


    ok = TryParseUnary (tok, ctx, result, error);

    for (;;)
    {
        op = ok ? FindBinaryOp (tok.Peek().type) : nullptr;

        if (op == nullptr)
        {
            break;
        }

        level = flat ? s_kLoosestBinaryLevel : op->level;

        if (level < minLevel)
        {
            break;
        }

        tok.Next();

        right = 0;
        ok    = TryParseBinary (tok, ctx, level + 1, right, error);

        if (ok)
        {
            ok = op->Apply (result, right, result, error);
        }
    }

    return ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExpressionEvaluator::Evaluate
//
//  The only public entry point. Trims, parses, and insists the whole
//  string was consumed. `hasUnresolved` is set when the failure was an
//  undefined symbol, which the assembler treats as "retry on pass two"
//  rather than as a hard error.
//
////////////////////////////////////////////////////////////////////////////////

ExprResult ExpressionEvaluator::Evaluate (const std::string & expr, const ExprContext & ctx)
{
    ExprResult   res     = {};
    std::string  trimmed = expr;
    std::string  error;
    int32_t      value   = 0;
    size_t       end     = 0;
    size_t       start   = trimmed.find_first_not_of (" \t");



    res.success       = false;
    res.value         = 0;
    res.hasUnresolved = false;

    if (start == std::string::npos)
    {
        res.error = "Empty expression";
    }
    else
    {
        end     = trimmed.find_last_not_of (" \t");
        trimmed = trimmed.substr (start, end - start + 1);

        Tokenizer  tok (trimmed, ctx.highAsciiCharDelimiter);

        if (!TryParseBinary (tok, ctx, s_kLoosestBinaryLevel, value, error))
        {
            res.error         = error;
            res.hasUnresolved = (error.find ("Undefined symbol") == 0);
        }
        else if (tok.Peek().type != TokType::End)
        {
            // Parsed something valid but stopped early, e.g. "1 2" or "1)".
            res.error = "Unexpected content after expression";
        }
        else
        {
            res.success = true;
            res.value   = value;
        }
    }

    return res;
}
