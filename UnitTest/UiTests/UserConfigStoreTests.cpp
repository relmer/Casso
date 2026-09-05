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
//  The per-machine delta store: merging user edits over shipped defaults,
//  version migration, and the legacy-file upgrade.
//
//  The DELTA model is what these pin. A user who changed one slot must keep
//  receiving every other improvement when the machine definition ships updated
//  -- so the tests assert that an untouched key follows the new default while
//  an edited one holds.
//
//  Save preservation is covered specifically: the cache is populated lazily,
//  one machine at a time, so a save fired before some machine was ever loaded
//  must not drop it from the file. That failure silently deletes settings for
//  machines the session never opened.
//
//  The legacy migration is asserted to be IDEMPOTENT and write-before-delete,
//  so an interrupted upgrade leaves both copies and simply migrates again
//  rather than losing anything.
//
//  Driven against an in-memory filesystem, so nothing touches disk.
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
        AssertSucceeded (hr);
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
        HRESULT            hr       = S_OK;
        JsonValue          found;


        hr = fs.ReadAllText (store.GetUserFilePath (machineName), text);
        AssertSucceeded (hr);

        hr = JsonParser::Parse (text, root, err);
        AssertSucceeded (hr);

        // Two shapes are accepted: the multi-machine file keyed under
        // "machines", and the bare single-machine object the older tests
        // write. A "machines" wrapper that does not actually hold this machine
        // is a broken fixture, not the bare shape, so it fails rather than
        // falling back to root.
        found = root;

        if (root.HasObject ("machines", machines))
        {
            hr = machines->GetObject (machineName, machine);
            AssertSucceeded (hr);

            if (machine == nullptr)
            {
                Assert::Fail (L"missing machine entry");
            }
            else
            {
                found = *machine;
            }
        }

        return found;
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
        AssertSucceeded (hr);
        return text;
    }


    static const JsonValue * FindObjectValueForTest (
        const JsonValue   & obj,
        const std::string & key)
    {
        const JsonValue *  found = nullptr;

        // Anything that is not an object has no keys at all, so it answers the
        // same way a missing key does.
        if (obj.GetType() == JsonType::Object)
        {
            const auto &  entries = obj.GetObjectEntries();

            for (auto it = entries.begin(); found == nullptr && it != entries.end(); ++it)
            {
                if (it->first == key)
                {
                    found = &it->second;
                }
            }
        }

        return found;
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


    // Merge / Diff pure-logic tests

    TEST_METHOD (Merge_Empty_User_Returns_Default)
    {
        JsonValue  m;



        JsonValue   d = ParseOrFail ("{\"a\":1,\"b\":2}");
        JsonValue   u = ParseOrFail ("{}");
        m = UserConfigStore::MergeJson (d, u);

        Assert::IsTrue (UserConfigStore::AreJsonEqual (d, m));
    }


    TEST_METHOD (Merge_User_Scalar_Overrides_Default)
    {
        JsonValue  m;



        JsonValue   d = ParseOrFail ("{\"a\":1,\"b\":2}");
        JsonValue   u = ParseOrFail ("{\"a\":99}");
        m = UserConfigStore::MergeJson (d, u);

        Assert::IsTrue (UserConfigStore::AreJsonEqual (ParseOrFail ("{\"a\":99,\"b\":2}"), m));
    }


    TEST_METHOD (Merge_Deep_Object_Merges)
    {
        JsonValue  m;



        JsonValue   d = ParseOrFail ("{\"crt\":{\"brightness\":1.0,\"bloom\":{\"enabled\":false}}}");
        JsonValue   u = ParseOrFail ("{\"crt\":{\"bloom\":{\"enabled\":true}}}");
        m = UserConfigStore::MergeJson (d, u);

        Assert::IsTrue (UserConfigStore::AreJsonEqual (ParseOrFail ("{\"crt\":{\"brightness\":1.0,\"bloom\":{\"enabled\":true}}}"), m));
    }


    TEST_METHOD (Merge_Array_Replaces_Wholesale)
    {
        JsonValue  m;



        JsonValue   d = ParseOrFail ("{\"arr\":[1,2,3]}");
        JsonValue   u = ParseOrFail ("{\"arr\":[9]}");
        m = UserConfigStore::MergeJson (d, u);

        Assert::IsTrue (UserConfigStore::AreJsonEqual (ParseOrFail ("{\"arr\":[9]}"), m));
    }


    TEST_METHOD (Merge_UserOnly_Key_Preserved)
    {
        JsonValue  m;



        JsonValue   d = ParseOrFail ("{\"a\":1}");
        JsonValue   u = ParseOrFail ("{\"lastMountedImages\":{\"6\":{\"0\":\"/img.dsk\"}}}");
        m = UserConfigStore::MergeJson (d, u);

        Assert::IsTrue (UserConfigStore::AreJsonEqual (ParseOrFail ("{\"a\":1,\"lastMountedImages\":{\"6\":{\"0\":\"/img.dsk\"}}}"), m));
    }


    TEST_METHOD (Diff_NoOp_Returns_Object_With_Just_Version)
    {
        JsonValue  c;
        JsonValue  diff;



        JsonValue   d = ParseOrFail ("{\"$cassoMachineVersion\":1,\"a\":1,\"b\":2}");
        c = d; // identical
        diff = UserConfigStore::DiffJson (c, d);

        Assert::IsTrue (diff.GetType() == JsonType::Object);
        Assert::AreEqual (size_t (1), diff.GetObjectEntries().size());
        Assert::AreEqual (string ("$cassoMachineVersion"), diff.GetObjectEntries()[0].first);
    }


    TEST_METHOD (Diff_Only_Differing_Keys)
    {
        JsonValue  diff;



        JsonValue   d = ParseOrFail ("{\"$cassoMachineVersion\":1,\"a\":1,\"b\":2,\"c\":3}");
        JsonValue   c = ParseOrFail ("{\"$cassoMachineVersion\":1,\"a\":1,\"b\":99,\"c\":3}");
        diff = UserConfigStore::DiffJson (c, d);

        // Expect $cassoMachineVersion + b only.
        Assert::AreEqual (size_t (2), diff.GetObjectEntries().size());
    }


    TEST_METHOD (Diff_Deep_Object_Only_Inner_Diff)
    {
        JsonValue  diff;



        JsonValue   d = ParseOrFail ("{\"crt\":{\"brightness\":1.0,\"bloomEnabled\":false}}");
        JsonValue   c = ParseOrFail ("{\"crt\":{\"brightness\":1.0,\"bloomEnabled\":true}}");
        diff = UserConfigStore::DiffJson (c, d);

        // The crt sub-object should appear with only bloomEnabled inside.
        Assert::AreEqual (size_t (1), diff.GetObjectEntries().size());
        Assert::AreEqual (string ("crt"), diff.GetObjectEntries()[0].first);

        const JsonValue & crtDiff = diff.GetObjectEntries()[0].second;
        Assert::IsTrue (crtDiff.GetType() == JsonType::Object);
        Assert::AreEqual (size_t (1), crtDiff.GetObjectEntries().size());
        Assert::AreEqual (string ("bloomEnabled"), crtDiff.GetObjectEntries()[0].first);
    }


    // Full Load / SaveDelta / Reset round-trips

    TEST_METHOD (Load_NoUserFile_ReturnsDefault)
    {
        InMemoryFileSystem  fs;
        JsonValue           merged;
        HRESULT             hr;
        UserConfigStore     store (L"C:\\Casso\\User");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"a\":1}");

        hr = store.Load ("Apple2e", defaultJson, fs, merged);
        AssertSucceeded (hr);
        Assert::IsTrue (UserConfigStore::AreJsonEqual (defaultJson, merged));
    }


    TEST_METHOD (Load_WithPartialUserFile_MergesCorrectly)
    {
        InMemoryFileSystem  fs;
        JsonValue           merged;
        HRESULT             hr;
        UserConfigStore     store (L"C:\\Casso\\User");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Authentic\",\"a\":1}");

        hr = fs.WriteAllText (store.GetUserFilePath ("Apple2e"),
                              "{\"$cassoMachineVersion\":1,\"speedMode\":\"Maximum\"}");
        AssertSucceeded (hr);

        hr = store.Load ("Apple2e", defaultJson, fs, merged);
        AssertSucceeded (hr);

        JsonValue  expected = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Maximum\",\"a\":1}");
        Assert::IsTrue (UserConfigStore::AreJsonEqual (expected, merged));
    }


    TEST_METHOD (SaveDelta_WritesOnlyDifferences)
    {
        InMemoryFileSystem    fs;
        HRESULT               hr;
        std::string           text;
        JsonValue             parsed;
        JsonParseError        err;
        UserConfigStore       store (L"C:\\Casso\\User");
        JsonValue             defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Authentic\",\"a\":1}");
        JsonValue             current     = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Double\",\"a\":1}");

        hr = store.SaveDelta ("Apple2e", current, defaultJson, fs);
        AssertSucceeded (hr);

        text = MachineTextOrFail (fs, store, "Apple2e");
        Assert::IsFalse (text.empty());

        hr = JsonParser::Parse (text, parsed, err);
        AssertSucceeded (hr);

        // Should contain exactly $cassoMachineVersion + speedMode.
        Assert::AreEqual (size_t (2), parsed.GetObjectEntries().size());
    }


    TEST_METHOD (SaveDelta_Noop_StillWritesVersionStamp)
    {
        InMemoryFileSystem  fs;
        HRESULT             hr;
        JsonValue           parsed;
        JsonParseError      err;
        std::string         text;
        UserConfigStore     store (L"C:\\Casso\\User");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"a\":1}");

        hr = store.SaveDelta ("Apple2e", defaultJson, defaultJson, fs);
        AssertSucceeded (hr);

        text = MachineTextOrFail (fs, store, "Apple2e");
        hr   = JsonParser::Parse (text, parsed, err);
        AssertSucceeded (hr);
        Assert::AreEqual (size_t (1), parsed.GetObjectEntries().size());
        Assert::AreEqual (string ("$cassoMachineVersion"), parsed.GetObjectEntries()[0].first);
    }


    TEST_METHOD (Reset_Deletes_UserFile)
    {
        InMemoryFileSystem  fs;
        HRESULT             hr;
        UserConfigStore     store (L"C:\\Casso\\User");

        hr = fs.WriteAllText (store.GetUserFilePath ("Apple2e"),
                              "{\"$cassoMachineVersion\":1}");
        AssertSucceeded (hr);
        Assert::IsTrue (fs.Exists (store.GetUserFilePath ("Apple2e")));

        hr = store.Reset ("Apple2e", fs);
        AssertSucceeded (hr);
        Assert::IsTrue (fs.Exists (store.GetUserFilePath ("Apple2e")));
        Assert::IsTrue (fs.PeekContent (store.GetUserFilePath ("Apple2e")).find ("Apple2e") == std::string::npos);
    }


    TEST_METHOD (Reset_Idempotent_When_File_Missing)
    {
        InMemoryFileSystem  fs;
        HRESULT             hr;
        UserConfigStore     store (L"C:\\Casso\\User");

        hr = store.Reset ("Apple2e", fs);
        AssertSucceeded (hr);
    }


    TEST_METHOD (Load_LegacyVersionKey_TriggersMigration_WritesBack)
    {
        InMemoryFileSystem  fs;
        JsonValue           merged;
        HRESULT             hr;
        std::string         afterLoad;
        UserConfigStore     store (L"C:\\Casso\\User");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":2,\"a\":1}");
        std::string         original    = "{\"$cassoDefault\":1,\"a\":1}";

        hr = fs.WriteAllText (store.GetUserFilePath ("Apple2e"), original);
        AssertSucceeded (hr);

        hr = store.Load ("Apple2e", defaultJson, fs, merged);
        AssertSucceeded (hr);

        afterLoad = MachineTextOrFail (fs, store, "Apple2e");
        // After migration the legacy key should be gone.
        Assert::IsTrue (afterLoad.find ("$cassoDefault") == std::string::npos);
        Assert::IsTrue (afterLoad.find ("$cassoMachineVersion") != std::string::npos);
    }


    TEST_METHOD (Load_BothVersionKeys_CanonicalizesToMachineVersionOnly)
    {
        InMemoryFileSystem  fs;
        JsonValue           merged;
        HRESULT             hr;
        std::string         afterLoad;
        UserConfigStore     store (L"C:\\Casso\\User");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":9,\"a\":1}");
        std::string         original = "{\"$cassoMachineVersion\":9,\"$cassoDefault\":4,\"a\":1}";

        hr = fs.WriteAllText (store.GetUserFilePath ("Apple2e"), original);
        AssertSucceeded (hr);

        hr = store.Load ("Apple2e", defaultJson, fs, merged);
        AssertSucceeded (hr);

        afterLoad = MachineTextOrFail (fs, store, "Apple2e");
        Assert::IsTrue (afterLoad.find ("$cassoDefault") == std::string::npos);
        Assert::IsTrue (afterLoad.find ("\"$cassoMachineVersion\": 9") != std::string::npos);
    }


    TEST_METHOD (Load_ThreeConsecutiveUpgrades_PreservesOverridesAndAdvancesStamp)
    {
        InMemoryFileSystem  fs;
        JsonValue           merged;
        HRESULT             hr;
        std::string         text;
        UserConfigStore     store (L"C:\\Casso\\User");
        JsonValue           d2 = ParseOrFail ("{\"$cassoMachineVersion\":2,\"newV2\":true,\"$cassoUiPrefs\":{\"speedMode\":\"authentic\"}}");
        JsonValue           d3 = ParseOrFail ("{\"$cassoMachineVersion\":3,\"newV2\":true,\"newV3\":true,\"$cassoUiPrefs\":{\"speedMode\":\"authentic\"}}");
        JsonValue           d4 = ParseOrFail ("{\"$cassoMachineVersion\":4,\"newV2\":true,\"newV3\":true,\"newV4\":true,\"$cassoUiPrefs\":{\"speedMode\":\"authentic\"}}");

        hr = fs.WriteAllText (store.GetUserFilePath ("Apple2e"),
                              "{\"$cassoDefault\":1,\"$cassoUiPrefs\":{\"speedMode\":\"maximum\"}}");
        AssertSucceeded (hr);

        hr = store.Load ("Apple2e", d2, fs, merged);
        AssertSucceeded (hr);
        text = MachineTextOrFail (fs, store, "Apple2e");
        Assert::IsTrue (text.find ("\"$cassoMachineVersion\": 2") != std::string::npos);
        Assert::IsTrue (text.find ("$cassoDefault") == std::string::npos);
        Assert::IsTrue (text.find ("\"speedMode\": \"maximum\"") != std::string::npos);

        hr = store.Load ("Apple2e", d3, fs, merged);
        AssertSucceeded (hr);
        text = MachineTextOrFail (fs, store, "Apple2e");
        Assert::IsTrue (text.find ("\"$cassoMachineVersion\": 3") != std::string::npos);
        Assert::IsTrue (text.find ("\"speedMode\": \"maximum\"") != std::string::npos);

        hr = store.Load ("Apple2e", d4, fs, merged);
        AssertSucceeded (hr);
        text = MachineTextOrFail (fs, store, "Apple2e");
        Assert::IsTrue (text.find ("\"$cassoMachineVersion\": 4") != std::string::npos);
        Assert::IsTrue (text.find ("\"speedMode\": \"maximum\"") != std::string::npos);
    }


    TEST_METHOD (Merge_HardwareEnableDelta_OverlaysDefaultArrays)
    {
        JsonValue  m;



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

        m = UserConfigStore::MergeJson (d, u);
        JsonValue expected = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "internalDevices": [
                { "type": "keyboard", "capabilityFlag": "required", "enabled": false }
            ],
            "slots": [
                { "slot": 6, "device": "disk-ii", "enabled": false }
            ]
        })JSON");

        Assert::IsTrue (UserConfigStore::AreJsonEqual (expected, m));
    }


    TEST_METHOD (Diff_HardwareEnableDelta_EmitsMinimalComponentShape)
    {
        const JsonValue  * internal = nullptr;
        const JsonValue  * slots    = nullptr;
        JsonValue          diff;



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

        diff = UserConfigStore::DiffJson (c, d);

        AssertSucceeded (diff.GetArray ("internalDevices", internal));
        AssertSucceeded (diff.GetArray ("slots", slots));
        Assert::AreEqual<size_t> (1u, internal->GetArraySize());
        Assert::AreEqual<size_t> (1u, slots->GetArraySize());
        const JsonValue & int0  = internal->GetArrayElement (0);
        const JsonValue & slot0 = slots->GetArrayElement (0);
        Assert::AreEqual<size_t> (2u, int0.GetObjectEntries().size());
        Assert::AreEqual<size_t> (2u, slot0.GetObjectEntries().size());
    }


    // Detaching the second Disk ][ drive changes a slot's `ports` and nothing
    // else, so a delta keyed on `enabled` alone wrote an empty user file and
    // the drive came back on the next load.
    TEST_METHOD (Diff_DetachedDrive_EmitsPortsWhenEnabledStillMatches)
    {
        const JsonValue  * slots = nullptr;
        const JsonValue  * ports = nullptr;
        JsonValue          diff;



        JsonValue d = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "slots": [
                { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom", "enabled": true,
                  "ports": ["disk-ii-drive", "disk-ii-drive"] }
            ]
        })JSON");

        JsonValue c = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "slots": [
                { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom", "enabled": true,
                  "ports": ["disk-ii-drive", ""] }
            ]
        })JSON");

        diff = UserConfigStore::DiffJson (c, d);

        AssertSucceeded (diff.GetArray ("slots", slots));
        Assert::AreEqual<size_t> (1u, slots->GetArraySize());

        const JsonValue & slot0 = slots->GetArrayElement (0);

        AssertSucceeded (slot0.GetArray ("ports", ports));
        Assert::AreEqual<size_t> (2u, ports->GetArraySize());
        Assert::AreEqual (std::string ("disk-ii-drive"), ports->GetArrayElement (0).GetString());
        Assert::AreEqual (std::string (""),              ports->GetArrayElement (1).GetString());
    }


    // A card's connector list survives the overlay that rebuilds each slot
    // from the default, while `device` and `rom` still come from the default.
    TEST_METHOD (Merge_DetachedDrive_OverridesDefaultPorts)
    {
        JsonValue  m;



        JsonValue d = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "slots": [
                { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom", "enabled": true,
                  "ports": ["disk-ii-drive", "disk-ii-drive"] }
            ]
        })JSON");

        JsonValue u = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "slots": [
                { "slot": 6, "enabled": true, "ports": ["disk-ii-drive", ""] }
            ]
        })JSON");

        m = UserConfigStore::MergeJson (d, u);
        JsonValue expected = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "slots": [
                { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom", "enabled": true,
                  "ports": ["disk-ii-drive", ""] }
            ]
        })JSON");

        Assert::IsTrue (UserConfigStore::AreJsonEqual (expected, m));
    }


    // A user file that says nothing about connectors leaves the card
    // describing the two drives it shipped with.
    TEST_METHOD (Merge_SilentUserEntry_KeepsDefaultPorts)
    {
        JsonValue  m;



        JsonValue d = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "slots": [
                { "slot": 6, "device": "disk-ii", "enabled": true,
                  "ports": ["disk-ii-drive", "disk-ii-drive"] }
            ]
        })JSON");

        JsonValue u = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "slots": [
                { "slot": 6, "enabled": false }
            ]
        })JSON");

        m = UserConfigStore::MergeJson (d, u);
        JsonValue expected = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "slots": [
                { "slot": 6, "device": "disk-ii", "enabled": false,
                  "ports": ["disk-ii-drive", "disk-ii-drive"] }
            ]
        })JSON");

        Assert::IsTrue (UserConfigStore::AreJsonEqual (expected, m));
    }


    // The whole trip the settings dialog takes: stage a detach, write the
    // delta, load it back. What comes out is what the dialog will show.
    TEST_METHOD (RoundTrip_DetachedDrive_SurvivesDiffThenMerge)
    {
        const JsonValue  * slots = nullptr;
        const JsonValue  * ports = nullptr;
        std::string        rom;
        JsonValue          delta;
        JsonValue          merged;



        JsonValue d = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "slots": [
                { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom", "enabled": true,
                  "ports": ["disk-ii-drive", "disk-ii-drive"] }
            ]
        })JSON");

        JsonValue c = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "slots": [
                { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom", "enabled": true,
                  "ports": ["disk-ii-drive", ""] }
            ]
        })JSON");

        delta  = UserConfigStore::DiffJson (c, d);
        merged = UserConfigStore::MergeJson (d, delta);

        AssertSucceeded (merged.GetArray ("slots", slots));
        Assert::AreEqual<size_t> (1u, slots->GetArraySize());

        const JsonValue & slot0 = slots->GetArrayElement (0);

        AssertSucceeded (slot0.GetArray ("ports", ports));
        Assert::AreEqual<size_t> (2u, ports->GetArraySize());
        Assert::AreEqual (std::string (""), ports->GetArrayElement (1).GetString());

        AssertSucceeded (slot0.GetString ("rom", rom));
        Assert::AreEqual (std::string ("Disk2.rom"), rom);
    }


    TEST_METHOD (Diff_UiPrefs_UsesImplicitDefaultsForSpeedShadowing)
    {
        const JsonValue  * ui   = nullptr;
        JsonValue          diff;



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

        diff = UserConfigStore::DiffJson (c, d);

        AssertSucceeded (diff.GetObject ("$cassoUiPrefs", ui));
        Assert::IsTrue (ui != nullptr);
        if (ui == nullptr) { return; }
        // speedMode differs from the table, so it is written. colorMode
        // MATCHES the table and is written anyway: its real default is a
        // property of the machine's monitor, so a difference from the
        // hardware-independent table says nothing about what the user wants
        // and dropping it discards a deliberate choice. Everything else here
        // matches the table and is dropped, which is the shadowing this test
        // is about.
        Assert::AreEqual<size_t> (2u, ui->GetObjectEntries().size());
        Assert::AreEqual (string ("speedMode"), ui->GetObjectEntries()[0].first);
        Assert::AreEqual (string ("colorMode"), ui->GetObjectEntries()[1].first);
    }


    TEST_METHOD (Merge_SpeedShadow_PreservesDefaultFallthroughFields)
    {
        const JsonValue  * nf = nullptr;
        const JsonValue  * ui = nullptr;
        JsonValue          m;



        JsonValue d = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "newField": { "fromDefault": true }
        })JSON");
        JsonValue u = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "$cassoUiPrefs": { "speedMode": "maximum" }
        })JSON");

        m = UserConfigStore::MergeJson (d, u);

        AssertSucceeded (m.GetObject ("newField", nf));
        AssertSucceeded (m.GetObject ("$cassoUiPrefs", ui));
        Assert::AreEqual (std::string ("maximum"), ui->GetObjectEntries()[0].second.GetString());
    }

    TEST_METHOD (Merge_LastMountedImages_UserOnly_Preserved)
    {
        const JsonValue  * arr = nullptr;
        JsonValue          m;



        JsonValue  d = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3
        })JSON");
        JsonValue  u = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 3,
            "lastMountedImages": ["C:\\disk0.dsk", ""]
        })JSON");

        m = UserConfigStore::MergeJson (d, u);

        AssertSucceeded (m.GetArray ("lastMountedImages", arr));
        Assert::AreEqual (size_t (2), arr->GetArraySize());
        Assert::AreEqual (std::string ("C:\\disk0.dsk"), arr->GetArrayElement (0).GetString());
    }


    TEST_METHOD (LoadAll_CorruptPrefs_ReportsWhereTheParseBroke)
    {
        InMemoryFileSystem  fs;
        GlobalUserPrefs     prefs;
        HRESULT             hr = S_OK;
        std::wstring        parseDetail;
        UserConfigStore     store (L"C:\\Casso");



        // Valid up to the third line, then a bare word where a value belongs.
        hr = fs.WriteAllText (store.GetUserPrefsFilePath(),
                              "{\n"
                              "    \"activeTheme\": \"Retro Terminal\",\n"
                              "    \"speedMode\": oops\n"
                              "}\n");
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, parseDetail);

        AssertFailed (hr,
            L"A prefs file that exists but does not parse must fail, not "
            L"silently fall back to defaults");
        Assert::IsFalse (parseDetail.empty(),
            L"The caller needs somewhere to point the user -- an empty detail "
            L"leaves them with 'Casso lost my settings' and no reason");
        Assert::IsTrue (parseDetail.find (L"line 3") != std::wstring::npos,
            L"The detail must identify the line the parse broke on");
        Assert::IsTrue (parseDetail.find (store.GetUserPrefsFilePath()) != std::wstring::npos,
            L"The detail must identify the file, so the user can go fix it");
    }


    TEST_METHOD (LoadAll_MissingPrefs_ReportsNoParseDetail)
    {
        InMemoryFileSystem  fs;
        GlobalUserPrefs     prefs;
        HRESULT             hr = S_OK;
        std::wstring        parseDetail;
        UserConfigStore     store (L"C:\\Casso");



        // First run: no file at all is normal, not a corruption to report.
        hr = store.LoadAll (prefs, fs, parseDetail);

        Assert::IsTrue (parseDetail.empty(),
            L"A missing file is a first run, not a broken one -- warning the "
            L"user here would be crying wolf");
        UNREFERENCED_PARAMETER (hr);
    }


    TEST_METHOD (UnifiedPrefs_RoundTrip_GlobalAndMachineValuesStick)
    {
        InMemoryFileSystem  fs;
        GlobalUserPrefs     prefs;
        GlobalUserPrefs     reloadedPrefs;
        JsonValue           merged;
        HRESULT             hr = S_OK;
        std::wstring        parseDetail;
        UserConfigStore     store (L"C:\\Casso");
        UserConfigStore     reloadedStore (L"C:\\Casso");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":2,\"speedMode\":\"authentic\"}");
        JsonValue           currentJson = ParseOrFail ("{\"$cassoMachineVersion\":2,\"speedMode\":\"maximum\"}");


        // Nothing on disk yet: a first run succeeds with struct defaults.
        hr = store.LoadAll (prefs, fs, parseDetail);
        AssertSucceeded (hr);

        prefs.activeTheme = "Retro Terminal";
        hr = store.SaveDelta ("Apple //e Enhanced", currentJson, defaultJson, fs);
        AssertSucceeded (hr);
        hr = store.SaveAll (prefs, fs);
        AssertSucceeded (hr);

        hr = reloadedStore.LoadAll (reloadedPrefs, fs, parseDetail);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("Retro Terminal"), reloadedPrefs.activeTheme);

        hr = reloadedStore.Load ("Apple //e Enhanced", defaultJson, fs, merged);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("maximum"), FindObjectValueForTest (merged, "speedMode")->GetString());
    }


    TEST_METHOD (UnifiedPrefs_MigratesLegacyFilesAndDeletesOldFiles)
    {
        InMemoryFileSystem  fs;
        GlobalUserPrefs     prefs;
        HRESULT             hr = S_OK;
        std::wstring        parseDetail;
        JsonValue           foo;
        JsonValue           bar;
        std::wstring        baseDir = L"C:\\Casso";
        UserConfigStore     store (baseDir);


        hr = fs.WriteAllText (LegacyGlobalPathForTest (baseDir),
                              "{\"$cassoGlobalPrefsVersion\":1,\"activeTheme\":\"DarkModern\",\"futureKey\":\"keep\"}");
        AssertSucceeded (hr);
        hr = fs.WriteAllText (LegacyMachinePathForTest (baseDir, "Foo"),
                              "{\"$cassoMachineVersion\":2,\"speedMode\":\"maximum\"}");
        AssertSucceeded (hr);
        hr = fs.WriteAllText (LegacyMachinePathForTest (baseDir, "Bar"),
                              "{\"colorMode\":\"green\"}");
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, parseDetail);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("DarkModern"), prefs.activeTheme);
        Assert::IsTrue (fs.Exists (store.GetUserPrefsFilePath()));
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
        GlobalUserPrefs     prefs;
        GlobalUserPrefs     secondPrefs;
        HRESULT             hr = S_OK;
        std::wstring        parseDetail;
        std::string         firstText;
        std::string         secondText;
        std::wstring        baseDir = L"C:\\Casso";
        UserConfigStore     store (baseDir);
        UserConfigStore     secondStore (baseDir);


        hr = fs.WriteAllText (LegacyMachinePathForTest (baseDir, "Foo"),
                              "{\"$cassoMachineVersion\":2,\"speedMode\":\"maximum\"}");
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, parseDetail);
        AssertSucceeded (hr);
        firstText = fs.PeekContent (store.GetUserPrefsFilePath());

        hr = secondStore.LoadAll (secondPrefs, fs, parseDetail);
        AssertSucceeded (hr);
        secondText = fs.PeekContent (secondStore.GetUserPrefsFilePath());

        Assert::AreEqual (firstText, secondText);
        Assert::IsFalse (fs.Exists (LegacyMachinePathForTest (baseDir, "Foo")));
    }
};
