#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/Floppy35/Apple2cSmartPort.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace fs = std::filesystem;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortChainTests
//
//  The //c disk port's SmartPort chain as the loader checks it: UniDisk 3.5
//  drives first, up to the limit, at most one 5.25-inch drive last, each
//  UniDisk's ROM resolved; and the port hook answering for drive 2.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskPortChainTests)
{
public:

    static std::vector<PortConfig> MakePorts (const std::vector<const char *> & devices)
    {
        std::vector<PortConfig>  ports (1);



        ports[0].name = "disk";

        for (const char * device : devices)
        {
            PortConfig  entry;



            entry.device = device;
            ports[0].chain.push_back (entry);
        }

        return ports;
    }


    static HRESULT Resolve (std::vector<PortConfig> & ports, std::string & error, const fs::path & rom = {})
    {
        MachineConfigLoader::FileResolver  resolver = [&rom] (const std::vector<fs::path> &, const fs::path &) { return rom; };



        return MachineConfigLoader::ResolveDiskPortChains ({}, resolver, ports, error);
    }


    TEST_METHOD (Chain_DrivesThenOneFloppy)
    {
        std::vector<PortConfig>  ports = MakePorts ({ kpszUniDisk35Device, kpszUniDisk35Device, kpszUniDisk35Device, kpszUniDisk35Device, kpszDiskIicDevice });
        std::string              error;



        Assert::AreEqual (S_OK, Resolve (ports, error));
        Assert::IsTrue (error.empty());
    }


    TEST_METHOD (Chain_OverTheLimitFails)
    {
        std::vector<PortConfig>  ports = MakePorts ({ kpszUniDisk35Device, kpszUniDisk35Device, kpszUniDisk35Device, kpszUniDisk35Device, kpszUniDisk35Device });
        std::string              error;



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), Resolve (ports, error));
        Assert::IsTrue (error.find ("at most 4") != std::string::npos);
    }


    TEST_METHOD (Chain_FloppyMustBeLast)
    {
        std::vector<PortConfig>  ports = MakePorts ({ kpszDiskIicDevice, kpszUniDisk35Device });
        std::string              error;



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), Resolve (ports, error));
        Assert::IsTrue (error.find ("last") != std::string::npos);
    }


    TEST_METHOD (Chain_OtherDevicesFail)
    {
        std::vector<PortConfig>  ports = MakePorts ({ kpszApple35DriveDevice });
        std::string              error;



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), Resolve (ports, error));
        Assert::IsTrue (error.find (kpszApple35DriveDevice) != std::string::npos);
    }


    //  Each UniDisk 3.5's ROM resolves into its entry when the file is the
    //  ROM's size, and is left empty otherwise.
    TEST_METHOD (Chain_ResolvesEachDrivesRom)
    {
        constexpr size_t  kRomSize = 8192;

        fs::path                 rom   = fs::temp_directory_path() / "Casso-DiskPortChainTests-UniDisk35.rom";
        std::vector<PortConfig>  ports = MakePorts ({ kpszUniDisk35Device, kpszDiskIicDevice });
        std::string              error;



        {
            std::ofstream  file (rom, std::ios::binary);



            file << std::string (kRomSize, '\0');
        }

        Assert::AreEqual (S_OK, Resolve (ports, error, rom));
        Assert::AreEqual (rom.string(), ports[0].chain[0].firmwarePath);
        Assert::IsTrue (ports[0].chain[1].firmwarePath.empty(), L"a 5.25-inch drive has no ROM");

        Assert::AreEqual (S_OK, Resolve (ports, error, {}));
        Assert::IsTrue (ports[0].chain[0].firmwarePath.empty(), L"for a missing ROM the path is empty");

        fs::remove (rom);
    }


    //  The hook returns the bus's byte for drive 2 with a drive on the chain;
    //  with a 5.25-inch drive after the chain, only while the bus is enabled.
    TEST_METHOD (Hook_AnswersForDriveTwo)
    {
        Apple2cSmartPort  plain (false);
        Apple2cSmartPort  withFloppy (true);
        Byte              value = 0;



        plain.GetPort().AddDrive (std::make_unique<UniDisk35Drive>());
        withFloppy.GetPort().AddDrive (std::make_unique<UniDisk35Drive>());

        Assert::IsFalse (plain.OnIwmAccess (0xC, false, 0, value), L"drive 1 is the built-in drive's");

        Assert::IsTrue  (plain.OnIwmAccess (0xB, false, 0, value));
        Assert::IsFalse (withFloppy.OnIwmAccess (0xB, false, 0, value), L"the 5.25-inch drive has the port");

        withFloppy.OnIwmAccess (0x3, false, 0, value);
        Assert::IsTrue (withFloppy.OnIwmAccess (0x7, false, 0, value), L"phases 1 and 3 enable the bus");
    }
};
