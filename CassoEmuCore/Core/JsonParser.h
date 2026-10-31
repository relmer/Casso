#pragma once

#include "Pch.h"

#include "JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParseError
//
////////////////////////////////////////////////////////////////////////////////

struct JsonParseError
{
    int     line    = 0;
    int     column  = 0;
    string  message;
};





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser
//
//  A recursive-descent JSON parser for Casso's own configuration files.
//
//  The only public entry point is a static Parse; the object itself is private
//  and per-parse, so no parse can inherit position or error state from
//  another.
//
//  Deliberately NOT a general-purpose JSON library. It extends the grammar in
//  two ways its callers need -- `0x` hex literals, because these files are
//  full of 6502 addresses, and `//` line comments, because a machine config
//  wants to explain itself -- and both are safe precisely because only
//  first-party files are ever read.
//
//  Line and column are tracked alongside the position so a parse error can
//  point at a place in the file. The reader is hand-editing JSON, and a byte
//  offset would not help them.
//
//  The input is held by REFERENCE, so a parser must not outlive the string it
//  reads. The static entry point makes that automatic.
//
////////////////////////////////////////////////////////////////////////////////

class JsonParser
{
public:
    static HRESULT Parse (const string & input, JsonValue & outValue, JsonParseError & outError);

private:
    JsonParser (const string & input);

    HRESULT ParseValue   (JsonValue & outValue);
    HRESULT ParseString  (string & outStr);
    HRESULT ParseNumber  (JsonValue & outValue);
    HRESULT ParseObject  (JsonValue & outValue);
    HRESULT ParseArray   (JsonValue & outValue);
    HRESULT ParseKeyword (const char * keyword, JsonValue & outValue);

    void SkipWhitespace ();
    char Peek     () const;
    char Advance  ();
    bool IsAtEnd  () const;
    void SetError (const string & msg);

    const string    & m_input;
    size_t            m_pos;
    int               m_line;
    int               m_column;
    JsonParseError    m_error;
};
