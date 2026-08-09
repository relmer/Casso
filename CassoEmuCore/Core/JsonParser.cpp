#include "Pch.h"

#include "JsonParser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser::JsonParser
//
////////////////////////////////////////////////////////////////////////////////

JsonParser::JsonParser (const string & input)
    : m_input  (input),
      m_pos    (0),
      m_line   (1),
      m_column (1)
{
    m_error.line    = 0;
    m_error.column  = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser::Parse
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::Parse (const string & input, JsonValue & outValue, JsonParseError & outError)
{
    HRESULT  hr          = S_OK;
    bool     consumedAll = false;
    JsonParser parser         (input);



    hr = parser.ParseValue (outValue);
    CHR (hr);

    parser.SkipWhitespace();

    consumedAll = parser.AtEnd();
    CBRF (consumedAll, parser.SetError ("Unexpected content after JSON value"));

Error:
    if (FAILED (hr))
    {
        outError = parser.m_error;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser::ParseValue
//
//  Dispatches on the first non-whitespace character to whichever value parser
//  the grammar demands, and is the recursive entry point objects and arrays
//  re-enter for each of their members.
//
//  A single lookahead character is enough because JSON is designed that way:
//  every value type begins with a character no other type can begin with.
//  There is no backtracking here and none is needed.
//
//  Numbers are the one case with no unique opener, so they fall to the default
//  and are recognized by a digit or a leading minus. Anything else is an error
//  naming the character, which is what makes a stray comma or a bare word in a
//  hand-edited config point at itself.
//
//  The keyword cases re-assign outValue AFTER ParseKeyword succeeds: the
//  keyword parser only verifies the spelling, so the typed value is built here
//  rather than being inferred from a string.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::ParseValue (JsonValue & outValue)
{
    HRESULT hr       = S_OK;
    char    ch       = 0;
    bool    hasInput = false;
    string  str;



    SkipWhitespace();

    hasInput = !AtEnd();
    CBR (hasInput);

    ch = Peek();
    switch (ch)
    {
        case '"':
            hr = ParseString (str);
            CHR (hr);

            outValue = JsonValue (str);
            break;
        
        case '{':
            hr = ParseObject (outValue);
            CHR (hr);
            break;

        case '[':
            hr = ParseArray (outValue);
            CHR (hr);
            break;

        case 't':
            hr = ParseKeyword ("true", outValue);
            CHR (hr);
            outValue = JsonValue (true);
            break;

        case 'f':
            hr = ParseKeyword ("false", outValue);
            CHR (hr);
            outValue = JsonValue (false);
            break;

        case 'n':
            hr = ParseKeyword ("null", outValue);
            CHR (hr);
            outValue = JsonValue (nullptr);
            break;
        
        default:
            if (ch == '-' || (ch >= '0' && ch <= '9'))
            {
                hr = ParseNumber (outValue);
                CHR (hr);
            }
            else
            {
                SetError (format ("Unexpected character '{}'", ch));
                CBR (false);
            }

            break;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser::ParseString
//
//  Parses a quoted string, decoding the JSON escape sequences.
//
//  \u escapes are decoded but only EMITTED below U+0080. The parser's output
//  is a narrow std::string, and the configs it reads -- machine definitions,
//  theme files, user prefs -- are ASCII by design, so a higher code point is
//  consumed and dropped rather than being written as a mojibake byte or
//  forcing a UTF-8 encoder into the core. It stays consumed so the four hex
//  digits never leak into the string as literal text.
//
//  A truncated escape, a truncated \u, and a missing closing quote are all
//  errors rather than being tolerated at end of input -- a file cut short mid
//  string should say so, not silently parse as a shorter value.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::ParseString (string & outStr)
{
    HRESULT       hr        = S_OK;
    char          ch        = 0;
    char          esc       = 0;
    string        hex;
    unsigned long code      = 0;
    bool          done      = false;
    bool          hasInput  = false;
    bool          isQuote   = false;



    isQuote = (Peek() == '"');
    CBR (isQuote);

    Advance();

    outStr.clear();

    while (!AtEnd() && !done)
    {
        ch = Advance();

        // Closing quote — done
        if (ch == '"')
        {
            done = true;
            continue;
        }

        // Regular character
        if (ch != '\\')
        {
            outStr += ch;
            continue;
        }

        // Escape sequence
        hasInput = !AtEnd();
        CBR (hasInput);

        esc = Advance();

        switch (esc)
        {
            case '"':  outStr += '"';  break;
            case '\\': outStr += '\\'; break;
            case '/':  outStr += '/';  break;
            case 'b':  outStr += '\b'; break;
            case 'f':  outStr += '\f'; break;
            case 'n':  outStr += '\n'; break;
            case 'r':  outStr += '\r'; break;
            case 't':  outStr += '\t'; break;
            case 'u':
            {
                hex.clear();

                for (int i = 0; i < 4; i++)
                {
                    hasInput = !AtEnd();
                    CBR (hasInput);

                    hex += Advance();
                }

                code = strtoul (hex.c_str(), nullptr, 16);

                if (code < 0x80)
                {
                    outStr += static_cast<char> (code);
                }

                break;
            }

            default:
            {
                SetError (format ("Invalid escape sequence '\\{}'", esc));
                CBR (false);
            }
        }
    }

    if (!done)
    {
        SetError ("Unterminated string");
        CBR (false);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser::ParseNumber
//
//  Parses a number, with one deliberate extension to the JSON grammar:
//  `0x` hex literals.
//
//  Hex is accepted because the files this parser exists to read are full of
//  6502 addresses, and a machine config that had to spell $C000 as 49152 would
//  be unreadable to the people maintaining it. This is a private parser for
//  first-party configs, not a general-purpose JSON library, so extending the
//  grammar costs nothing externally.
//
//  Everything is stored as a double, matching JSON's single numeric type; the
//  accessors narrow to int or Word at the point of use, where the expected
//  range is actually known.
//
//  The standard path scans the full number -- sign, fraction, exponent -- and
//  hands the whole span to strtod rather than accumulating digits by hand, so
//  rounding matches the platform's own conversion.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::ParseNumber (JsonValue & outValue)
{
    HRESULT  hr    = S_OK;
    size_t   start = m_pos;
    bool     isHex = false;
    double   value = 0.0;



    isHex = m_pos + 2 < m_input.size() &&
            m_input[m_pos] == '0' &&
            (m_input[m_pos + 1] == 'x' || m_input[m_pos + 1] == 'X');

    if (isHex)
    {
        string  hexStr;

        Advance();  // '0'
        Advance();  // 'x'

        while (!AtEnd() && isxdigit (static_cast<unsigned char> (Peek())))
        {
            Advance();
        }

        hexStr = m_input.substr (start + 2, m_pos - start - 2);

        value = static_cast<double> (strtoul (hexStr.c_str(), nullptr, 16));
    }
    else
    {
        string  numStr;

        // Standard JSON number
        if (Peek() == '-')
        {
            Advance();
        }

        while (!AtEnd() && isdigit (static_cast<unsigned char> (Peek())))
        {
            Advance();
        }

        if (!AtEnd() && Peek() == '.')
        {
            Advance();

            while (!AtEnd() && isdigit (static_cast<unsigned char> (Peek())))
            {
                Advance();
            }
        }

        if (!AtEnd() && (Peek() == 'e' || Peek() == 'E'))
        {
            Advance();

            if (!AtEnd() && (Peek() == '+' || Peek() == '-'))
            {
                Advance();
            }

            while (!AtEnd() && isdigit (static_cast<unsigned char> (Peek())))
            {
                Advance();
            }
        }

        numStr = m_input.substr (start, m_pos - start);

        value = strtod (numStr.c_str(), nullptr);
    }

    outValue = JsonValue (value);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser::ParseObject
//
//  Parses `{ "key": value, ... }`, recursing through ParseValue for each
//  member.
//
//  The empty object is tested before the loop rather than handled inside it,
//  because the loop is structured as "parse a member, then decide whether
//  another follows" -- entering it with `}` next would demand a key that is
//  not there.
//
//  A trailing comma is consequently REJECTED: after a comma the loop
//  unconditionally requires another key. That is stricter than some parsers,
//  and deliberately so, since a trailing comma in a hand-edited config is
//  nearly always a half-finished edit.
//
//  Entries are collected in a vector of pairs, not a map, so declaration order
//  survives parsing and duplicate keys are preserved rather than one silently
//  overwriting the other.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::ParseObject (JsonValue & outValue)
{
    HRESULT hr        = S_OK;
    bool    isBrace   = false;
    bool    hasInput  = false;
    bool    isQuote   = false;
    bool    hasColon  = false;
    bool    isComma   = false;
    bool    isEmpty   = false;



    isBrace = (Peek() == '{');
    CBR (isBrace);

    Advance();

    {
        vector<pair<string, JsonValue>> entries;

        SkipWhitespace();

        isEmpty = !AtEnd() && Peek() == '}';

        if (isEmpty)
        {
            Advance();
        }

        while (!isEmpty)
        {
            string     key;
            JsonValue  val;

            SkipWhitespace();

            hasInput = !AtEnd();
            CBR (hasInput);

            isQuote = (Peek() == '"');
            CBRF (isQuote, SetError ("Expected string key in object"));

            hr = ParseString (key);
            CHR (hr);

            SkipWhitespace();

            hasColon = !AtEnd() && Peek() == ':';
            CBR (hasColon);

            Advance();

            hr = ParseValue (val);
            CHR (hr);

            entries.emplace_back (move (key), move (val));

            SkipWhitespace();

            hasInput = !AtEnd();
            CBR (hasInput);

            if (Peek() == '}')
            {
                Advance();
                break;
            }

            isComma = (Peek() == ',');
            CBRF (isComma, SetError ("Expected ',' or '}' in object"));

            Advance();
        }

        outValue = JsonValue (move (entries));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser::ParseArray
//
//  Parses `[ value, ... ]` -- structurally the same walk as ParseObject
//  without the key and colon.
//
//  The empty array is likewise tested before the loop, since the loop starts
//  by demanding a value.
//
//  Elements are parsed through ParseValue, so arrays nest arbitrarily and can
//  hold mixed types. Nothing here validates homogeneity: that is the consuming
//  loader's business, and it can say something far more useful than "type
//  mismatch at element 3".
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::ParseArray (JsonValue & outValue)
{
    HRESULT hr        = S_OK;
    bool    isBracket = false;
    bool    hasInput  = false;
    bool    isComma   = false;
    bool    isEmpty   = false;



    isBracket = (Peek() == '[');
    CBR (isBracket);

    Advance();

    {
        vector<JsonValue> elements;

        SkipWhitespace();

        isEmpty = !AtEnd() && Peek() == ']';

        if (isEmpty)
        {
            Advance();
        }

        while (!isEmpty)
        {
            JsonValue val;
            hr = ParseValue (val);
            CHR (hr);

            elements.push_back (move (val));

            SkipWhitespace();

            hasInput = !AtEnd();
            CBR (hasInput);

            if (Peek() == ']')
            {
                Advance();
                break;
            }

            isComma = (Peek() == ',');
            CBRF (isComma, SetError ("Expected ',' or ']' in array"));

            Advance();
        }

        outValue = JsonValue (move (elements));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser::ParseKeyword
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::ParseKeyword (const char * keyword, JsonValue & outValue)
{
    HRESULT hr        = S_OK;
    size_t  len       = 0;
    bool    matchesCh = false;



    UNREFERENCED_PARAMETER (outValue);



    len = strlen (keyword);

    for (size_t i = 0; i < len; i++)
    {
        matchesCh = !AtEnd() && Peek() == keyword[i];
        CBRF (matchesCh, SetError (format ("Expected '{}'", keyword)));

        Advance();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser::SkipWhitespace
//
//  Skips whitespace and, as the parser's second deliberate extension to the
//  grammar, `//` line comments.
//
//  Comments are supported because these files are hand-maintained
//  configuration: a machine definition wants to say WHY a ROM sits at a given
//  address, and strict JSON gives it nowhere to say it. Like the hex literals
//  in ParseNumber, this is safe precisely because the parser reads only
//  first-party files.
//
//  Comment skipping lives here, in the one function every parse step already
//  calls between tokens, so a comment is legal anywhere whitespace is and no
//  individual parser needs to know comments exist.
//
//  Block comments are NOT supported. A `//` runs to end of line and cannot be
//  left unterminated, whereas an unclosed `/*` silently swallows the rest of
//  the file.
//
////////////////////////////////////////////////////////////////////////////////

void JsonParser::SkipWhitespace()
{
    while (!AtEnd())
    {
        char ch = Peek();

        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
        {
            Advance();
        }
        else if (ch == '/' && m_pos + 1 < m_input.size() && m_input[m_pos + 1] == '/')
        {
            // Line comment support for config files
            while (!AtEnd() && Peek() != '\n')
            {
                Advance();
            }
        }
        else
        {
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser::Peek
//
////////////////////////////////////////////////////////////////////////////////

char JsonParser::Peek() const
{
    return m_input[m_pos];
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser::Advance
//
////////////////////////////////////////////////////////////////////////////////

char JsonParser::Advance()
{
    char ch = m_input[m_pos++];

    if (ch == '\n')
    {
        m_line++;
        m_column = 1;
    }
    else
    {
        m_column++;
    }

    return ch;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser::AtEnd
//
////////////////////////////////////////////////////////////////////////////////

bool JsonParser::AtEnd() const
{
    return m_pos >= m_input.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser::SetError
//
////////////////////////////////////////////////////////////////////////////////

void JsonParser::SetError (const string & msg)
{
    m_error.line    = m_line;
    m_error.column  = m_column;
    m_error.message = msg;
}
