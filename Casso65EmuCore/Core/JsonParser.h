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
    int         line   = 0;
    int         column = 0;
    string message;
};





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser
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
    char Peek            () const;
    char Advance         ();
    bool AtEnd           () const;
    void SetError        (const string & msg);

    const string & m_input;
    size_t              m_pos;
    int                 m_line;
    int                 m_column;
    JsonParseError      m_error;
};
