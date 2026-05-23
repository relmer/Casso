#include "Pch.h"

#include "Ui/SettingsPanelState.h"

#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingSink
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    class RecordingSink : public ISettingsApplySink
    {
    public:
        SettingsSpeedMode  lastSpeed             = SettingsSpeedMode::Authentic;
        SettingsColorMode  lastColor             = SettingsColorMode::Color;
        bool               lastFloppySound       = true;
        std::string        lastMechanism;
        bool               lastWriteProtect[2]   = { false, false };
        int                queuedResetCount      = 0;
        int                applyCount            = 0;

        void ApplySpeedMode    (SettingsSpeedMode mode) override   { lastSpeed = mode; ++applyCount; }
        void ApplyColorMode    (SettingsColorMode mode) override   { lastColor = mode; ++applyCount; }
        void ApplyFloppySound  (bool enabled) override             { lastFloppySound = enabled; ++applyCount; }
        void ApplyMechanism    (const std::string & m) override    { lastMechanism = m; ++applyCount; }
        void ApplyWriteProtect (int drive, bool wp) override
        {
            if (drive >= 0 && drive < 2) lastWriteProtect[drive] = wp;
            ++applyCount;
        }
        void QueueMachineReset () override { ++queuedResetCount; }
    };


    JsonValue ParseOrFail (const std::string & text)
    {
        JsonValue        v;
        JsonParseError   e;
        HRESULT          hr = JsonParser::Parse (text, v, e);
        Assert::IsTrue (SUCCEEDED (hr), L"test fixture JSON must parse");
        return v;
    }


    const char * kFixtureJson = R"JSON({
        "$cassoMachineVersion": 1,
        "name": "TestMachine",
        "cpu": "6502",
        "internalDevices": [
            { "type": "keyboard" },
            { "type": "speaker" }
        ],
        "slots": [
            { "slot": 6, "device": "disk-ii" },
            { "slot": 4, "device": "mockingboard" }
        ]
    })JSON";


    // Same shape with a platform-locked + a required device, plus a
    // pre-existing $cassoUiPrefs block.
    const char * kFixtureJsonWithFlags = R"JSON({
        "$cassoMachineVersion": 1,
        "internalDevices": [
            { "type": "keyboard", "capabilityFlag": "required" },
            { "type": "80col-card", "capabilityFlag": "platform-locked", "lockReason": "integrated on //c" }
        ],
        "slots": [
            { "slot": 6, "device": "disk-ii" },
            { "slot": 4, "device": "mockingboard", "capabilityFlag": "optional" }
        ],
        "$cassoUiPrefs": {
            "speedMode": "double",
            "colorMode": "green",
            "floppySoundEnabled": false,
            "floppyMechanism": "alps",
            "writeProtect": [ true, false ]
        }
    })JSON";
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelStateTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SettingsPanelStateTests)
{
public:

    TEST_METHOD (Load_Defaults_NotDirty_NoResetRequired)
    {
        SettingsPanelState  st;
        JsonValue           v  = ParseOrFail (kFixtureJson);
        HRESULT             hr = st.LoadFromMachine ("TestMachine", v, v);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsFalse (st.IsDirty(),       L"fresh load -> not dirty");
        Assert::IsFalse (st.RequiresReset(), L"fresh load -> no reset needed");
        Assert::AreEqual (std::string ("TestMachine"), st.MachineName());
    }


    TEST_METHOD (Load_RejectsNonObjectJson)
    {
        SettingsPanelState  st;
        JsonValue           arr = ParseOrFail ("[1,2,3]");
        JsonValue           obj = ParseOrFail (kFixtureJson);
        HRESULT             hr;

        hr = st.LoadFromMachine ("X", arr, obj);
        Assert::IsTrue (FAILED (hr), L"non-object default should fail");

        hr = st.LoadFromMachine ("X", obj, arr);
        Assert::IsTrue (FAILED (hr), L"non-object merged should fail");
    }


    TEST_METHOD (SetSpeedMode_MakesStateDirtyButNotReset)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (kFixtureJson);
        st.LoadFromMachine ("X", v, v);

        st.SetSpeedMode (SettingsSpeedMode::Maximum);

        Assert::IsTrue  (st.IsDirty(),       L"speed change -> dirty");
        Assert::IsFalse (st.RequiresReset(), L"speed change is live-applicable");
    }


    TEST_METHOD (Cancel_RestoresOriginalPrefs)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (kFixtureJson);
        st.LoadFromMachine ("X", v, v);

        st.SetSpeedMode      (SettingsSpeedMode::Maximum);
        st.SetColorMode      (SettingsColorMode::Amber);
        st.SetFloppySound    (false);
        st.SetWriteProtect   (1, true);
        Assert::IsTrue (st.IsDirty());

        st.Cancel();

        Assert::IsFalse (st.IsDirty());
        Assert::IsTrue  (st.Prefs().speedMode          == SettingsSpeedMode::Authentic);
        Assert::IsTrue  (st.Prefs().colorMode          == SettingsColorMode::Color);
        Assert::IsTrue  (st.Prefs().floppySoundEnabled == true);
        Assert::IsFalse (st.Prefs().writeProtect[1]);
    }


    TEST_METHOD (LoadFromMachine_PicksUpExistingUiPrefsBlock)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (kFixtureJsonWithFlags);
        st.LoadFromMachine ("X", v, v);

        Assert::IsTrue  (st.Prefs().speedMode          == SettingsSpeedMode::Double);
        Assert::IsTrue  (st.Prefs().colorMode          == SettingsColorMode::Green);
        Assert::IsFalse (st.Prefs().floppySoundEnabled);
        Assert::AreEqual (std::string ("alps"), st.Prefs().floppyMechanism);
        Assert::IsTrue  (st.Prefs().writeProtect[0]);
        Assert::IsFalse (st.Prefs().writeProtect[1]);
        Assert::IsFalse (st.IsDirty(), L"loaded prefs are the baseline");
    }


    TEST_METHOD (SetHardwareEnabled_RequiredEntryRejected)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (kFixtureJsonWithFlags);
        st.LoadFromMachine ("X", v, v);

        // Find the keyboard entry (required) and try to disable it.
        size_t  kbdIdx = 0;
        for (size_t i = 0; i < st.Hardware().size(); ++i)
        {
            if (st.Hardware()[i].type == "keyboard")
            {
                kbdIdx = i;
                break;
            }
        }

        HRESULT  hr = st.SetHardwareEnabled (kbdIdx, false);

        Assert::IsTrue  (FAILED (hr),  L"required entry cannot be disabled");
        Assert::IsTrue  (st.Hardware()[kbdIdx].enabled);
        Assert::IsFalse (st.IsDirty());
    }


    TEST_METHOD (SetHardwareEnabled_PlatformLockedRejected)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (kFixtureJsonWithFlags);
        st.LoadFromMachine ("X", v, v);

        size_t  lockedIdx = 0;
        for (size_t i = 0; i < st.Hardware().size(); ++i)
        {
            if (st.Hardware()[i].type == "80col-card")
            {
                lockedIdx = i;
                break;
            }
        }

        HRESULT  hr = st.SetHardwareEnabled (lockedIdx, false);

        Assert::IsTrue  (FAILED (hr), L"platform-locked entry cannot be disabled");
        Assert::IsTrue  (st.Hardware()[lockedIdx].enabled);
    }


    TEST_METHOD (SetHardwareEnabled_OptionalSlotToggles_DirtyAndResetRequired)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (kFixtureJsonWithFlags);
        st.LoadFromMachine ("X", v, v);

        size_t  mbIdx = 0;
        for (size_t i = 0; i < st.Hardware().size(); ++i)
        {
            if (st.Hardware()[i].type == "mockingboard")
            {
                mbIdx = i;
                break;
            }
        }

        HRESULT  hr = st.SetHardwareEnabled (mbIdx, false);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue (st.IsDirty());
        Assert::IsTrue (st.RequiresReset(), L"hardware enable change forces reset");
    }


    TEST_METHOD (Apply_PushesLiveFieldsThroughSink)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (kFixtureJson);
        st.LoadFromMachine ("X", v, v);

        st.SetSpeedMode    (SettingsSpeedMode::Maximum);
        st.SetColorMode    (SettingsColorMode::Amber);
        st.SetFloppySound  (false);
        st.SetMechanism    ("alps");
        st.SetWriteProtect (0, true);

        RecordingSink  sink;
        JsonValue      outJson;
        HRESULT        hr = st.Apply (sink, outJson);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue  (sink.lastSpeed         == SettingsSpeedMode::Maximum);
        Assert::IsTrue  (sink.lastColor         == SettingsColorMode::Amber);
        Assert::IsFalse (sink.lastFloppySound);
        Assert::AreEqual (std::string ("alps"), sink.lastMechanism);
        Assert::IsTrue  (sink.lastWriteProtect[0]);
        Assert::AreEqual (0, sink.queuedResetCount, L"no hw change -> no reset queued");
    }


    TEST_METHOD (Apply_LiveEditsRemainNonBlockingAcrossRepeatedApplies)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (kFixtureJson);
        st.LoadFromMachine ("X", v, v);

        RecordingSink  sink;
        JsonValue      outJson;

        st.SetSpeedMode (SettingsSpeedMode::Double);
        Assert::IsTrue (SUCCEEDED (st.Apply (sink, outJson)));
        Assert::AreEqual (0, sink.queuedResetCount,
                          L"Live edits must not require reset/pause semantics.");

        st.SetSpeedMode (SettingsSpeedMode::Maximum);
        Assert::IsTrue (SUCCEEDED (st.Apply (sink, outJson)));
        Assert::AreEqual (0, sink.queuedResetCount,
                          L"Repeated applies while panel remains open must stay non-blocking.");
    }


    TEST_METHOD (Apply_HardwareChangeQueuesReset)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (kFixtureJsonWithFlags);
        st.LoadFromMachine ("X", v, v);

        for (size_t i = 0; i < st.Hardware().size(); ++i)
        {
            if (st.Hardware()[i].type == "mockingboard")
            {
                (void) st.SetHardwareEnabled (i, false);
                break;
            }
        }

        RecordingSink  sink;
        JsonValue      outJson;
        HRESULT        hr = st.Apply (sink, outJson);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (1, sink.queuedResetCount, L"hw change -> reset queued");
    }


    TEST_METHOD (Apply_EmitsJsonWithUpdatedUiPrefsBlock)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (kFixtureJson);
        st.LoadFromMachine ("X", v, v);

        st.SetSpeedMode (SettingsSpeedMode::Double);

        RecordingSink  sink;
        JsonValue      outJson;
        st.Apply (sink, outJson);

        // Round-trip through writer to assert structural shape.
        std::string         text;
        JsonWriter::Options opts;
        opts.fPretty = false;
        Assert::IsTrue (SUCCEEDED (JsonWriter::Write (outJson, opts, text)));
        Assert::IsTrue (text.find ("\"$cassoUiPrefs\"") != std::string::npos);
        Assert::IsTrue (text.find ("\"speedMode\":\"double\"") != std::string::npos);
        Assert::IsTrue (text.find ("\"$cassoMachineVersion\"") != std::string::npos,
                        L"version key must round-trip");
    }


    TEST_METHOD (MachineSwitch_RebindsToNewMachine)
    {
        SettingsPanelState  st;
        JsonValue           a = ParseOrFail (kFixtureJson);
        JsonValue           b = ParseOrFail (kFixtureJsonWithFlags);

        st.LoadFromMachine ("machineA", a, a);
        st.SetSpeedMode (SettingsSpeedMode::Maximum);
        Assert::IsTrue (st.IsDirty());

        // Switching machines reloads -- existing edits are discarded.
        st.LoadFromMachine ("machineB", b, b);

        Assert::AreEqual (std::string ("machineB"), st.MachineName());
        Assert::IsFalse  (st.IsDirty(),
                          L"snapshot reset to new machine's baseline");
        Assert::IsTrue   (st.Prefs().speedMode == SettingsSpeedMode::Double,
                          L"baseline pulled from machineB's $cassoUiPrefs");
    }


    TEST_METHOD (MachineSwitch_SpeedEditsStayMachineScoped)
    {
        SettingsPanelState  st;
        JsonValue           machineA = ParseOrFail (kFixtureJson);
        JsonValue           machineB = ParseOrFail (kFixtureJsonWithFlags);
        RecordingSink       sink;
        JsonValue           outA;

        st.LoadFromMachine ("machineA", machineA, machineA);
        st.SetSpeedMode (SettingsSpeedMode::Maximum);
        Assert::IsTrue (SUCCEEDED (st.Apply (sink, outA)));

        // Rebind to machineB merged data that still carries "double".
        st.LoadFromMachine ("machineB", machineB, machineB);
        Assert::IsTrue (st.Prefs().speedMode == SettingsSpeedMode::Double);

        // Rebind back to machineA using the applied JSON snapshot and
        // verify it restores machineA's saved speed only.
        st.LoadFromMachine ("machineA", machineA, outA);
        Assert::IsTrue (st.Prefs().speedMode == SettingsSpeedMode::Maximum);
    }


    TEST_METHOD (BuildJson_PreservesUnrelatedKeys)
    {
        // Build a JSON with a custom unknown field; ensure it survives.
        const char * j = R"JSON({
            "$cassoMachineVersion": 1,
            "name": "TestMachine",
            "customExtension": { "preserved": true, "n": 42 },
            "internalDevices": [ { "type": "keyboard" } ],
            "slots": [ { "slot": 6, "device": "disk-ii" } ]
        })JSON";

        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (j);
        st.LoadFromMachine ("X", v, v);

        st.SetSpeedMode (SettingsSpeedMode::Double);

        RecordingSink  sink;
        JsonValue      outJson;
        st.Apply (sink, outJson);

        std::string  text;
        JsonWriter::Options opts;
        opts.fPretty = false;
        JsonWriter::Write (outJson, opts, text);

        Assert::IsTrue (text.find ("\"customExtension\"") != std::string::npos);
        Assert::IsTrue (text.find ("\"preserved\":true") != std::string::npos);
    }
};
