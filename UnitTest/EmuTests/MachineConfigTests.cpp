#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

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
        std::string   json = MinimalJson ();
        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, paths, MockResolveAll,
                                                config, error);

        Assert::IsTrue (SUCCEEDED (hr),
            std::format (L"Load should succeed: {}",
                std::wstring (error.begin (), error.end ())).c_str ());
        Assert::AreEqual (std::string ("TestMachine"), config.name,
            L"Name must be 'TestMachine'");
        Assert::AreEqual (std::string ("6502"), config.cpu,
            L"CPU must be '6502'");
        Assert::AreEqual (1023000u, config.clockSpeed,
            L"Clock speed must be 1023000");
        Assert::AreEqual (size_t (1), config.ram.size (),
            L"Should have 1 RAM region");
        Assert::AreEqual (Word (0x0000), config.ram[0].address,
            L"First RAM address must be 0x0000");
        Assert::AreEqual (Word (0xC000), config.ram[0].size,
            L"First RAM size must be 0xC000");
    }

    TEST_METHOD (Load_SystemRomResolved)
    {
        std::string   json = MinimalJson ();
        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, paths, MockResolveAll,
                                                config, error);

        Assert::IsTrue (SUCCEEDED (hr),
            L"Load should succeed with valid ROM");

        Assert::IsFalse (config.systemRom.resolvedPath.empty (),
            L"systemRom resolvedPath must be populated");
        Assert::AreEqual (Word (0xD000), config.systemRom.address,
            L"systemRom address must be 0xD000");
    }

    TEST_METHOD (Load_MissingRom_ReturnsClearError)
    {
        std::string   json = MinimalJson ();
        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, paths, MockResolveNone,
                                                config, error);

        Assert::IsTrue (FAILED (hr),
            L"Load should fail when ROM not found");
        Assert::IsTrue (error.find ("ROM file not found") != std::string::npos,
            L"Error message must mention 'ROM file not found'");
    }

    TEST_METHOD (Load_MissingName_ReturnsError)
    {
        std::string json = R"({ "cpu": "6502" })";
        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths;
        HRESULT hr = MachineConfigLoader::Load (json, paths, config, error);

        Assert::IsTrue (FAILED (hr),
            L"Missing 'name' field should cause failure");
        Assert::IsTrue (error.find ("name") != std::string::npos,
            L"Error should mention 'name'");
    }

    TEST_METHOD (Load_InvalidCpu_ReturnsError)
    {
        std::string json = R"({
            "name": "Test",
            "cpu": "z80",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [],
            "systemRom": { "address": "0xD000", "file": "apple2plus.rom" },
            "internalDevices": [],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";

        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths;
        HRESULT hr = MachineConfigLoader::Load (json, paths, config, error);

        Assert::IsTrue (FAILED (hr),
            L"Invalid CPU 'z80' should cause failure");
        Assert::IsTrue (error.find ("z80") != std::string::npos,
            L"Error should mention the invalid CPU type");
    }

    TEST_METHOD (Load_AuxRamRegion_Preserved)
    {
        std::string   json = JsonWithAuxRam ();
        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, paths, MockResolveAll,
                                                config, error);

        Assert::IsTrue (SUCCEEDED (hr),
            std::format (L"aux ram config should load: {}",
                std::wstring (error.begin (), error.end ())).c_str ());

        Assert::AreEqual (size_t (2), config.ram.size (),
            L"Should have 2 RAM regions");
        Assert::AreEqual (std::string ("aux"), config.ram[1].bank,
            L"Second RAM bank must be 'aux'");
    }

    TEST_METHOD (Load_InternalDevices_Parsed)
    {
        std::string   json = MinimalJson ();
        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, paths, MockResolveAll,
                                                config, error);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (size_t (3), config.internalDevices.size (),
            L"Should have 3 internal devices");
        Assert::AreEqual (std::string ("apple2-keyboard"), config.internalDevices[0].type,
            L"First internal device should be apple2-keyboard");
    }

    TEST_METHOD (Load_Slot_RangeValidation)
    {
        std::string json = R"({
            "name": "Test",
            "cpu": "6502",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [],
            "systemRom": { "address": "0xD000", "file": "apple2plus.rom" },
            "internalDevices": [],
            "slots": [
                { "slot": 8, "device": "disk-ii" }
            ],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";

        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, paths, MockResolveAll,
                                                config, error);

        Assert::IsTrue (FAILED (hr),
            L"Slot 8 should fail validation (must be 1-7)");
        Assert::IsTrue (error.find ("slot must be") != std::string::npos,
            L"Error should mention slot range constraint");
    }

    TEST_METHOD (Load_Slot_MissingDeviceAndRom_Fails)
    {
        std::string json = R"({
            "name": "Test",
            "cpu": "6502",
            "timing": {
                "videoStandard": "ntsc",
                "clockSpeed": 1023000,
                "cyclesPerScanline": 65
            },
            "ram": [],
            "systemRom": { "address": "0xD000", "file": "apple2plus.rom" },
            "internalDevices": [],
            "slots": [
                { "slot": 6 }
            ],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";

        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, paths, MockResolveAll,
                                                config, error);

        Assert::IsTrue (FAILED (hr),
            L"Slot with neither device nor rom should fail");
    }

    TEST_METHOD (Load_KeyboardType_Parsed)
    {
        std::string   json = MinimalJson ();
        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, paths, MockResolveAll,
                                                config, error);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (std::string ("apple2-uppercase"), config.keyboardType,
            L"Keyboard type should be 'apple2-uppercase'");
    }

    TEST_METHOD (Load_VideoConfig_Parsed)
    {
        std::string   json = MinimalJson ();
        MachineConfig config;
        std::string   error;

        std::vector<fs::path> paths = { "/mock" };
        HRESULT hr = MachineConfigLoader::Load (json, paths, MockResolveAll,
                                                config, error);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (size_t (3), config.videoConfig.modes.size (),
            L"Should have 3 video modes");
    }

private:

    // Mock resolver that creates a temporary file of the expected size for the
    // requested ROM. This works in CI where actual ROM files are not present.
    static fs::path MockResolveAll (
        const std::vector<fs::path> & searchPaths,
        const fs::path              & relativePath)
    {
        UNREFERENCED_PARAMETER (searchPaths);

        // Determine expected size from filename
        std::string filename = relativePath.filename ().string ();
        size_t      expectedSize = 0;

        if (filename == "apple2plus.rom" || filename == "apple2.rom")
        {
            expectedSize = 12288;
        }
        else if (filename == "apple2e.rom" || filename == "apple2e-enhanced.rom")
        {
            expectedSize = 16384;
        }
        else if (filename == "disk2.rom")
        {
            expectedSize = 256;
        }
        else if (filename == "apple2-video.rom")
        {
            expectedSize = 2048;
        }
        else if (filename == "apple2e-enhanced-video.rom")
        {
            expectedSize = 4096;
        }
        else
        {
            expectedSize = 256;  // default
        }

        // Create temp file with the expected size
        fs::path tempPath = fs::temp_directory_path () / ("casso_test_" + filename);

        if (!fs::exists (tempPath) || fs::file_size (tempPath) != expectedSize)
        {
            std::vector<Byte>  buffer (expectedSize, 0);
            std::ofstream      out (tempPath, std::ios::binary);
            out.write (reinterpret_cast<const char *> (buffer.data ()), expectedSize);
        }

        return tempPath;
    }

    static fs::path MockResolveNone (
        const std::vector<fs::path> &,
        const fs::path              &)
    {
        return {};
    }

    static std::string MinimalJson ()
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
            "systemRom": { "address": "0xD000", "file": "apple2plus.rom" },
            "internalDevices": [
                { "type": "apple2-keyboard" },
                { "type": "apple2-speaker" },
                { "type": "apple2-softswitches" }
            ],
            "video": { "modes": ["apple2-text40", "apple2-lores", "apple2-hires"] },
            "keyboard": { "type": "apple2-uppercase" }
        })";
    }

    static std::string JsonWithAuxRam ()
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
            "systemRom": { "address": "0xC000", "file": "apple2e.rom" },
            "internalDevices": [],
            "video": { "modes": [] },
            "keyboard": { "type": "apple2e-full" }
        })";
    }
};
