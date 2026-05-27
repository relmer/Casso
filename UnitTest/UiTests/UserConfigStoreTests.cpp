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


    static JsonValue ReadMachineOrFail (
        InMemoryFileSystem  & fs,
        UserConfigStore     & store,
        const std::string   & machineName)
    {
        std::string        text;
        JsonValue          root;
        JsonParseError     err;
        const JsonValue  * machines = nullptr;
        const JsonValue  * machine  = nullptr;
        HRESULT           hr        = S_OK;


        hr = fs.ReadAllText (store.UserFilePath (machineName), text);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = JsonParser::Parse (text, root, err);
        Assert::IsTrue (SUCCEEDED (hr));

        if (SUCCEEDED (root.GetObject ("machines", machines)) && machines != nullptr)
        {
            hr = machines->GetObject (machineName, machine);
            Assert::IsTrue (SUCCEEDED (hr));
            if (machine == nullptr)
            {
                Assert::Fail (L"missing machine entry");
                return JsonValue();
            }
            return *machine;
        }

        return root;
    }


    static std::string MachineTextOrFail (
        InMemoryFileSystem  & fs,
        UserConfigStore     & store,
        const std::string   & machineName)
    {
        JsonValue            machine = ReadMachineOrFail (fs, store, machineName);
        JsonWriter::Options  opts;
        std::string          text;
        HRESULT              hr      = S_OK;


        opts.fPretty = true;
        hr = JsonWriter::Write (machine, opts, text);
        Assert::IsTrue (SUCCEEDED (hr));
        return text;
    }


    static const JsonValue * FindObjectValueForTest (
        const JsonValue   & obj,
        const std::string & key)
    {
        if (obj.GetType() != JsonType::Object)
        {
            return nullptr;
        }

        for (const auto & kv : obj.GetObjectEntries())
        {
            if (kv.first == key)
            {
                return &kv.second;
            }
        }

        return nullptr;
    }


    static std::wstring WidenForTest (const std::string & narrow)
    {
        std::wstring  out;


        out.reserve (narrow.size());
        for (char c : narrow)
        {
            out.push_back ((wchar_t) (unsigned char) c);
        }
        return out;
    }


    static std::wstring LegacyGlobalPathForTest (const std::wstring & baseDir)
    {
        return baseDir + L"\\" + std::wstring (L"Global") + L"User" + L"Prefs" + L".json";
    }


    static std::wstring LegacyMachinePathForTest (
        const std::wstring & baseDir,
        const std::string  & machineName)
    {
        return baseDir + L"\\" + WidenForTest (machineName) + std::wstring (L"_") + L"user" + L".json";
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

        text = MachineTextOrFail (fs, store, "Apple2e");
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

        text = MachineTextOrFail (fs, store, "Apple2e");
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
        Assert::IsTrue (fs.Exists (store.UserFilePath ("Apple2e")));
        Assert::IsTrue (fs.PeekContent (store.UserFilePath ("Apple2e")).find ("Apple2e") == std::string::npos);
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

        afterLoad = MachineTextOrFail (fs, store, "Apple2e");
        // After migration the legacy key should be gone.
        Assert::IsTrue (afterLoad.find ("$cassoDefault") == std::string::npos);
        Assert::IsTrue (afterLoad.find ("$cassoMachineVersion") != std::string::npos);
    }


    TEST_METHOD (Load_BothVersionKeys_CanonicalizesToMachineVersionOnly)
    {
        InMemoryFileSystem  fs;
        UserConfigStore     store (L"C:\\Casso\\User");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":9,\"a\":1}");
        JsonValue           merged;
        HRESULT             hr;
        std::string         original = "{\"$cassoMachineVersion\":9,\"$cassoDefault\":4,\"a\":1}";
        std::string         afterLoad;

        hr = fs.WriteAllText (store.UserFilePath ("Apple2e"), original);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = store.Load ("Apple2e", defaultJson, fs, merged);
        Assert::IsTrue (SUCCEEDED (hr));

        afterLoad = MachineTextOrFail (fs, store, "Apple2e");
        Assert::IsTrue (afterLoad.find ("$cassoDefault") == std::string::npos);
        Assert::IsTrue (afterLoad.find ("\"$cassoMachineVersion\": 9") != std::string::npos);
    }


    TEST_METHOD (Load_ThreeConsecutiveUpgrades_PreservesOverridesAndAdvancesStamp)
    {
        InMemoryFileSystem  fs;
        UserConfigStore     store (L"C:\\Casso\\User");
        JsonValue           merged;
        JsonValue           d2 = ParseOrFail ("{\"$cassoMachineVersion\":2,\"newV2\":true,\"$cassoUiPrefs\":{\"speedMode\":\"authentic\"}}");
        JsonValue           d3 = ParseOrFail ("{\"$cassoMachineVersion\":3,\"newV2\":true,\"newV3\":true,\"$cassoUiPrefs\":{\"speedMode\":\"authentic\"}}");
        JsonValue           d4 = ParseOrFail ("{\"$cassoMachineVersion\":4,\"newV2\":true,\"newV3\":true,\"newV4\":true,\"$cassoUiPrefs\":{\"speedMode\":\"authentic\"}}");
        HRESULT             hr;
        std::string         text;

        hr = fs.WriteAllText (store.UserFilePath ("Apple2e"),
                              "{\"$cassoDefault\":1,\"$cassoUiPrefs\":{\"speedMode\":\"maximum\"}}");
        Assert::IsTrue (SUCCEEDED (hr));

        hr = store.Load ("Apple2e", d2, fs, merged);
        Assert::IsTrue (SUCCEEDED (hr));
        text = MachineTextOrFail (fs, store, "Apple2e");
        Assert::IsTrue (text.find ("\"$cassoMachineVersion\": 2") != std::string::npos);
        Assert::IsTrue (text.find ("$cassoDefault") == std::string::npos);
        Assert::IsTrue (text.find ("\"speedMode\": \"maximum\"") != std::string::npos);

        hr = store.Load ("Apple2e", d3, fs, merged);
        Assert::IsTrue (SUCCEEDED (hr));
        text = MachineTextOrFail (fs, store, "Apple2e");
        Assert::IsTrue (text.find ("\"$cassoMachineVersion\": 3") != std::string::npos);
        Assert::IsTrue (text.find ("\"speedMode\": \"maximum\"") != std::string::npos);

        hr = store.Load ("Apple2e", d4, fs, merged);
        Assert::IsTrue (SUCCEEDED (hr));
        text = MachineTextOrFail (fs, store, "Apple2e");
        Assert::IsTrue (text.find ("\"$cassoMachineVersion\": 4") != std::string::npos);
        Assert::IsTrue (text.find ("\"speedMode\": \"maximum\"") != std::string::npos);
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
        if (ui == nullptr) { return; }
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

    TEST_METHOD (Merge_LastMountedImages_UserOnly_Preserved)
    {
        JsonValue  d = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3
        })JSON");
        JsonValue  u = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "lastMountedImages": ["C:\\disk0.dsk", ""]
        })JSON");

        JsonValue          m       = UserConfigStore::MergeJson (d, u);
        const JsonValue *  arr     = nullptr;

        Assert::IsTrue (SUCCEEDED (m.GetArray ("lastMountedImages", arr)));
        Assert::AreEqual (size_t (2), arr->ArraySize());
        Assert::AreEqual (std::string ("C:\\disk0.dsk"), arr->ArrayAt (0).GetString());
    }


    TEST_METHOD (UnifiedPrefs_RoundTrip_GlobalAndMachineValuesStick)
    {
        InMemoryFileSystem  fs;
        UserConfigStore     store (L"C:\\Casso");
        UserConfigStore     reloadedStore (L"C:\\Casso");
        GlobalUserPrefs     prefs;
        GlobalUserPrefs     reloadedPrefs;
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":2,\"speedMode\":\"authentic\"}");
        JsonValue           currentJson = ParseOrFail ("{\"$cassoMachineVersion\":2,\"speedMode\":\"maximum\"}");
        JsonValue           merged;
        HRESULT             hr = S_OK;


        hr = store.LoadAll (prefs, fs);
        Assert::IsTrue (hr == S_FALSE);

        prefs.activeTheme = "Retro Terminal";
        hr = store.SaveDelta ("Apple //e Enhanced", currentJson, defaultJson, fs);
        Assert::IsTrue (SUCCEEDED (hr));
        hr = store.SaveAll (prefs, fs);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = reloadedStore.LoadAll (reloadedPrefs, fs);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (std::string ("Retro Terminal"), reloadedPrefs.activeTheme);

        hr = reloadedStore.Load ("Apple //e Enhanced", defaultJson, fs, merged);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (std::string ("maximum"), FindObjectValueForTest (merged, "speedMode")->GetString());
    }


    TEST_METHOD (UnifiedPrefs_MigratesLegacyFilesAndDeletesOldFiles)
    {
        InMemoryFileSystem  fs;
        std::wstring        baseDir = L"C:\\Casso";
        UserConfigStore     store (baseDir);
        GlobalUserPrefs     prefs;
        HRESULT             hr = S_OK;
        JsonValue           foo;
        JsonValue           bar;


        hr = fs.WriteAllText (LegacyGlobalPathForTest (baseDir),
                              "{\"$cassoGlobalPrefsVersion\":1,\"activeTheme\":\"DarkModern\",\"futureKey\":\"keep\"}");
        Assert::IsTrue (SUCCEEDED (hr));
        hr = fs.WriteAllText (LegacyMachinePathForTest (baseDir, "Foo"),
                              "{\"$cassoMachineVersion\":2,\"speedMode\":\"maximum\"}");
        Assert::IsTrue (SUCCEEDED (hr));
        hr = fs.WriteAllText (LegacyMachinePathForTest (baseDir, "Bar"),
                              "{\"colorMode\":\"green\"}");
        Assert::IsTrue (SUCCEEDED (hr));

        hr = store.LoadAll (prefs, fs);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (std::string ("DarkModern"), prefs.activeTheme);
        Assert::IsTrue (fs.Exists (store.UserPrefsFilePath()));
        Assert::IsFalse (fs.Exists (LegacyGlobalPathForTest (baseDir)));
        Assert::IsFalse (fs.Exists (LegacyMachinePathForTest (baseDir, "Foo")));
        Assert::IsFalse (fs.Exists (LegacyMachinePathForTest (baseDir, "Bar")));

        foo = ReadMachineOrFail (fs, store, "foo");
        bar = ReadMachineOrFail (fs, store, "bar");
        Assert::AreEqual (std::string ("maximum"), FindObjectValueForTest (foo, "speedMode")->GetString());
        Assert::AreEqual (std::string ("green"), FindObjectValueForTest (bar, "colorMode")->GetString());
        Assert::AreEqual (1, (int) FindObjectValueForTest (bar, "$cassoMachineVersion")->GetNumber());
    }


    TEST_METHOD (UnifiedPrefs_MigrationIsIdempotent)
    {
        InMemoryFileSystem  fs;
        std::wstring        baseDir = L"C:\\Casso";
        UserConfigStore     store (baseDir);
        UserConfigStore     secondStore (baseDir);
        GlobalUserPrefs     prefs;
        GlobalUserPrefs     secondPrefs;
        HRESULT             hr = S_OK;
        std::string         firstText;
        std::string         secondText;


        hr = fs.WriteAllText (LegacyMachinePathForTest (baseDir, "Foo"),
                              "{\"$cassoMachineVersion\":2,\"speedMode\":\"maximum\"}");
        Assert::IsTrue (SUCCEEDED (hr));

        hr = store.LoadAll (prefs, fs);
        Assert::IsTrue (SUCCEEDED (hr));
        firstText = fs.PeekContent (store.UserPrefsFilePath());

        hr = secondStore.LoadAll (secondPrefs, fs);
        Assert::IsTrue (SUCCEEDED (hr));
        secondText = fs.PeekContent (secondStore.UserPrefsFilePath());

        Assert::AreEqual (firstText, secondText);
        Assert::IsFalse (fs.Exists (LegacyMachinePathForTest (baseDir, "Foo")));
    }
};
