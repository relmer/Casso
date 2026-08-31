#include "Pch.h"
#include "Core/MachineConfig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigTests
//
//  Validates the v2 machine config schema:
//    ram[], systemRom, characterRom, internalDevices[], slots[]
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineConfigTests)
{
public:

    TEST_METHOD (Load_ValidJson_ParsesAllFields)
    {
        std::string   json = MinimalJson();
        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertSucceeded (hr,
            std::format (L"Load should succeed: {}",
                std::wstring (error.begin(), error.end())).c_str());
        Assert::AreEqual (std::string ("TestMachine"), config.name,
            L"Name must be 'TestMachine'");
        Assert::AreEqual (std::string ("6502"), config.cpu,
            L"CPU must be '6502'");
        Assert::AreEqual (1023000u, config.clockSpeed,
            L"Clock speed must be 1023000");
        Assert::AreEqual (size_t (1), config.ram.size(),
            L"Should have 1 RAM region");
        Assert::AreEqual (Word (0x0000), config.ram[0].address,
            L"First RAM address must be 0x0000");
        Assert::AreEqual (Word (0xC000), config.ram[0].size,
            L"First RAM size must be 0xC000");
    }

    TEST_METHOD (Load_SystemRomResolved)
    {
        std::string   json = MinimalJson();
        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertSucceeded (hr,
            L"Load should succeed with valid ROM");

        Assert::IsFalse (config.systemRom.resolvedPath.empty(),
            L"systemRom resolvedPath must be populated");
        Assert::AreEqual (Word (0xD000), config.systemRom.address,
            L"systemRom address must be 0xD000");
    }

    TEST_METHOD (Load_MissingRom_ReturnsClearError)
    {
        std::string   json = MinimalJson();
        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveNone,
                                                config, error);

        AssertFailed (hr,
            L"Load should fail when ROM not found");
        Assert::IsTrue (error.find ("ROM file not found") != std::string::npos,
            L"Error message must mention 'ROM file not found'");
    }

    TEST_METHOD (Load_MissingName_ReturnsError)
    {
        MachineConfig          config;
        std::string            error;
        std::vector<fs::path>  paths;



        std::string json = R"({ "cpu": "6502" })";

        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, config, error);

        AssertFailed (hr,
            L"Missing 'name' field should cause failure");
        Assert::IsTrue (error.find ("name") != std::string::npos,
            L"Error should mention 'name'");
    }

    TEST_METHOD (Load_InvalidCpu_ReturnsError)
    {
        MachineConfig          config;
        std::string            error;
        std::vector<fs::path>  paths;



        std::string json = R"({
            "name": "Test",
            "cpu": "z80",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [],
            "systemRom": { "address": "0xD000", "file": "Apple2Plus.rom" },
            "internalDevices": [],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";


        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, config, error);

        AssertFailed (hr,
            L"Invalid CPU 'z80' should cause failure");
        Assert::IsTrue (error.find ("z80") != std::string::npos,
            L"Error should mention the invalid CPU type");
    }

    TEST_METHOD (Load_Cpu65C02_Accepted)
    {
        MachineConfig config;
        std::string   error;



        std::string json = R"({
            "name": "Test",
            "cpu": "65C02",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [],
            "systemRom": { "address": "0xD000", "file": "Apple2Plus.rom" },
            "internalDevices": [],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";


        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertSucceeded (hr,
            std::format (L"65C02 profile should load: {}",
                std::wstring (error.begin(), error.end())).c_str());
        Assert::AreEqual (std::string ("65C02"), config.cpu,
            L"CPU must be preserved as '65C02'");
    }

    TEST_METHOD (Load_BankedSystemRom_Parsed)
    {
        MachineConfig config;
        std::string   error;



        // Apple //c: a 32K ROM mapped as two 16K banks at $C000, toggled by
        // $C028. Each bank (not the whole file) must fit in 64K.
        std::string json = R"({
            "name": "Test //c",
            "cpu": "65C02",
            "timing": { "videoStandard": "ntsc", "clockSpeed": 1023000, "cyclesPerScanline": 65 },
            "ram": [],
            "systemRom": {
                "address": "0xC000",
                "file": "Apple2c.rom",
                "romBankSize": "0x4000",
                "romBankSelect": "0xC028"
            },
            "internalDevices": [],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";


        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertSucceeded (hr,
            std::format (L"Banked //c ROM should load: {}",
                std::wstring (error.begin(), error.end())).c_str());
        Assert::AreEqual (Word (0x4000), config.systemRom.romBankSize,
            L"romBankSize must parse to 0x4000");
        Assert::AreEqual (Word (0xC028), config.systemRom.romBankSelect,
            L"romBankSelect must parse to 0xC028");
        Assert::AreEqual (size_t (32768), config.systemRom.fileSize,
            L"32K file spans two 16K banks");
    }

    TEST_METHOD (Load_BankedSystemRom_RequiresSelect)
    {
        MachineConfig config;
        std::string   error;



        // romBankSize without romBankSelect is a config error.
        std::string json = R"({
            "name": "Test //c",
            "cpu": "65C02",
            "timing": { "videoStandard": "ntsc", "clockSpeed": 1023000, "cyclesPerScanline": 65 },
            "ram": [],
            "systemRom": { "address": "0xC000", "file": "Apple2c.rom", "romBankSize": "0x4000" },
            "internalDevices": [],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";


        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertFailed (hr,
            L"romBankSize without romBankSelect must fail");
        Assert::IsTrue (error.find ("romBankSelect") != std::string::npos,
            L"Error must identify the missing romBankSelect field");
    }

    TEST_METHOD (Load_AuxRamRegion_Preserved)
    {
        std::string   json = JsonWithAuxRam();
        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertSucceeded (hr,
            std::format (L"aux ram config should load: {}",
                std::wstring (error.begin(), error.end())).c_str());

        Assert::AreEqual (size_t (2), config.ram.size(),
            L"Should have 2 RAM regions");
        Assert::AreEqual (std::string ("aux"), config.ram[1].bank,
            L"Second RAM bank must be 'aux'");
    }

    TEST_METHOD (Load_InternalDevices_Parsed)
    {
        std::string   json = MinimalJson();
        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertSucceeded (hr);
        Assert::AreEqual (size_t (3), config.internalDevices.size(),
            L"Should have 3 internal devices");
        Assert::AreEqual (std::string ("apple2-keyboard"), config.internalDevices[0].type,
            L"First internal device should be apple2-keyboard");
    }

    TEST_METHOD (Load_Slot_RangeValidation)
    {
        MachineConfig config;
        std::string   error;



        std::string json = R"({
            "name": "Test",
            "cpu": "6502",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [],
            "systemRom": { "address": "0xD000", "file": "Apple2Plus.rom" },
            "internalDevices": [],
            "slots": [
                { "slot": 8, "device": "disk-ii" }
            ],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";


        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertFailed (hr,
            L"Slot 8 should fail validation (must be 1-7)");
        Assert::IsTrue (error.find ("slot must be") != std::string::npos,
            L"Error should mention slot range constraint");
    }

    TEST_METHOD (Load_Slot_MissingDeviceAndRom_Fails)
    {
        MachineConfig config;
        std::string   error;



        std::string json = R"({
            "name": "Test",
            "cpu": "6502",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [],
            "systemRom": { "address": "0xD000", "file": "Apple2Plus.rom" },
            "internalDevices": [],
            "slots": [
                { "slot": 6 }
            ],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";


        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertFailed (hr,
            L"Slot with neither device nor rom should fail");
    }

    TEST_METHOD (Load_KeyboardType_Parsed)
    {
        std::string   json = MinimalJson();
        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertSucceeded (hr);
        Assert::AreEqual (std::string ("apple2-uppercase"), config.keyboardType,
            L"Keyboard type should be 'apple2-uppercase'");
    }

    TEST_METHOD (Load_VideoConfig_Parsed)
    {
        std::string   json = MinimalJson();
        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertSucceeded (hr);
        Assert::AreEqual (size_t (3), config.videoConfig.modes.size(),
            L"Should have 3 video modes");
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  CollectRomFiles
    //
    //  CollectRomFiles is the pre-flight pass AssetBootstrap uses to
    //  decide which ROM files (if any) need downloading. Lock down
    //  exactly which references it picks up so the in-app downloader
    //  can never silently miss a ROM the loader will later require.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (CollectRomFiles_SystemRomOnly_ReturnsOneFile)
    {
        std::string              json = MinimalJson();
        std::vector<std::string> files;
        std::string              error;

        HRESULT hr = MachineConfigLoader::CollectRomFiles (json, files, error);

        AssertSucceeded (hr,
            std::format (L"CollectRomFiles should succeed: {}",
                std::wstring (error.begin(), error.end())).c_str());
        Assert::AreEqual (size_t (1), files.size(),
            L"Minimal config has only systemRom");
        Assert::AreEqual (std::string ("Apple2Plus.rom"), files[0],
            L"systemRom.file must be returned");
    }

    TEST_METHOD (CollectRomFiles_WithCharacterRom_ReturnsBoth)
    {
        std::string              json  = JsonWithCharRom();
        std::vector<std::string> files;
        std::string              error;

        HRESULT hr = MachineConfigLoader::CollectRomFiles (json, files, error);

        AssertSucceeded (hr);
        Assert::AreEqual (size_t (2), files.size(),
            L"systemRom + characterRom = 2 files");
        Assert::AreEqual (std::string ("Apple2.rom"),       files[0]);
        Assert::AreEqual (std::string ("Apple2_Video.rom"), files[1]);
    }

    TEST_METHOD (CollectRomFiles_WithSlotRoms_IncludesAll)
    {
        std::string              json  = JsonWithSlotRom();
        std::vector<std::string> files;
        std::string              error;

        HRESULT hr = MachineConfigLoader::CollectRomFiles (json, files, error);

        AssertSucceeded (hr);
        Assert::AreEqual (size_t (3), files.size(),
            L"system + character + 1 slot ROM = 3 files");
        Assert::AreEqual (std::string ("Apple2e.rom"),       files[0]);
        Assert::AreEqual (std::string ("Apple2e_Video.rom"), files[1]);
        Assert::AreEqual (std::string ("Disk2.rom"),         files[2]);
    }

    TEST_METHOD (CollectRomFiles_MalformedJson_ReturnsError)
    {
        std::vector<std::string>  files;
        std::string               error;
        HRESULT                   hr    = S_OK;



        std::string              json  = "{ not valid json";

        hr = MachineConfigLoader::CollectRomFiles (json, files, error);

        AssertFailed (hr,
            L"Malformed JSON must surface as an error");
        Assert::IsTrue (files.empty(),
            L"On parse failure no files should be reported");
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  007-ui-overhaul / FR-015 — capabilityFlag default inversion
    //
    //  Internal devices default to CapabilityFlag::Required; slot entries
    //  default to CapabilityFlag::Optional. The asymmetry is asserted in
    //  both directions in a single test method to guard against an
    //  accidental swap during refactor.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Load_CapabilityFlag_DefaultInversion)
    {
        MachineConfig config;
        std::string   error;



        std::string json = R"({
            "name": "Test",
            "cpu": "6502",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [],
            "systemRom": { "address": "0xD000", "file": "Apple2Plus.rom" },
            "internalDevices": [
                { "type": "apple2-keyboard" }
            ],
            "slots": [
                { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom" }
            ],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";


        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertSucceeded (hr,
            std::format (L"Load should succeed: {}",
                std::wstring (error.begin(), error.end())).c_str());

        Assert::AreEqual (size_t (1), config.internalDevices.size());
        Assert::IsTrue (config.internalDevices[0].capabilityFlag == CapabilityFlag::Required,
            L"Internal device without explicit capabilityFlag must default to Required.");

        Assert::AreEqual (size_t (1), config.slots.size());
        Assert::IsTrue (config.slots[0].capabilityFlag == CapabilityFlag::Optional,
            L"Slot entry without explicit capabilityFlag must default to Optional.");
    }

    TEST_METHOD (Load_CapabilityFlag_ExplicitPlatformLocked_Preserved)
    {
        MachineConfig config;
        std::string   error;



        std::string json = R"({
            "name": "Test",
            "cpu": "6502",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [],
            "systemRom": { "address": "0xD000", "file": "Apple2Plus.rom" },
            "internalDevices": [
                { "type": "apple2c-80col", "capabilityFlag": "platform-locked",
                  "lockReason": "Integrated on motherboard" }
            ],
            "slots": [],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";


        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertSucceeded (hr);
        Assert::IsTrue (config.internalDevices[0].capabilityFlag == CapabilityFlag::PlatformLocked,
            L"Explicit capabilityFlag must round-trip.");
        Assert::AreEqual (std::string ("Integrated on motherboard"),
                          config.internalDevices[0].lockReason);
    }

    TEST_METHOD (Load_CapabilityFlag_InvalidValue_Rejected)
    {
        MachineConfig config;
        std::string   error;



        std::string json = R"({
            "name": "Test",
            "cpu": "6502",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [],
            "systemRom": { "address": "0xD000", "file": "Apple2Plus.rom" },
            "internalDevices": [
                { "type": "apple2-keyboard", "capabilityFlag": "bogus" }
            ],
            "slots": [],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";


        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertFailed (hr,
            L"Unknown capabilityFlag value must surface as an error.");
        Assert::IsTrue (error.find ("capabilityFlag") != std::string::npos,
            L"Error message must mention the bad field.");
    }


    static SlotConfig MakeSlot (int slot, const std::string & device, bool enabled)
    {
        SlotConfig  s;

        s.slot    = slot;
        s.device  = device;
        s.enabled = enabled;
        return s;
    }


    TEST_METHOD (HasEnabledSlotDevice_MatchesEnabledOnly)
    {
        MachineConfig  config;

        config.slots.push_back (MakeSlot (1, "parallel-printer", true));
        config.slots.push_back (MakeSlot (6, "disk-ii",          true));

        Assert::IsTrue  (config.HasEnabledSlotDevice ("parallel-printer"), L"an enabled slot matches");
        Assert::IsTrue  (config.HasEnabledSlotDevice ("disk-ii"),          L"other enabled slots match too");
        Assert::IsFalse (config.HasEnabledSlotDevice ("mockingboard"),     L"an absent device does not match");
    }


    TEST_METHOD (HasEnabledSlotDevice_IgnoresDisabledSlot)
    {
        MachineConfig  config;

        // A slot the user disabled in Settings > Hardware must not count.
        config.slots.push_back (MakeSlot (1, "parallel-printer", false));

        Assert::IsFalse (config.HasEnabledSlotDevice ("parallel-printer"),
                         L"a disabled slot does not count as connected");
    }


    TEST_METHOD (HasEnabledSlotDevice_EmptyConfigHasNone)
    {
        MachineConfig  config;   // slotless (like the //c)

        Assert::IsFalse (config.HasEnabledSlotDevice ("parallel-printer"),
                         L"a slotless machine has no printer");
    }


    //
    //  The rule the whole feature rests on: a card that declares NO ports has
    //  not been described yet, not emptied. Reading an absent list as zero
    //  would take the drives away from every config written before the key
    //  existed -- which is all of them.
    //
    TEST_METHOD (AttachedDiskIiDriveCount_UndeclaredPortsMeanTheRealCardsTwo)
    {
        MachineConfig  config;
        SlotConfig     slot;

        slot.slot   = 6;
        slot.device = "disk-ii";
        config.slots.push_back (slot);

        Assert::AreEqual (2, config.AttachedDiskIiDriveCount(),
            L"A Disk ][ with no ports declared has the two drives it "
            L"has always behaved as having.");
    }


    TEST_METHOD (AttachedDiskIiDriveCount_CountsOnlyOccupiedConnectors)
    {
        MachineConfig  config;
        SlotConfig     slot;

        slot.slot   = 6;
        slot.device = "disk-ii";
        slot.ports.push_back ({ "", "disk-ii-drive" });
        slot.ports.push_back ({ "", "" });               // connector, no drive
        config.slots.push_back (slot);

        Assert::AreEqual (1, config.AttachedDiskIiDriveCount(),
            L"The card still has two connectors; only one has a drive.");
    }


    TEST_METHOD (AttachedDiskIiDriveCount_ADetachedPairReportsZero)
    {
        MachineConfig  config;
        SlotConfig     slot;

        slot.slot   = 6;
        slot.device = "disk-ii";
        slot.ports.push_back ({ "", "" });
        slot.ports.push_back ({ "", "" });
        config.slots.push_back (slot);

        Assert::AreEqual (0, config.AttachedDiskIiDriveCount(),
            L"A card with both connectors empty has no drives -- which is "
            L"the point of being able to detach one at all.");
    }


    TEST_METHOD (AttachedDiskIiDriveCount_IgnoresADisabledCard)
    {
        MachineConfig  config;
        SlotConfig     slot;

        slot.slot    = 6;
        slot.device  = "disk-ii";
        slot.enabled = false;
        config.slots.push_back (slot);

        Assert::AreEqual (0, config.AttachedDiskIiDriveCount(),
            L"A card the user turned off in Settings > Hardware is not "
            L"present, so neither are its drives.");
    }


    TEST_METHOD (AttachedDiskIiDriveCount_SlotlessMachineHasNoCardedDrives)
    {
        MachineConfig  config;   // the //c: built-in drive, no slots

        Assert::AreEqual (0, config.AttachedDiskIiDriveCount(),
            L"The //c's drives are not on a card, so this query reports "
            L"nothing about them.");
    }


    TEST_METHOD (FindPort_NamesTheBackPanelConnectorOrNothing)
    {
        MachineConfig  config;

        config.ports.push_back ({ "disk",     "" });
        config.ports.push_back ({ "joystick", "apple2-joystick" });

        Assert::IsTrue (config.FindPort ("disk") != nullptr,
            L"A declared connector must be findable even when unoccupied.");
        Assert::IsTrue (config.FindPort ("disk")->device.empty());
        Assert::AreEqual (std::string ("apple2-joystick"),
                          config.FindPort ("joystick")->device);
        Assert::IsTrue (config.FindPort ("serial1") == nullptr,
            L"A connector the machine does not have is absent, which is a "
            L"different result from present-and-empty.");
    }


    //
    //  A card's ports are numbered rather than named, so they are written as
    //  bare strings and read positionally. An empty string is a connector
    //  with nothing on it, which is a different state from the connector not
    //  existing -- the whole reason this is a list and not a drive count.
    //
    TEST_METHOD (Load_SlotPorts_BareStringsReadPositionally)
    {
        MachineConfig config;
        std::string   error;



        std::string json = R"({
            "name": "Test",
            "cpu": "6502",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [],
            "systemRom": { "address": "0xD000", "file": "Apple2Plus.rom" },
            "internalDevices": [],
            "slots": [
                { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom",
                  "ports": ["disk-ii-drive", ""] }
            ],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";


        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertSucceeded (hr,
            std::format (L"Load should succeed: {}",
                std::wstring (error.begin(), error.end())).c_str());

        Assert::AreEqual (size_t (1), config.slots.size());
        Assert::AreEqual (size_t (2), config.slots[0].ports.size(),
            L"The card declares two connectors whether or not both are used.");
        Assert::AreEqual (std::string ("disk-ii-drive"), config.slots[0].ports[0].device);
        Assert::IsTrue (config.slots[0].ports[1].device.empty(),
            L"An empty string is a present-but-unoccupied connector.");
        Assert::IsTrue (config.slots[0].ports[0].name.empty(),
            L"A card's connectors are numbered, not named.");
    }


    //
    //  A missing `ports` key must leave the vector EMPTY, which callers read
    //  as "this machine's default" -- never as "no connectors". Every config
    //  written before the key existed depends on that reading.
    //
    TEST_METHOD (Load_SlotPorts_AbsentKeyLeavesPortsEmpty)
    {
        MachineConfig config;
        std::string   error;



        std::string json = R"({
            "name": "Test",
            "cpu": "6502",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [],
            "systemRom": { "address": "0xD000", "file": "Apple2Plus.rom" },
            "internalDevices": [],
            "slots": [
                { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom" }
            ],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";


        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertSucceeded (hr);
        Assert::AreEqual (size_t (1), config.slots.size());
        Assert::IsTrue (config.slots[0].ports.empty(),
            L"No ports key means the machine's default, not a portless card.");
    }


    //
    //  A slotless machine still has connectors. The //c's hardware is built
    //  in, so its back panel belongs to the machine rather than to a card,
    //  and those connectors are NAMED because a disk port and a joystick port
    //  are not interchangeable.
    //
    TEST_METHOD (Load_MachinePorts_NamedBackPanelOnASlotlessMachine)
    {
        MachineConfig config;
        std::string   error;



        std::string json = R"({
            "name": "Test //c",
            "cpu": "65C02",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [],
            "systemRom": { "address": "0xD000", "file": "Apple2Plus.rom" },
            "internalDevices": [],
            "ports": [
                { "name": "disk",     "device": "disk-iic-drive" },
                { "name": "joystick", "device": "" }
            ],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";


        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertSucceeded (hr,
            std::format (L"Load should succeed: {}",
                std::wstring (error.begin(), error.end())).c_str());

        Assert::IsTrue (config.slots.empty(), L"The //c has no slots at all.");
        Assert::AreEqual (size_t (2), config.ports.size());
        Assert::AreEqual (std::string ("disk"),           config.ports[0].name);
        Assert::AreEqual (std::string ("disk-iic-drive"), config.ports[0].device);
        Assert::AreEqual (std::string ("joystick"),       config.ports[1].name);
        Assert::IsTrue (config.ports[1].device.empty(),
            L"A named connector with nothing on it is a real state.");
    }


    //
    //  A junk port entry becomes an unoccupied connector rather than failing
    //  the load. Refusing to start a machine over a malformed port would cost
    //  the user their whole config to punish a field with a harmless reading.
    //
    TEST_METHOD (Load_MachinePorts_MalformedEntryIsAnEmptyPortNotAnError)
    {
        MachineConfig config;
        std::string   error;



        std::string json = R"({
            "name": "Test",
            "cpu": "6502",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [],
            "systemRom": { "address": "0xD000", "file": "Apple2Plus.rom" },
            "internalDevices": [],
            "ports": [null, 47, "disk-ii-drive"],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";


        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, "TestMachine", paths, MockResolveAll,
                                                config, error);

        AssertSucceeded (hr, L"A junk port must not fail the whole load.");
        Assert::AreEqual (size_t (3), config.ports.size(),
            L"Every entry still counts as a connector.");
        Assert::IsTrue (config.ports[0].device.empty());
        Assert::IsTrue (config.ports[1].device.empty());
        Assert::AreEqual (std::string ("disk-ii-drive"), config.ports[2].device);
    }

private:

    // Mock resolver that creates a temporary file of the expected size for the
    // requested ROM. This works in CI where actual ROM files are not present.
    static fs::path MockResolveAll (
        const std::vector<fs::path> & searchPaths,
        const fs::path              & relativePath)
    {
        std::string  filename;
        size_t       expectedSize = 0;
        bool         needCreate   = false;



        UNREFERENCED_PARAMETER (searchPaths);

        // Determine expected size from filename
        filename = relativePath.filename().string();

        if (filename == "Apple2Plus.rom" || filename == "Apple2.rom")
        {
            expectedSize = 12288;
        }
        else if (filename == "Apple2e.rom" || filename == "Apple2eEnhanced.rom")
        {
            expectedSize = 16384;
        }
        else if (filename == "Disk2.rom")
        {
            expectedSize = 256;
        }
        else if (filename == "Apple2_Video.rom")
        {
            expectedSize = 2048;
        }
        else if (filename == "Apple2e_Video.rom" || filename == "Apple2c_Video.rom")
        {
            expectedSize = 4096;
        }
        else if (filename == "Apple2c.rom")
        {
            expectedSize = 32768;   // //c ROM 4: two 16K banks
        }
        else
        {
            expectedSize = 256;  // default
        }

        // Create temp file with the expected size if not already present
        fs::path tempPath = fs::temp_directory_path() / ("casso_test_" + filename);

        needCreate = !fs::exists (tempPath);

        if (!needCreate)
        {
            try
            {
                needCreate = fs::file_size (tempPath) != expectedSize;
            }
            catch (...)
            {
                needCreate = true;
            }
        }

        if (needCreate)
        {
            std::vector<Byte> buffer (expectedSize, 0);
            std::ofstream     out (tempPath, std::ios::binary | std::ios::trunc);
            out.write (reinterpret_cast<const char *> (buffer.data()), expectedSize);
            out.flush();
            out.close();
        }

        return tempPath;
    }

    static fs::path MockResolveNone (
        const std::vector<fs::path> &,
        const fs::path              &)
    {
        return {};
    }

    static std::string MinimalJson()
    {
        return R"({
            "name": "TestMachine",
            "cpu": "6502",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [
                { "address": "0x0000", "size": "0xC000" }
            ],
            "systemRom": { "address": "0xD000", "file": "Apple2Plus.rom" },
            "internalDevices": [
                { "type": "apple2-keyboard" },
                { "type": "apple2-speaker" },
                { "type": "apple2-softswitches" }
            ],
            "video": { "modes": ["apple2-text40", "apple2-lores", "apple2-hires"] },
            "keyboard": { "type": "apple2-uppercase" }
        })";
    }

    static std::string JsonWithAuxRam()
    {
        return R"({
            "name": "TestIIe",
            "cpu": "6502",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [
                { "address": "0x0000", "size": "0xC000" },
                { "address": "0x0000", "size": "0xC000", "bank": "aux" }
            ],
            "systemRom": { "address": "0xC000", "file": "Apple2e.rom" },
            "internalDevices": [],
            "video": { "modes": [] },
            "keyboard": { "type": "apple2e-full" }
        })";
    }

    static std::string JsonWithCharRom()
    {
        return R"({
            "name": "TestII",
            "cpu": "6502",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [ { "address": "0x0000", "size": "0xC000" } ],
            "systemRom":    { "address": "0xD000", "file": "Apple2.rom" },
            "characterRom": {                      "file": "Apple2_Video.rom" },
            "internalDevices": [],
            "video": { "modes": [] },
            "keyboard": { "type": "apple2-uppercase" }
        })";
    }

    static std::string JsonWithSlotRom()
    {
        return R"({
            "name": "TestIIeWithDisk",
            "cpu": "6502",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [ { "address": "0x0000", "size": "0xC000" } ],
            "systemRom":    { "address": "0xC000", "file": "Apple2e.rom" },
            "characterRom": {                      "file": "Apple2e_Video.rom" },
            "internalDevices": [],
            "slots": [
                { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom" }
            ],
            "video": { "modes": [] },
            "keyboard": { "type": "apple2e-full" }
        })";
    }
};
