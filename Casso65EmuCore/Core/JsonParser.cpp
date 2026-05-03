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
    HRESULT    hr     = S_OK;
    JsonParser parser (input);

    hr = parser.ParseValue (outValue);
    CHR (hr);

    parser.SkipWhitespace();

    CBRF (parser.AtEnd(), parser.SetError ("Unexpected content after JSON value"));

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
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::ParseValue (JsonValue & outValue)
{
    HRESULT hr = S_OK;

    SkipWhitespace();

    CBR (!AtEnd());

    {
        char ch = Peek();

        if (ch == '"')
        {
            string str;
            hr = ParseString (str);
            CHR (hr);
            outValue = JsonValue (str);
        }
        else if (ch == '{')
        {
            hr = ParseObject (outValue);
            CHR (hr);
        }
        else if (ch == '[')
        {
            hr = ParseArray (outValue);
            CHR (hr);
        }
        else if (ch == 't')
        {
            hr = ParseKeyword ("true", outValue);
            CHR (hr);
            outValue = JsonValue (true);
        }
        else if (ch == 'f')
        {
            hr = ParseKeyword ("false", outValue);
            CHR (hr);
            outValue = JsonValue (false);
        }
        else if (ch == 'n')
        {
            hr = ParseKeyword ("null", outValue);
            CHR (hr);
            outValue = JsonValue (nullptr);
        }
        else if (ch == '-' || (ch >= '0' && ch <= '9'))
        {
            hr = ParseNumber (outValue);
            CHR (hr);
        }
        else
        {
            CBRF (false, SetError (format ("Unexpected character '{}'", ch)));
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser::ParseString
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::ParseString (string & outStr)
{
    HRESULT hr = S_OK;

    CBR (Peek() == '"');
    Advance();

    outStr.clear();

    while (!AtEnd())
    {
        char ch = Advance();

        if (ch == '"')
        {
            return S_OK;
        }

        if (ch == '\\')
        {
            CBR (!AtEnd());

            char esc = Advance();

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
                    // Parse 4 hex digits
                    string hex;
                    for (int i = 0; i < 4; i++)
                    {
                        CBR (!AtEnd());
                        hex += Advance();
                    }
                    // Basic ASCII Unicode escape
                    unsigned long code = strtoul (hex.c_str(), nullptr, 16);
                    if (code < 0x80)
                    {
                        outStr += static_cast<char> (code);
                    }
                    break;
                }
                default:
                {
                    CBRF (false, SetError (format ("Invalid escape sequence '\\{}'", esc)));
                }
            }
        }
        else
        {
            outStr += ch;
        }
    }

    CBRF (false, SetError ("Unterminated string"));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser::ParseNumber
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::ParseNumber (JsonValue & outValue)
{
    HRESULT hr = S_OK;

    size_t start = m_pos;

    // Check for 0x hex prefix
    if (m_pos + 2 < m_input.size() &&
        m_input[m_pos] == '0' &&
        (m_input[m_pos + 1] == 'x' || m_input[m_pos + 1] == 'X'))
    {
        Advance();  // '0'
        Advance();  // 'x'

        while (!AtEnd() && isxdigit (static_cast<unsigned char> (Peek())))
        {
            Advance();
        }

        string hexStr = m_input.substr (start + 2, m_pos - start - 2);
        unsigned long value = strtoul (hexStr.c_str(), nullptr, 16);
        outValue = JsonValue (static_cast<double> (value));
        return S_OK;
    }

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

    string numStr = m_input.substr (start, m_pos - start);
    double value = strtod (numStr.c_str(), nullptr);
    outValue = JsonValue (value);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser::ParseObject
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::ParseObject (JsonValue & outValue)
{
    HRESULT hr = S_OK;

    CBR (Peek() == '{');
    Advance();

    {
        vector<pair<string, JsonValue>> entries;

        SkipWhitespace();

        if (!AtEnd() && Peek() == '}')
        {
            Advance();
            outValue = JsonValue (move (entries));
            return S_OK;
        }

        while (true)
        {
            SkipWhitespace();
            CBR (!AtEnd());

            CBRF (Peek() == '"', SetError ("Expected string key in object"));

            string key;
            hr = ParseString (key);
            CHR (hr);

            SkipWhitespace();
            CBR (!AtEnd() && Peek() == ':');
            Advance();

            JsonValue val;
            hr = ParseValue (val);
            CHR (hr);

            entries.emplace_back (move (key), move (val));

            SkipWhitespace();
            CBR (!AtEnd());

            if (Peek() == '}')
            {
                Advance();
                break;
            }

            CBRF (Peek() == ',', SetError ("Expected ',' or '}' in object"));

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
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::ParseArray (JsonValue & outValue)
{
    HRESULT hr = S_OK;

    CBR (Peek() == '[');
    Advance();

    {
        vector<JsonValue> elements;

        SkipWhitespace();

        if (!AtEnd() && Peek() == ']')
        {
            Advance();
            outValue = JsonValue (move (elements));
            return S_OK;
        }

        while (true)
        {
            JsonValue val;
            hr = ParseValue (val);
            CHR (hr);

            elements.push_back (move (val));

            SkipWhitespace();
            CBR (!AtEnd());

            if (Peek() == ']')
            {
                Advance();
                break;
            }

            CBRF (Peek() == ',', SetError ("Expected ',' or ']' in array"));

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
    HRESULT hr = S_OK;

    UNREFERENCED_PARAMETER (outValue);

    size_t len = strlen (keyword);

    for (size_t i = 0; i < len; i++)
    {
        CBRF (!AtEnd() && Peek() == keyword[i], SetError (format ("Expected '{}'", keyword)));

        Advance();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser::SkipWhitespace
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
