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
    explicit JsonValue (std::nullptr_t);
    explicit JsonValue (bool value);
    explicit JsonValue (double value);
    explicit JsonValue (const std::string & value);
    explicit JsonValue (std::vector<JsonValue> && arr);
    explicit JsonValue (std::vector<std::pair<std::string, JsonValue>> && obj);

    JsonType GetType () const { return m_type; }

    bool               GetBool   () const;
    double             GetNumber () const;
    int                GetInt    () const;
    const std::string & GetString () const;

    // Array access
    size_t             ArraySize  () const;
    const JsonValue &  ArrayAt    (size_t index) const;

    // Object access
    bool               HasKey    (const std::string & key) const;
    const JsonValue &  Get       (const std::string & key) const;

    // Convenience
    bool IsNull   () const { return m_type == JsonType::Null; }
    bool IsString () const { return m_type == JsonType::String; }
    bool IsNumber () const { return m_type == JsonType::Number; }
    bool IsArray  () const { return m_type == JsonType::Array; }
    bool IsObject () const { return m_type == JsonType::Object; }
    bool IsBool   () const { return m_type == JsonType::Bool; }

    const std::vector<std::pair<std::string, JsonValue>> & GetObjectEntries () const;

private:
    JsonType                                             m_type;
    bool                                                 m_bool;
    double                                               m_number;
    std::string                                          m_string;
    std::vector<JsonValue>                               m_array;
    std::vector<std::pair<std::string, JsonValue>>       m_object;
};





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParseError
//
////////////////////////////////////////////////////////////////////////////////

struct JsonParseError
{
    int         line;
    int         column;
    std::string message;
};





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParser
//
////////////////////////////////////////////////////////////////////////////////

class JsonParser
{
public:
    static HRESULT Parse (const std::string & input, JsonValue & outValue, JsonParseError & outError);

private:
    JsonParser (const std::string & input);

    HRESULT ParseValue   (JsonValue & outValue);
    HRESULT ParseString  (std::string & outStr);
    HRESULT ParseNumber  (JsonValue & outValue);
    HRESULT ParseObject  (JsonValue & outValue);
    HRESULT ParseArray   (JsonValue & outValue);
    HRESULT ParseKeyword (const char * keyword, JsonValue & outValue);

    void SkipWhitespace ();
    char Peek            () const;
    char Advance         ();
    bool AtEnd           () const;
    void SetError        (const std::string & msg);

    const std::string & m_input;
    size_t              m_pos;
    int                 m_line;
    int                 m_column;
    JsonParseError      m_error;
};
