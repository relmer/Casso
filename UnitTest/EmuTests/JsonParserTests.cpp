#include "../Casso65EmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/JsonParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  JsonParserTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (JsonParserTests)
{
public:

    TEST_METHOD (Parse_String_ReturnsValue)
    {
        JsonValue value;
        JsonParseError error;
        HRESULT hr = JsonParser::Parse ("\"hello\"", value, error);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue (value.IsString ());
        Assert::AreEqual (std::string ("hello"), value.GetString ());
    }

    TEST_METHOD (Parse_Number_Integer)
    {
        JsonValue value;
        JsonParseError error;
        HRESULT hr = JsonParser::Parse ("42", value, error);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue (value.IsNumber ());
        Assert::AreEqual (42, value.GetInt ());
    }

    TEST_METHOD (Parse_Number_Hex)
    {
        JsonValue value;
        JsonParseError error;
        HRESULT hr = JsonParser::Parse ("0xC000", value, error);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (0xC000, value.GetInt ());
    }

    TEST_METHOD (Parse_Boolean_True)
    {
        JsonValue value;
        JsonParseError error;
        HRESULT hr = JsonParser::Parse ("true", value, error);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue (value.IsBool ());
        Assert::IsTrue (value.GetBool ());
    }

    TEST_METHOD (Parse_Boolean_False)
    {
        JsonValue value;
        JsonParseError error;
        HRESULT hr = JsonParser::Parse ("false", value, error);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsFalse (value.GetBool ());
    }

    TEST_METHOD (Parse_Null)
    {
        JsonValue value;
        JsonParseError error;
        HRESULT hr = JsonParser::Parse ("null", value, error);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue (value.IsNull ());
    }

    TEST_METHOD (Parse_EmptyObject)
    {
        JsonValue value;
        JsonParseError error;
        HRESULT hr = JsonParser::Parse ("{}", value, error);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue (value.IsObject ());
    }

    TEST_METHOD (Parse_EmptyArray)
    {
        JsonValue value;
        JsonParseError error;
        HRESULT hr = JsonParser::Parse ("[]", value, error);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue (value.IsArray ());
        Assert::AreEqual (size_t (0), value.ArraySize ());
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

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue (value.IsObject ());

        Assert::IsTrue (SUCCEEDED (value.GetString ("name", name)));
        Assert::AreEqual (std::string ("test"), name);

        Assert::IsTrue (SUCCEEDED (value.GetInt ("count", count)));
        Assert::AreEqual (5, count);

        Assert::IsTrue (SUCCEEDED (value.GetObject ("nested", nested)));
        Assert::IsTrue (SUCCEEDED (nested->GetInt ("a", a)));
        Assert::AreEqual (1, a);
    }

    TEST_METHOD (Parse_Array_WithValues)
    {
        JsonValue value;
        JsonParseError error;
        HRESULT hr = JsonParser::Parse ("[1, \"two\", true]", value, error);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (size_t (3), value.ArraySize ());
        Assert::AreEqual (1, value.ArrayAt (0).GetInt ());
        Assert::AreEqual (std::string ("two"), value.ArrayAt (1).GetString ());
        Assert::IsTrue (value.ArrayAt (2).GetBool ());
    }

    TEST_METHOD (Parse_EscapedString)
    {
        JsonValue value;
        JsonParseError error;
        HRESULT hr = JsonParser::Parse ("\"hello\\nworld\"", value, error);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (std::string ("hello\nworld"), value.GetString ());
    }

    TEST_METHOD (Parse_MalformedJSON_ReportsError)
    {
        JsonValue value;
        JsonParseError error;
        HRESULT hr = JsonParser::Parse ("{\"key\":", value, error);

        Assert::IsTrue (FAILED (hr));
    }

    TEST_METHOD (Parse_UnterminatedString_ReportsError)
    {
        JsonValue value;
        JsonParseError error;
        HRESULT hr = JsonParser::Parse ("\"unterminated", value, error);

        Assert::IsTrue (FAILED (hr));
    }
};
