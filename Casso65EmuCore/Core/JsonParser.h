#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  JsonType
//
////////////////////////////////////////////////////////////////////////////////

enum class JsonType
{
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};





////////////////////////////////////////////////////////////////////////////////
//
//  JsonValue
//
////////////////////////////////////////////////////////////////////////////////

class JsonValue
{
public:
    JsonValue ();
    explicit JsonValue (nullptr_t);
    explicit JsonValue (bool value);
    explicit JsonValue (double value);
    explicit JsonValue (const string & value);
    explicit JsonValue (vector<JsonValue> && arr);
    explicit JsonValue (vector<pair<string, JsonValue>> && obj);

    JsonType GetType () const { return m_type; }

    bool               GetBool   () const;
    double             GetNumber () const;
    int                GetInt    () const;
    const string & GetString () const;

    // Array access
    size_t             ArraySize  () const;
    const JsonValue &  ArrayAt    (size_t index) const;

    // Convenience
    bool IsNull   () const { return m_type == JsonType::Null; }
    bool IsString () const { return m_type == JsonType::String; }
    bool IsNumber () const { return m_type == JsonType::Number; }
    bool IsArray  () const { return m_type == JsonType::Array; }
    bool IsObject () const { return m_type == JsonType::Object; }
    bool IsBool   () const { return m_type == JsonType::Bool; }

    // Typed object accessors — key lookup + type check + value extraction
    HRESULT GetString (const string & key, string &              outValue) const;
    HRESULT GetNumber (const string & key, double &              outValue) const;
    HRESULT GetInt    (const string & key, int &                 outValue) const;
    HRESULT GetUint32 (const string & key, uint32_t &            outValue) const;
    HRESULT GetBool   (const string & key, bool &                outValue) const;
    HRESULT GetObject (const string & key, const JsonValue *&    outValue) const;
    HRESULT GetArray  (const string & key, const JsonValue *&    outValue) const;

    const vector<pair<string, JsonValue>> & GetObjectEntries () const;

private:
    // Internal helpers used by typed accessors
    bool               HasKey    (const string & key) const;
    const JsonValue &  Get       (const string & key) const;

    JsonType                                             m_type   = JsonType::Null;
    bool                                                 m_bool   = false;
    double                                               m_number = 0.0;
    string                                          m_string;
    vector<JsonValue>                               m_array;
    vector<pair<string, JsonValue>>       m_object;
};





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
