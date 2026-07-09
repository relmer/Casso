#include "Pch.h"

#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/HardwarePage.h"

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
    constexpr uint32_t  s_kFixtureClockSpeedHz    = 1023000;
    constexpr size_t    s_kFixtureDevices         = 4;

    class RecordingSink : public ISettingsApplySink
    {
    public:
        SettingsSpeedMode  lastSpeed             = SettingsSpeedMode::Authentic;
        SettingsColorMode  lastColor             = SettingsColorMode::Color;
        bool               lastFloppySound       = true;
        std::string        lastMechanism;
        bool               lastWriteProtect[2]   = { false, false };
        float              lastDriveMotor        = -1.0f;
        float              lastDriveHead         = -1.0f;
        float              lastDriveDoor         = -1.0f;
        float              lastDriveOnePan       = 0.0f;
        float              lastDriveTwoPan       = 0.0f;
        int                queuedResetCount      = 0;
        int                applyCount            = 0;

        void ApplySpeedMode    (SettingsSpeedMode mode) override   { lastSpeed = mode; ++applyCount; }
        void ApplyColorMode    (SettingsColorMode mode) override   { lastColor = mode; ++applyCount; }
        void ApplyFloppySound  (bool enabled) override             { lastFloppySound = enabled; ++applyCount; }
        void ApplyMechanism    (const std::string & m) override    { lastMechanism = m; ++applyCount; }
        void ApplyDriveVolumes (float motor, float head, float door) override
        {
            lastDriveMotor = motor;
            lastDriveHead  = head;
            lastDriveDoor  = door;
            ++applyCount;
        }
        void ApplyDrivePan     (float driveOnePan, float driveTwoPan) override
        {
            lastDriveOnePan = driveOnePan;
            lastDriveTwoPan = driveTwoPan;
            ++applyCount;
        }
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
        "timing": { "clockSpeed": 1023000 },
        "ram": [ { "address": "0x0000", "size": "0xC000" } ],
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
            "writeMode": "copy-on-write",
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
        Assert::AreEqual (std::string ("TestMachine"), st.MachineInfo().name);
        Assert::AreEqual (s_kFixtureClockSpeedHz,  st.MachineInfo().clockSpeed);
        // Memory regions now a list of formatted strings (one per RAM
        // bank + systemRom if present). Fixture has 1 RAM entry, no
        // systemRom block.
        Assert::AreEqual (size_t (1), st.MachineInfo().memoryRegions.size());
        // Devices counts internal + slot entries.
        Assert::AreEqual (s_kFixtureDevices, st.MachineInfo().devices);
    }


    // System ROM display. A banked ROM (//c) shares one $C000-$FFFF window
    // between two `romBankSize` banks toggled by $C028, so it must report the
    // true installed size (2x a bank = 32K) with a name that explains the
    // size/window mismatch. Flat ROMs (][ / ][+ / //e) keep their single
    // fill-to-$FFFF span. Regression for the Settings memory map reading the
    // //c ROM as 16K when it is really 32K in two banks.
    TEST_METHOD (SystemRom_BankedReports32KTwoBanks_FlatUnchanged)
    {
        auto romRegion = [] (const SettingsMachineInfo & info)
        {
            for (const auto & r : info.memoryRegions)
            {
                if (r.name.rfind ("System ROM", 0) == 0)
                {
                    return r;
                }
            }
            return SettingsMemoryRegion {};
        };

        // Banked //c: romBankSize present, no explicit size.
        const char * cJson = R"JSON({
            "$cassoMachineVersion": 1,
            "name": "Apple //c",
            "timing": { "clockSpeed": 1023000 },
            "ram": [ { "address": "0x0000", "size": "0xC000" } ],
            "systemRom": { "address": "0xC000", "romBankSize": "0x4000", "romBankSelect": "0xC028" }
        })JSON";

        SettingsPanelState  cSt;
        JsonValue           cv = ParseOrFail (cJson);
        Assert::IsTrue (SUCCEEDED (cSt.LoadFromMachine ("Apple //c", cv, cv)));
        SettingsMemoryRegion  cRom = romRegion (cSt.MachineInfo());
        Assert::AreEqual (std::string ("System ROM (2 banks)"), cRom.name,         L"//c ROM name");
        Assert::AreEqual (std::string ("32K"),                  cRom.size,         L"//c ROM = 2x16K = 32K");
        Assert::AreEqual (std::string ("$C000-$FFFF"),          cRom.addressRange, L"//c banks share one window");

        // Flat //e: 16K fill-to-$FFFF from $C000.
        const char * eJson = R"JSON({
            "$cassoMachineVersion": 1,
            "name": "Apple //e",
            "timing": { "clockSpeed": 1023000 },
            "ram": [ { "address": "0x0000", "size": "0xC000" } ],
            "systemRom": { "address": "0xC000" }
        })JSON";

        SettingsPanelState  eSt;
        JsonValue           ev = ParseOrFail (eJson);
        Assert::IsTrue (SUCCEEDED (eSt.LoadFromMachine ("Apple //e", ev, ev)));
        SettingsMemoryRegion  eRom = romRegion (eSt.MachineInfo());
        Assert::AreEqual (std::string ("System ROM"),  eRom.name,         L"//e ROM name unchanged");
        Assert::AreEqual (std::string ("16K"),         eRom.size,         L"//e ROM = 16K");
        Assert::AreEqual (std::string ("$C000-$FFFF"), eRom.addressRange, L"//e ROM range");

        // Flat ][ / ][+: 12K fill-to-$FFFF from $D000.
        const char * twoJson = R"JSON({
            "$cassoMachineVersion": 1,
            "name": "Apple ][",
            "timing": { "clockSpeed": 1023000 },
            "ram": [ { "address": "0x0000", "size": "0xC000" } ],
            "systemRom": { "address": "0xD000" }
        })JSON";

        SettingsPanelState  twoSt;
        JsonValue           tv = ParseOrFail (twoJson);
        Assert::IsTrue (SUCCEEDED (twoSt.LoadFromMachine ("Apple ][", tv, tv)));
        SettingsMemoryRegion  twoRom = romRegion (twoSt.MachineInfo());
        Assert::AreEqual (std::string ("System ROM"),  twoRom.name,         L"][ ROM name unchanged");
        Assert::AreEqual (std::string ("12K"),         twoRom.size,         L"][ ROM = 12K");
        Assert::AreEqual (std::string ("$D000-$FFFF"), twoRom.addressRange, L"][ ROM range");
    }


    // RAM total headline. Sums every RAM region -- main + aux ($0000-$BFFF) and
    // both language-card banks ($D000-$FFFF) -- while EXCLUDING system ROM, so a
    // 128K //c reads "128K RAM" (not 160K with the 32K ROM folded in). A 48K ][
    // with no aux/LC reads "48K RAM".
    TEST_METHOD (MemoryTotal_SumsRamAcrossBanks_ExcludesRom)
    {
        // 128K //c: 48K main + 48K aux + 16K + 16K language-card banks.
        const char * cJson = R"JSON({
            "$cassoMachineVersion": 1,
            "name": "Apple //c",
            "timing": { "clockSpeed": 1023000 },
            "ram": [
                { "address": "0x0000", "size": "0xC000" },
                { "address": "0x0000", "size": "0xC000", "bank": "aux" }
            ],
            "systemRom": { "address": "0xC000", "romBankSize": "0x4000" },
            "internalDevices": [ { "type": "language-card" } ]
        })JSON";

        SettingsPanelState  cSt;
        JsonValue           cv = ParseOrFail (cJson);
        Assert::IsTrue (SUCCEEDED (cSt.LoadFromMachine ("Apple //c", cv, cv)));
        Assert::AreEqual (std::string ("128K RAM"), cSt.MachineInfo().ramSummary,
            L"48+48+16+16 = 128K; the 32K ROM must not be counted");

        // 48K ][: single main bank, no aux, no language card.
        const char * twoJson = R"JSON({
            "$cassoMachineVersion": 1,
            "name": "Apple ][",
            "timing": { "clockSpeed": 1023000 },
            "ram": [ { "address": "0x0000", "size": "0xC000" } ],
            "systemRom": { "address": "0xD000" }
        })JSON";

        SettingsPanelState  twoSt;
        JsonValue           tv = ParseOrFail (twoJson);
        Assert::IsTrue (SUCCEEDED (twoSt.LoadFromMachine ("Apple ][", tv, tv)));
        Assert::AreEqual (std::string ("48K RAM"), twoSt.MachineInfo().ramSummary,
            L"single 48K main bank");
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
        st.SetWriteMode      (SettingsWriteMode::CopyOnWrite);
        st.SetWriteProtect   (1, true);
        Assert::IsTrue (st.IsDirty());

        st.Cancel();

        Assert::IsFalse (st.IsDirty());
        Assert::IsTrue  (st.Prefs().speedMode          == SettingsSpeedMode::Authentic);
        Assert::IsTrue  (st.Prefs().colorMode          == SettingsColorMode::Color);
        Assert::IsTrue  (st.Prefs().writeMode          == SettingsWriteMode::BufferAndFlush);
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
        Assert::IsTrue  (st.Prefs().writeMode          == SettingsWriteMode::CopyOnWrite);
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
        st.SetWriteMode (SettingsWriteMode::CopyOnWrite);

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
        Assert::IsTrue (text.find ("\"writeMode\":\"copy-on-write\"") != std::string::npos);
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


    TEST_METHOD (MachineTab_List_Selection_Rebuilds_Downstream_State)
    {
        SettingsPanelState       st;
        HardwarePage             page;
        JsonValue                machineA = ParseOrFail (kFixtureJson);
        JsonValue                machineB = ParseOrFail (kFixtureJsonWithFlags);
        RECT                     rect     = { 0, 0, 640, 480 };
        std::vector<std::string> machines = { "machineA", "machineB" };



        DxuiDpiScaler                scaler;

        st.LoadFromMachine ("machineA", machineA, machineA);
        page.SetState (&st);
        page.SetMachineList (machines, { L"machineA", L"machineB" }, 0);
        page.SetOnMachineSelected ([&st, &machineB] (const std::string & machineName)
        {
            st.LoadFromMachine (machineName, machineB, machineB);
        });
        page.Layout (rect, scaler);
        page.Rebuild();

        Assert::AreEqual ((size_t) 2, page.Machines().size());
        Assert::AreEqual (0, page.ActiveMachineIndex());
        Assert::AreEqual (0, page.MachineDropdown().SelectedIndex());

        // Drive the dropdown directly: production routes the popup
        // through DxuiPopupHost (out-of-panel HWND), so the panel's
        // auto fan-out never sees clicks on an open menu. This test
        // exercises the dropdown's selection wiring, not the dispatch
        // path, so we hit the dropdown's legacy entry points directly.
        page.MachineDropdown().OnLButtonDown (180, 20);
        page.MachineDropdown().OnLButtonUp   (180, 20);
        page.MachineDropdown().OnLButtonDown (180, 80);
        page.MachineDropdown().OnLButtonUp   (180, 80);

        Assert::AreEqual (std::string ("machineB"), st.MachineName());
        Assert::AreEqual (1, page.ActiveMachineIndex());
        Assert::IsTrue   (st.Prefs().speedMode == SettingsSpeedMode::Double);
    }


    TEST_METHOD (HasDiskIIController_TracksSlot6EnabledState)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (kFixtureJson);


        st.LoadFromMachine ("X", v, v);

        // The fixture wires an enabled disk-ii controller into slot 6.
        Assert::IsTrue (st.HasDiskIIController(),
            L"A machine with an enabled disk-ii slot must report a controller.");

        // Locate the disk-ii entry and disable it; the controller must vanish
        // (this is exactly what hides the settings sheet's Disk tab, #84 B).
        const std::vector<HardwareEntry> & hw = st.Hardware();
        size_t  diskIdx = hw.size();
        for (size_t i = 0; i < hw.size(); ++i)
        {
            if (hw[i].type == "disk-ii") { diskIdx = i; break; }
        }
        Assert::IsTrue (diskIdx < hw.size(), L"Fixture must expose a disk-ii entry.");

        HRESULT  hr = st.SetHardwareEnabled (diskIdx, false);
        Assert::IsTrue  (SUCCEEDED (hr));
        Assert::IsFalse (st.HasDiskIIController(),
            L"Disabling the disk-ii slot must clear the controller.");

        // Re-enabling restores it.
        hr = st.SetHardwareEnabled (diskIdx, true);
        Assert::IsTrue (SUCCEEDED (hr));
        Assert::IsTrue (st.HasDiskIIController(),
            L"Re-enabling the disk-ii slot must restore the controller.");
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


    // FR-041: opening / closing the panel must not pause emulation.
    // The applied-sink interface deliberately lacks any pause hook so
    // the only way to reach the emulator from the panel is through the
    // existing per-field setters in Apply(). The state lifecycle calls
    // (LoadFromMachine, Cancel) never reach the sink at all -- this
    // test pins that contract by asserting zero sink invocations
    // around the load/cancel round-trip.
    TEST_METHOD (LoadAndCancel_DoNotDispatchAnythingToTheSink)
    {
        SettingsPanelState  st;
        RecordingSink       sink;
        JsonValue           v   = ParseOrFail (kFixtureJson);
        HRESULT             hr  = st.LoadFromMachine ("TestMachine", v, v);

        Assert::IsTrue (SUCCEEDED (hr));

        // No mutations applied: the sink should remain untouched
        // because nothing has called Apply().
        Assert::AreEqual (0, sink.applyCount,
            L"Load must never reach the apply sink (FR-041 -- no pause).");
        Assert::AreEqual (0, sink.queuedResetCount);

        st.SetSpeedMode (SettingsSpeedMode::Maximum);
        st.Cancel();

        // Cancel discards the dirty state without firing the sink.
        Assert::AreEqual (0, sink.applyCount,
            L"Cancel must never reach the apply sink.");
        Assert::AreEqual (0, sink.queuedResetCount);
    }
};

