#include "Pch.h"

#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/HardwarePage.h"

#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelStateTests
//
//  The Settings sheet's model: baseline versus staged state, dirty tracking,
//  and the reset-required question.
//
//  Cancel is the behavior under test, and it is a RESTORE rather than a
//  discard. Settings edits apply live to the running emulator, so reverting
//  means putting the baseline back -- the tests edit, cancel, and assert the
//  original values returned, which a discard-pending-changes model would fail.
//
//  Dirty and RequiresReset are asserted independently, since they answer
//  different questions: dirty decides whether to save, while only a hardware
//  ENABLE change needs a machine rebuild and the OK-button warning.
//
//  The //c's banked-ROM path through HasDiskIIController is covered, since its
//  built-in IWM never appears in the hardware list and the machine would
//  otherwise lose its Disk tab.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SettingsPanelStateTests)
{
public:

    static constexpr uint32_t  s_kFixtureClockSpeedHz    = 1023000;
    static constexpr size_t    s_kFixtureDevices         = 4;

    class RecordingSink : public ISettingsApplySink
    {
    public:
        SettingsSpeedMode  lastSpeed                  = SettingsSpeedMode::Authentic;
        SettingsColorMode  lastColor                  = SettingsColorMode::Color;
        bool               lastFloppySound            = true;
        std::string        lastMechanism;
        bool               lastWriteProtect[2]        = { false, false };
        float              lastDriveMotor             = -1.0f;
        float              lastDriveHead              = -1.0f;
        float              lastDriveDoor              = -1.0f;
        float              lastDriveOnePan            = 0.0f;
        float              lastDriveTwoPan            = 0.0f;
        bool               lastExternalDriveConnected = false;
        bool               lastMouseConnected         = true;
        int                queuedResetCount           = 0;
        int                applyCount                 = 0;

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

        void ApplyExternalDriveConnected (bool connected) override
        {
            lastExternalDriveConnected = connected;
            ++applyCount;
        }

        void ApplyMouseConnected (bool connected) override
        {
            lastMouseConnected = connected;
            ++applyCount;
        }

        void QueueMachineReset() override { ++queuedResetCount; }
    };


    JsonValue ParseOrFail (const std::string & text)
    {
        JsonValue        v;
        JsonParseError   e;
        HRESULT          hr = JsonParser::Parse (text, v, e);
        AssertSucceeded (hr, L"test fixture JSON must parse");
        return v;
    }


    // Compact-serialize emitted JSON so a test can assert on structural shape
    // by substring.
    std::string WriteCompactOrFail (const JsonValue & value)
    {
        std::string          text;
        JsonWriter::Options  opts;

        opts.fPretty = false;
        AssertSucceeded (JsonWriter::Write (value, opts, text),
                         L"emitted JSON must serialize");

        return text;
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

    TEST_METHOD (Load_Defaults_NotDirty_NoResetRequired)
    {
        SettingsPanelState  st;
        JsonValue           v  = ParseOrFail (kFixtureJson);
        HRESULT             hr = st.LoadFromMachine ("TestMachine", v, v);

        AssertSucceeded (hr);
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
        SettingsPanelState    cSt;
        SettingsPanelState    eSt;
        SettingsPanelState    twoSt;
        JsonValue             cv;
        SettingsMemoryRegion  cRom   = {};
        JsonValue             ev;
        SettingsMemoryRegion  eRom   = {};
        JsonValue             tv;
        SettingsMemoryRegion  twoRom = {};



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

        cv = ParseOrFail (cJson);
        AssertSucceeded (cSt.LoadFromMachine ("Apple //c", cv, cv));
        cRom = romRegion (cSt.MachineInfo());
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

        ev = ParseOrFail (eJson);
        AssertSucceeded (eSt.LoadFromMachine ("Apple //e", ev, ev));
        eRom = romRegion (eSt.MachineInfo());
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

        tv = ParseOrFail (twoJson);
        AssertSucceeded (twoSt.LoadFromMachine ("Apple ][", tv, tv));
        twoRom = romRegion (twoSt.MachineInfo());
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
        SettingsPanelState  cSt;
        SettingsPanelState  twoSt;
        JsonValue           cv;
        JsonValue           tv;



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

        cv = ParseOrFail (cJson);
        AssertSucceeded (cSt.LoadFromMachine ("Apple //c", cv, cv));
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

        tv = ParseOrFail (twoJson);
        AssertSucceeded (twoSt.LoadFromMachine ("Apple ][", tv, tv));
        Assert::AreEqual (std::string ("48K RAM"), twoSt.MachineInfo().ramSummary,
            L"single 48K main bank");
    }


    TEST_METHOD (Load_RejectsNonObjectJson)
    {
        SettingsPanelState  st;
        HRESULT             hr;
        JsonValue           obj;
        JsonValue           arr = ParseOrFail ("[1,2,3]");
        obj = ParseOrFail (kFixtureJson);

        hr = st.LoadFromMachine ("X", arr, obj);
        AssertFailed (hr, L"non-object default should fail");

        hr = st.LoadFromMachine ("X", obj, arr);
        AssertFailed (hr, L"non-object merged should fail");
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
        JsonValue           v      = ParseOrFail (kFixtureJsonWithFlags);
        size_t              kbdIdx = 0;
        HRESULT             hr     = S_OK;
        st.LoadFromMachine ("X", v, v);

        // Find the keyboard entry (required) and try to disable it.
        for (size_t i = 0; i < st.Hardware().size(); ++i)
        {
            if (st.Hardware()[i].type == "keyboard")
            {
                kbdIdx = i;
                break;
            }
        }

        hr = st.SetHardwareEnabled (kbdIdx, false);

        AssertFailed (hr,  L"required entry cannot be disabled");
        Assert::IsTrue  (st.Hardware()[kbdIdx].enabled);
        Assert::IsFalse (st.IsDirty());
    }


    TEST_METHOD (SetHardwareEnabled_PlatformLockedRejected)
    {
        SettingsPanelState  st;
        JsonValue           v         = ParseOrFail (kFixtureJsonWithFlags);
        size_t              lockedIdx = 0;
        HRESULT             hr        = S_OK;
        st.LoadFromMachine ("X", v, v);

        for (size_t i = 0; i < st.Hardware().size(); ++i)
        {
            if (st.Hardware()[i].type == "80col-card")
            {
                lockedIdx = i;
                break;
            }
        }

        hr = st.SetHardwareEnabled (lockedIdx, false);

        AssertFailed (hr, L"platform-locked entry cannot be disabled");
        Assert::IsTrue  (st.Hardware()[lockedIdx].enabled);
    }


    TEST_METHOD (SetHardwareEnabled_OptionalSlotToggles_DirtyAndResetRequired)
    {
        SettingsPanelState  st;
        JsonValue           v     = ParseOrFail (kFixtureJsonWithFlags);
        size_t              mbIdx = 0;
        HRESULT             hr    = S_OK;
        st.LoadFromMachine ("X", v, v);

        for (size_t i = 0; i < st.Hardware().size(); ++i)
        {
            if (st.Hardware()[i].type == "mockingboard")
            {
                mbIdx = i;
                break;
            }
        }

        hr = st.SetHardwareEnabled (mbIdx, false);

        AssertSucceeded (hr);
        Assert::IsTrue (st.IsDirty());
        Assert::IsTrue (st.RequiresReset(), L"hardware enable change forces reset");
    }


    TEST_METHOD (Apply_PushesLiveFieldsThroughSink)
    {
        SettingsPanelState  st;
        JsonValue           v       = ParseOrFail (kFixtureJson);
        RecordingSink       sink;
        JsonValue           outJson;
        HRESULT             hr      = S_OK;
        st.LoadFromMachine ("X", v, v);

        st.SetSpeedMode    (SettingsSpeedMode::Maximum);
        st.SetColorMode    (SettingsColorMode::Amber);
        st.SetFloppySound  (false);
        st.SetMechanism    ("alps");
        st.SetWriteProtect (0, true);

        hr = st.Apply (sink, outJson);

        AssertSucceeded (hr);
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
        JsonValue           v       = ParseOrFail (kFixtureJson);
        RecordingSink       sink;
        JsonValue           outJson;
        st.LoadFromMachine ("X", v, v);


        st.SetSpeedMode (SettingsSpeedMode::Double);
        AssertSucceeded (st.Apply (sink, outJson));
        Assert::AreEqual (0, sink.queuedResetCount,
                          L"Live edits must not require reset/pause semantics.");

        st.SetSpeedMode (SettingsSpeedMode::Maximum);
        AssertSucceeded (st.Apply (sink, outJson));
        Assert::AreEqual (0, sink.queuedResetCount,
                          L"Repeated applies while panel remains open must stay non-blocking.");
    }


    TEST_METHOD (Apply_HardwareChangeQueuesReset)
    {
        SettingsPanelState  st;
        JsonValue           v       = ParseOrFail (kFixtureJsonWithFlags);
        RecordingSink       sink;
        JsonValue           outJson;
        HRESULT             hr      = S_OK;
        st.LoadFromMachine ("X", v, v);

        for (size_t i = 0; i < st.Hardware().size(); ++i)
        {
            if (st.Hardware()[i].type == "mockingboard")
            {
                (void) st.SetHardwareEnabled (i, false);
                break;
            }
        }

        hr = st.Apply (sink, outJson);

        AssertSucceeded (hr);
        Assert::AreEqual (1, sink.queuedResetCount, L"hw change -> reset queued");
    }


    TEST_METHOD (Apply_EmitsJsonWithUpdatedUiPrefsBlock)
    {
        SettingsPanelState   st;
        JsonValue            v       = ParseOrFail (kFixtureJson);
        RecordingSink        sink;
        JsonValue            outJson;
        std::string          text;
        JsonWriter::Options  opts;
        st.LoadFromMachine ("X", v, v);

        st.SetSpeedMode (SettingsSpeedMode::Double);
        st.SetWriteMode (SettingsWriteMode::CopyOnWrite);

        st.Apply (sink, outJson);

        // Round-trip through writer to assert structural shape.
        opts.fPretty = false;
        AssertSucceeded (JsonWriter::Write (outJson, opts, text));
        Assert::IsTrue (text.find ("\"$cassoUiPrefs\"") != std::string::npos);
        Assert::IsTrue (text.find ("\"speedMode\":\"double\"") != std::string::npos);
        Assert::IsTrue (text.find ("\"writeMode\":\"copy-on-write\"") != std::string::npos);
        Assert::IsTrue (text.find ("\"$cassoMachineVersion\"") != std::string::npos,
                        L"version key must round-trip");
    }


    // //c external-drive connect toggle. A live UI pref: it must
    // round-trip through $cassoUiPrefs, push through the sink on Apply, make
    // the panel dirty, and -- unlike a hardware enable -- never queue a reset.
    TEST_METHOD (ExternalDriveConnected_RoundTripsAndPushesLiveNoReset)
    {
        SettingsPanelState  st;
        JsonValue           v        = ParseOrFail (kFixtureJson);
        RecordingSink       sink;
        JsonValue           outJson;
        SettingsUiPrefs     reloaded;
        st.LoadFromMachine ("X", v, v);

        Assert::IsFalse (st.Prefs().externalDriveConnected, L"defaults to not-connected");
        Assert::IsFalse (st.IsDirty());

        st.SetExternalDriveConnected (true);
        Assert::IsTrue  (st.Prefs().externalDriveConnected);
        Assert::IsTrue  (st.IsDirty(), L"toggling connect makes the panel dirty");
        Assert::IsFalse (st.RequiresReset(), L"live UI pref -> no reset required");

        AssertSucceeded (st.Apply (sink, outJson));
        Assert::IsTrue (sink.lastExternalDriveConnected, L"pushed live through sink");
        Assert::AreEqual (0, sink.queuedResetCount, L"connect toggle never queues a reset");

        // The connected state persists into the emitted $cassoUiPrefs, and
        // re-loading that JSON restores it.
        AssertSucceeded (SettingsPanelState::ExtractUiPrefs (outJson, reloaded));
        Assert::IsTrue (reloaded.externalDriveConnected, L"externalDriveConnected round-trips");
    }


    // A //c-shaped machine: no slots, a back panel instead. Its disk port is
    // where the external drive lives now.
    const char * kFixtureCcJson = R"JSON({
        "$cassoMachineVersion": 2,
        "name": "TestCc",
        "cpu": "65C02",
        "timing": { "clockSpeed": 1023000 },
        "ram": [ { "address": "0x0000", "size": "0xC000" } ],
        "systemRom": { "address": "0xC000", "file": "x.rom", "romBankSize": "0x4000" },
        "internalDevices": [ { "type": "keyboard" } ],
        "ports": [
            { "name": "disk",     "device": "" },
            { "name": "serial1",  "device": "" },
            { "name": "joystick", "device": "" }
        ]
    })JSON";


    // The disk port is the answer once the machine declares one. Both it and
    // the legacy boolean can be on disk at the same time -- the fold that
    // retires the boolean only runs on a version bump -- so the precedence
    // has to be explicit rather than incidental.
    TEST_METHOD (DiskPortOutranksTheLegacyExternalDriveBoolean)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 2,
            "name": "TestCc",
            "cpu": "65C02",
            "timing": { "clockSpeed": 1023000 },
            "ram": [],
            "internalDevices": [],
            "ports": [ { "name": "disk", "device": "disk-iic-drive" } ],
            "$cassoUiPrefs": { "externalDriveConnected": false }
        })JSON");

        st.LoadFromMachine ("TestCc", v, v);

        Assert::IsTrue (st.Prefs().externalDriveConnected,
            L"an occupied disk port means attached, whatever the stale "
            L"boolean beside it says.");
        Assert::IsFalse (st.IsDirty(),
            L"reading the port is not a user edit.");
    }


    // The port is what gets written now. The legacy key must not come back
    // alongside it, or there are two answers to one question on disk again.
    TEST_METHOD (ExternalDriveTogglePersistsAsAPortNotABoolean)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (kFixtureCcJson);
        RecordingSink       sink;
        JsonValue           outJson;
        std::string         text;

        st.LoadFromMachine ("TestCc", v, v);
        Assert::IsFalse (st.Prefs().externalDriveConnected, L"starts unoccupied");

        st.SetExternalDriveConnected (true);
        AssertSucceeded (st.Apply (sink, outJson));

        text = WriteCompactOrFail (outJson);

        Assert::IsTrue (text.find ("\"disk-iic-drive\"") != std::string::npos,
            L"the attached drive is written onto the disk port.");
        Assert::IsTrue (text.find ("\"externalDriveConnected\"") == std::string::npos,
            L"the legacy boolean must NOT be written when a disk port exists.");
        Assert::IsTrue (text.find ("\"serial1\"")  != std::string::npos &&
                        text.find ("\"joystick\"") != std::string::npos,
            L"the whole back panel is written back -- a user ports array "
            L"replaces the default's wholesale.");
        Assert::IsTrue (sink.lastExternalDriveConnected,
            L"still pushed live through the sink; no reset.");
        Assert::AreEqual (0, sink.queuedResetCount);
    }


    // Round-trip: what Apply emitted must load back as the same answer.
    TEST_METHOD (ExternalDrivePortRoundTripsThroughLoad)
    {
        SettingsPanelState  first;
        SettingsPanelState  second;
        JsonValue           v = ParseOrFail (kFixtureCcJson);
        RecordingSink       sink;
        JsonValue           outJson;

        first.LoadFromMachine ("TestCc", v, v);
        first.SetExternalDriveConnected (true);
        AssertSucceeded (first.Apply (sink, outJson));

        second.LoadFromMachine ("TestCc", outJson, outJson);

        Assert::IsTrue (second.Prefs().externalDriveConnected,
            L"the attached drive survives a save/load round trip.");
    }


    // A machine with no back panel keeps the boolean, because there is no
    // port to hold the answer instead. This is what stops the //e family
    // losing a setting they never had anywhere else to put.
    TEST_METHOD (AMachineWithNoDiskPortStillWritesTheBoolean)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (kFixtureJson);
        RecordingSink       sink;
        JsonValue           outJson;
        std::string         text;

        st.LoadFromMachine ("X", v, v);
        st.SetExternalDriveConnected (true);
        AssertSucceeded (st.Apply (sink, outJson));
        text = WriteCompactOrFail (outJson);

        Assert::IsTrue (text.find ("\"externalDriveConnected\"") != std::string::npos,
            L"no disk port means the boolean is still the only home.");
    }


    // A carded machine's second drive is the Disk ][ card's second connector.
    // An UNDECLARED port list means the card has not been described rather
    // than emptied, so it must read as attached -- the two drives every such
    // config has always behaved as having.
    TEST_METHOD (SecondDrive_UndeclaredPortsReadAsAttached)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (kFixtureJson);

        st.LoadFromMachine ("X", v, v);

        Assert::IsTrue (st.SecondDriveAttached(),
            L"a Disk ][ with no ports declared has both drives.");
    }


    // Detaching must write BOTH connectors. A one-element list would take
    // drive 1 away as a side effect of removing drive 2.
    TEST_METHOD (SecondDrive_DetachingWritesBothConnectors)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (kFixtureJson);
        RecordingSink       sink;
        JsonValue           outJson;
        std::string         text;

        st.LoadFromMachine ("X", v, v);
        st.SetSecondDriveAttached (false);

        Assert::IsFalse (st.SecondDriveAttached());
        Assert::IsTrue  (st.IsDirty(), L"detaching a drive is an edit");
        Assert::IsFalse (st.RequiresReset(),
            L"attaching a drive is a live change, not a machine rebuild");

        AssertSucceeded (st.Apply (sink, outJson));
        text = WriteCompactOrFail (outJson);

        Assert::IsTrue (text.find ("[\"disk-ii-drive\",\"\"]") != std::string::npos,
            L"drive 1 stays attached and drive 2 goes -- both written, or "
            L"removing the second would silently remove the first.");
    }


    TEST_METHOD (SecondDrive_ReattachingRestoresTheDrive)
    {
        SettingsPanelState  st;
        SettingsPanelState  reloaded;
        JsonValue           v = ParseOrFail (kFixtureJson);
        RecordingSink       sink;
        JsonValue           outJson;

        st.LoadFromMachine ("X", v, v);
        st.SetSecondDriveAttached (false);
        AssertSucceeded (st.Apply (sink, outJson));

        reloaded.LoadFromMachine ("X", outJson, outJson);
        Assert::IsFalse (reloaded.SecondDriveAttached(), L"detached survives a round trip");

        reloaded.SetSecondDriveAttached (true);
        Assert::IsTrue (reloaded.SecondDriveAttached(), L"and can be put back");
    }


    // The //c answers from its back-panel disk port, not from a card. Its
    // second drive is an external unit on a cable, so the two stores must not
    // be confused for one another.
    TEST_METHOD (SecondDrive_OnACcReadsTheBackPanelPort)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (kFixtureCcJson);

        st.LoadFromMachine ("TestCc", v, v);
        Assert::IsFalse (st.SecondDriveAttached(), L"//c disk port starts empty");

        st.SetSecondDriveAttached (true);
        Assert::IsTrue (st.SecondDriveAttached());
        Assert::IsTrue (st.Prefs().externalDriveConnected,
            L"the //c's second drive IS its external drive -- one question.");
    }


    // A card the user turned off has no drives to attach anything to.
    TEST_METHOD (SecondDrive_ADisabledCardReportsDetached)
    {
        SettingsPanelState  st;
        JsonValue           v = ParseOrFail (R"JSON({
            "$cassoMachineVersion": 1,
            "internalDevices": [],
            "slots": [
                { "slot": 6, "device": "disk-ii", "enabled": false }
            ]
        })JSON");

        st.LoadFromMachine ("X", v, v);

        Assert::IsFalse (st.SecondDriveAttached(),
            L"a disabled Disk ][ is not present, so neither are its drives.");
    }


    // supportsExternalDrive is derived from a banked system ROM (romBankSize),
    // the //c's defining trait -- it is the only machine whose second drive is
    // an optional add-on. Flat-ROM machines (//e / ][) report false.
    TEST_METHOD (SupportsExternalDrive_TrueForBankedRomOnly)
    {
        SettingsPanelState  cSt;
        SettingsPanelState  eSt;
        JsonValue           cv;
        JsonValue           ev;



        const char * cJson = R"JSON({
            "$cassoMachineVersion": 1,
            "name": "Apple //c",
            "timing": { "clockSpeed": 1023000 },
            "ram": [ { "address": "0x0000", "size": "0xC000" } ],
            "systemRom": { "address": "0xC000", "romBankSize": "0x4000", "romBankSelect": "0xC028" }
        })JSON";

        const char * eJson = R"JSON({
            "$cassoMachineVersion": 1,
            "name": "Apple //e",
            "timing": { "clockSpeed": 1023000 },
            "ram": [ { "address": "0x0000", "size": "0xC000" } ],
            "systemRom": { "address": "0xC000" }
        })JSON";

        cv = ParseOrFail (cJson);
        AssertSucceeded (cSt.LoadFromMachine ("Apple //c", cv, cv));
        Assert::IsTrue (cSt.MachineInfo().supportsExternalDrive, L"//c has optional external drive");

        ev = ParseOrFail (eJson);
        AssertSucceeded (eSt.LoadFromMachine ("Apple //e", ev, ev));
        Assert::IsFalse (eSt.MachineInfo().supportsExternalDrive, L"//e second drive is fixed hardware");
    }


    // The //c's drive is a built-in IWM, not a "disk-ii" slot, so the
    // hardware-list scan alone would hide the Settings Disk tab on the //c
    // (the dynamic-tab gate). A banked system ROM must count as a
    // controller. Regression: Settings > Disk disappeared on the //c.
    TEST_METHOD (HasDiskIIController_TrueForBuiltInIwmMachine)
    {
        SettingsPanelState  st;
        JsonValue           v;



        const char * cJson = R"JSON({
            "$cassoMachineVersion": 1,
            "name": "Apple //c",
            "timing": { "clockSpeed": 1023000 },
            "ram": [ { "address": "0x0000", "size": "0xC000" } ],
            "systemRom": { "address": "0xC000", "romBankSize": "0x4000", "romBankSelect": "0xC028" }
        })JSON";

        v = ParseOrFail (cJson);
        AssertSucceeded (st.LoadFromMachine ("Apple //c", v, v));
        Assert::IsTrue (st.HasDiskIIController(),
            L"//c built-in IWM (no disk-ii slot) must still yield a Disk tab");
    }


    // //c mouse peripheral toggle: defaults CONNECTED,
    // round-trips $cassoUiPrefs, pushes live through the sink, no reset.
    TEST_METHOD (MouseConnected_DefaultsOnRoundTripsNoReset)
    {
        SettingsPanelState  st;
        JsonValue           v        = ParseOrFail (kFixtureJson);
        RecordingSink       sink;
        JsonValue           outJson;
        SettingsUiPrefs     reloaded;
        st.LoadFromMachine ("X", v, v);

        Assert::IsTrue  (st.Prefs().mouseConnected, L"defaults to connected");
        st.SetMouseConnected (false);
        Assert::IsTrue  (st.IsDirty());
        Assert::IsFalse (st.RequiresReset(), L"live UI pref -> no reset");

        AssertSucceeded (st.Apply (sink, outJson));
        Assert::IsFalse (sink.lastMouseConnected, L"pushed live");
        Assert::AreEqual (0, sink.queuedResetCount);

        AssertSucceeded (SettingsPanelState::ExtractUiPrefs (outJson, reloaded));
        Assert::IsFalse (reloaded.mouseConnected, L"mouseConnected round-trips");
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
        AssertSucceeded (st.Apply (sink, outA));

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
        SettingsPanelState  st;
        HardwarePage        page;
        JsonValue           machineA = ParseOrFail (kFixtureJson);
        JsonValue           machineB = ParseOrFail (kFixtureJsonWithFlags);
        RECT                rect     = { 0, 0, 640, 480 };
        DxuiDpiScaler       scaler;
        std::vector<std::string> machines = { "machineA", "machineB" };




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
        JsonValue           v       = ParseOrFail (kFixtureJson);
        size_t              diskIdx = 0;
        HRESULT             hr      = S_OK;


        st.LoadFromMachine ("X", v, v);

        // The fixture wires an enabled disk-ii controller into slot 6.
        Assert::IsTrue (st.HasDiskIIController(),
            L"A machine with an enabled disk-ii slot must report a controller.");

        // Locate the disk-ii entry and disable it; the controller must vanish
        // (this is exactly what hides the settings sheet's Disk tab, #84 B).
        const std::vector<HardwareEntry>  & hw      = st.Hardware();
        diskIdx = hw.size();
        for (size_t i = 0; i < hw.size(); ++i)
        {
            if (hw[i].type == "disk-ii") { diskIdx = i; break; }
        }

        Assert::IsTrue (diskIdx < hw.size(), L"Fixture must expose a disk-ii entry.");

        hr = st.SetHardwareEnabled (diskIdx, false);
        AssertSucceeded (hr);
        Assert::IsFalse (st.HasDiskIIController(),
            L"Disabling the disk-ii slot must clear the controller.");

        // Re-enabling restores it.
        hr = st.SetHardwareEnabled (diskIdx, true);
        AssertSucceeded (hr);
        Assert::IsTrue (st.HasDiskIIController(),
            L"Re-enabling the disk-ii slot must restore the controller.");
    }


    TEST_METHOD (BuildJson_PreservesUnrelatedKeys)
    {
        SettingsPanelState   st;
        RecordingSink        sink;
        JsonValue            outJson;
        std::string          text;
        JsonWriter::Options  opts;
        JsonValue            v;



        // Build a JSON with a custom unknown field; ensure it survives.
        const char * j = R"JSON({
            "$cassoMachineVersion": 1,
            "name": "TestMachine",
            "customExtension": { "preserved": true, "n": 42 },
            "internalDevices": [ { "type": "keyboard" } ],
            "slots": [ { "slot": 6, "device": "disk-ii" } ]
        })JSON";

        v = ParseOrFail (j);
        st.LoadFromMachine ("X", v, v);

        st.SetSpeedMode (SettingsSpeedMode::Double);

        st.Apply (sink, outJson);

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

        AssertSucceeded (hr);

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


