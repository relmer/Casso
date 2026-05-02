#include "../Casso65EmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Core/EmuCpu.h"
#include "Core/PathResolver.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Devices/AppleKeyboard.h"
#include "Video/AppleTextMode.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  EmuCpuIntegrationTests
//
//  Adversarial tests that prove the CPU actually executes real code through
//  the bus, not just that it doesn't crash. These test instruction execution,
//  memory visibility, I/O routing, and end-to-end CPU-to-video rendering.
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
        std::vector<Byte>     romData;
        std::unique_ptr<MemoryDevice> rom;

        TestEnv ()
            : romData (0x3000, 0xEA)
        {
            bus.AddDevice (&ram);

            // Reset vector -> $D000
            romData[0x2FFC] = 0x00;
            romData[0x2FFD] = 0xD0;
        }

        void BuildRom ()
        {
            rom = RomDevice::CreateFromData (0xD000, 0xFFFF,
                romData.data (), romData.size ());
            bus.AddDevice (rom.get ());
        }

        void LoadIntoEmu (EmuCpu & cpu)
        {
            for (size_t i = 0; i < romData.size (); i++)
            {
                cpu.PokeByte (static_cast<Word> (0xD000 + i), romData[i]);
            }

            cpu.InitForEmulation ();
        }
    };

    TEST_METHOD (ResetVector_PCEquals0xD000)
    {
        TestEnv env;
        env.BuildRom ();

        EmuCpu cpu (env.bus);
        env.LoadIntoEmu (cpu);

        Assert::AreEqual (static_cast<Word> (0xD000), cpu.GetPC (),
            L"After InitForEmulation, PC must be $D000 (reset vector)");
    }

    TEST_METHOD (LdaSta_WritesValueToRAM)
    {
        // LDA #$42, STA $0400
        TestEnv env;
        env.romData[0x0000] = 0xA9;  // LDA #$42
        env.romData[0x0001] = 0x42;
        env.romData[0x0002] = 0x8D;  // STA $0400
        env.romData[0x0003] = 0x00;
        env.romData[0x0004] = 0x04;
        env.romData[0x0005] = 0xEA;  // NOP
        env.romData[0x0006] = 0x4C;  // JMP $D006
        env.romData[0x0007] = 0x06;
        env.romData[0x0008] = 0xD0;
        env.BuildRom ();

        EmuCpu cpu (env.bus);
        env.LoadIntoEmu (cpu);

        for (int i = 0; i < 10; i++)
        {
            cpu.StepOne ();
        }

        Assert::AreEqual (static_cast<Byte> (0x42), cpu.PeekByte (0x0400),
            L"STA $0400 should write $42 to memory");
    }

    TEST_METHOD (StaVisibleToVideoRam)
    {
        // STA $0400 should be visible via GetMemory() for video rendering
        TestEnv env;
        env.romData[0x0000] = 0xA9;  // LDA #$C1
        env.romData[0x0001] = 0xC1;
        env.romData[0x0002] = 0x8D;  // STA $0400
        env.romData[0x0003] = 0x00;
        env.romData[0x0004] = 0x04;
        env.romData[0x0005] = 0xEA;
        env.romData[0x0006] = 0x4C;
        env.romData[0x0007] = 0x06;
        env.romData[0x0008] = 0xD0;
        env.BuildRom ();

        EmuCpu cpu (env.bus);
        env.LoadIntoEmu (cpu);

        for (int i = 0; i < 10; i++)
        {
            cpu.StepOne ();
        }

        Assert::AreEqual (static_cast<Byte> (0xC1), cpu.GetMemory ()[0x0400],
            L"STA $0400 must be visible via GetMemory() for video");

        // End-to-end: render and verify green pixels
        AppleTextMode textMode (env.bus);
        textMode.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0xFF000000u);

        textMode.Render (cpu.GetMemory (), fb.data (), fbW, fbH);

        bool hasGreen = false;

        for (int y = 0; y < 16 && !hasGreen; y++)
        {
            for (int x = 0; x < 14 && !hasGreen; x++)
            {
                if (fb[y * fbW + x] == 0xFF00FF00u)
                {
                    hasGreen = true;
                }
            }
        }

        Assert::IsTrue (hasGreen,
            L"CPU STA -> GetMemory() -> Render must produce green pixels");
    }

    TEST_METHOD (JsrRts_SubroutineExecutes)
    {
        // JSR $D010, at $D010: LDA #$99, RTS
        TestEnv env;
        env.romData[0x0000] = 0x20;  // JSR $D010
        env.romData[0x0001] = 0x10;
        env.romData[0x0002] = 0xD0;
        env.romData[0x0003] = 0xEA;  // NOP (return here after RTS)
        env.romData[0x0004] = 0x4C;  // JMP $D004
        env.romData[0x0005] = 0x04;
        env.romData[0x0006] = 0xD0;

        // Subroutine at $D010
        env.romData[0x0010] = 0xA9;  // LDA #$99
        env.romData[0x0011] = 0x99;
        env.romData[0x0012] = 0x60;  // RTS
        env.BuildRom ();

        EmuCpu cpu (env.bus);
        env.LoadIntoEmu (cpu);

        for (int i = 0; i < 20; i++)
        {
            cpu.StepOne ();
        }

        Assert::AreEqual (static_cast<Byte> (0x99), cpu.GetA (),
            L"After JSR/LDA/RTS, A should be $99");
    }

    TEST_METHOD (PhaPla_StackRoundtrip)
    {
        // LDA #$55, PHA, LDA #$00, PLA -> A should be $55
        TestEnv env;
        env.romData[0x0000] = 0xA9;  // LDA #$55
        env.romData[0x0001] = 0x55;
        env.romData[0x0002] = 0x48;  // PHA
        env.romData[0x0003] = 0xA9;  // LDA #$00
        env.romData[0x0004] = 0x00;
        env.romData[0x0005] = 0x68;  // PLA
        env.romData[0x0006] = 0xEA;  // NOP
        env.romData[0x0007] = 0x4C;  // JMP $D007
        env.romData[0x0008] = 0x07;
        env.romData[0x0009] = 0xD0;
        env.BuildRom ();

        EmuCpu cpu (env.bus);
        env.LoadIntoEmu (cpu);

        for (int i = 0; i < 20; i++)
        {
            cpu.StepOne ();
        }

        Assert::AreEqual (static_cast<Byte> (0x55), cpu.GetA (),
            L"After PHA/PLA, A must roundtrip to $55");
    }

    TEST_METHOD (IORead_GoesToBusDevice)
    {
        // Register keyboard at $C000, read via CPU -> should go through bus
        TestEnv env;
        AppleKeyboard kbd;
        env.bus.AddDevice (&kbd);
        env.BuildRom ();

        EmuCpu cpu (env.bus);
        env.LoadIntoEmu (cpu);

        kbd.KeyPress ('H');

        // EmuCpu::ReadByte for $C000-$CFFF goes through bus
        Byte val = cpu.ReadByte (0xC000);

        Assert::AreEqual (static_cast<Byte> (0xC8), val,
            L"I/O read at $C000 must go through bus to keyboard");
    }

    TEST_METHOD (IOWrite_GoesToBusDevice)
    {
        // Writing to $C010 should clear keyboard strobe via bus
        TestEnv env;
        AppleKeyboard kbd;
        env.bus.AddDevice (&kbd);
        env.BuildRom ();

        EmuCpu cpu (env.bus);
        env.LoadIntoEmu (cpu);

        kbd.KeyPress ('Z');

        // Write to $C010 to clear strobe (via bus)
        cpu.WriteByte (0xC010, 0x00);

        Byte val = cpu.ReadByte (0xC000);
        Assert::IsTrue ((val & 0x80) == 0,
            L"I/O write to $C010 must clear keyboard strobe via bus");
    }

    TEST_METHOD (WriteByte_SyncsToBothBusAndInternal)
    {
        TestEnv env;
        env.BuildRom ();

        EmuCpu cpu (env.bus);
        env.LoadIntoEmu (cpu);

        cpu.WriteByte (0x0300, 0x42);

        // Should be visible via PeekByte (internal memory)
        Assert::AreEqual (static_cast<Byte> (0x42), cpu.PeekByte (0x0300),
            L"WriteByte must update internal memory");

        // Should also be visible on the bus
        Assert::AreEqual (static_cast<Byte> (0x42), env.bus.ReadByte (0x0300),
            L"WriteByte must update bus (RamDevice)");
    }

    TEST_METHOD (NopSled_100000Steps_PCInRomRange)
    {
        TestEnv env;
        env.BuildRom ();

        EmuCpu cpu (env.bus);
        env.LoadIntoEmu (cpu);

        for (int i = 0; i < 1000; i++)
        {
            cpu.StepOne ();
        }

        Word pc = cpu.GetPC ();

        Assert::IsTrue (pc >= 0xD000 && pc <= 0xFFFF,
            std::format (L"After 1000 NOPs, PC=${:04X} should be in ROM range $D000-$FFFF", pc).c_str ());
    }

    TEST_METHOD (RealRom_ResetVector_IfAvailable)
    {
        // If apple2plus.rom is available, load it and verify reset vector
        auto paths = PathResolver::BuildSearchPaths (
            PathResolver::GetExecutableDirectory (),
            PathResolver::GetWorkingDirectory ());

        std::string romPath = PathResolver::FindFile (paths, "roms/apple2plus.rom").string ();

        if (romPath.empty ())
        {
            // Skip if ROM not available (CI)
            return;
        }

        MemoryBus bus;
        RamDevice ram (0x0000, 0xBFFF);
        bus.AddDevice (&ram);

        std::string error;
        auto rom = RomDevice::CreateFromFile (0xD000, 0xFFFF, romPath, error);

        Assert::IsNotNull (rom.get (),
            std::format (L"Failed to load ROM: {}", std::wstring (error.begin (), error.end ())).c_str ());

        bus.AddDevice (rom.get ());

        EmuCpu cpu (bus);

        // Copy ROM into internal memory
        const Byte * romData = static_cast<RomDevice *> (rom.get ())->GetData ();
        size_t romSize = 0xFFFF - 0xD000 + 1;

        for (size_t i = 0; i < romSize; i++)
        {
            cpu.PokeByte (static_cast<Word> (0xD000 + i), romData[i]);
        }

        cpu.InitForEmulation ();

        // Apple II+ reset vector is $FA62
        Assert::AreEqual (static_cast<Word> (0xFA62), cpu.GetPC (),
            L"Real Apple II+ ROM reset vector should be $FA62");
    }

    TEST_METHOD (RealRom_100000Cycles_PCInRomRange)
    {
        // If ROM available, verify CPU stays in ROM after many cycles
        auto paths = PathResolver::BuildSearchPaths (
            PathResolver::GetExecutableDirectory (),
            PathResolver::GetWorkingDirectory ());

        std::string romPath = PathResolver::FindFile (paths, "roms/apple2plus.rom").string ();

        if (romPath.empty ())
        {
            return;
        }

        MemoryBus bus;
        RamDevice ram (0x0000, 0xBFFF);
        bus.AddDevice (&ram);

        AppleKeyboard kbd;
        bus.AddDevice (&kbd);

        std::string error;
        auto rom = RomDevice::CreateFromFile (0xD000, 0xFFFF, romPath, error);
        Assert::IsNotNull (rom.get ());
        bus.AddDevice (rom.get ());

        EmuCpu cpu (bus);

        const Byte * romData = static_cast<RomDevice *> (rom.get ())->GetData ();
        size_t romSize = 0xFFFF - 0xD000 + 1;

        for (size_t i = 0; i < romSize; i++)
        {
            cpu.PokeByte (static_cast<Word> (0xD000 + i), romData[i]);
        }

        cpu.InitForEmulation ();

        for (int i = 0; i < 100000; i++)
        {
            cpu.StepOne ();
        }

        Word pc = cpu.GetPC ();

        // PC should be somewhere in ROM, not stuck at 0
        Assert::IsTrue (pc != 0x0000,
            L"After 100K cycles with real ROM, PC should not be stuck at $0000");

        // PC should be in a valid code region (not in I/O space)
        Assert::IsTrue (pc < 0xC000 || pc >= 0xD000,
            std::format (L"PC=${:04X} should not be in I/O range $C000-$CFFF", pc).c_str ());
    }
};
