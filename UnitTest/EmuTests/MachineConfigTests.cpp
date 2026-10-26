#include "Pch.h"
#include "Core/MachineConfig.h"
#include "Machines/MachineDefinitions.h"
#include "HResultAssert.h"
#include "Machines/IMachine.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigTests
//
//  Validates the v2 machine config schema:
//    ram[], systemRom, characterRom, internalDevices[], slots[]
//
//  The loads that resolve a ROM write it to the system temp folder, so they
//  are in the scenario suite's MachineConfigTests.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineConfigTests)
{
public:

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
                TextEncoding::NarrowToWide (error)).c_str());
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


    ////////////////////////////////////////////////////////////////////////
    //
    //  Super Serial Card slot keys: `firmware`, `switches` and a port `path`.
    //
    //  The firmware is optional in a way `rom` is not: a missing or
    //  wrong-size file must load, with the reason in the slot. The
    //  present case resolves to a fixture that happens to be 2 KB (the ][
    //  character ROM), so no test writes a file.
    //
    ////////////////////////////////////////////////////////////////////////

    static std::string MakeSscJson()
    {
        return R"({
            "name": "Test",
            "cpu": "6502",
            "timing": { "videoStandard": "ntsc", "clockSpeed": 1023000, "cyclesPerScanline": 65 },
            "ram": [],
            "systemRom": { "address": "0xD000", "file": "Apple2Plus.rom" },
            "internalDevices": [],
            "slots": [
                { "slot": 2, "device": "super-serial-card", "firmware": "SSC.rom", "enabled": false,
                  "switches": { "sw1": "on on on on off on on", "sw2": "off off off off off on off" },
                  "ports": [ { "device": "serial-file-out", "path": "C:\\serial.txt" } ] }
            ],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";
    }

    TEST_METHOD (CollectRomFiles_DoesNotListSlotFirmware)
    {
        std::vector<std::string>  files;
        std::string               error;
        HRESULT                   hr = MachineConfigLoader::CollectRomFiles (MakeSscJson(), files, error);

        AssertSucceeded (hr);
        Assert::IsTrue (std::find (files.begin(), files.end(), "SSC.rom") == files.end(),
            L"optional firmware is never listed by the startup download dialog");
        Assert::IsTrue (std::find (files.begin(), files.end(), "Apple2Plus.rom") != files.end(),
            L"the scan itself ran");
    }
private:

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
                { "type": "apple2-family-keyboard" },
                { "type": "apple2-family-speaker" },
                { "type": "apple2-family-softswitches" }
            ],
            "video": { "modes": ["apple2-text40", "apple2-lores", "apple2-hires"] },
            "keyboard": { "type": "apple2-family-layout" }
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
            "keyboard": { "type": "apple2-family-layout" }
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
            "keyboard": { "type": "apple2e-family-layout" }
        })";
    }
};
