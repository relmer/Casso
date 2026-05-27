#pragma once

#include "Pch.h"

#include "JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  JsonWriter
//
//  Serializes a JsonValue tree to compact-or-pretty JSON text.
//
//  Round-trip guarantee: for any string `s` such that JsonParser::Parse (s)
//  produces a JsonValue `v` without error, JsonParser::Parse (JsonWriter::Write (v))
//  produces a JsonValue structurally equal to `v`. Whitespace, key order,
//  and number formatting are NOT preserved across the round-trip.
//
//  Output encoding: UTF-8 strings are passed through verbatim. Control
//  characters (U+0000..U+001F), the backslash, and the double quote are
//  escaped per RFC 8259. Non-ASCII bytes (0x80..0xFF) are emitted unchanged
//  on the assumption the caller has already encoded as UTF-8.
//
//  Number formatting: integers (m_number == truncated double, within INT64
//  range) are written without a decimal point; non-integer doubles are
//  written with the shortest round-trip representation `std::format("{:g}", v)`
//  produces. Infinity and NaN are not legal JSON and produce an empty
//  string + HRESULT_FROM_WIN32 (ERROR_DATATYPE_MISMATCH).
//
////////////////////////////////////////////////////////////////////////////////

class JsonWriter
{
public:
    struct Options
    {
        bool  fPretty    = true;   // emit newlines + indentation
        int   indentSize = 2;      // spaces per indent level (pretty only)
    };

    static string  Write (const JsonValue & value);
    static HRESULT Write (const JsonValue & value,
                          const Options   & opts,
                          string          & outText);

private:
    static HRESULT WriteValue  (const JsonValue & v, int depth, const Options & opts, string & out);
    static void    WriteString (const string    & s, string & out);
    static HRESULT WriteNumber (double            n, string & out);
    static void    WriteIndent (int               depth, const Options & opts, string & out);
};
