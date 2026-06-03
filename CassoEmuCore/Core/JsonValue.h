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
    JsonValue () = default;
    explicit JsonValue (nullptr_t) {}

    explicit JsonValue (bool   value) : m_type (JsonType::Bool),   m_bool   (value) {}
    explicit JsonValue (double value) : m_type (JsonType::Number), m_number (value) {}

    explicit JsonValue (const string & value) : m_type (JsonType::String), m_string (value) {}

    explicit JsonValue (vector<JsonValue>               && arr) : m_type (JsonType::Array),  m_array  (move (arr)) {}
    explicit JsonValue (vector<pair<string, JsonValue>> && obj) : m_type (JsonType::Object), m_object (move (obj)) {}

    JsonType GetType () const { return m_type; }

    bool           GetBool   () const { return m_bool;   }
    double         GetNumber () const { return m_number; }
    int            GetInt    () const { return static_cast<int> (m_number); }
    const string & GetString () const { return m_string; }

    // Array access
    size_t             ArraySize  () const             { return m_array.size(); }
    const JsonValue &  ArrayAt    (size_t index) const { return m_array[index]; }

    // Typed object accessors — key lookup + type check + value extraction
    HRESULT GetString (const string & key, string &           outValue) const { return GetValue (key, JsonType::String, outValue); }
    HRESULT GetNumber (const string & key, double &           outValue) const { return GetValue (key, JsonType::Number, outValue); }
    HRESULT GetInt    (const string & key, int &              outValue) const { return GetValue (key, JsonType::Number, outValue); }
    HRESULT GetUint32 (const string & key, uint32_t &         outValue) const { return GetValue (key, JsonType::Number, outValue); }
    HRESULT GetBool   (const string & key, bool &             outValue) const { return GetValue (key, JsonType::Bool,   outValue); }
    HRESULT GetObject (const string & key, const JsonValue *& outValue) const { return GetValue (key, JsonType::Object, outValue); }
    HRESULT GetArray  (const string & key, const JsonValue *& outValue) const { return GetValue (key, JsonType::Array,  outValue); }

    const vector<pair<string, JsonValue>> & GetObjectEntries () const { return m_object; }

private:
    // Internal helpers used by typed accessors
    const JsonValue *  Find (const string & key) const;

    template <typename T>
    HRESULT GetValue (const string & key, JsonType expected, T & outValue) const;

    JsonType                        m_type   = JsonType::Null;
    bool                            m_bool   = false;
    double                          m_number = 0.0;
    string                          m_string;
    vector<JsonValue>               m_array;
    vector<pair<string, JsonValue>> m_object;
};





////////////////////////////////////////////////////////////////////////////////
//
//  JsonValue::GetValue
//
//  Shared implementation for every typed object accessor: look up the key,
//  verify the value's type, then extract it. The result type is deduced from
//  the caller's out-parameter and dispatched at compile time.
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
HRESULT JsonValue::GetValue (const string & key, JsonType expected, T & outValue) const
{
    HRESULT            hr    = S_OK;
    const JsonValue *  value = Find (key);



    CBREx (value != nullptr,          JSON_E_KEY_MISSING);
    CBREx (value->m_type == expected, JSON_E_TYPE_MISMATCH);

    if constexpr (is_same_v<T, string>)
    {
        outValue = value->m_string;
    }
    else if constexpr (is_same_v<T, bool>)
    {
        outValue = value->m_bool;
    }
    else if constexpr (is_same_v<T, double>)
    {
        outValue = value->m_number;
    }
    else if constexpr (is_same_v<T, int>)
    {
        outValue = static_cast<int> (value->m_number);
    }
    else if constexpr (is_same_v<T, uint32_t>)
    {
        outValue = static_cast<uint32_t> (value->m_number);
    }
    else
    {
        static_assert (is_same_v<T, const JsonValue *>, "JsonValue::GetValue: unsupported out-parameter type");
        outValue = value;
    }

Error:
    return hr;
}
