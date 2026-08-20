#include "Pch.h"

#include "ApplesoftTokenizer.h"




//
//  The whole token set, in TABLE ORDER, which is load-bearing rather than
//  cosmetic: the first form that matches wins, so `ONERR` must precede `ON`
//  and `HGR2` must precede `HGR`, while `TO` preceding nothing is exactly why
//  `TOTAL` tokenizes as TO followed by TAL. Index 0 is $80.
//
//  Every value here was corroborated against real stored bytes -- the stock DOS
//  3.3 master's own HELLO and lines typed into a booted machine -- rather than
//  copied from a reference.
//
static constexpr const char *  s_kApplesoftTokens[] =
{
    "END",     "FOR",     "NEXT",    "DATA",    "INPUT",   "DEL",     "DIM",
    "READ",    "GR",      "TEXT",    "PR#",     "IN#",     "CALL",    "PLOT",
    "HLIN",    "VLIN",    "HGR2",    "HGR",     "HCOLOR=", "HPLOT",   "DRAW",
    "XDRAW",   "HTAB",    "HOME",    "ROT=",    "SCALE=",  "SHLOAD",  "TRACE",
    "NOTRACE", "NORMAL",  "INVERSE", "FLASH",   "COLOR=",  "POP",     "VTAB",
    "HIMEM:",  "LOMEM:",  "ONERR",   "RESUME",  "RECALL",  "STORE",   "SPEED=",
    "LET",     "GOTO",    "RUN",     "IF",      "RESTORE", "&",       "GOSUB",
    "RETURN",  "REM",     "STOP",    "ON",      "WAIT",    "LOAD",    "SAVE",
    "DEF",     "POKE",    "PRINT",   "CONT",    "LIST",    "CLEAR",   "GET",
    "NEW",     "TAB(",    "TO",      "FN",      "SPC(",    "THEN",    "AT",
    "NOT",     "STEP",    "+",       "-",       "*",       "/",       "^",
    "AND",     "OR",      ">",       "=",       "<",       "SGN",     "INT",
    "ABS",     "USR",     "FRE",     "SCRN(",   "PDL",     "POS",     "SQR",
    "RND",     "LOG",     "EXP",     "COS",     "SIN",     "TAN",     "ATN",
    "PEEK",    "LEN",     "STR$",    "VAL",     "ASC",     "CHR$",    "LEFT$",
    "RIGHT$",  "MID$",
};





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizer::GetKeyword
//
////////////////////////////////////////////////////////////////////////////////

const char * ApplesoftTokenizer::GetKeyword (Byte token)
{
    bool  isToken = token >= kFirstToken && token <= kLastToken;



    if (!isToken)
    {
        return nullptr;
    }

    return s_kApplesoftTokens[token - kFirstToken];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizer::ToUpper
//
////////////////////////////////////////////////////////////////////////////////

char ApplesoftTokenizer::ToUpper (char c)
{
    if (c >= 'a' && c <= 'z')
    {
        return (char) (c - 'a' + 'A');
    }

    return c;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizer::IsPrintable
//
//  Printable ASCII only. A tab in a listing is indentation somebody meant for
//  their editor, not a byte they meant to store, and everything above $7E is a
//  token's territory once the high bit is set.
//
////////////////////////////////////////////////////////////////////////////////

bool ApplesoftTokenizer::IsPrintable (char c)
{
    constexpr unsigned char  kLowest  = 0x20;
    constexpr unsigned char  kHighest = 0x7E;
    unsigned char            u        = (unsigned char) c;



    return u >= kLowest && u <= kHighest;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizer::IsLowerNumbered
//
////////////////////////////////////////////////////////////////////////////////

bool ApplesoftTokenizer::IsLowerNumbered (const ParsedLine & left, const ParsedLine & right)
{
    return left.number < right.number;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizer::SkipSpaces
//
////////////////////////////////////////////////////////////////////////////////

size_t ApplesoftTokenizer::SkipSpaces (const std::string & text, size_t at)
{
    size_t  i   = at;
    size_t  len = text.size();



    while (i < len && text[i] == ' ')
    {
        i++;
    }

    return i;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizer::SplitLines
//
//  Any of the three host line conventions. A trailing newline does not add an
//  empty line, because a file that ends with one and a file that does not carry
//  the same program.
//
////////////////////////////////////////////////////////////////////////////////

void ApplesoftTokenizer::SplitLines (
    const std::string         &  hostListing,
    std::vector<std::string>  &  outLines)
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
            bool  isCrLf = c == '\r' && (i + 1) < len && hostListing[i + 1] == '\n';

            outLines.push_back (current);
            current.clear();

            if (isCrLf)
            {
                i++;
            }

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
//  ApplesoftTokenizer::TryParseLineNumber
//
////////////////////////////////////////////////////////////////////////////////

bool ApplesoftTokenizer::TryParseLineNumber (
    const std::string  &  text,
    size_t             &  inOutAt,
    uint32_t           &  outNumber)
{
    constexpr uint32_t  kRadix   = 10;
    constexpr uint32_t  kCeiling = 1000000;

    size_t    i      = SkipSpaces (text, inOutAt);
    size_t    len    = text.size();
    size_t    digits = 0;
    uint32_t  value  = 0;



    outNumber = 0;

    while (i < len && text[i] >= '0' && text[i] <= '9')
    {
        if (value < kCeiling)
        {
            value = value * kRadix + (uint32_t) (text[i] - '0');
        }

        digits++;
        i++;
    }

    if (digits == 0)
    {
        return false;
    }

    inOutAt   = i;
    outNumber = value;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizer::TryMatchKeyword
//
//  SPACES ARE SKIPPED INSIDE THE FORM, not merely around it. That is
//  Applesoft's own behavior and it is measured, not assumed: `PR INT` typed into
//  a booted machine is stored as the single PRINT token.
//
////////////////////////////////////////////////////////////////////////////////

bool ApplesoftTokenizer::TryMatchKeyword (
    const std::string  &  text,
    size_t                at,
    const char         *  keyword,
    size_t             &  outEnd)
{
    size_t  i   = at;
    size_t  k   = 0;
    size_t  len = text.size();



    while (keyword[k] != '\0')
    {
        i = SkipSpaces (text, i);

        if (i >= len)
        {
            return false;
        }

        if (ToUpper (text[i]) != keyword[k])
        {
            return false;
        }

        i++;
        k++;
    }

    outEnd = i;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizer::TryMatchAnyKeyword
//
////////////////////////////////////////////////////////////////////////////////

bool ApplesoftTokenizer::TryMatchAnyKeyword (
    const std::string  &  text,
    size_t                at,
    Byte               &  outToken,
    size_t             &  outEnd)
{
    size_t  i     = 0;
    size_t  count = std::size (s_kApplesoftTokens);



    for (i = 0; i < count; i++)
    {
        bool  matched = TryMatchKeyword (text, at, s_kApplesoftTokens[i], outEnd);

        if (matched)
        {
            outToken = (Byte) (kFirstToken + i);

            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizer::TryCopyQuoted
//
//  From the opening quote through the closing one, or to the end of the line
//  when there is no closing one -- which Applesoft accepts and which a listing
//  full of CTRL-D command strings routinely contains.
//
////////////////////////////////////////////////////////////////////////////////

bool ApplesoftTokenizer::TryCopyQuoted (
    const std::string  &  text,
    size_t             &  inOutAt,
    std::vector<Byte>  &  outBody,
    std::string        &  outReason)
{
    size_t  i      = inOutAt;
    size_t  len    = text.size();
    bool    closed = false;



    outBody.push_back ((Byte) text[i]);
    i++;

    while (i < len && !closed)
    {
        bool  printable = IsPrintable (text[i]);

        if (!printable)
        {
            outReason = "carries a character with no Apple II representation";
            inOutAt   = i;

            return false;
        }

        outBody.push_back ((Byte) text[i]);

        closed = text[i] == '"';
        i++;
    }

    inOutAt = i;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizer::TokenizeBody
//
//  The character loop, and the whole of what makes this not a table lookup.
//
//  Three modes, because the same byte means different things in each. Normal
//  drops spaces and matches keywords; REM and DATA copy what they are given. The
//  mode changes only where the guest changes it -- at the REM and DATA tokens,
//  and back out of DATA at a colon outside a string.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ApplesoftTokenizer::TokenizeBody (
    const std::string  &  text,
    size_t                from,
    std::vector<Byte>  &  outBody,
    std::string        &  outReason)
{
    HRESULT  hr       = S_OK;
    Mode     mode     = Mode::Normal;
    size_t   i        = from;
    size_t   len      = text.size();
    size_t   end      = 0;
    size_t   after    = 0;
    Byte     token    = 0;
    bool     ok       = true;
    bool     isAtn    = false;
    bool     isAt     = false;
    bool     startsTo = false;



    outBody.clear();

    while (i < len && ok)
    {
        char  c = text[i];

        if (!IsPrintable (c))
        {
            outReason = "carries a character with no Apple II representation";
            ok        = false;
            continue;
        }

        if (mode == Mode::Rem)
        {
            outBody.push_back ((Byte) c);
            i++;
            continue;
        }

        if (mode == Mode::Data)
        {
            if (c == '"')
            {
                ok = TryCopyQuoted (text, i, outBody, outReason);
                continue;
            }

            if (c == ':')
            {
                mode = Mode::Normal;
            }

            outBody.push_back ((Byte) c);
            i++;
            continue;
        }

        if (c == ' ')
        {
            i++;
            continue;
        }

        if (c == '"')
        {
            ok = TryCopyQuoted (text, i, outBody, outReason);
            continue;
        }

        if (c == '?')
        {
            // Applesoft BASIC accepts "?" as shorthand for "PRINT". Both encode
            // to the same token, so there's no way to distinguish whether the
            // user actually entered ? or PRINT. Since Applesoft's LIST command
            // shows PRINT, we'll follow suit here.
            outBody.push_back (kTokenPrint);
            i++;
            continue;
        }

        if (c == 'A' || c == 'a')
        {
            isAtn = TryMatchKeyword (text, i, "ATN", end);

            if (isAtn)
            {
                outBody.push_back (kTokenAtn);
                i = end;
                continue;
            }

            isAt = TryMatchKeyword (text, i, "AT", end);

            if (isAt)
            {
                after    = SkipSpaces (text, end);
                startsTo = after < len && ToUpper (text[after]) == 'O';

                if (startsTo)
                {
                    // `A TO` has to come apart, because the stored form carries
                    // no spaces and AT would otherwise swallow the T. Emitting
                    // the letter and resuming one character on lets TO match.
                    outBody.push_back ((Byte) 'A');
                    i++;
                    continue;
                }

                outBody.push_back (kTokenAt);
                i = end;
                continue;
            }
        }

        if (TryMatchAnyKeyword (text, i, token, end))
        {
            outBody.push_back (token);
            i = end;

            if (token == kTokenRem)
            {
                mode = Mode::Rem;
            }
            else if (token == kTokenData)
            {
                mode = Mode::Data;
            }

            continue;
        }

        outBody.push_back ((Byte) ToUpper (c));
        i++;
    }

    CBREx (ok, E_INVALIDARG);

Error:
    if (FAILED (hr))
    {
        outBody.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizer::ParseOneLine
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ApplesoftTokenizer::ParseOneLine (
    const std::string      &  text,
    size_t                    index,
    ParsedLine             &  outParsed,
    ApplesoftListingError  &  outError)
{
    HRESULT      hr        = S_OK;
    size_t       at        = 0;
    uint32_t     number    = 0;
    bool         numbered  = TryParseLineNumber (text, at, number);
    bool         inRange   = false;
    bool         hasBody   = false;
    std::string  reason;



    outParsed       = ParsedLine();
    outParsed.text  = text;
    outParsed.index = index;

    outError                 = ApplesoftListingError();
    outError.sourceLine      = text;
    outError.sourceLineIndex = index;

    if (!numbered)
    {
        outError.reason = "has no line number, so there is no program line to make of it";
    }

    CBREx (numbered, E_INVALIDARG);

    outParsed.number       = number;
    outError.lineNumber    = number;
    outError.hasLineNumber = true;

    inRange = number <= kMaxLineNumber;

    if (!inRange)
    {
        outError.reason = "is numbered above the highest Applesoft accepts, which is 63999";
    }

    CBREx (inRange, E_INVALIDARG);

    at      = SkipSpaces (text, at);
    hasBody = at < text.size();

    if (!hasBody)
    {
        // Applesoft reads a bare number as an instruction to DELETE that line,
        // so there is no stored form of one and placing it would silently drop
        // whatever the user thought they were writing.
        outError.reason = "carries a number and no statement, which Applesoft reads as deleting that line";
    }

    CBREx (hasBody, E_INVALIDARG);

    hr = TokenizeBody (text, at, outParsed.body, reason);

    if (FAILED (hr))
    {
        outError.reason = reason;
    }

    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizer::EmitProgram
//
//  The links, which are the part a hand-written tokenizer gets wrong quietly.
//  Each one is the ABSOLUTE address of the next line with the program loaded at
//  $0801, so they cannot be filled in until every line's length is known -- and
//  a program whose links are self-consistent but based somewhere else looks
//  perfectly fine until a guest runs it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ApplesoftTokenizer::EmitProgram (
    std::vector<ParsedLine>  &  lines,
    std::vector<Byte>        &  outBytes,
    ApplesoftListingError    &  outError)
{
    HRESULT   hr      = S_OK;
    size_t    i       = 0;
    size_t    count   = lines.size();
    uint32_t  address = kProgramBase;
    bool      hasAny  = count > 0;
    bool      unique  = true;
    bool      fits    = true;



    outBytes.clear();

    if (!hasAny)
    {
        outError        = ApplesoftListingError();
        outError.reason = "the listing carries no numbered lines";
    }

    CBREx (hasAny, E_INVALIDARG);

    // Applesoft's own editor accepts lines in any order and stores them sorted,
    // so ordering here is fidelity rather than invention. Two lines with the
    // same number are not: the editor would keep whichever was typed last, and
    // a file cannot say which that was.
    std::stable_sort (lines.begin(), lines.end(), IsLowerNumbered);

    for (i = 1; i < count && unique; i++)
    {
        unique = lines[i].number != lines[i - 1].number;

        if (!unique)
        {
            outError                 = ApplesoftListingError();
            outError.sourceLine      = lines[i].text;
            outError.sourceLineIndex = lines[i].index;
            outError.lineNumber      = lines[i].number;
            outError.hasLineNumber   = true;
            outError.reason          = "shares its number with another line, so which one the program keeps is unsayable";
        }
    }

    CBREx (unique, E_INVALIDARG);

    for (i = 0; i < count && fits; i++)
    {
        size_t  lineBytes = kLinkBytes + kNumberBytes + lines[i].body.size() + kTerminatorBytes;

        fits = lineBytes <= kMaxTokenizedLine;

        if (!fits)
        {
            outError                 = ApplesoftListingError();
            outError.sourceLine      = lines[i].text;
            outError.sourceLineIndex = lines[i].index;
            outError.lineNumber      = lines[i].number;
            outError.hasLineNumber   = true;
            outError.reason          = "is longer tokenized than Applesoft can hold in one line";

            break;
        }

        address += (uint32_t) lineBytes;
        fits     = (address + kLinkBytes) <= kProgramCeiling;

        if (!fits)
        {
            outError                 = ApplesoftListingError();
            outError.sourceLine      = lines[i].text;
            outError.sourceLineIndex = lines[i].index;
            outError.lineNumber      = lines[i].number;
            outError.hasLineNumber   = true;
            outError.reason          = "is past the point where the program stops fitting in the memory Applesoft has for it";

            break;
        }

        outBytes.push_back ((Byte) (address & 0xFF));
        outBytes.push_back ((Byte) ((address >> 8) & 0xFF));
        outBytes.push_back ((Byte) (lines[i].number & 0xFF));
        outBytes.push_back ((Byte) ((lines[i].number >> 8) & 0xFF));

        outBytes.insert (outBytes.end(), lines[i].body.begin(), lines[i].body.end());
        outBytes.push_back (0);
    }

    CBREx (fits, E_INVALIDARG);

    outBytes.push_back (0);
    outBytes.push_back (0);

Error:
    if (FAILED (hr))
    {
        outBytes.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizer::Tokenize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ApplesoftTokenizer::Tokenize (
    const std::string      &  hostListing,
    std::vector<Byte>      &  outBytes,
    ApplesoftListingError  &  outError)
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
        size_t  firstReal = SkipSpaces (lines[i], 0);
        bool    isBlank   = firstReal >= lines[i].size();

        if (isBlank)
        {
            continue;
        }

        one = ParsedLine();

        hr = ParseOneLine (lines[i], i + 1, one, outError);
        CHR (hr);

        parsed.push_back (one);
    }

    hr = EmitProgram (parsed, outBytes, outError);
    CHR (hr);

    // Nothing is wrong, so nothing may be left describing a line as though
    // something were: every parsed line leaves its own text here on the way past.
    outError = ApplesoftListingError();

Error:
    if (FAILED (hr))
    {
        outBytes.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizer::RenderOneLine
//
//  A token gets a space in front of it, and one behind it unless it is REM or
//  DATA. THE EXCEPTION IS WHAT MAKES THE ROUND TRIP EXACT: everything after
//  those two is stored verbatim, so a space written behind them would be read
//  back as part of the payload and the listing would grow a space per trip.
//  Everywhere else a space is dropped on the way in, so writing one costs
//  nothing and buys a listing somebody can read.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ApplesoftTokenizer::RenderOneLine (
    const std::vector<Byte>  &  body,
    uint32_t                    number,
    std::string              &  outText,
    std::string              &  outReason)
{
    HRESULT       hr      = S_OK;
    Mode          mode    = Mode::Normal;
    size_t        i       = 0;
    size_t        count   = body.size();
    bool          inQuote = false;
    bool          ok      = true;
    const char *  keyword = nullptr;



    outText = std::to_string (number);
    outText += ' ';

    for (i = 0; i < count && ok; i++)
    {
        Byte  b        = body[i];
        bool  verbatim = inQuote || mode != Mode::Normal;

        if (b >= kFirstToken)
        {
            if (verbatim)
            {
                // Applesoft never stores a token where the bytes are data, so
                // this is a program no guest produced -- and form it out
                // would hand back a listing that tokenizes to something else.
                outReason = "carries a token byte inside a string, a REM or a DATA payload";
                ok        = false;
                continue;
            }

            keyword = GetKeyword (b);

            if (keyword == nullptr)
            {
                outReason = "carries a byte that is not an Applesoft token";
                ok        = false;
                continue;
            }

            outText += ' ';
            outText += keyword;

            if (b == kTokenRem)
            {
                mode = Mode::Rem;
            }
            else if (b == kTokenData)
            {
                mode = Mode::Data;
            }
            else if ((i + 1) < count)
            {
                outText += ' ';
            }

            continue;
        }

        if (!IsPrintable ((char) b))
        {
            outReason = "carries a byte no listing can show";
            ok        = false;
            continue;
        }

        outText += (char) b;

        if (b == (Byte) '"')
        {
            inQuote = !inQuote;
        }
        else if (b == (Byte) ':' && mode == Mode::Data && !inQuote)
        {
            mode = Mode::Normal;
        }
    }

    CBREx (ok, E_INVALIDARG);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizer::Detokenize
//
//  Walks by the line terminator and CHECKS the link rather than trusting it.
//  The link is redundant with the layout, and a redundancy nobody compares is a
//  place for two answers to disagree quietly -- which on this filesystem means a
//  program that lists correctly here and runs off the end of itself on the
//  machine.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ApplesoftTokenizer::Detokenize (
    const std::vector<Byte>  &  programBytes,
    std::string              &  outHostListing,
    ApplesoftListingError    &  outError)
{
    HRESULT            hr      = S_OK;
    size_t             at      = 0;
    size_t             count   = programBytes.size();
    size_t             lines   = 0;
    bool               ok      = true;
    bool               ended   = false;
    uint32_t           number  = 0;
    uint32_t           link    = 0;
    std::string        text;
    std::string        reason;
    std::vector<Byte>  body;



    outHostListing.clear();
    outError = ApplesoftListingError();

    while (ok && !ended)
    {
        size_t  bodyAt = 0;
        size_t  endAt  = 0;

        if ((at + kLinkBytes) > count)
        {
            reason = "the program ends before its last line does";
            ok     = false;
            continue;
        }

        link = (uint32_t) (programBytes[at] | (programBytes[at + 1] << 8));

        if (link == 0)
        {
            ended = true;
            ok    = (at + kLinkBytes) == count;

            if (!ok)
            {
                reason = "carries bytes past the end of the program";
            }

            continue;
        }

        if ((at + kLinkBytes + kNumberBytes) > count)
        {
            reason = "the program ends inside a line header";
            ok     = false;
            continue;
        }

        number = (uint32_t) (programBytes[at + kLinkBytes] | (programBytes[at + kLinkBytes + 1] << 8));
        bodyAt = at + kLinkBytes + kNumberBytes;
        endAt  = bodyAt;

        while (endAt < count && programBytes[endAt] != 0)
        {
            endAt++;
        }

        if (endAt >= count)
        {
            reason = "has no byte ending it";
            ok     = false;
        }

        if (!ok)
        {
            outError.lineNumber    = number;
            outError.hasLineNumber = true;
            continue;
        }

        endAt++;

        if (link != (kProgramBase + endAt))
        {
            reason                 = "points somewhere other than the line that follows it";
            outError.lineNumber    = number;
            outError.hasLineNumber = true;
            ok                     = false;
            continue;
        }

        body.assign (programBytes.begin() + (ptrdiff_t) bodyAt,
                     programBytes.begin() + (ptrdiff_t) (endAt - 1));

        hr = RenderOneLine (body, number, text, reason);

        if (FAILED (hr))
        {
            outError.lineNumber    = number;
            outError.hasLineNumber = true;
            ok                     = false;
            continue;
        }

        outHostListing += text;
        outHostListing += '\n';

        lines++;
        at = endAt;
    }

    if (ok && lines == 0)
    {
        reason = "holds no program lines at all";
        ok     = false;
    }

    if (!ok)
    {
        outError.reason = reason;
    }

    CBREx (ok, E_INVALIDARG);

Error:
    if (FAILED (hr))
    {
        outHostListing.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizer::RoundTripHelpText
//
//  What --basic does and does not preserve, written with the prefix the reader
//  asked for.
//
//  IT LEADS WITH WHAT THE CONVERSION IS, because the paragraph that followed
//  read as a list of damage. `--basic` emits the binary token stream Applesoft
//  itself stores -- the same bytes the machine writes when the line is typed at
//  its own prompt -- and the substitutions below are what a LISTING loses when
//  it is normalized into that form and read back out. They are not things the
//  conversion invents, and a reader who met the list first had no way to tell
//  the two apart.
//
//  It says the asymmetry out loud because the two directions genuinely differ
//  and only one of them is exact. Someone who extracts a program, edits it and
//  places it back has not lost anything they typed; someone who writes a
//  listing, places it and extracts it again gets Applesoft's own normalization
//  handed back, which looks like the tool damaged their file unless the help
//  said so first.
//
////////////////////////////////////////////////////////////////////////////////

std::string ApplesoftTokenizer::RoundTripHelpText (char flagPrefix)
{
    std::string  lp = (flagPrefix == '/') ? "/" : "--";



    return lp + "basic is real tokenization: it writes the binary token stream\n"
           "  Applesoft itself stores, and reads one back. Extracting a program and\n"
           "  placing it back is byte-exact. A listing placed and extracted again is\n"
           "  NOT, and what is LOST there belongs to the listing's formatting, not\n"
           "  to the program: spacing outside strings, REM and DATA is dropped, ?\n"
           "  becomes PRINT, lowercase outside those three becomes uppercase, and\n"
           "  lines are ordered by number. Applesoft normalizes every one of those\n"
           "  itself when a line is typed at its own prompt, so the program on the\n"
           "  disk is the one you meant either way.";
}
