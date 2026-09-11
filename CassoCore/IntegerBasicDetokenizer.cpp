#include "Pch.h"

#include "IntegerBasicDetokenizer.h"



//
//  Integer BASIC's token set, one entry per byte from $00 to $7F. Several
//  words appear more than once because the ROM assigns a token per syntactic
//  position -- PRINT with an expression, with a string, and alone are three
//  tokens -- and a listing writes all of them the same way.
//
static constexpr const char *  s_kIntegerTokens[] =
{
    "HIMEM:", "",       "_",      ":",      "LOAD",   "SAVE",   "CON",    "RUN",      // $00
    "RUN",    "DEL",    ",",      "NEW",    "CLR",    "AUTO",   ",",      "MAN",      // $08
    "HIMEM:", "LOMEM:", "+",      "-",      "*",      "/",      "=",      "#",        // $10
    ">=",     ">",      "<=",     "<>",     "<",      "AND",    "OR",     "MOD",      // $18
    "^",      "+",      "(",      ",",      "THEN",   "THEN",   ",",      ",",        // $20
    "\"",     "\"",     "(",      "!",      "!",      "(",      "PEEK",   "RND",      // $28
    "SGN",    "ABS",    "PDL",    "RNDX",   "(",      "+",      "-",      "NOT",      // $30
    "(",      "=",      "#",      "LEN(",   "ASC(",   "SCRN(",  ",",      "(",        // $38
    "$",      "$",      "(",      ",",      ",",      ";",      ";",      ";",        // $40
    ",",      ",",      ",",      "TEXT",   "GR",     "CALL",   "DIM",    "DIM",      // $48
    "TAB",    "END",    "INPUT",  "INPUT",  "INPUT",  "FOR",    "=",      "TO",       // $50
    "STEP",   "NEXT",   ",",      "RETURN", "GOSUB",  "REM",    "LET",    "GOTO",     // $58
    "IF",     "PRINT",  "PRINT",  "PRINT",  "POKE",   ",",      "COLOR=", "PLOT",     // $60
    ",",      "HLIN",   ",",      "AT",     "VLIN",   ",",      "AT",     "VTAB",     // $68
    "=",      "=",      ")",      ")",      "LIST",   ",",      "LIST",   "POP",      // $70
    "NODSP",  "NODSP",  "NOTRACE", "DSP",   "DSP",    "TRACE",  "PR#",    "IN#",      // $78
};

static_assert (std::size (s_kIntegerTokens) == 128, "one entry per token byte");





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicDetokenizer::GetKeyword
//
////////////////////////////////////////////////////////////////////////////////

const char * IntegerBasicDetokenizer::GetKeyword (Byte token)
{
    if (token > kLastToken)
    {
        return nullptr;
    }

    return s_kIntegerTokens[token];
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicDetokenizer::IsWordKeyword
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicDetokenizer::IsWordKeyword (const char * keyword)
{
    return keyword != nullptr && keyword[0] >= 'A' && keyword[0] <= 'Z';
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicDetokenizer::IsHighDigit
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicDetokenizer::IsHighDigit (Byte value)
{
    return value >= (Byte) ('0' | kHighBit) && value <= (Byte) ('9' | kHighBit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicDetokenizer::IsHighLetter
//
////////////////////////////////////////////////////////////////////////////////

bool IntegerBasicDetokenizer::IsHighLetter (Byte value)
{
    return value >= (Byte) ('A' | kHighBit) && value <= (Byte) ('Z' | kHighBit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicDetokenizer::RenderOneLine
//
//  One line's tokens, from the byte after the number to the byte before the
//  terminator, as the text a LIST would show.
//
//  SPACING IS PUT THERE BY THIS ROUTINE, as the ROM's LIST puts it there: the
//  stored form carries none. A word token gets a space on each side, a symbol
//  none, and the last space on a line is dropped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT IntegerBasicDetokenizer::RenderOneLine (
    const std::vector<Byte>  & programBytes,
    size_t                     bodyAt,
    size_t                     endAt,
    std::string              & outText,
    std::string              & outReason,
    size_t                   & outFaultAt)
{
    HRESULT  hr           = S_OK;
    size_t   at           = bodyAt;
    bool     inString     = false;
    bool     inRemark     = false;
    bool     inIdentifier = false;
    bool     closed       = true;



    outText.clear();

    while (at < endAt)
    {
        Byte  value = programBytes[at];

        outFaultAt = at;

        if (inRemark || inString)
        {
            bool  isCharacter = value >= kHighBit;

            if (inString && value == kTokenQuoteClose)
            {
                outText  += '"';
                inString  = false;
                closed    = true;
                at++;
                continue;
            }

            // A string or a remark holds characters, and the ROM stores them
            // with the high bit set; a bare token byte inside one would be a
            // character the machine cannot show.
            CBRFEx (isCharacter, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA),
                    outReason = "a token byte appears inside a string or remark");

            outText += (char) (value & ~kHighBit);
            at++;
            continue;
        }

        if (value >= kHighBit)
        {
            bool  startsConstant = IsHighDigit (value) && !inIdentifier;

            if (startsConstant)
            {
                bool  hasValue = (at + 1 + kConstantValueBytes) <= endAt;

                CBRFEx (hasValue, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA),
                        outReason = "a numeric constant is cut off");

                outText += std::to_string ((unsigned) (programBytes[at + 1] | (programBytes[at + 2] << 8)));
                at      += 1 + kConstantValueBytes;
                inIdentifier = false;
                continue;
            }

            outText     += (char) (value & ~kHighBit);
            inIdentifier = IsHighLetter (value) || IsHighDigit (value);
            at++;
            continue;
        }

        {
            std::string_view  keyword  = GetKeyword (value);
            bool              isWord   = IsWordKeyword (keyword.data());
            bool              endsWord = isWord && keyword.back() >= 'A' && keyword.back() <= 'Z';

            inIdentifier = false;

            if (value == kTokenQuoteOpen)
            {
                outText += '"';
                inString = true;
                closed   = false;
                at++;
                continue;
            }

            if (isWord && !outText.empty() && outText.back() != ' ' && outText.back() != '(')
            {
                outText += ' ';
            }

            outText += keyword;

            if (endsWord)
            {
                outText += ' ';
            }

            if (value == kTokenRem)
            {
                inRemark = true;
            }

            at++;
        }
    }

    outFaultAt = endAt;

    CBRFEx (closed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA),
            outReason = "a string never closes");

    while (!outText.empty() && outText.back() == ' ')
    {
        outText.pop_back();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicDetokenizer::Detokenize
//
//  Walks the length-prefixed chain and checks each line's shape before
//  rendering it. The length byte is trusted only as far as the buffer goes,
//  and the byte it says ends the line has to be the terminator, or the length
//  and the contents disagree and nothing past that point can be read safely.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT IntegerBasicDetokenizer::Detokenize (
    const std::vector<Byte>   & programBytes,
    std::string               & outListing,
    IntegerBasicListingError  & outError)
{
    HRESULT      hr      = S_OK;
    size_t       at      = 0;
    size_t       count   = programBytes.size();
    std::string  text;
    std::string  reason;



    outListing.clear();
    outError = IntegerBasicListingError();

    while (at < count)
    {
        size_t    length  = programBytes[at];
        size_t    endAt   = at + length;
        size_t    faultAt = at;
        uint32_t  number  = 0;
        bool      fits    = length >= kMinimumLineBytes && endAt <= count;
        bool      ends    = false;
        bool      ranged  = false;

        if (!fits)
        {
            outError.reason = "the program ends before its last line does";
            outError.offset = at;
            hr              = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
            break;
        }

        number = (uint32_t) (programBytes[at + 1] | (programBytes[at + 2] << 8));
        ends   = programBytes[endAt - 1] == kTokenEndOfLine;
        ranged = number <= kMaxLineNumber;

        outError.lineNumber    = number;
        outError.hasLineNumber = true;

        if (!ranged)
        {
            outError.reason = "the line number is past the largest Integer BASIC allows";
            outError.offset = at + 1;
            hr              = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
            break;
        }

        if (!ends)
        {
            outError.reason = "the line does not end where its length says it does";
            outError.offset = endAt - 1;
            hr              = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
            break;
        }

        hr = RenderOneLine (programBytes, at + kLineHeaderBytes, endAt - 1, text, reason, faultAt);

        if (FAILED (hr))
        {
            outError.reason = reason;
            outError.offset = faultAt;
            break;
        }

        outListing += std::to_string (number);

        if (!text.empty())
        {
            outListing += ' ';
            outListing += text;
        }

        outListing += '\n';

        at = endAt;
    }

    if (FAILED (hr))
    {
        outListing.clear();
    }
    else
    {
        outError = IntegerBasicListingError();
    }

    return hr;
}
