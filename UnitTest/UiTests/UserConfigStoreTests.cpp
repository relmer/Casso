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

    // Fails a read or a delete on demand. InMemoryFileSystem cannot fail a
    // read of a file that is present, and both halves of the preserve step --
    // "no bytes to copy" and "the copy landed but the original stayed" -- are
    // reachable only through a double that can.
    class FaultyFileSystem : public InMemoryFileSystem
    {
    public:
        bool          fFailRead = false;

        // The ONE path whose delete fails. A blanket switch would fail the
        // cleanup delete too, which is a different file and would hide whether
        // the cleanup happened at all.
        std::wstring  blockedDeletePath;

        HRESULT ReadAllText (const std::wstring & path,
                             std::string        & outContent) override
        {
            if (fFailRead)
            {
                outContent.clear();
                return E_ACCESSDENIED;
            }

            return InMemoryFileSystem::ReadAllText (path, outContent);
        }

        HRESULT Delete (const std::wstring & path) override
        {
            if (!blockedDeletePath.empty() && path == blockedDeletePath)
            {
                return E_ACCESSDENIED;
            }

            return InMemoryFileSystem::Delete (path);
        }
    };


    // A fixed clock so the preserved copy's filename is deterministic.
    // 1756500000 is 2025-08-29 in local time on the build machine; the tests
    // below assert the shape of the name rather than its digits, so the exact
    // rendering does not matter.
    static constexpr time_t  kFixedStamp = 1756500000;


    // A prefs file carrying a global section worth losing: two non-default
    // values plus a key no build knows about. The machine entry is stamped
    // version 1 so a version-2 default triggers migration.
    static constexpr const char *  kpszSeededPrefs =
        "{"
            "\"global\":{"
                "\"$cassoGlobalPrefsVersion\":1,"
                "\"activeTheme\":\"Retro Terminal\","
                "\"showFrameRate\":true,"
                "\"futureKey\":\"keep\""
            "},"
            "\"machines\":{"
                "\"Apple2e\":{\"$cassoMachineVersion\":1,\"speedMode\":\"Maximum\"}"
            "}"
        "}";

    // Two machines plus a global section. Reset must take exactly one of the
    // machines out of the written file and leave everything else standing.
    static constexpr const char *  kpszTwoMachinePrefs =
        "{"
            "\"global\":{"
                "\"$cassoGlobalPrefsVersion\":1,"
                "\"activeTheme\":\"Retro Terminal\","
                "\"futureKey\":\"keep\""
            "},"
            "\"machines\":{"
                "\"Apple2e\":{\"$cassoMachineVersion\":1,\"speedMode\":\"Maximum\"},"
                "\"Apple2plus\":{\"$cassoMachineVersion\":1,\"colorMode\":\"green\"}"
            "}"
        "}";


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


    // The prefs file's "global" object, pretty-printed, so a test can compare
    // the section before and after a save.
    static std::string GlobalTextOrFail (
        InMemoryFileSystem  & fs,
        UserConfigStore     & store)
    {
        std::string          text;
        JsonValue            root;
        JsonParseError       err;
        JsonWriter::Options  opts;
        std::string          out;
        const JsonValue    * global = nullptr;
        HRESULT              hr     = S_OK;


        hr = fs.ReadAllText (store.GetUserPrefsFilePath(), text);
        AssertSucceeded (hr);

        hr = JsonParser::Parse (text, root, err);
        AssertSucceeded (hr);

        global = FindObjectValueForTest (root, "global");

        if (global == nullptr)
        {
            Assert::Fail (L"missing global section");
        }
        else
        {
            opts.fPretty = true;
            hr = JsonWriter::Write (*global, opts, out);
            AssertSucceeded (hr);
        }

        return out;
    }


    // Whether the prefs file still carries an entry for this machine. Reset's
    // assertions are about ABSENCE, which ReadMachineOrFail cannot express --
    // it fails the test when the entry is missing.
    static bool HasMachineOnDisk (
        InMemoryFileSystem  & fs,
        UserConfigStore     & store,
        const std::string   & machineName)
    {
        std::string        text;
        JsonValue          root;
        JsonParseError     err;
        const JsonValue  * machines = nullptr;
        HRESULT            hr       = S_OK;
        bool               present  = false;


        hr = fs.ReadAllText (store.GetUserPrefsFilePath(), text);
        AssertSucceeded (hr);

        hr = JsonParser::Parse (text, root, err);
        AssertSucceeded (hr);

        machines = FindObjectValueForTest (root, "machines");

        if (machines != nullptr)
        {
            present = (FindObjectValueForTest (*machines, machineName) != nullptr);
        }

        return present;
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
        InMemoryFileSystem           fs;
        GlobalUserPrefs              prefs;
        HRESULT                      hr     = S_OK;
        UserConfigStore::LoadReport  report;
        UserConfigStore     store (L"C:\\Casso");



        // Valid up to the third line, then a bare word where a value belongs.
        hr = fs.WriteAllText (store.GetUserPrefsFilePath(),
                              "{\n"
                              "    \"activeTheme\": \"Retro Terminal\",\n"
                              "    \"speedMode\": oops\n"
                              "}\n");
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, report);

        AssertFailed (hr,
            L"A prefs file that exists but does not parse must fail, not "
            L"silently fall back to defaults");
        Assert::IsFalse (report.parseDetail.empty(),
            L"The caller needs somewhere to point the user -- an empty detail "
            L"leaves them with 'Casso lost my settings' and no reason");
        Assert::IsTrue (report.parseDetail.find (L"line 3") != std::wstring::npos,
            L"The detail must identify the line the parse broke on");
        Assert::IsTrue (report.parseDetail.find (store.GetUserPrefsFilePath()) != std::wstring::npos,
            L"The detail must identify the file, so the user can go fix it");
    }


    TEST_METHOD (LoadAll_MissingPrefs_ReportsNoParseDetail)
    {
        InMemoryFileSystem           fs;
        GlobalUserPrefs              prefs;
        HRESULT                      hr     = S_OK;
        UserConfigStore::LoadReport  report;
        UserConfigStore     store (L"C:\\Casso");



        // First run: no file at all is normal, not a corruption to report.
        hr = store.LoadAll (prefs, fs, report);

        Assert::IsTrue (report.parseDetail.empty(),
            L"A missing file is a first run, not a broken one -- warning the "
            L"user here would be crying wolf");
        UNREFERENCED_PARAMETER (hr);
    }


    TEST_METHOD (UnifiedPrefs_RoundTrip_GlobalAndMachineValuesStick)
    {
        InMemoryFileSystem           fs;
        GlobalUserPrefs              prefs;
        GlobalUserPrefs              reloadedPrefs;
        JsonValue                    merged;
        HRESULT                      hr            = S_OK;
        UserConfigStore::LoadReport  report;
        UserConfigStore     store (L"C:\\Casso");
        UserConfigStore     reloadedStore (L"C:\\Casso");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":2,\"speedMode\":\"authentic\"}");
        JsonValue           currentJson = ParseOrFail ("{\"$cassoMachineVersion\":2,\"speedMode\":\"maximum\"}");


        // Nothing on disk yet: a first run succeeds with struct defaults.
        hr = store.LoadAll (prefs, fs, report);
        AssertSucceeded (hr);

        prefs.activeTheme = "Retro Terminal";
        hr = store.SaveDelta ("Apple //e Enhanced", currentJson, defaultJson, fs);
        AssertSucceeded (hr);
        hr = store.SaveAll (prefs, fs);
        AssertSucceeded (hr);

        hr = reloadedStore.LoadAll (reloadedPrefs, fs, report);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("Retro Terminal"), reloadedPrefs.activeTheme);

        hr = reloadedStore.Load ("Apple //e Enhanced", defaultJson, fs, merged);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("maximum"), FindObjectValueForTest (merged, "speedMode")->GetString());
    }


    TEST_METHOD (UnifiedPrefs_MigratesLegacyFilesAndDeletesOldFiles)
    {
        InMemoryFileSystem           fs;
        GlobalUserPrefs              prefs;
        HRESULT                      hr     = S_OK;
        UserConfigStore::LoadReport  report;
        JsonValue                    foo;
        JsonValue                    bar;
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

        hr = store.LoadAll (prefs, fs, report);
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


    // The upgrade used to copy the legacy document straight into the new
    // file's global section while its sibling branch wrote prefs.ToJson(),
    // so whatever loading had normalized or converted was thrown away -- and
    // the legacy file is deleted right after, leaving no other copy.
    TEST_METHOD (UnifiedPrefs_Migration_WritesWhatWasLoadedNotWhatWasRead)
    {
        InMemoryFileSystem           fs;
        GlobalUserPrefs              prefs;
        HRESULT                      hr      = S_OK;
        UserConfigStore::LoadReport  report;
        JsonWriter::Options          opts;
        std::string                  expected;
        std::wstring                 baseDir = L"C:\\Casso";
        UserConfigStore              store (baseDir);


        hr = fs.WriteAllText (LegacyGlobalPathForTest (baseDir),
                              "{\"$cassoGlobalPrefsVersion\":1,\"activeTheme\":\"DarkModern\","
                              "\"crt\":{\"green\":{\"userOverride\":true,\"brightness\":1.42}}}");
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, report);
        AssertSucceeded (hr);

        opts.fPretty = true;
        hr = JsonWriter::Write (prefs.ToJson(), opts, expected);
        AssertSucceeded (hr);

        Assert::AreEqual (expected, GlobalTextOrFail (fs, store));
    }


    // The conversion runs inside FromJson, so it reaches the file only if the
    // upgrade writes what FromJson produced. This is the concrete case the
    // previous test generalizes.
    TEST_METHOD (UnifiedPrefs_Migration_CarriesALegacyCrtBlockAcrossAsOverrides)
    {
        InMemoryFileSystem           fs;
        GlobalUserPrefs              prefs;
        HRESULT                      hr      = S_OK;
        UserConfigStore::LoadReport  report;
        std::string                  global;
        std::wstring                 baseDir = L"C:\\Casso";
        UserConfigStore              store (baseDir);


        hr = fs.WriteAllText (LegacyGlobalPathForTest (baseDir),
                              "{\"$cassoGlobalPrefsVersion\":1,"
                              "\"crt\":{\"green\":{\"userOverride\":true,\"brightness\":1.42}}}");
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, report);
        AssertSucceeded (hr);

        global = GlobalTextOrFail (fs, store);

        Assert::IsTrue  (global.find ("crtOverrides")         != std::string::npos);
        Assert::IsTrue  (global.find ("AppleMonitorII/green") != std::string::npos);
        Assert::IsFalse (global.find ("userOverride")         != std::string::npos);
    }


    // ToJson has to round-trip what it did not recognize, or agreeing with the
    // sibling branch would trade one kind of loss for another.
    TEST_METHOD (UnifiedPrefs_Migration_KeepsAnUnknownTopLevelKey)
    {
        InMemoryFileSystem           fs;
        GlobalUserPrefs              prefs;
        HRESULT                      hr      = S_OK;
        UserConfigStore::LoadReport  report;
        std::wstring                 baseDir = L"C:\\Casso";
        UserConfigStore              store (baseDir);


        hr = fs.WriteAllText (LegacyGlobalPathForTest (baseDir),
                              "{\"$cassoGlobalPrefsVersion\":1,\"activeTheme\":\"DarkModern\","
                              "\"futureKey\":\"keep me\"}");
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, report);
        AssertSucceeded (hr);

        Assert::IsTrue (GlobalTextOrFail (fs, store).find ("keep me") != std::string::npos);
    }


    TEST_METHOD (UnifiedPrefs_MigrationIsIdempotent)
    {
        InMemoryFileSystem           fs;
        GlobalUserPrefs              prefs;
        GlobalUserPrefs              secondPrefs;
        HRESULT                      hr          = S_OK;
        UserConfigStore::LoadReport  report;
        std::string                  firstText;
        std::string                  secondText;
        std::wstring        baseDir = L"C:\\Casso";
        UserConfigStore     store (baseDir);
        UserConfigStore     secondStore (baseDir);


        hr = fs.WriteAllText (LegacyMachinePathForTest (baseDir, "Foo"),
                              "{\"$cassoMachineVersion\":2,\"speedMode\":\"maximum\"}");
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, report);
        AssertSucceeded (hr);
        firstText = fs.PeekContent (store.GetUserPrefsFilePath());

        hr = secondStore.LoadAll (secondPrefs, fs, report);
        AssertSucceeded (hr);
        secondText = fs.PeekContent (secondStore.GetUserPrefsFilePath());

        Assert::AreEqual (firstText, secondText);
        Assert::IsFalse (fs.Exists (LegacyMachinePathForTest (baseDir, "Foo")));
    }


    // ------------------------------------------------------------------
    //  SpliceUiPrefs
    //
    //  The read-modify-write every per-machine setting goes through. What it
    //  must never do is drop a key it was not asked about: a save that
    //  rewrote the block from scratch would take the user's color mode and
    //  mounted disks with it.
    // ------------------------------------------------------------------

    static const JsonValue * GetUiPrefsOrFail (const JsonValue & doc)
    {
        const JsonValue *  uiPrefs = nullptr;
        bool               found   = doc.HasObject ("$cassoUiPrefs", uiPrefs);

        Assert::IsTrue (found && uiPrefs != nullptr, L"no $cassoUiPrefs block");
        return uiPrefs;
    }


    TEST_METHOD (SpliceUiPrefs_ReplacesExistingKey_AndKeepsTheRest)
    {
        JsonValue                                       doc = ParseOrFail (
            "{\"name\":\"Apple2e\",\"$cassoUiPrefs\":"
            "{\"colorMode\":\"green\",\"speedMode\":\"maximum\"}}");
        std::vector<std::pair<std::string, JsonValue>>  values;
        JsonValue                                       updated;
        const JsonValue                               * uiPrefs = nullptr;
        std::string                                     colorMode;
        std::string                                     speedMode;
        std::string                                     name;


        values.emplace_back ("colorMode", JsonValue (std::string ("amber")));

        updated = UserConfigStore::SpliceUiPrefs (doc, values);
        uiPrefs = GetUiPrefsOrFail (updated);

        Assert::IsTrue (uiPrefs->HasString ("colorMode", colorMode));
        Assert::AreEqual (std::string ("amber"), colorMode);

        // Untouched neighbors survive -- the whole point of a splice.
        Assert::IsTrue (uiPrefs->HasString ("speedMode", speedMode));
        Assert::AreEqual (std::string ("maximum"), speedMode);

        Assert::IsTrue (updated.HasString ("name", name));
        Assert::AreEqual (std::string ("Apple2e"), name);
    }


    TEST_METHOD (SpliceUiPrefs_AppendsUnknownKeys_AndWritesABatchAtOnce)
    {
        JsonValue                                       doc = ParseOrFail (
            "{\"$cassoUiPrefs\":{\"colorMode\":\"green\"}}");
        std::vector<std::pair<std::string, JsonValue>>  values;
        JsonValue                                       updated;
        const JsonValue                               * uiPrefs = nullptr;
        std::string                                     pointer;
        bool                                            arrows  = false;
        std::string                                     colorMode;


        values.emplace_back ("arrowsToJoystick", JsonValue (true));
        values.emplace_back ("pointerMapping",   JsonValue (std::string ("mouse")));

        updated = UserConfigStore::SpliceUiPrefs (doc, values);
        uiPrefs = GetUiPrefsOrFail (updated);

        Assert::IsTrue (uiPrefs->HasBool ("arrowsToJoystick", arrows));
        Assert::IsTrue (arrows);

        Assert::IsTrue (uiPrefs->HasString ("pointerMapping", pointer));
        Assert::AreEqual (std::string ("mouse"), pointer);

        Assert::IsTrue (uiPrefs->HasString ("colorMode", colorMode));
        Assert::AreEqual (std::string ("green"), colorMode);
    }


    TEST_METHOD (SpliceUiPrefs_CreatesTheBlock_WhenTheDocumentHasNone)
    {
        JsonValue                                       doc = ParseOrFail ("{\"name\":\"Apple2c\"}");
        std::vector<std::pair<std::string, JsonValue>>  values;
        JsonValue                                       updated;
        const JsonValue                               * uiPrefs = nullptr;
        bool                                            arrows  = false;


        values.emplace_back ("arrowsToJoystick", JsonValue (true));

        updated = UserConfigStore::SpliceUiPrefs (doc, values);
        uiPrefs = GetUiPrefsOrFail (updated);

        Assert::IsTrue (uiPrefs->HasBool ("arrowsToJoystick", arrows));
        Assert::IsTrue (arrows);
    }


    TEST_METHOD (SpliceUiPrefs_NonObjectDocument_IsReturnedUnchanged)
    {
        JsonValue                                       doc = ParseOrFail ("[1,2,3]");
        std::vector<std::pair<std::string, JsonValue>>  values;
        JsonValue                                       updated;


        values.emplace_back ("arrowsToJoystick", JsonValue (true));

        updated = UserConfigStore::SpliceUiPrefs (doc, values);

        Assert::IsTrue (updated.GetType() == JsonType::Array);
        Assert::AreEqual (size_t (3), updated.GetArraySize());
    }


    // The keys the input mapping uses must NOT be in the UI-prefs default
    // table, or the delta would drop them whenever they matched -- and an
    // absent key means "fall back to the old global setting", not "off". A
    // user who turned the mapping off on one machine would get it back.
    TEST_METHOD (SaveDelta_KeepsInputMappingKeys_EvenAtTheirDefaultValues)
    {
        InMemoryFileSystem  fs;
        GlobalUserPrefs     prefs;
        UserConfigStore     store (L"C:\\Casso");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"name\":\"Apple2e\"}");
        JsonValue           current     = ParseOrFail (
            "{\"$cassoMachineVersion\":1,\"name\":\"Apple2e\",\"$cassoUiPrefs\":"
            "{\"arrowsToJoystick\":false,\"pointerMapping\":\"off\"}}");
        JsonValue           stored;
        const JsonValue   * uiPrefs     = nullptr;
        std::string         pointer;
        bool                arrows      = true;
        HRESULT             hr          = S_OK;


        hr = store.SaveDelta ("Apple2e", current, defaultJson, fs);
        AssertSucceeded (hr);

        stored  = ReadMachineOrFail (fs, store, "Apple2e");
        uiPrefs = GetUiPrefsOrFail (stored);

        Assert::IsTrue (uiPrefs->HasBool ("arrowsToJoystick", arrows));
        Assert::IsFalse (arrows);

        Assert::IsTrue (uiPrefs->HasString ("pointerMapping", pointer));
        Assert::AreEqual (std::string ("off"), pointer);
    }


    // A store that never ran LoadAll holds no global prefs, so every save path
    // through one must leave the on-disk global section exactly as it found
    // it. Casso builds such stores during startup -- to read a remembered disk
    // path, to fold a machine delta over the shipped config -- and each of the
    // three saves below fires from one. Writing constructed defaults there
    // resets every global preference the user ever set, and the only visible
    // symptom is settings that quietly revert on the next launch.

    TEST_METHOD (SaveDelta_WithoutLoadAll_PreservesGlobalSection)
    {
        InMemoryFileSystem  fs;
        HRESULT             hr          = S_OK;
        std::string         before;
        std::string         after;
        UserConfigStore     store (L"C:\\Casso");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Authentic\"}");
        JsonValue           currentJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Double\"}");


        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), kpszSeededPrefs);
        AssertSucceeded (hr);
        before = GlobalTextOrFail (fs, store);

        hr = store.SaveDelta ("Apple2e", currentJson, defaultJson, fs);
        AssertSucceeded (hr);

        after = GlobalTextOrFail (fs, store);
        Assert::AreEqual (before, after);
    }


    TEST_METHOD (Reset_WithoutLoadAll_PreservesGlobalSection)
    {
        InMemoryFileSystem  fs;
        HRESULT             hr = S_OK;
        std::string         before;
        std::string         after;
        UserConfigStore     store (L"C:\\Casso");


        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), kpszSeededPrefs);
        AssertSucceeded (hr);
        before = GlobalTextOrFail (fs, store);

        hr = store.Reset ("Apple2e", fs);
        AssertSucceeded (hr);

        after = GlobalTextOrFail (fs, store);
        Assert::AreEqual (before, after);
    }


    TEST_METHOD (Load_MigratingWithoutLoadAll_PreservesGlobalSection)
    {
        InMemoryFileSystem  fs;
        JsonValue           merged;
        JsonValue           savedMachine;
        HRESULT             hr          = S_OK;
        std::string         before;
        std::string         after;
        UserConfigStore     store (L"C:\\Casso");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":2,\"speedMode\":\"Authentic\"}");


        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), kpszSeededPrefs);
        AssertSucceeded (hr);
        before = GlobalTextOrFail (fs, store);

        // The seeded entry is stamped version 1 against a version-2 default,
        // so Load migrates it and writes the whole document back.
        hr = store.Load ("Apple2e", defaultJson, fs, merged);
        AssertSucceeded (hr);

        savedMachine = ReadMachineOrFail (fs, store, "Apple2e");
        Assert::AreEqual (2, (int) FindObjectValueForTest (savedMachine, "$cassoMachineVersion")->GetNumber());

        after = GlobalTextOrFail (fs, store);
        Assert::AreEqual (before, after);
    }


    // The counterpart on a store that DID load: preserving the on-disk section
    // must not cost a loaded store its writes, so the live prefs object is
    // still what reaches disk.

    TEST_METHOD (SaveDelta_AfterLoadAll_WritesLivePrefs)
    {
        InMemoryFileSystem           fs;
        GlobalUserPrefs              prefs;
        JsonValue                    savedGlobal;
        HRESULT                      hr          = S_OK;
        UserConfigStore::LoadReport  report;
        UserConfigStore     store (L"C:\\Casso");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Authentic\"}");
        JsonValue           currentJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Double\"}");


        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), kpszSeededPrefs);
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, report);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("Retro Terminal"), prefs.activeTheme);

        prefs.activeTheme = "DarkModern";
        hr = store.SaveDelta ("Apple2e", currentJson, defaultJson, fs);
        AssertSucceeded (hr);

        savedGlobal = ParseOrFail (GlobalTextOrFail (fs, store).c_str());
        Assert::AreEqual (std::string ("DarkModern"),
                          FindObjectValueForTest (savedGlobal, "activeTheme")->GetString());
    }


    // Reset has to take the entry out of the WRITTEN FILE, not just this
    // store's cache. BuildCombinedJson reads the file back and merges every
    // machine it finds, so an erase that stops at the cache is undone before
    // the document is written and Reset returns S_OK having changed nothing.

    TEST_METHOD (Reset_RemovesTheMachineFromTheWrittenFile)
    {
        InMemoryFileSystem  fs;
        JsonValue           other;
        std::string         globalBefore;
        std::string         globalAfter;
        HRESULT             hr = S_OK;
        UserConfigStore     store (L"C:\\Casso");


        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), kpszTwoMachinePrefs);
        AssertSucceeded (hr);
        globalBefore = GlobalTextOrFail (fs, store);

        hr = store.Reset ("Apple2e", fs);
        AssertSucceeded (hr);

        Assert::IsFalse (HasMachineOnDisk (fs, store, "Apple2e"));

        // The read-back that used to resurrect the reset entry is the same one
        // that preserves machines this process never loaded, so removing one
        // machine must leave the other and the global section standing.
        other = ReadMachineOrFail (fs, store, "Apple2plus");
        Assert::AreEqual (std::string ("green"),
                          FindObjectValueForTest (other, "colorMode")->GetString());

        globalAfter = GlobalTextOrFail (fs, store);
        Assert::AreEqual (globalBefore, globalAfter);
    }


    // A plain regression test, not a pin on where the on-disk skip lives: the
    // erasure set is cleared when Reset returns, so this passes wherever the
    // skip is applied. It guards the sequence a restore-defaults button would
    // produce.
    TEST_METHOD (Reset_ThenSaveDelta_WritesTheNewDelta)
    {
        InMemoryFileSystem  fs;
        JsonValue           afterSave;
        HRESULT             hr          = S_OK;
        UserConfigStore     store (L"C:\\Casso");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Authentic\"}");
        JsonValue           currentJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Double\"}");


        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), kpszTwoMachinePrefs);
        AssertSucceeded (hr);

        hr = store.Reset ("Apple2e", fs);
        AssertSucceeded (hr);
        Assert::IsFalse (HasMachineOnDisk (fs, store, "Apple2e"));

        hr = store.SaveDelta ("Apple2e", currentJson, defaultJson, fs);
        AssertSucceeded (hr);

        afterSave = ReadMachineOrFail (fs, store, "Apple2e");
        Assert::AreEqual (std::string ("Double"),
                          FindObjectValueForTest (afterSave, "speedMode")->GetString());
    }


    TEST_METHOD (Reset_WhenPrefsFileMissing_WritesNothing)
    {
        InMemoryFileSystem  fs;
        HRESULT             hr = S_OK;
        UserConfigStore     store (L"C:\\Casso");


        hr = store.Reset ("Apple2e", fs);
        AssertSucceeded (hr);

        // Creating a file here would cost a user upgrading from an older build
        // everything they have: LoadAll gates the legacy-file migration on the
        // unified file being absent, so an empty one written here strands the
        // legacy files permanently unread.
        Assert::IsFalse (fs.Exists (store.GetUserPrefsFilePath()));
        Assert::AreEqual (size_t (0), fs.FileCount());
    }


    TEST_METHOD (Reset_FailedWrite_KeepsTheMachineInTheCache)
    {
        InMemoryFileSystem           fs;
        GlobalUserPrefs              prefs;
        JsonValue                    merged;
        UserConfigStore::LoadReport  report;
        HRESULT                      hr     = S_OK;
        UserConfigStore     store (L"C:\\Casso");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Authentic\"}");


        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), kpszTwoMachinePrefs);
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, report);
        AssertSucceeded (hr);

        hr = fs.SetReadOnlyAttribute (store.GetUserPrefsFilePath(), true);
        AssertSucceeded (hr);

        hr = store.Reset ("Apple2e", fs);
        AssertFailed (hr);

        // The write is what removes the entry, so a failed write leaves it on
        // disk and the cache has to agree. A cache that dropped it anyway
        // answers with the shipped defaults for the rest of the session, and
        // because the cache still holds the OTHER machine, Load's re-read
        // guard never fires to correct it.
        hr = store.Load ("Apple2e", defaultJson, fs, merged);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("Maximum"),
                          FindObjectValueForTest (merged, "speedMode")->GetString());
    }


    // LoadAll promises, through the dialog EmulatorShell composes from its
    // result, that a settings file it could not read was not destroyed. That
    // promise has two halves and both are pinned here: the file is moved aside
    // under a stamped name when its bytes are in hand, and saving is refused
    // for as long as an unreadable file is still sitting there.

    TEST_METHOD (LoadAll_CorruptPrefs_MovesTheFileAsideAndKeepsEveryByte)
    {
        InMemoryFileSystem           fs;
        GlobalUserPrefs              prefs;
        UserConfigStore::LoadReport  report;
        std::string                  preservedText;
        HRESULT                      hr            = S_OK;
        UserConfigStore     store (L"C:\\Casso");
        std::string         original = "{\"global\":{\"activeTheme\":\"Retro Terminal\"},}";


        store.SetTimestampSource ([] { return kFixedStamp; });

        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), original);
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, report);
        AssertFailed (hr);

        // Every byte survived, under a name that ends in .json so the user can
        // open it, and the unreadable original is gone.
        Assert::IsFalse (report.preservedPath.empty());
        Assert::IsTrue (report.preservedPath.ends_with (L".original.json"));
        Assert::IsFalse (fs.Exists (store.GetUserPrefsFilePath()));

        hr = fs.ReadAllText (report.preservedPath, preservedText);
        AssertSucceeded (hr);
        Assert::AreEqual (original, preservedText);
    }


    TEST_METHOD (SaveAll_AfterCorruptPrefsWereMovedAside_WritesNormally)
    {
        InMemoryFileSystem           fs;
        GlobalUserPrefs              prefs;
        UserConfigStore::LoadReport  report;
        std::string                  preservedText;
        HRESULT                      hr            = S_OK;
        UserConfigStore     store (L"C:\\Casso");


        store.SetTimestampSource ([] { return kFixedStamp; });

        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), "{not json at all");
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, report);
        AssertFailed (hr);

        // Moving the file aside is what buys back the right to write: the
        // unreadable file is no longer there to be overwritten, so the session
        // saves normally instead of being stuck.
        prefs = GlobalUserPrefs {};
        prefs.activeTheme = "DarkModern";

        hr = store.SaveAll (prefs, fs);
        AssertSucceeded (hr);

        hr = fs.ReadAllText (report.preservedPath, preservedText);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("{not json at all"), preservedText);
    }


    TEST_METHOD (SaveAll_AfterALoadThatCouldNotMoveTheFileAside_StaysRefused)
    {
        FaultyFileSystem             fs;
        GlobalUserPrefs              prefs;
        GlobalUserPrefs              reloaded;
        UserConfigStore::LoadReport  report;
        HRESULT                      hr       = S_OK;
        UserConfigStore              store (L"C:\\Casso");
        std::string                  original = "{\"global\":{\"activeTheme\":\"Retro Terminal\"}}";


        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), original);
        AssertSucceeded (hr);

        // A read that fails leaves no bytes to copy, so nothing can be moved
        // aside and the original -- which is perfectly good JSON -- stays put.
        // An editor, a backup tool or a virus scanner holding the file is
        // enough, and it lets go again a moment later.
        fs.fFailRead = true;

        hr = store.LoadAll (prefs, fs, report);
        AssertFailed (hr);
        Assert::IsTrue (report.preservedPath.empty());

        hr = store.SaveAll (prefs, fs);
        AssertFailed (hr);

        // The lock clears. The store still holds nothing it read from that
        // file: the caller has since reset prefs to defaults, exactly as
        // EmulatorShell does. Resuming saves here would write those defaults
        // over a global section that was intact the whole time, so the refusal
        // has to outlive the condition that caused it.
        fs.fFailRead = false;
        prefs = GlobalUserPrefs {};

        hr = store.SaveAll (prefs, fs);
        AssertFailed (hr);
        Assert::AreEqual (original, fs.PeekContent (store.GetUserPrefsFilePath()));

        // A load that succeeds is what clears it, because that is the point at
        // which the store holds the file's own contents again.
        hr = store.LoadAll (reloaded, fs, report);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("Retro Terminal"), reloaded.activeTheme);

        hr = store.SaveAll (reloaded, fs);
        AssertSucceeded (hr);
    }


    TEST_METHOD (LoadAll_WhenTheOriginalCannotBeDeleted_ReportsNoPreservedPath)
    {
        FaultyFileSystem             fs;
        GlobalUserPrefs              prefs;
        UserConfigStore::LoadReport  report;
        HRESULT                      hr     = S_OK;
        UserConfigStore   store (L"C:\\Casso");


        store.SetTimestampSource ([] { return kFixedStamp; });
        fs.blockedDeletePath = store.GetUserPrefsFilePath();

        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), "{broken");
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, report);
        AssertFailed (hr);

        // The copy landed but the original stayed, so the move did not finish.
        // Reporting a path here would tell the caller the file is safe to
        // overwrite when it is exactly the file still at risk.
        Assert::IsTrue (report.preservedPath.empty());
        Assert::IsTrue (fs.Exists (store.GetUserPrefsFilePath()));

        // And the copy is taken back out. The original is intact, so the copy
        // protects nothing, and leaving it would stack up one more on every
        // launch for as long as the file stays unreadable and undeletable.
        Assert::AreEqual (size_t (1), fs.FileCount());

        hr = store.SaveAll (prefs, fs);
        AssertFailed (hr);
    }


    TEST_METHOD (SaveDelta_OverACorruptFile_DoesNotDestroyIt)
    {
        InMemoryFileSystem  fs;
        HRESULT             hr          = S_OK;
        UserConfigStore     store (L"C:\\Casso");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Authentic\"}");
        JsonValue           currentJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Double\"}");
        std::string         original    = "{\"global\":{},}";


        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), original);
        AssertSucceeded (hr);

        // A store that never ran LoadAll has no idea the file is corrupt, and
        // this is the path Casso takes during startup before the shell reads
        // anything. It must not write over a file it could not read.
        hr = store.SaveDelta ("Apple2e", currentJson, defaultJson, fs);
        AssertFailed (hr);
        Assert::AreEqual (original, fs.PeekContent (store.GetUserPrefsFilePath()));
    }


    TEST_METHOD (ComposeLoadFailureMessage_SaysWhereTheCopyWentOrThatSavingIsRefused)
    {
        UserConfigStore::LoadReport  movedReport;
        UserConfigStore::LoadReport  stayedReport;
        UserConfigStore::LoadReport  neverWrittenReport;
        std::wstring                 moved;
        std::wstring                 stayed;
        std::wstring                 neverWritten;


        movedReport.hadPrefsFile  = true;
        movedReport.preservedPath = L"C:\\Casso\\UserPrefs.20260903-141530.original.json";
        movedReport.parseDetail   = L"line 2, column 1: trailing comma";

        moved = UserConfigStore::ComposeLoadFailureMessage (
                    L"C:\\Casso", L"C:\\Casso\\UserPrefs.json", movedReport);

        Assert::IsTrue (moved.find (L"UserPrefs.20260903-141530.original.json") != std::wstring::npos);
        Assert::IsTrue (moved.find (L"line 2, column 1") != std::wstring::npos);

        // The old message promised the file was left untouched. It is moved
        // now, so the words that said otherwise must be gone.
        Assert::IsTrue (moved.find (L"left untouched") == std::wstring::npos);

        stayedReport.hadPrefsFile = true;

        stayed = UserConfigStore::ComposeLoadFailureMessage (
                     L"C:\\Casso", L"C:\\Casso\\UserPrefs.json", stayedReport);

        // Nothing was moved, so the message reports the original and says
        // saving is refused rather than implying the session keeps anything.
        Assert::IsTrue (stayed.find (L"UserPrefs.json") != std::wstring::npos);
        Assert::IsTrue (stayed.find (L"not be saved") != std::wstring::npos);

        // No prefs file at all: the failure came from the legacy migration, so
        // pointing at UserPrefs.json would send the user after a file that is
        // not there, and saving is not refused either.
        neverWritten = UserConfigStore::ComposeLoadFailureMessage (
                           L"C:\\Casso", L"C:\\Casso\\UserPrefs.json",
                           neverWrittenReport);

        Assert::IsTrue (neverWritten.find (L"older layout") != std::wstring::npos);
        Assert::IsTrue (neverWritten.find (L"not be saved") == std::wstring::npos);
        Assert::IsTrue (neverWritten.find (L"still where it was") == std::wstring::npos);

        // It must point at the DIRECTORY. There is no UserPrefs.json in this
        // case -- that is why this branch exists -- so printing its path sends
        // the user after a file that is not there.
        Assert::IsTrue (neverWritten.find (L"UserPrefs.json") == std::wstring::npos);
        Assert::IsTrue (neverWritten.find (L"C:\\Casso") != std::wstring::npos);
    }


    TEST_METHOD (Load_WhenMigrationCannotSaveOverACorruptFile_StillSucceeds)
    {
        InMemoryFileSystem           fs;
        GlobalUserPrefs              prefs;
        JsonValue                    merged;
        UserConfigStore::LoadReport  report;
        HRESULT                      hr     = S_OK;
        UserConfigStore     store (L"C:\\Casso");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":2,\"speedMode\":\"Authentic\"}");
        std::string         corrupt     = "{\"global\":{},}";


        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), kpszSeededPrefs);
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, report);
        AssertSucceeded (hr);

        // The file read cleanly at startup and was hand-edited into garbage
        // since. Loading a machine whose delta needs migrating now cannot
        // write the result back.
        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), corrupt);
        AssertSucceeded (hr);

        // Migration failure is deliberately non-fatal, and it has to stay that
        // way: EmulatorShell answers a failed Load with CHRA, so propagating
        // the refused save would turn an unreadable prefs file into a debug
        // assert -- exactly what the startup path's own banner forbids.
        hr = store.Load ("Apple2e", defaultJson, fs, merged);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("Maximum"),
                          FindObjectValueForTest (merged, "speedMode")->GetString());

        // And the file the save refused to touch is still exactly as it was.
        Assert::AreEqual (corrupt, fs.PeekContent (store.GetUserPrefsFilePath()));
    }


    TEST_METHOD (LoadAll_WhenOnlyTheGlobalSectionIsBad_KeepsTheMachineDeltas)
    {
        InMemoryFileSystem           fs;
        GlobalUserPrefs              prefs;
        JsonValue                    machine;
        UserConfigStore::LoadReport  report;
        HRESULT                      hr      = S_OK;
        UserConfigStore     store (L"C:\\Casso");
        std::string         original = "{\"global\":42,\"machines\":{\"Apple2e\":{\"$cassoMachineVersion\":1,\"speedMode\":\"Maximum\"}}}";


        store.SetTimestampSource ([] { return kFixedStamp; });

        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), original);
        AssertSucceeded (hr);

        // The document parses and its machines section is perfectly readable;
        // only the global section is wrong. The whole file is still set aside,
        // because the global is what could not be loaded -- but the deltas were
        // readable, so they must survive into the file that replaces it. Losing
        // them costs the user their slot configuration and remembered disks
        // over a fault in a different section.
        hr = store.LoadAll (prefs, fs, report);
        AssertFailed (hr);
        Assert::IsFalse (report.preservedPath.empty());

        hr = store.SaveAll (prefs, fs);
        AssertSucceeded (hr);

        machine = ReadMachineOrFail (fs, store, "Apple2e");
        Assert::AreEqual (std::string ("Maximum"),
                          FindObjectValueForTest (machine, "speedMode")->GetString());
    }


    // One unreadable legacy file must not cost the user the others. The whole
    // migration used to fail on it, which wrote nothing and deleted nothing --
    // and once anything else created the unified file, LoadAll's migration gate
    // never opened again, so every legacy file was stranded unread.

    TEST_METHOD (LoadAll_WhenOneLegacyFileIsUnreadable_MigratesTheRest)
    {
        InMemoryFileSystem           fs;
        GlobalUserPrefs              prefs;
        JsonValue                    foo;
        UserConfigStore::LoadReport  report;
        HRESULT                      hr     = S_OK;
        std::wstring        baseDir = L"C:\\Casso";
        UserConfigStore     store (baseDir);


        hr = fs.WriteAllText (LegacyGlobalPathForTest (baseDir),
                              "{\"$cassoGlobalPrefsVersion\":1,\"activeTheme\":\"DarkModern\"}");
        AssertSucceeded (hr);
        hr = fs.WriteAllText (LegacyMachinePathForTest (baseDir, "Foo"),
                              "{\"$cassoMachineVersion\":2,\"speedMode\":\"maximum\"}");
        AssertSucceeded (hr);
        hr = fs.WriteAllText (LegacyMachinePathForTest (baseDir, "Bar"), "{ truncated");
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, report);
        AssertSucceeded (hr);

        // The readable files came across and are gone from disk.
        Assert::AreEqual (std::string ("DarkModern"), prefs.activeTheme);
        Assert::IsTrue (fs.Exists (store.GetUserPrefsFilePath()));
        Assert::IsFalse (fs.Exists (LegacyGlobalPathForTest (baseDir)));
        Assert::IsFalse (fs.Exists (LegacyMachinePathForTest (baseDir, "Foo")));

        foo = ReadMachineOrFail (fs, store, "foo");
        Assert::AreEqual (std::string ("maximum"),
                          FindObjectValueForTest (foo, "speedMode")->GetString());

        // The one that could not be read stays on disk. Casso can do nothing
        // with it, so deleting it would destroy the only copy of whatever the
        // user still has in there.
        Assert::IsTrue (fs.Exists (LegacyMachinePathForTest (baseDir, "Bar")));
        Assert::AreEqual (std::string ("{ truncated"),
                          fs.PeekContent (LegacyMachinePathForTest (baseDir, "Bar")));
    }


    TEST_METHOD (LoadAll_WhenTheLegacyGlobalIsUnreadable_StillMigratesTheMachines)
    {
        InMemoryFileSystem           fs;
        GlobalUserPrefs              prefs;
        JsonValue                    foo;
        UserConfigStore::LoadReport  report;
        HRESULT                      hr     = S_OK;
        std::wstring        baseDir = L"C:\\Casso";
        UserConfigStore     store (baseDir);


        hr = fs.WriteAllText (LegacyGlobalPathForTest (baseDir), "{ truncated");
        AssertSucceeded (hr);
        hr = fs.WriteAllText (LegacyMachinePathForTest (baseDir, "Foo"),
                              "{\"$cassoMachineVersion\":2,\"speedMode\":\"maximum\"}");
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, report);
        AssertSucceeded (hr);

        foo = ReadMachineOrFail (fs, store, "foo");
        Assert::AreEqual (std::string ("maximum"),
                          FindObjectValueForTest (foo, "speedMode")->GetString());

        Assert::AreEqual (std::string ("Skeuomorphic"), prefs.activeTheme);
        Assert::IsTrue (fs.Exists (LegacyGlobalPathForTest (baseDir)));
    }


    // The review that produced these found the branch's own gate covered only
    // one of two writers, and that three of the paths around it left the store
    // or the sheet out of step with disk. Each test below pins one of those.

    TEST_METHOD (SaveDelta_WhenTheWriteIsRefused_KeepsTheOldDelta)
    {
        FaultyFileSystem             fs;
        GlobalUserPrefs              prefs;
        JsonValue                    merged;
        UserConfigStore::LoadReport  report;
        HRESULT                      hr          = S_OK;
        UserConfigStore              store (L"C:\\Casso");
        JsonValue                    defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Authentic\"}");
        JsonValue                    currentJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Double\"}");


        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), kpszSeededPrefs);
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, report);
        AssertSucceeded (hr);

        // The file becomes unreadable behind the store's back, so the save is
        // refused. The cache has to fall back with it: keeping the new delta
        // makes every later read answer with a value that never reached disk.
        fs.fFailRead = true;

        hr = store.SaveDelta ("Apple2e", currentJson, defaultJson, fs);
        AssertFailed (hr);

        fs.fFailRead = false;

        hr = store.Load ("Apple2e", defaultJson, fs, merged);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("Maximum"),
                          FindObjectValueForTest (merged, "speedMode")->GetString());
    }


    TEST_METHOD (Load_WhenOnlyTheGlobalIsBad_StillAnswersAHelperStore)
    {
        InMemoryFileSystem  fs;
        JsonValue           merged;
        HRESULT             hr          = S_OK;
        UserConfigStore     store (L"C:\\Casso");
        JsonValue           defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Authentic\"}");
        std::string         original    = "{\"global\":42,\"machines\":{\"Apple2e\":{\"$cassoMachineVersion\":1,\"speedMode\":\"Maximum\"}}}";


        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), original);
        AssertSucceeded (hr);

        // A store that never ran LoadAll -- Casso builds several during
        // startup to read a remembered disk or fold a machine delta. The
        // global section is the only thing wrong here, and it is not what this
        // caller came for, so failing over it would cost the machine's
        // overrides for the whole launch.
        hr = store.Load ("Apple2e", defaultJson, fs, merged);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("Maximum"),
                          FindObjectValueForTest (merged, "speedMode")->GetString());
    }


    TEST_METHOD (LoadAll_WhenALegacyFileIsSkipped_ReportsItForTheUser)
    {
        InMemoryFileSystem           fs;
        GlobalUserPrefs              prefs;
        UserConfigStore::LoadReport  report;
        std::wstring                 message;
        HRESULT                      hr      = S_OK;
        std::wstring                 baseDir = L"C:\\Casso";
        UserConfigStore              store (baseDir);


        hr = fs.WriteAllText (LegacyMachinePathForTest (baseDir, "Foo"),
                              "{\"$cassoMachineVersion\":2}");
        AssertSucceeded (hr);
        hr = fs.WriteAllText (LegacyMachinePathForTest (baseDir, "Bar"), "{ truncated");
        AssertSucceeded (hr);

        hr = store.LoadAll (prefs, fs, report);
        AssertSucceeded (hr);

        // Reporting success while one file did not come across is the whole
        // hazard: the unified file now exists, so the migration gate never
        // opens again and nothing would ever mention Bar.
        Assert::AreEqual (size_t (1), report.skippedLegacyFiles.size());

        message = UserConfigStore::ComposeSkippedLegacyMessage (baseDir, report);
        Assert::IsFalse (message.empty());
        Assert::IsTrue (message.find (report.skippedLegacyFiles[0]) != std::wstring::npos);
    }


    TEST_METHOD (ComposeSkippedLegacyMessage_IsEmptyWhenNothingWasSkipped)
    {
        UserConfigStore::LoadReport  report;


        // The caller shows whatever comes back, so a clean migration has to
        // produce nothing rather than a message about no files.
        Assert::IsTrue (UserConfigStore::ComposeSkippedLegacyMessage (
                            L"C:\\Casso", report).empty());
    }


    TEST_METHOD (Load_AfterATransientReadFailure_GoesBackForTheFile)
    {
        FaultyFileSystem             fs;
        GlobalUserPrefs              prefs;
        JsonValue                    merged;
        UserConfigStore::LoadReport  report;
        HRESULT                      hr          = S_OK;
        UserConfigStore              store (L"C:\\Casso");
        JsonValue                    defaultJson = ParseOrFail ("{\"$cassoMachineVersion\":1,\"speedMode\":\"Authentic\"}");


        hr = fs.WriteAllText (store.GetUserPrefsFilePath(), kpszSeededPrefs);
        AssertSucceeded (hr);

        fs.fFailRead = true;

        hr = store.LoadAll (prefs, fs, report);
        AssertFailed (hr);

        // The read never happened, so the store established nothing about the
        // document. Once the lock clears a machine load has to go and get it:
        // treating the failed attempt as "already read" left every machine on
        // shipped defaults for the rest of the session over a readable file.
        fs.fFailRead = false;

        hr = store.Load ("Apple2e", defaultJson, fs, merged);
        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("Maximum"),
                          FindObjectValueForTest (merged, "speedMode")->GetString());
    }
};
