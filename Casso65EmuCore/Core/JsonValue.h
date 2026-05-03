#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  JSON error codes
//
////////////////////////////////////////////////////////////////////////////////

static const HRESULT JSON_E_KEY_MISSING   = HRESULT_FROM_WIN32 (ERROR_NOT_FOUND);
static const HRESULT JSON_E_TYPE_MISMATCH = HRESULT_FROM_WIN32 (ERROR_DATATYPE_MISMATCH);





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
