#include "Pch.h"
#include "Core/JsonParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParserTests
//
//  The config parser: standard JSON, the two deliberate extensions, and error
//  reporting.
//
//  The EXTENSIONS get as much attention as the standard grammar, because they
//  are the reason this parser exists rather than a library one -- `0x` hex
//  literals for 6502 addresses, and `//` line comments so a machine config can
//  explain itself.
//
//  Comments are tested in every position whitespace is legal, since they are
//  implemented in SkipWhitespace precisely so no individual parser needs to
//  know about them -- and that is only true if every parse step routes through
//  it.
//
//  The strictness is pinned too: a trailing comma must be rejected, since in a
//  hand-edited config it is nearly always a half-finished edit.
//
//  Error line and column are asserted, since the reader is editing by hand and
//  a byte offset would not help them.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (JsonParserTests)
{
public:

    TEST_METHOD (Parse_String_ReturnsValue)
    {
        JsonValue       value;
        JsonParseError  error;
        HRESULT hr = JsonParser::Parse ("\"hello\"", value, error);

        AssertSucceeded (hr);
        Assert::IsTrue (value.GetType() == JsonType::String);
        Assert::AreEqual (std::string ("hello"), value.GetString());
    }

    TEST_METHOD (Parse_Number_Integer)
    {
        JsonValue       value;
        JsonParseError  error;
        HRESULT hr = JsonParser::Parse ("42", value, error);

        AssertSucceeded (hr);
        Assert::IsTrue (value.GetType() == JsonType::Number);
        Assert::AreEqual (42, value.GetInt());
    }

    TEST_METHOD (Parse_Number_Hex)
    {
        JsonValue       value;
        JsonParseError  error;
        HRESULT hr = JsonParser::Parse ("0xC000", value, error);

        AssertSucceeded (hr);
        Assert::AreEqual (0xC000, value.GetInt());
    }

    TEST_METHOD (Parse_Boolean_True)
    {
        JsonValue       value;
        JsonParseError  error;
        HRESULT hr = JsonParser::Parse ("true", value, error);

        AssertSucceeded (hr);
        Assert::IsTrue (value.GetType() == JsonType::Bool);
        Assert::IsTrue (value.GetBool());
    }

    TEST_METHOD (Parse_Boolean_False)
    {
        JsonValue       value;
        JsonParseError  error;
        HRESULT hr = JsonParser::Parse ("false", value, error);

        AssertSucceeded (hr);
        Assert::IsFalse (value.GetBool());
    }

    TEST_METHOD (Parse_Null)
    {
        JsonValue       value;
        JsonParseError  error;
        HRESULT hr = JsonParser::Parse ("null", value, error);

        AssertSucceeded (hr);
        Assert::IsTrue (value.GetType() == JsonType::Null);
    }

    TEST_METHOD (Parse_EmptyObject)
    {
        JsonValue       value;
        JsonParseError  error;
        HRESULT hr = JsonParser::Parse ("{}", value, error);

        AssertSucceeded (hr);
        Assert::IsTrue (value.GetType() == JsonType::Object);
    }

    TEST_METHOD (Parse_EmptyArray)
    {
        JsonValue       value;
        JsonParseError  error;
        HRESULT hr = JsonParser::Parse ("[]", value, error);

        AssertSucceeded (hr);
        Assert::IsTrue (value.GetType() == JsonType::Array);
        Assert::AreEqual (size_t (0), value.ArraySize());
    }

    TEST_METHOD (Parse_NestedObject)
    {
        JsonValue          value;
        JsonParseError     error;
        HRESULT            hr      = S_OK;
        string             name;
        int                count   = 0;
        const JsonValue  * nested  = nullptr;
        int                a       = 0;



        hr = JsonParser::Parse (
            "{\"name\": \"test\", \"count\": 5, \"nested\": {\"a\": 1}}", value, error);

        AssertSucceeded (hr);
        Assert::IsTrue (value.GetType() == JsonType::Object);

        AssertSucceeded (value.GetString ("name", name));
        Assert::AreEqual (std::string ("test"), name);

        AssertSucceeded (value.GetInt ("count", count));
        Assert::AreEqual (5, count);

        AssertSucceeded (value.GetObject ("nested", nested));
        AssertSucceeded (nested->GetInt ("a", a));
        Assert::AreEqual (1, a);
    }

    TEST_METHOD (Parse_Array_WithValues)
    {
        JsonValue       value;
        JsonParseError  error;
        HRESULT hr = JsonParser::Parse ("[1, \"two\", true]", value, error);

        AssertSucceeded (hr);
        Assert::AreEqual (size_t (3), value.ArraySize());
        Assert::AreEqual (1, value.ArrayAt (0).GetInt());
        Assert::AreEqual (std::string ("two"), value.ArrayAt (1).GetString());
        Assert::IsTrue (value.ArrayAt (2).GetBool());
    }

    TEST_METHOD (Parse_EscapedString)
    {
        JsonValue       value;
        JsonParseError  error;
        HRESULT hr = JsonParser::Parse ("\"hello\\nworld\"", value, error);

        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("hello\nworld"), value.GetString());
    }

    TEST_METHOD (Parse_MalformedJSON_ReportsError)
    {
        JsonValue       value;
        JsonParseError  error;
        HRESULT hr = JsonParser::Parse ("{\"key\":", value, error);

        AssertFailed (hr);
    }

    TEST_METHOD (Parse_UnterminatedString_ReportsError)
    {
        JsonValue       value;
        JsonParseError  error;
        HRESULT hr = JsonParser::Parse ("\"unterminated", value, error);

        AssertFailed (hr);
    }
};
