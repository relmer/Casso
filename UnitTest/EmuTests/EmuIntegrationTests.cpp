#include "../Casso65EmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Core/EmuCpu.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Devices/AppleKeyboard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  EmuIntegrationTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (EmuIntegrationTests)
{
public:

    TEST_METHOD (CpuExecutes_FromResetVector)
    {
        MemoryBus bus;

        // 48KB RAM ($0000-$BFFF)
        RamDevice ram (0x0000, 0xBFFF);
        bus.AddDevice (&ram);

        // Keyboard at $C000-$C01F
        AppleKeyboard kbd;
        bus.AddDevice (&kbd);

        // Create a small ROM at $D000-$FFFF
        std::vector<Byte> romData (0x3000, 0xEA);  // Fill with NOP

        // Program at $D000:
        //   LDA #$42   (A9 42)
        //   STA $0200  (8D 00 02)
        //   NOP        (EA)
        //   JMP $D006  (4C 06 D0)
        romData[0x0000] = 0xA9;
        romData[0x0001] = 0x42;
        romData[0x0002] = 0x8D;
        romData[0x0003] = 0x00;
        romData[0x0004] = 0x02;
        romData[0x0005] = 0xEA;
        romData[0x0006] = 0x4C;
        romData[0x0007] = 0x06;
        romData[0x0008] = 0xD0;

        // Reset vector at $FFFC -> $D000
        romData[0x2FFC] = 0x00;
        romData[0x2FFD] = 0xD0;

        auto rom = RomDevice::CreateFromData (0xD000, 0xFFFF,
            romData.data (), romData.size ());

        bus.AddDevice (rom.get ());

        // Create CPU and load ROM into its internal memory
        // (Cpu::StepOne reads opcodes from internal memory directly)
        EmuCpu cpu (bus);

        for (size_t i = 0; i < romData.size (); i++)
        {
            cpu.PokeByte (static_cast<Word> (0xD000 + i), romData[i]);
        }

        cpu.InitForEmulation ();

        // PC should be at reset vector ($D000)
        Word initialPC = cpu.GetPC ();
        Assert::AreEqual (static_cast<Word> (0xD000), initialPC);

        // Run 100 cycles
        for (int i = 0; i < 100; i++)
        {
            cpu.StepOne ();
        }

        // PC should have advanced past $D000
        Word finalPC = cpu.GetPC ();
        Assert::AreNotEqual (initialPC, finalPC);

        // The program wrote $42 to $0200 (via CPU internal memory)
        Byte storedVal = cpu.PeekByte (0x0200);
        Assert::AreEqual (static_cast<Byte> (0x42), storedVal);
    }

    TEST_METHOD (CpuExecutes_NopSled)
    {
        MemoryBus bus;

        RamDevice ram (0x0000, 0xBFFF);
        bus.AddDevice (&ram);

        std::vector<Byte> romData (0x3000, 0xEA);
        romData[0x2FFC] = 0x00;
        romData[0x2FFD] = 0xD0;

        auto rom = RomDevice::CreateFromData (0xD000, 0xFFFF,
            romData.data (), romData.size ());

        bus.AddDevice (rom.get ());

        EmuCpu cpu (bus);

        for (size_t i = 0; i < romData.size (); i++)
        {
            cpu.PokeByte (static_cast<Word> (0xD000 + i), romData[i]);
        }

        cpu.InitForEmulation ();

        // Execute many cycles — should not crash
        for (int i = 0; i < 500; i++)
        {
            cpu.StepOne ();
        }

        // PC advanced beyond start
        Assert::IsTrue (cpu.GetPC () != 0xD000);
    }
};
