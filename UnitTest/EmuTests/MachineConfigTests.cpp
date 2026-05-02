#include "../Casso65EmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MachineConfig.h"
#include "Core/PathResolver.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineConfigTests
//
//  Adversarial tests proving config loading catches real problems:
//  missing fields, invalid CPUs, missing ROMs, and correct device mapping.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineConfigTests)
{
public:

    TEST_METHOD (Load_ValidJson_ParsesAllFields)
    {
        std::string json = MinimalJson ();
        MachineConfig config;
        std::string error;

        std::vector<fs::path> paths;
        HRESULT hr = MachineConfigLoader::Load (json, paths, config, error);

        Assert::IsTrue (SUCCEEDED (hr),
            L"Load should succeed for valid minimal JSON");
        Assert::AreEqual (std::string ("TestMachine"), config.name,
            L"Name must be 'TestMachine'");
        Assert::AreEqual (std::string ("6502"), config.cpu,
            L"CPU must be '6502'");
        Assert::AreEqual (1023000u, config.clockSpeed,
            L"Clock speed must be 1023000");
        Assert::AreEqual (size_t (1), config.memoryRegions.size (),
            L"Should have 1 memory region");
        Assert::AreEqual (std::string ("ram"), config.memoryRegions[0].type,
            L"First region type must be 'ram'");
    }

    TEST_METHOD (Load_RomResolvedPath_Populated)
    {
        std::string repoRoot = FindRepoRoot ();

        if (repoRoot.empty ())
        {
            return;
        }

        std::string json = JsonWithRom ();
        MachineConfig config;
        std::string error;

        std::vector<fs::path> paths = { repoRoot };
        HRESULT hr = MachineConfigLoader::Load (json, paths, config, error);

        Assert::IsTrue (SUCCEEDED (hr),
            L"Load should succeed with valid ROM");

        bool foundRom = false;

        for (const auto & region : config.memoryRegions)
        {
            if (region.type == "rom" && !region.file.empty ())
            {
                Assert::IsFalse (region.resolvedPath.empty (),
                    L"resolvedPath must be populated for ROM regions");
                foundRom = true;
            }
        }

        Assert::IsTrue (foundRom,
            L"Should have at least one ROM region");
    }

    TEST_METHOD (Load_MissingRom_ReturnsClearError)
    {
        std::string json = JsonWithRom ();
        MachineConfig config;
        std::string error;

        std::vector<fs::path> paths = { "C:/nonexistent" };
        HRESULT hr = MachineConfigLoader::Load (json, paths, config, error);

        Assert::IsTrue (FAILED (hr),
            L"Load should fail when ROM not found");
        Assert::IsTrue (error.find ("ROM file not found") != std::string::npos,
            L"Error message must mention 'ROM file not found'");
    }

    TEST_METHOD (Load_MissingName_ReturnsError)
    {
        std::string json = R"({ "cpu": "6502", "clockSpeed": 1023000 })";
        MachineConfig config;
        std::string error;

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
            "clockSpeed": 1023000,
            "memory": [],
            "devices": [],
            "video": { "modes": [] },
            "keyboard": { "type": "test" }
        })";

        MachineConfig config;
        std::string error;

        std::vector<fs::path> paths;
        HRESULT hr = MachineConfigLoader::Load (json, paths, config, error);

        Assert::IsTrue (FAILED (hr),
            L"Invalid CPU 'z80' should cause failure");
        Assert::IsTrue (error.find ("z80") != std::string::npos,
            L"Error should mention the invalid CPU type");
    }

    TEST_METHOD (Load_RealApple2PlusJson_ResolvesAllRegions)
    {
        std::string repoRoot = FindRepoRoot ();

        if (repoRoot.empty ())
        {
            return;
        }

        // Load the actual apple2plus.json config
        std::string configPath = (fs::path (repoRoot) / "machines" / "apple2plus.json").string ();
        std::ifstream file (configPath);

        if (!file.good ())
        {
            return;
        }

        std::string json ((std::istreambuf_iterator<char> (file)),
            std::istreambuf_iterator<char> ());

        MachineConfig config;
        std::string error;
        std::vector<fs::path> paths = { repoRoot };

        HRESULT hr = MachineConfigLoader::Load (json, paths, config, error);

        Assert::IsTrue (SUCCEEDED (hr),
            std::format (L"apple2plus.json should load: {}",
                std::wstring (error.begin (), error.end ())).c_str ());

        Assert::AreEqual (std::string ("6502"), config.cpu,
            L"Apple II+ must use 6502 CPU");

        // Verify all ROM regions have resolved paths
        for (const auto & region : config.memoryRegions)
        {
            if (region.type == "rom" && !region.file.empty ())
            {
                Assert::IsFalse (region.resolvedPath.empty (),
                    std::format (L"ROM '{}' must have resolvedPath",
                        std::wstring (region.file.begin (), region.file.end ())).c_str ());
            }
        }
    }

    TEST_METHOD (Load_KeyboardType_Parsed)
    {
        std::string json = MinimalJson ();
        MachineConfig config;
        std::string error;

        std::vector<fs::path> paths;
        HRESULT hr = MachineConfigLoader::Load (json, paths, config, error);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (std::string ("apple2-uppercase"), config.keyboardType,
            L"Keyboard type should be 'apple2-uppercase'");
    }

    TEST_METHOD (Load_VideoConfig_Parsed)
    {
        std::string json = MinimalJson ();
        MachineConfig config;
        std::string error;

        std::vector<fs::path> paths;
        HRESULT hr = MachineConfigLoader::Load (json, paths, config, error);

        Assert::IsTrue (SUCCEEDED (hr));
        Assert::AreEqual (560, config.videoConfig.width,
            L"Video width should be 560");
        Assert::AreEqual (384, config.videoConfig.height,
            L"Video height should be 384");
    }

private:

    static std::string MinimalJson ()
    {
        return R"({
            "name": "TestMachine",
            "cpu": "6502",
            "clockSpeed": 1023000,
            "memory": [
                { "type": "ram", "start": "0x0000", "end": "0xBFFF" }
            ],
            "devices": [],
            "video": { "modes": ["apple2-text40"], "width": 560, "height": 384 },
            "keyboard": { "type": "apple2-uppercase" }
        })";
    }

    static std::string JsonWithRom ()
    {
        return R"({
            "name": "TestWithRom",
            "cpu": "6502",
            "clockSpeed": 1023000,
            "memory": [
                { "type": "ram", "start": "0x0000", "end": "0xBFFF" },
                { "type": "rom", "start": "0xD000", "end": "0xFFFF", "file": "apple2plus.rom" }
            ],
            "devices": [],
            "video": { "modes": ["apple2-text40"], "width": 560, "height": 384 },
            "keyboard": { "type": "apple2-uppercase" }
        })";
    }

    static std::string FindRepoRoot ()
    {
        auto paths = PathResolver::BuildSearchPaths (
            PathResolver::GetExecutableDirectory (),
            PathResolver::GetWorkingDirectory ());

        fs::path found = PathResolver::FindFile (paths, "machines/apple2plus.json");

        if (found.empty ())
        {
            return "";
        }

        // Return the repo root (two levels up from machines/apple2plus.json)
        return found.parent_path ().parent_path ().string ();
    }
};
