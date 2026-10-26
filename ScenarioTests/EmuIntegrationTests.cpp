#include "Pch.h"
#include "Core/MemoryBus.h"
#include "Core/EmuCpu.h"
#include "Core/Prng.h"
#include "Core/PathResolver.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/AppleTextMode.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  EmuCpuIntegrationTests
//
//  Adversarial tests that prove the CPU actually executes real code through
//  the bus, not just that it doesn't crash. These test instruction execution,
//  memory visibility, I/O routing, and end-to-end CPU-to-video rendering.
//
//  This file has the cases that read a real ROM from %LOCALAPPDATA%\Casso. The
//  rest are in the unit suite's EmuCpuIntegrationTests.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (EmuCpuIntegrationTests)
{
public:

    ////////////////////////////////////////////////////////////////////////////
    //  Helper: create a standard bus+RAM+ROM setup
    ////////////////////////////////////////////////////////////////////////////

    struct TestEnv
    {
        MemoryBus             bus;
        RamDevice             ram {0x0000, 0xBFFF};
        std::vector<Byte>              romData;
        std::unique_ptr<MemoryDevice>  rom;

        TestEnv()
            : romData (0x3000, 0xEA)
        {
            bus.AddDevice (&ram);

            // Reset vector -> $D000
            romData[0x2FFC] = 0x00;
            romData[0x2FFD] = 0xD0;
        }

        void BuildRom()
        {
            rom = RomDevice::CreateFromData (0xD000, 0xFFFF,
                romData.data(), romData.size());
            bus.AddDevice (rom.get());
        }

        void LoadIntoEmu (EmuCpu & cpu)
        {
            for (size_t i = 0; i < romData.size(); i++)
            {
                cpu.PokeByte (static_cast<Word> (0xD000 + i), romData[i]);
            }

            Prng prng { 0xCA550001ULL };
            cpu.InitForEmulation (prng);
        }
    };

    TEST_METHOD (RealRom_ResetVector_IfAvailable)
    {
        MemoryBus                        bus;
        std::string                      error;
        const Byte                     * romData = nullptr;
        size_t                           romSize = 0;
        std::unique_ptr<MemoryDevice>    rom;



        // If Apple2Plus.rom is available, load it and verify reset vector
        auto paths = PathResolver::BuildSearchPaths (
            PathResolver::GetExecutableDirectory(),
            PathResolver::GetWorkingDirectory());

        std::string romPath = PathResolver::FindFile (paths, "ROMs/Apple2Plus.rom").string();

        if (romPath.empty())
        {
            // Skip if ROM not available (CI)
            return;
        }

        RamDevice ram (0x0000, 0xBFFF);
        bus.AddDevice (&ram);

        rom = RomDevice::CreateFromFile (0xD000, 0xFFFF, romPath, error);

        Assert::IsNotNull (rom.get(),
            std::format (L"Failed to load ROM: {}", TextEncoding::NarrowToWide (error)).c_str());

        bus.AddDevice (rom.get());

        EmuCpu cpu (bus);

        // Copy ROM into internal memory
        romData = static_cast<RomDevice *> (rom.get())->GetData();
        romSize = 0xFFFF - 0xD000 + 1;

        for (size_t i = 0; i < romSize; i++)
        {
            cpu.PokeByte (static_cast<Word> (0xD000 + i), romData[i]);
        }

        Prng prng { 0xCA550001ULL };
        cpu.InitForEmulation (prng);

        // Apple II+ reset vector is $FA62
        Assert::AreEqual (static_cast<Word> (0xFA62), cpu.GetPC(),
            L"Real Apple II+ ROM reset vector should be $FA62");
    }

    TEST_METHOD (RealRom_100000Cycles_PCInRomRange)
    {
        MemoryBus                        bus;
        AppleKeyboard                    kbd;
        std::string                      error;
        const Byte                     * romData = nullptr;
        size_t                           romSize = 0;
        Word                             pc      = 0;
        std::unique_ptr<MemoryDevice>    rom;



        // If ROM available, verify CPU stays in ROM after many cycles
        auto paths = PathResolver::BuildSearchPaths (
            PathResolver::GetExecutableDirectory(),
            PathResolver::GetWorkingDirectory());

        std::string romPath = PathResolver::FindFile (paths, "ROMs/Apple2Plus.rom").string();

        if (romPath.empty())
        {
            return;
        }

        RamDevice ram (0x0000, 0xBFFF);
        bus.AddDevice (&ram);

        bus.AddDevice (&kbd);

        rom = RomDevice::CreateFromFile (0xD000, 0xFFFF, romPath, error);
        Assert::IsNotNull (rom.get());
        bus.AddDevice (rom.get());

        EmuCpu cpu (bus);

        romData = static_cast<RomDevice *> (rom.get())->GetData();
        romSize = 0xFFFF - 0xD000 + 1;

        for (size_t i = 0; i < romSize; i++)
        {
            cpu.PokeByte (static_cast<Word> (0xD000 + i), romData[i]);
        }

        Prng prng { 0xCA550001ULL };
        cpu.InitForEmulation (prng);

        for (int i = 0; i < 100000; i++)
        {
            cpu.StepOne();
        }

        pc = cpu.GetPC();

        // PC should be somewhere in ROM, not stuck at 0
        Assert::IsTrue (pc != 0x0000,
            L"After 100K cycles with real ROM, PC should not be stuck at $0000");

        // PC should be in a valid code region (not in I/O space)
        Assert::IsTrue (pc < 0xC000 || pc >= 0xD000,
            std::format (L"PC=${:04X} should not be in I/O range $C000-$CFFF", pc).c_str());
    }
};
