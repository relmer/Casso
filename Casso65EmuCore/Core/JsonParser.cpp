#include "Pch.h"

#include "JsonParser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  JsonValue — constructors
//
////////////////////////////////////////////////////////////////////////////////

JsonValue::JsonValue ()
{
}


JsonValue::JsonValue (nullptr_t)
{
}


JsonValue::JsonValue (bool value)
    : m_type (JsonType::Bool),
      m_bool (value)
{
}


JsonValue::JsonValue (double value)
    : m_type   (JsonType::Number),
      m_number (value)
{
}


JsonValue::JsonValue (const string & value)
    : m_type   (JsonType::String),
      m_string (value)
{
}


JsonValue::JsonValue (vector<JsonValue> && arr)
    : m_type  (JsonType::Array),
      m_array (move (arr))
{
}


JsonValue::JsonValue (vector<pair<string, JsonValue>> && obj)
    : m_type   (JsonType::Object),
      m_object (move (obj))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonValue — accessors
//
////////////////////////////////////////////////////////////////////////////////

bool JsonValue::GetBool () const
{
    return m_bool;
}


double JsonValue::GetNumber () const
{
    return m_number;
}


int JsonValue::GetInt () const
{
    return static_cast<int> (m_number);
}


const string & JsonValue::GetString () const
{
    return m_string;
}


size_t JsonValue::ArraySize () const
{
    return m_array.size ();
}


const JsonValue & JsonValue::ArrayAt (size_t index) const
{
    return m_array[index];
}


bool JsonValue::HasKey (const string & key) const
{
    for (const auto & pair : m_object)
    {
        if (pair.first == key)
        {
            return true;
        }
    }

    return false;
}


const JsonValue & JsonValue::Get (const string & key) const
{
    for (const auto & pair : m_object)
    {
        if (pair.first == key)
        {
            return pair.second;
        }
    }

    static JsonValue s_null;
    return s_null;
}


const vector<pair<string, JsonValue>> & JsonValue::GetObjectEntries () const
{
    return m_object;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser — constructor
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
//  Parse (static entry point)
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::Parse (const string & input, JsonValue & outValue, JsonParseError & outError)
{
    HRESULT    hr     = S_OK;
    JsonParser parser (input);

    hr = parser.ParseValue (outValue);
    CHR (hr);

    parser.SkipWhitespace ();

    CBR_SetError (parser.AtEnd (), parser.SetError ("Unexpected content after JSON value"));

Error:
    if (FAILED (hr))
    {
        outError = parser.m_error;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseValue
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::ParseValue (JsonValue & outValue)
{
    HRESULT hr = S_OK;

    SkipWhitespace ();

    CBR (!AtEnd ());

    {
        char ch = Peek ();

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
            CBR_SetError (false, SetError (format ("Unexpected character '{}'", ch)));
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseString
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::ParseString (string & outStr)
{
    HRESULT hr = S_OK;

    CBR (Peek () == '"');
    Advance ();

    outStr.clear ();

    while (!AtEnd ())
    {
        char ch = Advance ();

        if (ch == '"')
        {
            return S_OK;
        }

        if (ch == '\\')
        {
            CBR (!AtEnd ());

            char esc = Advance ();

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
                        CBR (!AtEnd ());
                        hex += Advance ();
                    }
                    // Basic ASCII Unicode escape
                    unsigned long code = strtoul (hex.c_str (), nullptr, 16);
                    if (code < 0x80)
                    {
                        outStr += static_cast<char> (code);
                    }
                    break;
                }
                default:
                {
                    CBR_SetError (false, SetError (format ("Invalid escape sequence '\\{}'", esc)));
                }
            }
        }
        else
        {
            outStr += ch;
        }
    }

    CBR_SetError (false, SetError ("Unterminated string"));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseNumber
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::ParseNumber (JsonValue & outValue)
{
    HRESULT hr = S_OK;

    size_t start = m_pos;

    // Check for 0x hex prefix
    if (m_pos + 2 < m_input.size () &&
        m_input[m_pos] == '0' &&
        (m_input[m_pos + 1] == 'x' || m_input[m_pos + 1] == 'X'))
    {
        Advance ();  // '0'
        Advance ();  // 'x'

        while (!AtEnd () && isxdigit (static_cast<unsigned char> (Peek ())))
        {
            Advance ();
        }

        string hexStr = m_input.substr (start + 2, m_pos - start - 2);
        unsigned long value = strtoul (hexStr.c_str (), nullptr, 16);
        outValue = JsonValue (static_cast<double> (value));
        return S_OK;
    }

    // Standard JSON number
    if (Peek () == '-')
    {
        Advance ();
    }

    while (!AtEnd () && isdigit (static_cast<unsigned char> (Peek ())))
    {
        Advance ();
    }

    if (!AtEnd () && Peek () == '.')
    {
        Advance ();

        while (!AtEnd () && isdigit (static_cast<unsigned char> (Peek ())))
        {
            Advance ();
        }
    }

    if (!AtEnd () && (Peek () == 'e' || Peek () == 'E'))
    {
        Advance ();

        if (!AtEnd () && (Peek () == '+' || Peek () == '-'))
        {
            Advance ();
        }

        while (!AtEnd () && isdigit (static_cast<unsigned char> (Peek ())))
        {
            Advance ();
        }
    }

    string numStr = m_input.substr (start, m_pos - start);
    double value = strtod (numStr.c_str (), nullptr);
    outValue = JsonValue (value);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseObject
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::ParseObject (JsonValue & outValue)
{
    HRESULT hr = S_OK;

    CBR (Peek () == '{');
    Advance ();

    {
        vector<pair<string, JsonValue>> entries;

        SkipWhitespace ();

        if (!AtEnd () && Peek () == '}')
        {
            Advance ();
            outValue = JsonValue (move (entries));
            return S_OK;
        }

        while (true)
        {
            SkipWhitespace ();
            CBR (!AtEnd ());

            CBR_SetError (Peek () == '"', SetError ("Expected string key in object"));

            string key;
            hr = ParseString (key);
            CHR (hr);

            SkipWhitespace ();
            CBR (!AtEnd () && Peek () == ':');
            Advance ();

            JsonValue val;
            hr = ParseValue (val);
            CHR (hr);

            entries.emplace_back (move (key), move (val));

            SkipWhitespace ();
            CBR (!AtEnd ());

            if (Peek () == '}')
            {
                Advance ();
                break;
            }

            CBR_SetError (Peek () == ',', SetError ("Expected ',' or '}' in object"));

            Advance ();
        }

        outValue = JsonValue (move (entries));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseArray
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::ParseArray (JsonValue & outValue)
{
    HRESULT hr = S_OK;

    CBR (Peek () == '[');
    Advance ();

    {
        vector<JsonValue> elements;

        SkipWhitespace ();

        if (!AtEnd () && Peek () == ']')
        {
            Advance ();
            outValue = JsonValue (move (elements));
            return S_OK;
        }

        while (true)
        {
            JsonValue val;
            hr = ParseValue (val);
            CHR (hr);

            elements.push_back (move (val));

            SkipWhitespace ();
            CBR (!AtEnd ());

            if (Peek () == ']')
            {
                Advance ();
                break;
            }

            CBR_SetError (Peek () == ',', SetError ("Expected ',' or ']' in array"));

            Advance ();
        }

        outValue = JsonValue (move (elements));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseKeyword
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonParser::ParseKeyword (const char * keyword, JsonValue & outValue)
{
    HRESULT hr = S_OK;

    UNREFERENCED_PARAMETER (outValue);

    size_t len = strlen (keyword);

    for (size_t i = 0; i < len; i++)
    {
        CBR_SetError (!AtEnd () && Peek () == keyword[i], SetError (format ("Expected '{}'", keyword)));

        Advance ();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Helper methods
//
////////////////////////////////////////////////////////////////////////////////

void JsonParser::SkipWhitespace ()
{
    while (!AtEnd ())
    {
        char ch = Peek ();

        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
        {
            Advance ();
        }
        else if (ch == '/' && m_pos + 1 < m_input.size () && m_input[m_pos + 1] == '/')
        {
            // Line comment support for config files
            while (!AtEnd () && Peek () != '\n')
            {
                Advance ();
            }
        }
        else
        {
            break;
        }
    }
}


char JsonParser::Peek () const
{
    return m_input[m_pos];
}


char JsonParser::Advance ()
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


bool JsonParser::AtEnd () const
{
    return m_pos >= m_input.size ();
}


void JsonParser::SetError (const string & msg)
{
    m_error.line    = m_line;
    m_error.column  = m_column;
    m_error.message = msg;
}
