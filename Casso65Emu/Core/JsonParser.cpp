#include "Pch.h"

#include "JsonParser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  JsonValue — constructors
//
////////////////////////////////////////////////////////////////////////////////

JsonValue::JsonValue ()
    : m_type   (JsonType::Null),
      m_bool   (false),
      m_number (0.0)
{
}


JsonValue::JsonValue (std::nullptr_t)
    : m_type   (JsonType::Null),
      m_bool   (false),
      m_number (0.0)
{
}


JsonValue::JsonValue (bool value)
    : m_type   (JsonType::Bool),
      m_bool   (value),
      m_number (0.0)
{
}


JsonValue::JsonValue (double value)
    : m_type   (JsonType::Number),
      m_bool   (false),
      m_number (value)
{
}


JsonValue::JsonValue (const std::string & value)
    : m_type   (JsonType::String),
      m_bool   (false),
      m_number (0.0),
      m_string (value)
{
}


JsonValue::JsonValue (std::vector<JsonValue> && arr)
    : m_type   (JsonType::Array),
      m_bool   (false),
      m_number (0.0),
      m_array  (std::move (arr))
{
}


JsonValue::JsonValue (std::vector<std::pair<std::string, JsonValue>> && obj)
    : m_type   (JsonType::Object),
      m_bool   (false),
      m_number (0.0),
      m_object (std::move (obj))
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


const std::string & JsonValue::GetString () const
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


bool JsonValue::HasKey (const std::string & key) const
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


const JsonValue & JsonValue::Get (const std::string & key) const
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


const std::vector<std::pair<std::string, JsonValue>> & JsonValue::GetObjectEntries () const
{
    return m_object;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser — constructor
//
////////////////////////////////////////////////////////////////////////////////

JsonParser::JsonParser (const std::string & input)
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

HRESULT JsonParser::Parse (const std::string & input, JsonValue & outValue, JsonParseError & outError)
{
    HRESULT    hr     = S_OK;
    JsonParser parser (input);

    CHR (parser.ParseValue (outValue));

    parser.SkipWhitespace ();

    if (!parser.AtEnd ())
    {
        parser.SetError ("Unexpected content after JSON value");
        hr = E_FAIL;
        goto Error;
    }

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

    CBREx (!AtEnd (), E_FAIL);

    {
        char ch = Peek ();

        if (ch == '"')
        {
            std::string str;
            CHR (ParseString (str));
            outValue = JsonValue (str);
        }
        else if (ch == '{')
        {
            CHR (ParseObject (outValue));
        }
        else if (ch == '[')
        {
            CHR (ParseArray (outValue));
        }
        else if (ch == 't')
        {
            CHR (ParseKeyword ("true", outValue));
            outValue = JsonValue (true);
        }
        else if (ch == 'f')
        {
            CHR (ParseKeyword ("false", outValue));
            outValue = JsonValue (false);
        }
        else if (ch == 'n')
        {
            CHR (ParseKeyword ("null", outValue));
            outValue = JsonValue (nullptr);
        }
        else if (ch == '-' || (ch >= '0' && ch <= '9'))
        {
            CHR (ParseNumber (outValue));
        }
        else
        {
            SetError (std::format ("Unexpected character '{}'", ch));
            hr = E_FAIL;
            goto Error;
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

HRESULT JsonParser::ParseString (std::string & outStr)
{
    HRESULT hr = S_OK;

    CBREx (Peek () == '"', E_FAIL);
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
            CBREx (!AtEnd (), E_FAIL);

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
                    std::string hex;
                    for (int i = 0; i < 4; i++)
                    {
                        CBREx (!AtEnd (), E_FAIL);
                        hex += Advance ();
                    }
                    // Basic ASCII Unicode escape
                    unsigned long code = std::strtoul (hex.c_str (), nullptr, 16);
                    if (code < 0x80)
                    {
                        outStr += static_cast<char> (code);
                    }
                    break;
                }
                default:
                {
                    SetError (std::format ("Invalid escape sequence '\\{}'", esc));
                    hr = E_FAIL;
                    goto Error;
                }
            }
        }
        else
        {
            outStr += ch;
        }
    }

    SetError ("Unterminated string");
    hr = E_FAIL;

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

        while (!AtEnd () && std::isxdigit (static_cast<unsigned char> (Peek ())))
        {
            Advance ();
        }

        std::string hexStr = m_input.substr (start + 2, m_pos - start - 2);
        unsigned long value = std::strtoul (hexStr.c_str (), nullptr, 16);
        outValue = JsonValue (static_cast<double> (value));
        return S_OK;
    }

    // Standard JSON number
    if (Peek () == '-')
    {
        Advance ();
    }

    while (!AtEnd () && std::isdigit (static_cast<unsigned char> (Peek ())))
    {
        Advance ();
    }

    if (!AtEnd () && Peek () == '.')
    {
        Advance ();

        while (!AtEnd () && std::isdigit (static_cast<unsigned char> (Peek ())))
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

        while (!AtEnd () && std::isdigit (static_cast<unsigned char> (Peek ())))
        {
            Advance ();
        }
    }

    std::string numStr = m_input.substr (start, m_pos - start);
    double value = std::strtod (numStr.c_str (), nullptr);
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

    CBREx (Peek () == '{', E_FAIL);
    Advance ();

    {
        std::vector<std::pair<std::string, JsonValue>> entries;

        SkipWhitespace ();

        if (!AtEnd () && Peek () == '}')
        {
            Advance ();
            outValue = JsonValue (std::move (entries));
            return S_OK;
        }

        while (true)
        {
            SkipWhitespace ();
            CBREx (!AtEnd (), E_FAIL);

            if (Peek () != '"')
            {
                SetError ("Expected string key in object");
                hr = E_FAIL;
                goto Error;
            }

            std::string key;
            CHR (ParseString (key));

            SkipWhitespace ();
            CBREx (!AtEnd () && Peek () == ':', E_FAIL);
            Advance ();

            JsonValue val;
            CHR (ParseValue (val));

            entries.emplace_back (std::move (key), std::move (val));

            SkipWhitespace ();
            CBREx (!AtEnd (), E_FAIL);

            if (Peek () == '}')
            {
                Advance ();
                break;
            }

            if (Peek () != ',')
            {
                SetError ("Expected ',' or '}' in object");
                hr = E_FAIL;
                goto Error;
            }

            Advance ();
        }

        outValue = JsonValue (std::move (entries));
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

    CBREx (Peek () == '[', E_FAIL);
    Advance ();

    {
        std::vector<JsonValue> elements;

        SkipWhitespace ();

        if (!AtEnd () && Peek () == ']')
        {
            Advance ();
            outValue = JsonValue (std::move (elements));
            return S_OK;
        }

        while (true)
        {
            JsonValue val;
            CHR (ParseValue (val));

            elements.push_back (std::move (val));

            SkipWhitespace ();
            CBREx (!AtEnd (), E_FAIL);

            if (Peek () == ']')
            {
                Advance ();
                break;
            }

            if (Peek () != ',')
            {
                SetError ("Expected ',' or ']' in array");
                hr = E_FAIL;
                goto Error;
            }

            Advance ();
        }

        outValue = JsonValue (std::move (elements));
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

    size_t len = std::strlen (keyword);

    for (size_t i = 0; i < len; i++)
    {
        if (AtEnd () || Peek () != keyword[i])
        {
            SetError (std::format ("Expected '{}'", keyword));
            hr = E_FAIL;
            goto Error;
        }

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


void JsonParser::SetError (const std::string & msg)
{
    m_error.line    = m_line;
    m_error.column  = m_column;
    m_error.message = msg;
}
