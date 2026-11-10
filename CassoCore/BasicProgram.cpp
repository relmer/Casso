#include "Pch.h"

#include "BasicProgram.h"
#include "ApplesoftTokenizer.h"
#include "IntegerBasicDetokenizer.h"



static constexpr size_t  s_kApplesoftHeaderBytes = 4;
static constexpr size_t  s_kIntegerHeaderBytes   = 3;
static constexpr size_t  s_kMaxLineDigits        = 5;
static constexpr Byte    s_kFirstApplesoftBinary = 0xC8;
static constexpr Byte    s_kLastApplesoftBinary  = 0xD1;
static constexpr Byte    s_kFirstIntegerBinary   = 0x12;
static constexpr Byte    s_kLastIntegerBinary    = 0x20;
static constexpr Byte    s_kIntegerClose         = 0x72;





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::ParseApplesoft
//
////////////////////////////////////////////////////////////////////////////////

bool BasicProgram::ParseApplesoft (const std::vector<Byte> & bytes, bool checkLinks, Program & outProgram)
{
    size_t  size    = bytes.size();
    size_t  at      = 0;
    size_t  end     = 0;
    size_t  length  = 0;
    Word    link    = 0;
    bool    valid   = size >= 2;
    bool    isEnded = false;
    Line    line;



    outProgram         = Program();
    outProgram.dialect = Dialect::Applesoft;
    outProgram.base    = ApplesoftTokenizer::kProgramBase;

    while (valid && !isEnded)
    {
        valid = at + 2 <= size;

        if (!valid)
        {
            break;
        }

        link = (Word) (bytes[at] | (bytes[at + 1] << 8));

        if (link == 0)
        {
            isEnded = true;
            outProgram.tail.assign (bytes.begin() + at + 2, bytes.end());
            break;
        }

        valid = at + s_kApplesoftHeaderBytes <= size;

        if (!valid)
        {
            break;
        }

        end = at + s_kApplesoftHeaderBytes;

        while (end < size && bytes[end] != 0)
        {
            end++;
        }

        valid = end < size;

        if (!valid)
        {
            break;
        }

        length      = end + 1 - at;
        line.number = (uint32_t) (bytes[at + 2] | (bytes[at + 3] << 8));
        line.body.assign (bytes.begin() + at + s_kApplesoftHeaderBytes, bytes.begin() + end);

        if (checkLinks && outProgram.lines.empty())
        {
            valid           = link >= length;
            outProgram.base = (Word) (link - length);
        }
        else if (checkLinks)
        {
            valid = link == outProgram.base + end + 1;
        }

        outProgram.lines.push_back (line);
        at = end + 1;
    }

    return valid && isEnded;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::ParseInteger
//
////////////////////////////////////////////////////////////////////////////////

bool BasicProgram::ParseInteger (const std::vector<Byte> & bytes, Program & outProgram)
{
    size_t  size   = bytes.size();
    size_t  at     = 0;
    size_t  length = 0;
    bool    valid  = size > 0;
    Line    line;



    outProgram         = Program();
    outProgram.dialect = Dialect::Integer;

    while (valid && at < size)
    {
        length = bytes[at];
        valid  = length > s_kIntegerHeaderBytes && at + length <= size && bytes[at + length - 1] == IntegerBasicDetokenizer::kTokenEndOfLine;

        if (!valid)
        {
            break;
        }

        line.number = (uint32_t) (bytes[at + 1] | (bytes[at + 2] << 8));
        line.body.assign (bytes.begin() + at + s_kIntegerHeaderBytes, bytes.begin() + at + length - 1);

        valid = line.number <= IntegerBasicDetokenizer::kMaxLineNumber;

        outProgram.lines.push_back (line);
        at += length;
    }

    return valid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::Parse
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BasicProgram::Parse (const std::vector<Byte> & bytes, Program & outProgram, std::string & outReason)
{
    HRESULT  hr    = S_OK;
    bool     valid = false;



    outReason.clear();

    valid = ParseApplesoft (bytes, true, outProgram);

    if (!valid)
    {
        valid = ParseInteger (bytes, outProgram);
    }

    if (!valid)
    {
        valid = ParseApplesoft (bytes, false, outProgram);
    }

    if (!valid)
    {
        outProgram = Program();
        outReason  = "the file is not a tokenized Applesoft or Integer BASIC program";
    }

    CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::Emit
//
//  Fails with the line when an Integer BASIC line is longer than its length
//  byte counts, or when an Applesoft program ends past $FFFF.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BasicProgram::Emit (const Program & program, std::vector<Byte> & outBytes, uint32_t & outLongLine)
{
    HRESULT  hr      = S_OK;
    bool     fits    = true;
    size_t   length  = 0;
    size_t   address = program.base;



    outBytes.clear();
    outLongLine = 0;

    for (const Line & line : program.lines)
    {
        if (program.dialect == Dialect::Integer)
        {
            length = s_kIntegerHeaderBytes + line.body.size() + 1;
            fits   = length <= kIntegerMaxLineBytes;

            outBytes.push_back ((Byte) length);
        }
        else
        {
            address += s_kApplesoftHeaderBytes + line.body.size() + 1;
            fits     = address + 2 <= 0x10000;

            outBytes.push_back ((Byte) (address & 0xFF));
            outBytes.push_back ((Byte) ((address >> 8) & 0xFF));
        }

        if (!fits)
        {
            outLongLine = line.number;
            break;
        }

        outBytes.push_back ((Byte) (line.number & 0xFF));
        outBytes.push_back ((Byte) (line.number >> 8));
        outBytes.insert    (outBytes.end(), line.body.begin(), line.body.end());
        outBytes.push_back ((program.dialect == Dialect::Integer) ? IntegerBasicDetokenizer::kTokenEndOfLine : (Byte) 0);
    }

    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    if (program.dialect == Dialect::Applesoft)
    {
        outBytes.push_back (0);
        outBytes.push_back (0);
        outBytes.insert    (outBytes.end(), program.tail.begin(), program.tail.end());
    }

Error:
    if (FAILED (hr))
    {
        outBytes.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::SkipApplesoftLiteral
//
//  The index past a string, a REM or a DATA statement that starts at `at`, or
//  `at` when none starts there.
//
////////////////////////////////////////////////////////////////////////////////

size_t BasicProgram::SkipApplesoftLiteral (const std::vector<Byte> & body, size_t at)
{
    size_t  size     = body.size();
    size_t  next     = at;
    bool    inString = false;



    if (body[at] == '"')
    {
        next = at + 1;

        while (next < size && body[next] != '"')
        {
            next++;
        }

        next = (next < size) ? next + 1 : size;
    }
    else if (body[at] == ApplesoftTokenizer::kTokenRem)
    {
        next = size;
    }
    else if (body[at] == ApplesoftTokenizer::kTokenData)
    {
        next = at + 1;

        while (next < size && (inString || body[next] != ':'))
        {
            inString = (body[next] == '"') ? !inString : inString;
            next++;
        }
    }

    return next;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::SkipApplesoftNumber
//
//  The index past a numeric constant: digits, a point and an exponent. The
//  sign of an exponent is stored as the + or - token.
//
////////////////////////////////////////////////////////////////////////////////

size_t BasicProgram::SkipApplesoftNumber (const std::vector<Byte> & body, size_t at)
{
    size_t  size = body.size();
    size_t  next = at;
    size_t  mark = 0;



    while (next < size && (IsDigit (body[next]) || body[next] == '.'))
    {
        next++;
    }

    if (next < size && body[next] == 'E')
    {
        mark = next + 1;

        if (mark < size && (body[mark] == kApplesoftPlus || body[mark] == kApplesoftMinus))
        {
            mark++;
        }

        if (mark < size && IsDigit (body[mark]))
        {
            next = mark;

            while (next < size && IsDigit (body[next]))
            {
                next++;
            }
        }
    }

    return next;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::SkipIntegerLiteral
//
//  The index past a string or a REM that starts at `at`, or `at` when none
//  starts there.
//
////////////////////////////////////////////////////////////////////////////////

size_t BasicProgram::SkipIntegerLiteral (const std::vector<Byte> & body, size_t at)
{
    size_t  size = body.size();
    size_t  next = at;



    if (body[at] == IntegerBasicDetokenizer::kTokenQuoteOpen)
    {
        next = at + 1;

        while (next < size && body[next] != IntegerBasicDetokenizer::kTokenQuoteClose)
        {
            next++;
        }

        next = (next < size) ? next + 1 : size;
    }
    else if (body[at] == IntegerBasicDetokenizer::kTokenRem)
    {
        next = size;
    }

    return next;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::ReadApplesoftNumber
//
//  A whole-number constant with an optional minus that is the whole operand:
//  the byte after it is not an operator, a point or an exponent.
//
////////////////////////////////////////////////////////////////////////////////

bool BasicProgram::ReadApplesoftNumber (const std::vector<Byte> & body, size_t & ioAt, int32_t & outValue)
{
    size_t   size       = body.size();
    size_t   at         = ioAt;
    size_t   digits     = 0;
    int32_t  value      = 0;
    bool     isNegative = false;
    bool     isWhole    = false;



    while (at < size && body[at] == ' ')
    {
        at++;
    }

    if (at < size && body[at] == kApplesoftMinus)
    {
        isNegative = true;
        at++;
    }

    while (at < size && IsDigit (body[at]) && digits <= s_kMaxLineDigits)
    {
        value = value * 10 + (body[at] - '0');
        digits++;
        at++;
    }

    while (at < size && body[at] == ' ')
    {
        at++;
    }

    isWhole = digits > 0 && digits <= s_kMaxLineDigits;

    if (isWhole && at < size)
    {
        isWhole = !(body[at] >= s_kFirstApplesoftBinary && body[at] <= s_kLastApplesoftBinary) && body[at] != '.' && body[at] != 'E' && !IsDigit (body[at]);
    }

    if (isWhole)
    {
        ioAt     = at;
        outValue = isNegative ? -value : value;
    }

    return isWhole;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::ReadIntegerNumber
//
//  A constant with an optional unary minus that is the whole operand.
//
////////////////////////////////////////////////////////////////////////////////

bool BasicProgram::ReadIntegerNumber (const std::vector<Byte> & body, size_t & ioAt, int32_t & outValue)
{
    size_t  size       = body.size();
    size_t  at         = ioAt;
    bool    isNegative = at < size && body[at] == kIntegerUnaryMinus;
    bool    isWhole    = false;



    at += isNegative ? 1 : 0;

    isWhole = at + kIntegerConstantBytes <= size && IsHighDigit (body[at]);

    if (isWhole && at + kIntegerConstantBytes < size)
    {
        Byte  after = body[at + kIntegerConstantBytes];

        isWhole = !(after >= s_kFirstIntegerBinary && after <= s_kLastIntegerBinary);
    }

    if (isWhole)
    {
        outValue = body[at + 1] | (body[at + 2] << 8);
        outValue = isNegative ? -outValue : outValue;
        ioAt     = at + kIntegerConstantBytes;
    }

    return isWhole;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::FindApplesoftLines
//
//  GOTO, GOSUB, LIST and DEL take a list: ON has several targets after its
//  GOTO or GOSUB, and LIST and DEL have a range written with a comma or a
//  minus. THEN and RUN take one number.
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgram::FindApplesoftLines (const std::vector<Byte> & body, std::vector<LineReference> & outReferences)
{
    size_t         size   = body.size();
    size_t         at     = 0;
    size_t         next   = 0;
    size_t         start  = 0;
    uint32_t       value  = 0;
    Byte           token  = 0;
    bool           isList = false;
    bool           isOne  = false;
    bool           isMore = false;
    LineReference  reference;



    while (at < size)
    {
        next = SkipApplesoftLiteral (body, at);

        if (next != at)
        {
            at = next;
            continue;
        }

        token  = body[at++];
        isList = token == kApplesoftGoto || token == kApplesoftGosub || token == kApplesoftList || token == kApplesoftDel;
        isOne  = token == kApplesoftThen || token == kApplesoftRun;
        isMore = isList || isOne;

        while (isMore)
        {
            while (at < size && body[at] == ' ')
            {
                at++;
            }

            start = at;
            value = 0;

            while (at < size && IsDigit (body[at]) && at - start <= s_kMaxLineDigits)
            {
                value = value * 10 + (uint32_t) (body[at] - '0');
                at++;
            }

            if (at > start && at - start <= s_kMaxLineDigits)
            {
                reference.offset = start;
                reference.length = at - start;
                reference.target = value;

                outReferences.push_back (reference);
            }

            while (at < size && body[at] == ' ')
            {
                at++;
            }

            isMore = isList && at < size && (body[at] == ',' || body[at] == kApplesoftMinus);
            at    += isMore ? 1 : 0;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::FindIntegerLines
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgram::FindIntegerLines (const std::vector<Byte> & body, std::vector<LineReference> & outReferences)
{
    size_t         size     = body.size();
    size_t         at       = 0;
    size_t         next     = 0;
    int32_t        value    = 0;
    Byte           previous = 0;
    bool           follows  = false;
    LineReference  reference;



    while (at < size)
    {
        next = SkipIntegerLiteral (body, at);

        if (next != at)
        {
            at       = next;
            previous = 0;
            continue;
        }

        if (IsHighDigit (body[at]) && at + kIntegerConstantBytes <= size)
        {
            follows = previous == kIntegerGoto       || previous == kIntegerGosub      || previous == kIntegerThenNumber ||
                      previous == kIntegerListNumber || previous == kIntegerListComma;
            next    = at;

            if (follows && ReadIntegerNumber (body, next, value))
            {
                reference.offset = at;
                reference.length = kIntegerConstantBytes;
                reference.target = (uint32_t) value;

                outReferences.push_back (reference);
            }

            at      += kIntegerConstantBytes;
            previous = 0;
            continue;
        }

        if (body[at] >= IntegerBasicDetokenizer::kHighBit)
        {
            while (at < size && body[at] >= IntegerBasicDetokenizer::kHighBit)
            {
                at++;
            }

            previous = 0;
            continue;
        }

        previous = body[at++];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::FindLineReferences
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgram::FindLineReferences (Dialect dialect, const std::vector<Byte> & body, std::vector<LineReference> & outReferences)
{
    outReferences.clear();

    if (dialect == Dialect::Integer)
    {
        FindIntegerLines (body, outReferences);
    }
    else
    {
        FindApplesoftLines (body, outReferences);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::FindApplesoftAddresses
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgram::FindApplesoftAddresses (const std::vector<Byte> & body, std::vector<AddressReference> & outReferences)
{
    size_t            size  = body.size();
    size_t            at    = 0;
    size_t            next  = 0;
    Byte              token = 0;
    AddressReference  reference;



    while (at < size)
    {
        next = SkipApplesoftLiteral (body, at);

        if (next != at)
        {
            at = next;
            continue;
        }

        token          = body[at++];
        reference.word = (token == kApplesoftCall) ? "CALL" :
                         (token == kApplesoftPoke) ? "POKE" :
                         (token == kApplesoftPeek) ? "PEEK" : nullptr;

        if (reference.word == nullptr)
        {
            continue;
        }

        next = at;

        if (token == kApplesoftPeek)
        {
            while (next < size && (body[next] == ' ' || body[next] == '('))
            {
                next++;
            }
        }

        if (ReadApplesoftNumber (body, next, reference.address))
        {
            outReferences.push_back (reference);
            at = next;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::FindIntegerAddresses
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgram::FindIntegerAddresses (const std::vector<Byte> & body, std::vector<AddressReference> & outReferences)
{
    size_t            size  = body.size();
    size_t            at    = 0;
    size_t            next  = 0;
    Byte              token = 0;
    AddressReference  reference;



    while (at < size)
    {
        next = SkipIntegerLiteral (body, at);

        if (next != at)
        {
            at = next;
            continue;
        }

        if (IsHighDigit (body[at]) && at + kIntegerConstantBytes <= size)
        {
            at += kIntegerConstantBytes;
            continue;
        }

        token          = body[at++];
        reference.word = (token == kIntegerCall) ? "CALL" :
                         (token == kIntegerPoke) ? "POKE" :
                         (token == kIntegerPeek) ? "PEEK" : nullptr;

        if (reference.word == nullptr)
        {
            continue;
        }

        next = at;

        if (token == kIntegerPeek && next < size && body[next] == kIntegerCallOpen)
        {
            next++;
        }

        if (ReadIntegerNumber (body, next, reference.address))
        {
            outReferences.push_back (reference);
            at = next;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::FindAddressReferences
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgram::FindAddressReferences (Dialect dialect, const std::vector<Byte> & body, std::vector<AddressReference> & outReferences)
{
    outReferences.clear();

    if (dialect == Dialect::Integer)
    {
        FindIntegerAddresses (body, outReferences);
    }
    else
    {
        FindApplesoftAddresses (body, outReferences);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::FindApplesoftVariables
//
//  A name is written as it is in the program, with $ or % and with ( for an
//  array. A function defined with DEF FN is listed as FN and its name.
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgram::FindApplesoftVariables (const std::vector<Byte> & body, std::vector<std::string> & outNames)
{
    size_t       size       = body.size();
    size_t       at         = 0;
    size_t       next       = 0;
    bool         isFunction = false;
    std::string  name;



    while (at < size)
    {
        next = SkipApplesoftLiteral (body, at);

        if (next != at)
        {
            at = next;
            continue;
        }

        if (IsDigit (body[at]) || body[at] == '.')
        {
            next = SkipApplesoftNumber (body, at);
            at   = (next > at) ? next : at + 1;
            continue;
        }

        if (body[at] == kApplesoftFn)
        {
            isFunction = true;
            at++;
            continue;
        }

        if (!IsLetter (body[at]))
        {
            isFunction = isFunction && body[at] == ' ';
            at++;
            continue;
        }

        name = isFunction ? "FN " : "";

        while (at < size && (IsLetter (body[at]) || IsDigit (body[at])))
        {
            name.push_back ((char) body[at++]);
        }

        if (!isFunction && at < size && (body[at] == '$' || body[at] == '%'))
        {
            name.push_back ((char) body[at++]);
        }

        if (!isFunction && at < size && body[at] == '(')
        {
            name.push_back ('(');
        }

        isFunction = false;

        outNames.push_back (name);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::FindIntegerVariables
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgram::FindIntegerVariables (const std::vector<Byte> & body, std::vector<std::string> & outNames)
{
    size_t       size = body.size();
    size_t       at   = 0;
    size_t       next = 0;
    std::string  name;



    while (at < size)
    {
        next = SkipIntegerLiteral (body, at);

        if (next != at)
        {
            at = next;
            continue;
        }

        if (IsHighDigit (body[at]) && at + kIntegerConstantBytes <= size)
        {
            at += kIntegerConstantBytes;
            continue;
        }

        if (body[at] < IntegerBasicDetokenizer::kHighBit)
        {
            at++;
            continue;
        }

        name.clear();

        while (at < size && body[at] >= IntegerBasicDetokenizer::kHighBit)
        {
            name.push_back ((char) (body[at++] & 0x7F));
        }

        if (at < size && body[at] == kIntegerDollar)
        {
            name.push_back ('$');
        }

        outNames.push_back (name);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgram::FindVariables
//
////////////////////////////////////////////////////////////////////////////////

void BasicProgram::FindVariables (Dialect dialect, const std::vector<Byte> & body, std::vector<std::string> & outNames)
{
    outNames.clear();

    if (dialect == Dialect::Integer)
    {
        FindIntegerVariables (body, outNames);
    }
    else
    {
        FindApplesoftVariables (body, outNames);
    }
}
