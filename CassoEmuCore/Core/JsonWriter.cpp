#include "Pch.h"

#include "JsonWriter.h"

#include "../../CassoCore/Ehm.h"






////////////////////////////////////////////////////////////////////////////////
//
//  JsonWriter::Write (compact)
//
//  Compact write (no whitespace). Always succeeds for well-formed JsonValue.
//
////////////////////////////////////////////////////////////////////////////////

string JsonWriter::Write (const JsonValue & value)
{
    HRESULT  hr      = S_OK;
    Options  opts    = {};
    string   text;



    opts.fPretty = false;

    IGNORE_RETURN_VALUE (hr, Write (value, opts, text));
    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonWriter::Write (with options)
//
//  Pretty/compact write per `opts`. Returns S_OK on success;
//  HRESULT_FROM_WIN32 (ERROR_DATATYPE_MISMATCH) if `value` (or any nested
//  value) is a Number that is not finite (NaN or +/-Inf).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonWriter::Write (
    const JsonValue   & value,
    const Options     & opts,
    string            & outText)
{
    HRESULT hr = S_OK;



    outText.clear();
    hr = WriteValue (value, 0, opts, outText);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonWriter::WriteValue
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonWriter::WriteValue (
    const JsonValue   & v,
    int                 depth,
    const Options     & opts,
    string            & out)
{
    HRESULT hr = S_OK;



    switch (v.GetType())
    {
        case JsonType::Null:
        {
            out += "null";
            break;
        }

        case JsonType::Bool:
        {
            out += v.GetBool() ? "true" : "false";
            break;
        }

        case JsonType::Number:
        {
            hr = WriteNumber (v.GetNumber(), out);
            CHR (hr);
            break;
        }

        case JsonType::String:
        {
            WriteString (v.GetString(), out);
            break;
        }

        case JsonType::Array:
        {
            size_t  count = v.ArraySize();

            if (count == 0)
            {
                out += "[]";
                break;
            }

            out += '[';

            for (size_t i = 0; i < count; ++i)
            {
                if (opts.fPretty)
                {
                    out += '\n';
                    WriteIndent (depth + 1, opts, out);
                }

                hr = WriteValue (v.ArrayAt (i), depth + 1, opts, out);
                CHR (hr);

                if (i + 1 < count)
                {
                    out += ',';
                    if (!opts.fPretty)
                    {
                        // tight: no trailing space
                    }
                }
            }

            if (opts.fPretty)
            {
                out += '\n';
                WriteIndent (depth, opts, out);
            }

            out += ']';
            break;
        }

        case JsonType::Object:
        {
            const auto  & entries = v.GetObjectEntries();

            if (entries.empty())
            {
                out += "{}";
                break;
            }

            out += '{';

            for (size_t i = 0; i < entries.size(); ++i)
            {
                if (opts.fPretty)
                {
                    out += '\n';
                    WriteIndent (depth + 1, opts, out);
                }

                WriteString (entries[i].first, out);
                out += ':';
                if (opts.fPretty)
                {
                    out += ' ';
                }

                hr = WriteValue (entries[i].second, depth + 1, opts, out);
                CHR (hr);

                if (i + 1 < entries.size())
                {
                    out += ',';
                }
            }

            if (opts.fPretty)
            {
                out += '\n';
                WriteIndent (depth, opts, out);
            }

            out += '}';
            break;
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonWriter::WriteString
//
//  Writes `s` as a JSON-quoted string. Escapes per RFC 8259 §7.
//
////////////////////////////////////////////////////////////////////////////////

void JsonWriter::WriteString (const string & s, string & out)
{
    char    buf[8];
    size_t  i = 0;



    out += '"';

    for (i = 0; i < s.size(); ++i)
    {
        unsigned char c = (unsigned char) s[i];

        switch (c)
        {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20)
                {
                    std::snprintf (buf, sizeof (buf), "\\u%04x", c);
                    out += buf;
                }
                else
                {
                    // Pass through. Bytes >= 0x80 are assumed UTF-8.
                    out += (char) c;
                }
                break;
        }
    }

    out += '"';
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonWriter::WriteNumber
//
////////////////////////////////////////////////////////////////////////////////

HRESULT JsonWriter::WriteNumber (double n, string & out)
{
    HRESULT  hr        = S_OK;
    char     buf[64]   = {};
    int      written   = 0;
    double   truncated = 0.0;



    if (!std::isfinite (n))
    {
        hr = HRESULT_FROM_WIN32 (ERROR_DATATYPE_MISMATCH);
        CHR (hr);
    }

    truncated = std::trunc (n);

    if (n == truncated &&
        n >= -9.2233720368547758e18 &&
        n <=  9.2233720368547758e18)
    {
        // Integer-valued double in int64 range: emit without decimal.
        written = std::snprintf (buf, sizeof (buf), "%lld", (long long) n);
    }
    else
    {
        // Shortest round-trip via %.17g (always round-trips a double).
        written = std::snprintf (buf, sizeof (buf), "%.17g", n);
    }

    if (written > 0)
    {
        out.append (buf, (size_t) written);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  JsonWriter::WriteIndent
//
////////////////////////////////////////////////////////////////////////////////

void JsonWriter::WriteIndent (int depth, const Options & opts, string & out)
{
    int     count = depth * opts.indentSize;
    int     i     = 0;



    for (i = 0; i < count; ++i)
    {
        out += ' ';
    }
}
