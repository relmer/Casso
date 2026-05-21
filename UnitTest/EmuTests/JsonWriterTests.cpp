#include "Pch.h"

#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  JsonWriterTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (JsonWriterTests)
{
public:

    // Helper: round-trip parse -> write -> parse and confirm equal structure.
    static void ExpectRoundTrip (const string & jsonText)
    {
        JsonValue        parsed1;
        JsonValue        parsed2;
        JsonParseError   err;
        HRESULT          hr;
        string           rewritten;

        hr = JsonParser::Parse (jsonText, parsed1, err);
        Assert::IsTrue (SUCCEEDED (hr), L"first parse failed");

        rewritten = JsonWriter::Write (parsed1);

        hr = JsonParser::Parse (rewritten, parsed2, err);
        Assert::IsTrue (SUCCEEDED (hr), L"second parse failed");

        Assert::IsTrue (parsed1.GetType () == parsed2.GetType (), L"type mismatch");
    }



    TEST_METHOD (Write_Null)
    {
        JsonValue   v (nullptr);
        string      out = JsonWriter::Write (v);
        Assert::AreEqual (string ("null"), out);
    }

    TEST_METHOD (Write_Bool_True)
    {
        JsonValue  v (true);
        Assert::AreEqual (string ("true"), JsonWriter::Write (v));
    }

    TEST_METHOD (Write_Bool_False)
    {
        JsonValue  v (false);
        Assert::AreEqual (string ("false"), JsonWriter::Write (v));
    }

    TEST_METHOD (Write_Integer)
    {
        JsonValue  v (42.0);
        Assert::AreEqual (string ("42"), JsonWriter::Write (v));
    }

    TEST_METHOD (Write_NegativeInteger)
    {
        JsonValue  v (-12345.0);
        Assert::AreEqual (string ("-12345"), JsonWriter::Write (v));
    }

    TEST_METHOD (Write_Float)
    {
        JsonValue  v (3.14);
        string     out = JsonWriter::Write (v);

        // Verify it round-trips, not exact text (formatting may vary).
        JsonValue        parsed;
        JsonParseError   err;
        HRESULT          hr = JsonParser::Parse (out, parsed, err);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (3.14, parsed.GetNumber (), 1e-12);
    }

    TEST_METHOD (Write_String_Simple)
    {
        JsonValue  v (string ("hello"));
        Assert::AreEqual (string ("\"hello\""), JsonWriter::Write (v));
    }

    TEST_METHOD (Write_String_Escapes)
    {
        JsonValue  v (string ("a\"b\\c\nd\te"));
        Assert::AreEqual (string ("\"a\\\"b\\\\c\\nd\\te\""), JsonWriter::Write (v));
    }

    TEST_METHOD (Write_String_ControlChar)
    {
        string     raw;
        JsonValue  v;
        string     out;

        raw.push_back ((char) 0x01);
        v = JsonValue (raw);

        out = JsonWriter::Write (v);
        Assert::AreEqual (string ("\"\\u0001\""), out);
    }

    TEST_METHOD (Write_EmptyArray)
    {
        vector<JsonValue>  empty;
        JsonValue          v (move (empty));
        Assert::AreEqual (string ("[]"), JsonWriter::Write (v));
    }

    TEST_METHOD (Write_EmptyObject)
    {
        vector<pair<string, JsonValue>>  empty;
        JsonValue                         v (move (empty));
        Assert::AreEqual (string ("{}"), JsonWriter::Write (v));
    }

    TEST_METHOD (Write_Array_Compact)
    {
        vector<JsonValue>           items;
        JsonValue                   v;
        JsonWriter::Options         opts;
        string                      out;
        HRESULT                     hr;

        items.push_back (JsonValue (1.0));
        items.push_back (JsonValue (2.0));
        items.push_back (JsonValue (3.0));
        v = JsonValue (move (items));

        opts.fPretty = false;

        hr = JsonWriter::Write (v, opts, out);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (string ("[1,2,3]"), out);
    }

    TEST_METHOD (Write_Array_Pretty)
    {
        vector<JsonValue>     items;
        JsonValue             v;
        JsonWriter::Options   opts;
        string                out;
        HRESULT               hr;

        items.push_back (JsonValue (1.0));
        items.push_back (JsonValue (2.0));
        v = JsonValue (move (items));

        hr = JsonWriter::Write (v, opts, out);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (string ("[\n  1,\n  2\n]"), out);
    }

    TEST_METHOD (Write_Object_Compact)
    {
        vector<pair<string, JsonValue>>  entries;
        JsonValue                         v;
        JsonWriter::Options               opts;
        string                            out;
        HRESULT                           hr;

        entries.push_back ({ "a", JsonValue (1.0) });
        entries.push_back ({ "b", JsonValue (string ("two")) });
        v = JsonValue (move (entries));

        opts.fPretty = false;

        hr = JsonWriter::Write (v, opts, out);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (string ("{\"a\":1,\"b\":\"two\"}"), out);
    }

    TEST_METHOD (Write_Object_Pretty)
    {
        vector<pair<string, JsonValue>>  entries;
        JsonValue                         v;
        JsonWriter::Options               opts;
        string                            out;
        HRESULT                           hr;

        entries.push_back ({ "a", JsonValue (1.0) });
        v = JsonValue (move (entries));

        hr = JsonWriter::Write (v, opts, out);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (string ("{\n  \"a\": 1\n}"), out);
    }

    TEST_METHOD (RoundTrip_FlatObject)
    {
        ExpectRoundTrip ("{\"x\":1,\"y\":\"hi\",\"z\":true}");
    }

    TEST_METHOD (RoundTrip_NestedArrayOfObjects)
    {
        ExpectRoundTrip (
            "{\"items\":["
                "{\"id\":1,\"name\":\"a\"},"
                "{\"id\":2,\"name\":\"b\"}"
            "]}");
    }

    TEST_METHOD (RoundTrip_DeepNesting)
    {
        ExpectRoundTrip ("{\"a\":{\"b\":{\"c\":{\"d\":{\"e\":42}}}}}");
    }

    TEST_METHOD (RoundTrip_AllPrimitives)
    {
        ExpectRoundTrip (
            "{"
                "\"nul\":null,"
                "\"yes\":true,"
                "\"no\":false,"
                "\"num\":3.14,"
                "\"str\":\"hello\","
                "\"arr\":[1,2,3],"
                "\"obj\":{\"k\":\"v\"}"
            "}");
    }

    TEST_METHOD (Write_Nan_Fails)
    {
        JsonValue            v (std::nan (""));
        JsonWriter::Options  opts;
        string               out;
        HRESULT              hr;

        hr = JsonWriter::Write (v, opts, out);
        Assert::IsTrue (FAILED (hr));
    }
};
