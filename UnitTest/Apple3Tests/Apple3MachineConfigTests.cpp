#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Core/TextEncoding.h"
#include "EmuTests/EmbeddedMachineJson.h"
#include "HResultAssert.h"
#include "Machines/MachineDefinition.h"
#include "Machines/MachineDefinitions.h"
#include "resource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3MachineConfigTests
//
//  The Apple /// machine definition and file: the shipped file loads, and a
//  `characterRom` key, a `memory` value the machine doesn't offer, an
//  `externalDrives` count out of range and a card in a slot each fail with
//  their message.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3MachineConfigTests)
{
public:

    static constexpr size_t  kRomBytes = 4096;


    //  A resolver that makes a 4K file for any ROM.
    static fs::path ResolveRom (const std::vector<fs::path> & searchPaths, const fs::path & relativePath)
    {
        fs::path       path = fs::temp_directory_path() / "CassoApple3ConfigTests" / relativePath.filename();
        std::ofstream  out;



        UNREFERENCED_PARAMETER (searchPaths);

        fs::create_directories (path.parent_path());

        if (!fs::exists (path) || fs::file_size (path) != kRomBytes)
        {
            out.open (path, std::ios::binary | std::ios::trunc);
            out << std::string (kRomBytes, '\0');
        }

        return path;
    }


    //  The shipped file with one text replaced.
    static std::string GetShippedJson (const std::string & from = {}, const std::string & to = {})
    {
        std::string  json = EmbeddedMachineJson::Load (IDR_MACHINE_APPLE3);
        size_t       at   = from.empty() ? std::string::npos : json.find (from);



        if (at != std::string::npos)
        {
            json.replace (at, from.size(), to);
        }

        return json;
    }


    static HRESULT Load (const std::string & json, MachineConfig & config, std::string & error)
    {
        return MachineConfigLoader::Load (json, "Apple3", { fs::path (".") }, ResolveRom, config, error);
    }


    TEST_METHOD (Definition_IsRegistered)
    {
        const MachineDefinition  * definition = MachineDefinitions::Find ("Apple3");



        Assert::IsNotNull (definition);
        Assert::IsTrue    (definition->hasBankedMemory);
        Assert::AreEqual  (1, definition->builtInDriveCount);
        Assert::AreEqual  (3, definition->maxExternalDrives);
        Assert::AreEqual  (4, definition->slotCount);
        Assert::AreEqual  ((size_t) 2, definition->memoryOptions.size());
    }


    TEST_METHOD (ShippedFile_Loads)
    {
        MachineConfig  config;
        std::string    error;
        HRESULT        hr = Load (GetShippedJson(), config, error);



        AssertSucceeded (hr, TextEncoding::NarrowToWide (error).c_str());
        Assert::AreEqual (std::string ("Apple ///"), config.name);
        Assert::AreEqual ((uint32_t) 128,     config.memoryK);
        Assert::AreEqual (1,                  config.externalDrives);
        Assert::AreEqual ((uint32_t) 2040968, config.clockSpeed);
        Assert::AreEqual ((uint32_t) 130,     config.cyclesPerScanline);
        Assert::AreEqual ((Word) 0xF000,      config.systemRom.address);
        Assert::IsTrue   (config.ram.empty());
    }


    TEST_METHOD (Memory256K_Loads)
    {
        MachineConfig  config;
        std::string    error;
        HRESULT        hr = Load (GetShippedJson ("\"memory\": \"128K\"", "\"memory\": \"256K\""), config, error);



        AssertSucceeded (hr);
        Assert::AreEqual ((uint32_t) 256, config.memoryK);
    }


    TEST_METHOD (CharacterRom_IsAnError)
    {
        MachineConfig  config;
        std::string    error;
        HRESULT        hr = Load (GetShippedJson ("\"memory\":", "\"characterRom\": { \"file\": \"x.rom\" }, \"memory\":"), config, error);



        Assert::IsTrue   (FAILED (hr));
        Assert::AreEqual (std::string ("Invalid field: 'characterRom' (the Apple /// has its character set in RAM)"), error);
    }


    TEST_METHOD (BadMemory_IsAnError_ListingTheOptions)
    {
        MachineConfig  config;
        std::string    error;
        HRESULT        hr = Load (GetShippedJson ("\"memory\": \"128K\"", "\"memory\": \"64K\""), config, error);



        Assert::IsTrue   (FAILED (hr));
        Assert::AreEqual (std::string ("Invalid memory: '64K' (the options are 128K or 256K)"), error);
    }


    TEST_METHOD (BadExternalDrives_IsAnError)
    {
        MachineConfig  config;
        std::string    error;
        HRESULT        hr = Load (GetShippedJson ("\"externalDrives\": 1", "\"externalDrives\": 4"), config, error);



        Assert::IsTrue   (FAILED (hr));
        Assert::AreEqual (std::string ("Invalid externalDrives: 4 (the range is 0 to 3)"), error);
    }


    TEST_METHOD (CardInASlot_IsAnError)
    {
        MachineConfig  config;
        std::string    error;
        HRESULT        hr = Load (GetShippedJson ("\"ports\":", "\"slots\": [ { \"slot\": 2, \"device\": \"mockingboard\" } ], \"ports\":"), config, error);



        Assert::IsTrue   (FAILED (hr));
        Assert::AreEqual (std::string ("Unsupported device in slot 2: 'mockingboard' (the slot can only be empty)"), error);
    }
};
