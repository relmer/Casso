#include "Pch.h"

#include "IntegerBasicTokenizer.h"
#include "IntegerBasicDetokenizer.h"



//
//  Integer BASIC has a token for each syntactic position, so most words and
//  marks have more than one.
//
static constexpr Byte  s_kEndOfLine       = 0x01;
static constexpr Byte  s_kColon           = 0x03;
static constexpr Byte  s_kSubstringComma  = 0x23;
static constexpr Byte  s_kThenNumber      = 0x24;
static constexpr Byte  s_kThenStatement   = 0x25;
static constexpr Byte  s_kInputString     = 0x26;
static constexpr Byte  s_kInputNumber     = 0x27;
static constexpr Byte  s_kQuoteOpen       = 0x28;
static constexpr Byte  s_kQuoteClose      = 0x29;
static constexpr Byte  s_kSubstringOpen   = 0x2A;
static constexpr Byte  s_kSubscriptOpen   = 0x2D;
static constexpr Byte  s_kDimNumberOpen   = 0x34;
static constexpr Byte  s_kUnaryPlus       = 0x35;
static constexpr Byte  s_kUnaryMinus      = 0x36;
static constexpr Byte  s_kNot             = 0x37;
static constexpr Byte  s_kGroupOpen       = 0x38;
static constexpr Byte  s_kStringEqual     = 0x39;
static constexpr Byte  s_kStringNotEqual  = 0x3A;
static constexpr Byte  s_kLen             = 0x3B;
static constexpr Byte  s_kAsc             = 0x3C;
static constexpr Byte  s_kScrn            = 0x3D;
static constexpr Byte  s_kScrnComma       = 0x3E;
static constexpr Byte  s_kCallOpen        = 0x3F;
static constexpr Byte  s_kDollar          = 0x40;
static constexpr Byte  s_kDimStringOpen   = 0x22;
static constexpr Byte  s_kTargetOpen      = 0x42;
static constexpr Byte  s_kDimCommaString  = 0x43;
static constexpr Byte  s_kDimCommaNumber  = 0x44;
static constexpr Byte  s_kSemiString      = 0x45;
static constexpr Byte  s_kSemiNumber      = 0x46;
static constexpr Byte  s_kSemiAlone       = 0x47;
static constexpr Byte  s_kCommaString     = 0x48;
static constexpr Byte  s_kCommaNumber     = 0x49;
static constexpr Byte  s_kCommaAlone      = 0x4A;
static constexpr Byte  s_kDimString       = 0x4E;
static constexpr Byte  s_kDimNumber       = 0x4F;
static constexpr Byte  s_kInputTarget     = 0x52;
static constexpr Byte  s_kInputPrompt     = 0x53;
static constexpr Byte  s_kInputVariable   = 0x54;
static constexpr Byte  s_kFor             = 0x55;
static constexpr Byte  s_kForEqual        = 0x56;
static constexpr Byte  s_kTo              = 0x57;
static constexpr Byte  s_kStep            = 0x58;
static constexpr Byte  s_kNext            = 0x59;
static constexpr Byte  s_kNextComma       = 0x5A;
static constexpr Byte  s_kRem             = 0x5D;
static constexpr Byte  s_kLet             = 0x5E;
static constexpr Byte  s_kIf              = 0x60;
static constexpr Byte  s_kPrintString     = 0x61;
static constexpr Byte  s_kPrintNumber     = 0x62;
static constexpr Byte  s_kPrintAlone      = 0x63;
static constexpr Byte  s_kStringAssign    = 0x70;
static constexpr Byte  s_kNumberAssign    = 0x71;
static constexpr Byte  s_kClose           = 0x72;
static constexpr Byte  s_kListNumber      = 0x74;
static constexpr Byte  s_kListComma       = 0x75;
static constexpr Byte  s_kListAlone       = 0x76;
static constexpr Byte  s_kHighBit         = 0x80;

static constexpr uint32_t  s_kRadix           = 10;
static constexpr size_t    s_kMaxProgramBytes = 0xB800;





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::Save
//
////////////////////////////////////////////////////////////////////////////////

IntegerBasicTokenizer::Mark IntegerBasicTokenizer::Save (const Scan & scan)
{
    Mark  mark;



    mark.at   = scan.at;
    mark.size = scan.out.size();

    return mark;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::Restore
//
////////////////////////////////////////////////////////////////////////////////

void IntegerBasicTokenizer::Restore (Scan & scan, Mark mark)
{
    scan.at = mark.at;
    scan.out.resize (mark.size);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::Peek
//
//  The next character that is not a space, upper-cased, or zero at the end of
//  the line. The spaces are consumed.
//
////////////////////////////////////////////////////////////////////////////////

char IntegerBasicTokenizer::Peek (Scan & scan)
{
    char  c = 0;



    while (scan.at < scan.text.size() && scan.text[scan.at] == ' ')
    {
        scan.at++;
    }

    scan.furthest = (std::max) (scan.furthest, scan.at);

    if (scan.at >= scan.text.size())
    {
        return 0;
    }

    c = scan.text[scan.at];

    if (c >= 'a' && c <= 'z')
    {
        c = (char) (c - 'a' + 'A');
    }

    //  A NUL in the text is not the end of the line.
    return c == 0 ? (char) 0x7F : c;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::Match
//
//  Spaces are skipped ahead of each character of the word, as the ROM does.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::Match (Scan & scan, const char * word)
{
    size_t  start = scan.at;
    size_t  k     = 0;



    for (k = 0; word[k] != '\0'; k++)
    {
        if (Peek (scan) != word[k])
        {
            scan.at = start;

            return false;
        }

        scan.at++;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::MatchToken
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::MatchToken (Scan & scan, const char * word, Byte token)
{
    bool  matched = Match (scan, word);



    if (matched)
    {
        scan.out.push_back (token);
    }

    return matched;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::IsLetter
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::IsLetter (char c)
{
    return c >= 'A' && c <= 'Z';
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::IsDigit
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::IsDigit (char c)
{
    return c >= '0' && c <= '9';
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::IsAtNameStop
//
//  Whether one of the words that end a variable begins here.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::IsAtNameStop (Scan & scan)
{
    size_t  start = scan.at;
    static constexpr const char *  s_kStops[] = { "AND", "OR", "MOD", "TO", "STEP", "THEN", "AT" };



    for (const char * stop : s_kStops)
    {
        if (Match (scan, stop))
        {
            scan.at = start;

            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseName
//
//  A letter, then letters and digits, each stored with the high bit.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseName (Scan & scan)
{
    char  c = Peek (scan);



    if (!IsLetter (c))
    {
        return false;
    }

    while (IsLetter (c) || IsDigit (c))
    {
        scan.out.push_back ((Byte) (c | s_kHighBit));
        scan.at++;

        c = Peek (scan);

        if (IsLetter (c) && IsAtNameStop (scan))
        {
            break;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseNumber
//
//  The first digit as typed, with the high bit, then the value.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseNumber (Scan & scan)
{
    Mark      mark  = Save (scan);
    char      c     = Peek (scan);
    char      first = c;
    uint32_t  value = 0;



    if (!IsDigit (c))
    {
        return false;
    }

    while (IsDigit (c))
    {
        if (value <= kMaxConstant)
        {
            value = value * s_kRadix + (uint32_t) (c - '0');
        }

        scan.at++;

        c = Peek (scan);
    }

    if (value > kMaxConstant)
    {
        Restore (scan, mark);

        return false;
    }

    scan.out.push_back ((Byte) (first | s_kHighBit));
    scan.out.push_back ((Byte) (value & 0xFF));
    scan.out.push_back ((Byte) (value >> 8));

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseString
//
//  A quoted literal. There is no way to write a quote inside one.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseString (Scan & scan)
{
    Mark  mark   = Save (scan);
    bool  closed = false;



    if (Peek (scan) != '"')
    {
        return false;
    }

    scan.out.push_back (s_kQuoteOpen);
    scan.at++;

    while (scan.at < scan.text.size() && !closed)
    {
        char  c = scan.text[scan.at];

        scan.at++;

        if (c == '"')
        {
            scan.out.push_back (s_kQuoteClose);
            closed = true;
        }
        else if (c <= 0)
        {
            scan.badCharacter = true;
            break;
        }
        else
        {
            scan.out.push_back ((Byte) (c | s_kHighBit));
        }
    }

    scan.furthest = (std::max) (scan.furthest, scan.at);

    if (!closed)
    {
        Restore (scan, mark);
    }

    return closed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseGroup
//
//  A parenthesized expression, opened with the token of its position.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseGroup (Scan & scan, Byte open)
{
    Mark  mark = Save (scan);



    if (MatchToken (scan, "(", open) && ParseExpression (scan) && MatchToken (scan, ")", s_kClose))
    {
        return true;
    }

    Restore (scan, mark);

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseVariable
//
//  A numeric variable with at most one subscript.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseVariable (Scan & scan)
{
    if (!ParseName (scan))
    {
        return false;
    }

    ParseGroup (scan, s_kSubscriptOpen);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseStringTarget
//
//  A string variable that is assigned or read into: one subscript at most.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseStringTarget (Scan & scan)
{
    Mark  mark = Save (scan);



    if (ParseName (scan) && MatchToken (scan, "$", s_kDollar))
    {
        ParseGroup (scan, s_kTargetOpen);

        return true;
    }

    Restore (scan, mark);

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseStringOperand
//
//  A literal, or a string variable with an optional substring range.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseStringOperand (Scan & scan)
{
    Mark  mark  = Save (scan);
    Mark  range;
    Mark  last;



    if (ParseString (scan))
    {
        return true;
    }

    if (!ParseName (scan) || !MatchToken (scan, "$", s_kDollar))
    {
        Restore (scan, mark);

        return false;
    }

    range = Save (scan);

    if (MatchToken (scan, "(", s_kSubstringOpen) && ParseExpression (scan))
    {
        last = Save (scan);

        if (!MatchToken (scan, ",", s_kSubstringComma) || !ParseExpression (scan))
        {
            Restore (scan, last);
        }

        if (MatchToken (scan, ")", s_kClose))
        {
            return true;
        }
    }

    Restore (scan, range);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseComparison
//
//  Two strings compared with = or #, which is a numeric operand.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseComparison (Scan & scan)
{
    Mark  mark     = Save (scan);
    bool  compared = false;



    if (ParseStringOperand (scan))
    {
        compared = MatchToken (scan, "=", s_kStringEqual) || MatchToken (scan, "#", s_kStringNotEqual);

        if (compared && ParseStringOperand (scan))
        {
            return true;
        }
    }

    Restore (scan, mark);

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseStringCall
//
//  LEN( and ASC(, each one token with its parenthesis.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseStringCall (Scan & scan, const char * word, Byte token)
{
    Mark  mark = Save (scan);



    if (MatchToken (scan, word, token) && ParseStringOperand (scan) && MatchToken (scan, ")", s_kClose))
    {
        return true;
    }

    Restore (scan, mark);

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseScreenCall
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseScreenCall (Scan & scan)
{
    Mark  mark = Save (scan);



    if (MatchToken (scan, "SCRN(", s_kScrn)     && ParseExpression (scan) &&
        MatchToken (scan, ",", s_kScrnComma)    && ParseExpression (scan) &&
        MatchToken (scan, ")", s_kClose))
    {
        return true;
    }

    Restore (scan, mark);

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseNumberCall
//
//  A function word that is not followed by a parenthesis is a variable.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseNumberCall (Scan & scan)
{
    Mark  mark = Save (scan);
    static constexpr WordToken  s_kCalls[] =
    {
        { "PEEK", 0x2E }, { "RND", 0x2F }, { "SGN", 0x30 }, { "ABS", 0x31 }, { "PDL", 0x32 },
    };



    for (const WordToken & call : s_kCalls)
    {
        if (MatchToken (scan, call.word, call.token) && ParseGroup (scan, s_kCallOpen))
        {
            return true;
        }

        Restore (scan, mark);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParsePrimary
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParsePrimary (Scan & scan)
{
    return ParseComparison (scan)                 ||
           ParseStringCall (scan, "LEN(", s_kLen) ||
           ParseStringCall (scan, "ASC(", s_kAsc) ||
           ParseScreenCall (scan)                 ||
           ParseNumberCall (scan)                 ||
           ParseGroup      (scan, s_kGroupOpen)   ||
           ParseNumber     (scan)                 ||
           ParseVariable   (scan);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseUnary
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseUnary (Scan & scan)
{
    Mark  mark = Save (scan);
    static constexpr WordToken  s_kUnary[] =
    {
        { "+", s_kUnaryPlus }, { "-", s_kUnaryMinus }, { "NOT", s_kNot },
    };



    for (const WordToken & unary : s_kUnary)
    {
        if (MatchToken (scan, unary.word, unary.token) && ParseOperand (scan))
        {
            return true;
        }

        Restore (scan, mark);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseOperand
//
//  The depth and the budget bound the work a hostile line can cost.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseOperand (Scan & scan)
{
    bool  parsed = false;



    if (scan.depth >= kMaxDepth || scan.budget == 0)
    {
        return false;
    }

    scan.depth++;
    scan.budget--;

    parsed = ParseUnary (scan) || ParsePrimary (scan);

    scan.depth--;

    return parsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseExpression
//
//  Operands joined by operators, stored in the order typed. The forms of two
//  characters come first.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseExpression (Scan & scan)
{
    Mark  mark;
    bool  more = true;
    static constexpr WordToken  s_kOperators[] =
    {
        { ">=",  0x18 }, { "<=", 0x1A }, { "<>",  0x1B }, { "+", 0x12 }, { "-", 0x13 },
        { "*",   0x14 }, { "/",  0x15 }, { "=",   0x16 }, { "#", 0x17 }, { ">", 0x19 },
        { "<",   0x1C }, { "AND", 0x1D }, { "OR", 0x1E }, { "MOD", 0x1F }, { "^", 0x20 },
    };



    if (!ParseOperand (scan))
    {
        return false;
    }

    while (more)
    {
        mark = Save (scan);
        more = false;

        for (const WordToken & op : s_kOperators)
        {
            if (MatchToken (scan, op.word, op.token))
            {
                more = true;
                break;
            }
        }

        if (more && !ParseOperand (scan))
        {
            Restore (scan, mark);

            more = false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseRemark
//
//  The rest of the line, verbatim. The one space the listing has after REM
//  is not stored.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseRemark (Scan & scan)
{
    if (!MatchToken (scan, "REM", s_kRem))
    {
        return false;
    }

    if (scan.at < scan.text.size() && scan.text[scan.at] == ' ')
    {
        scan.at++;
    }

    while (scan.at < scan.text.size())
    {
        char  c = scan.text[scan.at];

        if (c <= 0)
        {
            scan.badCharacter = true;
            scan.furthest     = (std::max) (scan.furthest, scan.at);

            return false;
        }

        scan.out.push_back ((Byte) (c | s_kHighBit));
        scan.at++;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseOneOperand
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseOneOperand (Scan & scan)
{
    Mark  mark = Save (scan);
    static constexpr WordToken  s_kWords[] =
    {
        { "CALL", 0x4D }, { "TAB",   0x50 }, { "VTAB", 0x6F }, { "COLOR=", 0x66 },
        { "GOTO", 0x5F }, { "GOSUB", 0x5C }, { "PR#",  0x7E }, { "IN#",    0x7F },
    };



    for (const WordToken & word : s_kWords)
    {
        if (MatchToken (scan, word.word, word.token) && ParseExpression (scan))
        {
            return true;
        }

        Restore (scan, mark);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseTwoOperands
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseTwoOperands (Scan & scan)
{
    Mark  mark = Save (scan);
    static constexpr WordToken  s_kWords[] = { { "POKE", 0x64 }, { "PLOT", 0x67 } };



    for (const WordToken & word : s_kWords)
    {
        if (MatchToken (scan, word.word, word.token)            && ParseExpression (scan) &&
            MatchToken (scan, ",", (Byte) (word.token + 1))     && ParseExpression (scan))
        {
            return true;
        }

        Restore (scan, mark);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseLine
//
//  HLIN and VLIN: two ends, then AT and the row or column.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseLine (Scan & scan, const char * word, Byte first, Byte second, Byte third)
{
    Mark  mark = Save (scan);



    if (MatchToken (scan, word, first)  && ParseExpression (scan) &&
        MatchToken (scan, ",", second)  && ParseExpression (scan) &&
        MatchToken (scan, "AT", third)  && ParseExpression (scan))
    {
        return true;
    }

    Restore (scan, mark);

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseFor
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseFor (Scan & scan)
{
    Mark  mark = Save (scan);
    Mark  step;



    if (MatchToken (scan, "FOR", s_kFor)    && ParseName (scan)       &&
        MatchToken (scan, "=", s_kForEqual) && ParseExpression (scan) &&
        MatchToken (scan, "TO", s_kTo)      && ParseExpression (scan))
    {
        step = Save (scan);

        if (!MatchToken (scan, "STEP", s_kStep) || !ParseExpression (scan))
        {
            Restore (scan, step);
        }

        return true;
    }

    Restore (scan, mark);

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseNext
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseNext (Scan & scan)
{
    Mark  mark = Save (scan);
    bool  more = true;



    if (!MatchToken (scan, "NEXT", s_kNext) || !ParseName (scan))
    {
        Restore (scan, mark);

        return false;
    }

    while (more)
    {
        mark = Save (scan);
        more = MatchToken (scan, ",", s_kNextComma) && ParseName (scan);
    }

    Restore (scan, mark);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseIf
//
//  THEN is followed by a line number or by a statement, and has a token for
//  each.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseIf (Scan & scan)
{
    Mark  mark = Save (scan);
    Mark  then;



    if (MatchToken (scan, "IF", s_kIf) && ParseExpression (scan) && Match (scan, "THEN"))
    {
        then = Save (scan);

        scan.out.push_back (s_kThenNumber);

        if (ParseNumber (scan))
        {
            return true;
        }

        Restore (scan, then);

        scan.out.push_back (s_kThenStatement);

        if (ParseStatement (scan))
        {
            return true;
        }
    }

    Restore (scan, mark);

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseDimItem
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseDimItem (Scan & scan, Byte stringToken, Byte numericToken)
{
    Mark  mark = Save (scan);



    scan.out.push_back (stringToken);

    if (ParseName (scan) && MatchToken (scan, "$", s_kDollar) && ParseGroup (scan, s_kDimStringOpen))
    {
        return true;
    }

    Restore (scan, mark);

    scan.out.push_back (numericToken);

    if (ParseName (scan) && ParseGroup (scan, s_kDimNumberOpen))
    {
        return true;
    }

    Restore (scan, mark);

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseDim
//
//  The DIM token and each comma depend on whether the item that follows is a
//  string.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseDim (Scan & scan)
{
    Mark  mark = Save (scan);
    bool  more = true;



    if (!Match (scan, "DIM") || !ParseDimItem (scan, s_kDimString, s_kDimNumber))
    {
        Restore (scan, mark);

        return false;
    }

    while (more)
    {
        mark = Save (scan);
        more = Match (scan, ",") && ParseDimItem (scan, s_kDimCommaString, s_kDimCommaNumber);
    }

    Restore (scan, mark);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseInput
//
//  The INPUT token depends on what follows it: a prompt, a string variable or
//  a numeric one. Each later comma depends on its variable the same way.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseInput (Scan & scan)
{
    Mark  mark  = Save (scan);
    Mark  first;
    Mark  item;
    bool  more  = true;



    if (!Match (scan, "INPUT"))
    {
        return false;
    }

    first = Save (scan);

    scan.out.push_back (s_kInputPrompt);

    if (!ParseString (scan))
    {
        Restore (scan, first);

        scan.out.push_back (s_kInputTarget);

        if (!ParseStringTarget (scan))
        {
            Restore (scan, first);

            scan.out.push_back (s_kInputVariable);

            if (!ParseVariable (scan))
            {
                Restore (scan, mark);

                return false;
            }
        }
    }

    while (more)
    {
        mark = Save (scan);
        more = Match (scan, ",");

        if (more)
        {
            item = Save (scan);

            scan.out.push_back (s_kInputString);

            if (!ParseStringTarget (scan))
            {
                Restore (scan, item);

                scan.out.push_back (s_kInputNumber);

                more = ParseVariable (scan);
            }
        }
    }

    Restore (scan, mark);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParsePrint
//
//  The PRINT token and each separator depend on what follows: a string, a
//  number or nothing. A string is parsed first. PRINT alone ends the
//  statement.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParsePrint (Scan & scan)
{
    Mark  mark;
    char  c         = 0;
    bool  semicolon = false;



    if (!Match (scan, "PRINT"))
    {
        return false;
    }

    mark = Save (scan);

    scan.out.push_back (s_kPrintString);

    if (!ParseStringOperand (scan))
    {
        Restore (scan, mark);

        scan.out.push_back (s_kPrintNumber);

        if (!ParseExpression (scan))
        {
            Restore (scan, mark);

            scan.out.push_back (s_kPrintAlone);

            return true;
        }
    }

    c = Peek (scan);

    while (c == ';' || c == ',')
    {
        semicolon = c == ';';

        scan.at++;

        mark = Save (scan);

        scan.out.push_back (semicolon ? s_kSemiString : s_kCommaString);

        if (!ParseStringOperand (scan))
        {
            Restore (scan, mark);

            scan.out.push_back (semicolon ? s_kSemiNumber : s_kCommaNumber);

            if (!ParseExpression (scan))
            {
                Restore (scan, mark);

                scan.out.push_back (semicolon ? s_kSemiAlone : s_kCommaAlone);
            }
        }

        c = Peek (scan);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseList
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseList (Scan & scan)
{
    Mark  mark;



    if (!Match (scan, "LIST"))
    {
        return false;
    }

    mark = Save (scan);

    scan.out.push_back (s_kListNumber);

    if (!ParseNumber (scan))
    {
        Restore (scan, mark);

        scan.out.push_back (s_kListAlone);

        return true;
    }

    mark = Save (scan);

    if (!MatchToken (scan, ",", s_kListComma) || !ParseNumber (scan))
    {
        Restore (scan, mark);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseDisplay
//
//  DSP and NODSP, each followed by one variable with no subscript.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseDisplay (Scan & scan, const char * word, Byte stringToken, Byte numericToken)
{
    Mark  mark = Save (scan);
    Mark  variable;



    if (!Match (scan, word))
    {
        return false;
    }

    variable = Save (scan);

    scan.out.push_back (stringToken);

    if (ParseName (scan) && MatchToken (scan, "$", s_kDollar))
    {
        return true;
    }

    Restore (scan, variable);

    scan.out.push_back (numericToken);

    if (ParseName (scan))
    {
        return true;
    }

    Restore (scan, mark);

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseAssignment
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseAssignment (Scan & scan)
{
    Mark  mark     = Save (scan);
    bool  assigned = false;



    if (!ParseName (scan))
    {
        return false;
    }

    if (MatchToken (scan, "$", s_kDollar))
    {
        ParseGroup (scan, s_kTargetOpen);

        assigned = MatchToken (scan, "=", s_kStringAssign) && ParseStringOperand (scan);
    }
    else
    {
        ParseGroup (scan, s_kSubscriptOpen);

        assigned = MatchToken (scan, "=", s_kNumberAssign) && ParseExpression (scan);
    }

    if (!assigned)
    {
        Restore (scan, mark);
    }

    return assigned;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseKeyword
//
//  A statement that begins with a word. One that fails is followed by the
//  next, and at the end by an assignment, so `PLOTX=1` assigns to PLOTX.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseKeyword (Scan & scan)
{
    Mark  mark = Save (scan);
    static constexpr WordToken  s_kAlone[] =
    {
        { "TEXT", 0x4B }, { "GR",    0x4C }, { "END",     0x51 }, { "RETURN", 0x5B },
        { "POP",  0x77 }, { "TRACE", 0x7D }, { "NOTRACE", 0x7A },
    };



    for (const WordToken & word : s_kAlone)
    {
        if (MatchToken (scan, word.word, word.token))
        {
            return true;
        }
    }

    if (ParseOneOperand  (scan)                           ||
        ParseTwoOperands (scan)                           ||
        ParseLine        (scan, "HLIN", 0x69, 0x6A, 0x6B) ||
        ParseLine        (scan, "VLIN", 0x6C, 0x6D, 0x6E) ||
        ParseFor         (scan)                           ||
        ParseNext        (scan)                           ||
        ParseIf          (scan)                           ||
        ParseDim         (scan)                           ||
        ParseInput       (scan)                           ||
        ParsePrint       (scan)                           ||
        ParseList        (scan)                           ||
        ParseDisplay     (scan, "DSP", 0x7B, 0x7C)        ||
        ParseDisplay     (scan, "NODSP", 0x78, 0x79))
    {
        return true;
    }

    if (MatchToken (scan, "LET", s_kLet) && ParseAssignment (scan))
    {
        return true;
    }

    Restore (scan, mark);

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseStatement
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseStatement (Scan & scan)
{
    bool  parsed = false;



    if (scan.depth >= kMaxDepth)
    {
        return false;
    }

    scan.depth++;

    parsed = ParseRemark (scan) || ParseKeyword (scan) || ParseAssignment (scan);

    scan.depth--;

    return parsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseStatements
//
//  Statements joined by colons. A colon may end the line; two in a row and
//  one at the start are errors.
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseStatements (Scan & scan)
{
    bool  more = true;



    while (more)
    {
        if (!ParseStatement (scan))
        {
            return false;
        }

        more = MatchToken (scan, ":", s_kColon) && Peek (scan) != 0;
    }

    return Peek (scan) == 0 && !scan.badCharacter;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::TokenizeStatements
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::TokenizeStatements (std::string_view text, std::vector<Byte> & outBody)
{
    Scan  scan;
    bool  parsed = false;



    scan.text   = text;
    scan.budget = kParseBudget;

    parsed = ParseStatements (scan);

    outBody.clear();

    if (parsed)
    {
        outBody = std::move (scan.out);
    }

    return parsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::SplitLines
//
//  Any of the host line conventions.
//
////////////////////////////////////////////////////////////////////////////////

void IntegerBasicTokenizer::SplitLines (const std::string & hostListing, std::vector<std::string> & outLines)
{
    std::string  current;
    size_t       i   = 0;
    size_t       len = hostListing.size();



    outLines.clear();

    for (i = 0; i < len; i++)
    {
        char  c = hostListing[i];

        if (c == '\r' || c == '\n')
        {
            if (c == '\r' && (i + 1) < len && hostListing[i + 1] == '\n')
            {
                i++;
            }

            outLines.push_back (current);
            current.clear();

            continue;
        }

        current += c;
    }

    if (!current.empty())
    {
        outLines.push_back (current);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseLineNumber
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::ParseLineNumber (std::string_view text, size_t & ioAt, uint32_t & outNumber)
{
    size_t    i      = ioAt;
    size_t    end    = ioAt;
    size_t    digits = 0;
    uint32_t  value  = 0;
    constexpr uint32_t  kCeiling = 1000000;



    outNumber = 0;

    while (i < text.size() && (text[i] == ' ' || IsDigit (text[i])))
    {
        if (IsDigit (text[i]))
        {
            if (value < kCeiling)
            {
                value = value * s_kRadix + (uint32_t) (text[i] - '0');
            }

            digits++;
            end = i + 1;
        }

        i++;
    }

    if (digits == 0)
    {
        return false;
    }

    ioAt      = end;
    outNumber = value;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::SetLineError
//
////////////////////////////////////////////////////////////////////////////////

void IntegerBasicTokenizer::SetLineError (const ParsedLine & line, const char * reason, ApplesoftListingError & outError)
{
    outError                 = ApplesoftListingError();
    outError.sourceLine      = line.text;
    outError.sourceLineIndex = line.index;
    outError.lineNumber      = line.number;
    outError.hasLineNumber   = true;
    outError.reason          = reason;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::IsLowerNumbered
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicTokenizer::IsLowerNumbered (const ParsedLine & left, const ParsedLine & right)
{
    return left.number < right.number;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::ParseOneLine
//
////////////////////////////////////////////////////////////////////////////////

HRESULT IntegerBasicTokenizer::ParseOneLine (
    const std::string      & text,
    size_t                   index,
    ParsedLine             & outParsed,
    ApplesoftListingError  & outError)
{
    HRESULT   hr       = S_OK;
    size_t    at       = 0;
    uint32_t  number   = 0;
    bool      numbered = ParseLineNumber (text, at, number);
    bool      inRange  = false;
    bool      hasBody  = false;
    bool      parsed   = false;
    Scan      scan;



    outParsed       = ParsedLine();
    outParsed.text  = text;
    outParsed.index = index;

    outError                 = ApplesoftListingError();
    outError.sourceLine      = text;
    outError.sourceLineIndex = index;

    CBREx (numbered, E_INVALIDARG);

    outParsed.number       = number;
    outError.lineNumber    = number;
    outError.hasLineNumber = true;

    inRange = number <= kMaxLineNumber;
    CBREx (inRange, E_INVALIDARG);

    scan.text   = text;
    scan.at     = at;
    scan.budget = kParseBudget;

    hasBody = Peek (scan) != 0;
    CBREx (hasBody, E_INVALIDARG);

    parsed = ParseStatements (scan);
    CBREx (parsed, E_INVALIDARG);

    outParsed.body = std::move (scan.out);

Error:
    if (!numbered)
    {
        outError.reason = "has no line number, so there is no program line to make of it";
    }
    else if (!inRange)
    {
        outError.reason = "is numbered above the highest Integer BASIC accepts, which is 32767";
    }
    else if (!hasBody)
    {
        outError.reason = "has a line number and no statement, which Integer BASIC reads as deleting that line";
    }
    else if (scan.badCharacter)
    {
        outError.reason = "has a character with no Apple II representation";
    }
    else if (!parsed)
    {
        outError.reason = std::format ("is not a line Integer BASIC accepts; the parse stops at column {}", scan.furthest + 1);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::EmitProgram
//
//  Each line is its length, its number, its body and $01. The lines are
//  stored in number order, as the ROM stores them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT IntegerBasicTokenizer::EmitProgram (
    std::vector<ParsedLine>  & lines,
    std::vector<Byte>        & outBytes,
    ApplesoftListingError    & outError)
{
    HRESULT  hr     = S_OK;
    size_t   i      = 0;
    size_t   count  = lines.size();
    bool     hasAny = count > 0;
    bool     unique = true;
    bool     fits   = true;



    outBytes.clear();

    if (!hasAny)
    {
        outError        = ApplesoftListingError();
        outError.reason = "the listing has no numbered lines";
    }

    CBREx (hasAny, E_INVALIDARG);

    std::stable_sort (lines.begin(), lines.end(), IsLowerNumbered);

    for (i = 1; i < count && unique; i++)
    {
        unique = lines[i].number != lines[i - 1].number;

        if (!unique)
        {
            SetLineError (lines[i], "has the same number as another line", outError);
        }
    }

    CBREx (unique, E_INVALIDARG);

    for (i = 0; i < count && fits; i++)
    {
        size_t  lineBytes = kLengthBytes + kNumberBytes + lines[i].body.size() + kTerminatorBytes;

        fits = lineBytes <= kMaxTokenizedLine;

        if (!fits)
        {
            SetLineError (lines[i], "is longer tokenized than Integer BASIC can store in one line", outError);
            break;
        }

        fits = (outBytes.size() + lineBytes) <= s_kMaxProgramBytes;

        if (!fits)
        {
            SetLineError (lines[i], "is past the point where the program stops fitting in the memory Integer BASIC has for it", outError);
            break;
        }

        outBytes.push_back ((Byte) lineBytes);
        outBytes.push_back ((Byte) (lines[i].number & 0xFF));
        outBytes.push_back ((Byte) (lines[i].number >> 8));

        outBytes.insert (outBytes.end(), lines[i].body.begin(), lines[i].body.end());
        outBytes.push_back (s_kEndOfLine);
    }

    CBREx (fits, E_INVALIDARG);

Error:
    if (FAILED (hr))
    {
        outBytes.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer::Tokenize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT IntegerBasicTokenizer::Tokenize (
    const std::string      & hostListing,
    std::vector<Byte>      & outBytes,
    ApplesoftListingError  & outError)
{
    HRESULT                   hr = S_OK;
    size_t                    i  = 0;
    std::vector<std::string>  lines;
    std::vector<ParsedLine>   parsed;
    ParsedLine                one;



    outBytes.clear();
    outError = ApplesoftListingError();

    SplitLines (hostListing, lines);

    for (i = 0; i < lines.size(); i++)
    {
        bool  isBlank = lines[i].find_first_not_of (' ') == std::string::npos;

        if (isBlank)
        {
            continue;
        }

        hr = ParseOneLine (lines[i], i + 1, one, outError);
        CHR (hr);

        parsed.push_back (one);
    }

    hr = EmitProgram (parsed, outBytes, outError);
    CHR (hr);

    outError = ApplesoftListingError();

Error:
    if (FAILED (hr))
    {
        outBytes.clear();
    }

    return hr;
}
