#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Core/TextEncoding.h"
#include "EmuTests/EmbeddedMachineJson.h"
#include "EmuTests/TestMachine.h"
#include "HResultAssert.h"
#include "Machines/Apple3/Apple3Mmu.h"
#include "Machines/Apple3/Apple3System.h"
#include "Machines/MachineDefinition.h"
#include "Machines/MachineDefinitions.h"
#include "resource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3PlusTests
//
//  The Apple /// Plus machine: registered beside the Apple ///, with 256K as
//  its only memory option, a shipped file that loads at 256K and an error
//  for 128K, and a machine that builds with 256K and starts in the ROM.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3PlusTests)
{
public:

    static constexpr size_t  kRomBytes = 4096;


    //  A resolver that makes a 4K file for any ROM.
    static fs::path ResolveRom (const std::vector<fs::path> & searchPaths, const fs::path & relativePath)
    {
        fs::path       path = fs::temp_directory_path() / "CassoApple3PlusTests" / relativePath.filename();
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


    static HRESULT Load (const std::string & json, MachineConfig & config, std::string & error)
    {
        return MachineConfigLoader::Load (json, "Apple3Plus", { fs::path (".") }, ResolveRom, config, error);
    }


    TEST_METHOD (Definition_IsRegistered_WithOnly256K)
    {
        const MachineDefinition  * plus   = MachineDefinitions::Find ("Apple3Plus");
        const MachineDefinition  * apple3 = MachineDefinitions::Find ("Apple3");



        Assert::IsNotNull (plus);
        Assert::IsTrue    (plus->hasBankedMemory);
        Assert::AreEqual  (apple3->builtInDriveCount, plus->builtInDriveCount);
        Assert::AreEqual  (apple3->maxExternalDrives, plus->maxExternalDrives);
        Assert::AreEqual  (apple3->slotCount,         plus->slotCount);
        Assert::AreEqual  ((size_t) 1,                plus->memoryOptions.size());
        Assert::AreEqual  (std::string ("256K"),      plus->memoryOptions[0]);
    }


    TEST_METHOD (ShippedFile_LoadsAt256K)
    {
        MachineConfig  config;
        std::string    error;
        HRESULT        hr = Load (EmbeddedMachineJson::Load (IDR_MACHINE_APPLE3_PLUS), config, error);



        AssertSucceeded (hr, TextEncoding::NarrowToWide (error).c_str());
        Assert::AreEqual (std::string ("Apple /// Plus"), config.name);
        Assert::AreEqual ((uint32_t) 256,                 config.memoryK);
        Assert::AreEqual ((Word) 0xF000,                  config.systemRom.address);
    }


    TEST_METHOD (Memory128K_IsAnError)
    {
        MachineConfig  config;
        std::string    error;
        std::string    json = EmbeddedMachineJson::Load (IDR_MACHINE_APPLE3_PLUS);
        size_t         at   = json.find ("\"memory\": \"256K\"");
        HRESULT        hr   = S_OK;



        Assert::AreNotEqual (std::string::npos, at);
        json.replace (at, strlen ("\"memory\": \"256K\""), "\"memory\": \"128K\"");

        hr = Load (json, config, error);

        Assert::IsTrue   (FAILED (hr));
        Assert::AreEqual (std::string ("Invalid memory: '128K' (the option is 256K)"), error);
    }


    TEST_METHOD (Machine_BuildsWith256K_AndStartsInTheRom)
    {
        TestMachine  machine ("Apple3Plus");



        Assert::IsNotNull (machine.GetApple3());
        Assert::AreEqual  (Apple3Mmu::kRam256K, machine.GetApple3()->GetMmu().GetRamSize());
        Assert::IsTrue    (machine.GetCpu()->GetPC() >= Apple3System::kRomStart);
    }
};
