#include "Pch.h"

#include "InMemoryFileSystem.h"

#include "Config/UserConfigStore.h"

#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  UserConfigStoreTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (UserConfigStoreTests)
{
public:

    // Helper: parse a JSON literal into a JsonValue, asserting success.
    static JsonValue ParseOrFail (const char * text)
    {
        JsonValue        v;
        JsonParseError   err;
        HRESULT          hr = JsonParser::Parse (text, v, err);
        Assert::IsTrue (SUCCEEDED (hr));
        return v;
    }


    // ---- Merge / Diff pure-logic tests ----------------------------------

    TEST_METHOD (Merge_Empty_User_Returns_Default)
    {
        JsonValue   d = ParseOrFail ("{\"a\":1,\"b\":2}");
        JsonValue   u = ParseOrFail ("{}");
        JsonValue   m = UserConfigStore::MergeJson (d, u);

        Assert::IsTrue (UserConfigStore::JsonEqual (d, m));
    }


    TEST_METHOD (Merge_User_Scalar_Overrides_Default)
    {
        JsonValue   d = ParseOrFail ("{\"a\":1,\"b\":2}");
        JsonValue   u = ParseOrFail ("{\"a\":99}");
        JsonValue   m = UserConfigStore::MergeJson (d, u);

        Assert::IsTrue (UserConfigStore::JsonEqual (ParseOrFail ("{\"a\":99,\"b\":2}"), m));
    }


    TEST_METHOD (Merge_Deep_Object_Merges)
    {
        JsonValue   d = ParseOrFail ("{\"crt\":{\"brightness\":1.0,\"bloom\":{\"enabled\":false}}}");
        JsonValue   u = ParseOrFail ("{\"crt\":{\"bloom\":{\"enabled\":true}}}");
        JsonValue   m = UserConfigStore::MergeJson (d, u);

        Assert::IsTrue (UserConfigStore::JsonEqual (ParseOrFail ("{\"crt\":{\"brightness\":1.0,\"bloom\":{\"enabled\":true}}}"), m));
    }


    TEST_METHOD (Merge_Array_Replaces_Wholesale)
    {
        JsonValue   d = ParseOrFail ("{\"arr\":[1,2,3]}");
        JsonValue   u = ParseOrFail ("{\"arr\":[9]}");
        JsonValue   m = UserConfigStore::MergeJson (d, u);

        Assert::IsTrue (UserConfigStore::JsonEqual (ParseOrFail ("{\"arr\":[9]}"), m));
    }


    TEST_METHOD (Merge_UserOnly_Key_Preserved)
    {
        JsonValue   d = ParseOrFail ("{\"a\":1}");
        JsonValue   u = ParseOrFail ("{\"lastMountedImages\":{\"6\":{\"0\":\"/img.dsk\"}}}");
        JsonValue   m = UserConfigStore::MergeJson (d, u);

        Assert::IsTrue (UserConfigStore::JsonEqual (ParseOrFail ("{\"a\":1,\"lastMountedImages\":{\"6\":{\"0\":\"/img.dsk\"}}}"), m));
    }


    TEST_METHOD (Diff_NoOp_Returns_Object_With_Just_Version)
    {
        JsonValue   d = ParseOrFail ("{\"$cassoMachineVersion\":1,\"a\":1,\"b\":2}");
        JsonValue   c = d;  // identical
        JsonValue   diff = UserConfigStore::DiffJson (c, d);

        Assert::IsTrue (diff.GetType() == JsonType::Object);
        Assert::AreEqual (size_t (1), diff.GetObjectEntries().size());
        Assert::AreEqual (string ("$cassoMachineVersion"), diff.GetObjectEntries()[0].first);
    }


    TEST_METHOD (Diff_Only_Differing_Keys)
    {
        JsonValue   d = ParseOrFail ("{\"$cassoMachineVersion\":1,\"a\":1,\"b\":2,\"c\":3}");
        JsonValue   c = ParseOrFail ("{\"$cassoMachineVersion\":1,\"a\":1,\"b\":99,\"c\":3}");
        JsonValue   diff = UserConfigStore::DiffJson (c, d);

        // Expect $cassoMachineVersion + b only.
        Assert::AreEqual (size_t (2), diff.GetObjectEntries().size());
    }


    TEST_METHOD (Diff_Deep_Object_Only_Inner_Diff)
    {
        JsonValue   d = ParseOrFail ("{\"crt\":{\"brightness\":1.0,\"bloomEnabled\":false}}");
        JsonValue   c = ParseOrFail ("{\"crt\":{\"brightness\":1.0,\"bloomEnabled\":true}}");
        JsonValue   diff = UserConfigStore::DiffJson (c, d);

        // The crt sub-object should appear with only bloomEnabled inside.
        Assert::AreEqual (size_t (1), diff.GetObjectEntries().size());
        Assert::AreEqual (string ("crt"), diff.GetObjectEntries()[0].first);

        const JsonValue & crtDiff = diff.GetObjectEntries()[0].second;
        Assert::IsTrue (crtDiff.GetType() == JsonType::Object);
        Assert::AreEqual (size_t (1), crtDiff.GetObjectEntries().size());
        Assert::AreEqual (string ("bloomEnabled"), crtDiff.GetObjectEntries()[0].first);
    }


    // ---- Full Load / SaveDelta / Reset round-trips ----------------------

    TEST_METHOD (Load_NoUserFile_ReturnsDefault)
    {
        InMemoryFileSystem  fs;
        UserConfigStore     store (L"C:\\Casso\\User");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"a\":1}");
        JsonValue           merged;
        HRESULT             hr;

        hr = store.Load ("Apple2e", defaultJson, fs, merged);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue (UserConfigStore::JsonEqual (defaultJson, merged));
    }


    TEST_METHOD (Load_WithPartialUserFile_MergesCorrectly)
    {
        InMemoryFileSystem  fs;
        UserConfigStore     store (L"C:\\Casso\\User");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Authentic\",\"a\":1}");
        JsonValue           merged;
        HRESULT             hr;

        hr = fs.WriteAllText (store.UserFilePath ("Apple2e"),
                              "{\"$cassoMachineVersion\":1,\"speedMode\":\"Maximum\"}");
        Assert::IsTrue (SUCCEEDED (hr));

        hr = store.Load ("Apple2e", defaultJson, fs, merged);
        Assert::IsTrue (SUCCEEDED (hr));

        JsonValue  expected = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Maximum\",\"a\":1}");
        Assert::IsTrue (UserConfigStore::JsonEqual (expected, merged));
    }


    TEST_METHOD (SaveDelta_WritesOnlyDifferences)
    {
        InMemoryFileSystem    fs;
        UserConfigStore       store (L"C:\\Casso\\User");
        JsonValue             defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Authentic\",\"a\":1}");
        JsonValue             current     = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Double\",\"a\":1}");
        HRESULT               hr;
        std::string           text;
        JsonValue             parsed;
        JsonParseError        err;

        hr = store.SaveDelta ("Apple2e", current, defaultJson, fs);
        Assert::IsTrue (SUCCEEDED (hr));

        text = fs.PeekContent (store.UserFilePath ("Apple2e"));
        Assert::IsFalse (text.empty());

        hr = JsonParser::Parse (text, parsed, err);
        Assert::IsTrue (SUCCEEDED (hr));

        // Should contain exactly $cassoMachineVersion + speedMode.
        Assert::AreEqual (size_t (2), parsed.GetObjectEntries().size());
    }


    TEST_METHOD (SaveDelta_Noop_StillWritesVersionStamp)
    {
        InMemoryFileSystem  fs;
        UserConfigStore     store (L"C:\\Casso\\User");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"a\":1}");
        HRESULT             hr;
        JsonValue           parsed;
        JsonParseError      err;
        std::string         text;

        hr = store.SaveDelta ("Apple2e", defaultJson, defaultJson, fs);
        Assert::IsTrue (SUCCEEDED (hr));

        text = fs.PeekContent (store.UserFilePath ("Apple2e"));
        hr   = JsonParser::Parse (text, parsed, err);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (size_t (1), parsed.GetObjectEntries().size());
        Assert::AreEqual (string ("$cassoMachineVersion"), parsed.GetObjectEntries()[0].first);
    }


    TEST_METHOD (Reset_Deletes_UserFile)
    {
        InMemoryFileSystem  fs;
        UserConfigStore     store (L"C:\\Casso\\User");
        HRESULT             hr;

        hr = fs.WriteAllText (store.UserFilePath ("Apple2e"),
                              "{\"$cassoMachineVersion\":1}");
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue (fs.Exists (store.UserFilePath ("Apple2e")));

        hr = store.Reset ("Apple2e", fs);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsFalse (fs.Exists (store.UserFilePath ("Apple2e")));
    }


    TEST_METHOD (Reset_Idempotent_When_File_Missing)
    {
        InMemoryFileSystem  fs;
        UserConfigStore     store (L"C:\\Casso\\User");
        HRESULT             hr;

        hr = store.Reset ("Apple2e", fs);
        Assert::IsTrue (SUCCEEDED (hr));
    }


    TEST_METHOD (Load_LegacyVersionKey_TriggersMigration_WritesBack)
    {
        InMemoryFileSystem  fs;
        UserConfigStore     store (L"C:\\Casso\\User");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":2,\"a\":1}");
        JsonValue           merged;
        HRESULT             hr;
        std::string         original    = "{\"$cassoDefault\":1,\"a\":1}";
        std::string         afterLoad;

        hr = fs.WriteAllText (store.UserFilePath ("Apple2e"), original);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = store.Load ("Apple2e", defaultJson, fs, merged);
        Assert::IsTrue (SUCCEEDED (hr));

        afterLoad = fs.PeekContent (store.UserFilePath ("Apple2e"));
        // After migration the legacy key should be gone.
        Assert::IsTrue (afterLoad.find ("$cassoDefault") == std::string::npos);
        Assert::IsTrue (afterLoad.find ("$cassoMachineVersion") != std::string::npos);
    }


    TEST_METHOD (Merge_HardwareEnableDelta_OverlaysDefaultArrays)
    {
        JsonValue d = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "internalDevices": [
                { "type": "keyboard", "capabilityFlag": "required", "enabled": true }
            ],
            "slots": [
                { "slot": 6, "device": "disk-ii", "enabled": true }
            ]
        })JSON");

        JsonValue u = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "internalDevices": [
                { "type": "keyboard", "enabled": false }
            ],
            "slots": [
                { "slot": 6, "enabled": false }
            ]
        })JSON");

        JsonValue m = UserConfigStore::MergeJson (d, u);
        JsonValue expected = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "internalDevices": [
                { "type": "keyboard", "capabilityFlag": "required", "enabled": false }
            ],
            "slots": [
                { "slot": 6, "device": "disk-ii", "enabled": false }
            ]
        })JSON");

        Assert::IsTrue (UserConfigStore::JsonEqual (expected, m));
    }


    TEST_METHOD (Diff_HardwareEnableDelta_EmitsMinimalComponentShape)
    {
        JsonValue d = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "internalDevices": [
                { "type": "keyboard", "capabilityFlag": "required", "enabled": true }
            ],
            "slots": [
                { "slot": 6, "device": "disk-ii", "enabled": true }
            ]
        })JSON");

        JsonValue c = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "internalDevices": [
                { "type": "keyboard", "capabilityFlag": "required", "enabled": false }
            ],
            "slots": [
                { "slot": 6, "device": "disk-ii", "enabled": false }
            ]
        })JSON");

        JsonValue diff = UserConfigStore::DiffJson (c, d);
        const JsonValue * internal = nullptr;
        const JsonValue * slots    = nullptr;

        Assert::IsTrue (SUCCEEDED (diff.GetArray ("internalDevices", internal)));
        Assert::IsTrue (SUCCEEDED (diff.GetArray ("slots", slots)));
        Assert::AreEqual<size_t> (1u, internal->ArraySize());
        Assert::AreEqual<size_t> (1u, slots->ArraySize());
        const JsonValue & int0  = internal->ArrayAt (0);
        const JsonValue & slot0 = slots->ArrayAt (0);
        Assert::AreEqual<size_t> (2u, int0.GetObjectEntries().size());
        Assert::AreEqual<size_t> (2u, slot0.GetObjectEntries().size());
    }


    TEST_METHOD (Diff_UiPrefs_UsesImplicitDefaultsForSpeedShadowing)
    {
        JsonValue d = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "name": "Apple2e"
        })JSON");

        JsonValue c = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "name": "Apple2e",
            "$cassoUiPrefs": {
                "speedMode": "double",
                "colorMode": "color",
                "floppySoundEnabled": true,
                "floppyMechanism": "shugart",
                "writeProtect": [ false, false ]
            }
        })JSON");

        JsonValue diff = UserConfigStore::DiffJson (c, d);
        const JsonValue * ui = nullptr;

        Assert::IsTrue (SUCCEEDED (diff.GetObject ("$cassoUiPrefs", ui)));
        Assert::IsTrue (ui != nullptr);
        Assert::AreEqual<size_t> (1u, ui->GetObjectEntries().size());
        Assert::AreEqual (string ("speedMode"), ui->GetObjectEntries()[0].first);
    }


    TEST_METHOD (Merge_SpeedShadow_PreservesDefaultFallthroughFields)
    {
        JsonValue d = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "newField": { "fromDefault": true }
        })JSON");
        JsonValue u = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "$cassoUiPrefs": { "speedMode": "maximum" }
        })JSON");

        JsonValue m = UserConfigStore::MergeJson (d, u);
        const JsonValue * nf = nullptr;
        const JsonValue * ui = nullptr;

        Assert::IsTrue (SUCCEEDED (m.GetObject ("newField", nf)));
        Assert::IsTrue (SUCCEEDED (m.GetObject ("$cassoUiPrefs", ui)));
        Assert::AreEqual (std::string ("maximum"), ui->GetObjectEntries()[0].second.GetString());
    }
};
