#include "../Casso65EmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Core/EmuCpu.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  EmuCpuTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (EmuCpuTests)
{
public:

    TEST_METHOD (BusRouting_WriteAndReadRAM)
    {
        MemoryBus bus;
        RamDevice ram (0x0000, 0xBFFF);
        bus.AddDevice (&ram);

        // Minimal ROM with reset vector
        std::vector<Byte> romData (0x3000, 0xEA);
        romData[0x2FFC] = 0x00;  // Reset vector -> $D000
        romData[0x2FFD] = 0xD0;

        auto rom = RomDevice::CreateFromData (0xD000, 0xFFFF,
            romData.data (), romData.size ());
        bus.AddDevice (rom.get ());

        EmuCpu cpu (bus);
        cpu.InitForEmulation ();

        // Write through CPU's WriteByte (should go through bus to RAM)
        cpu.WriteByte (0x0300, 0xAB);
        Byte val = cpu.ReadByte (0x0300);

        Assert::AreEqual (static_cast<Byte> (0xAB), val);
    }

    TEST_METHOD (BusRouting_ReadWord)
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
        cpu.InitForEmulation ();

        cpu.WriteByte (0x0050, 0x34);
        cpu.WriteByte (0x0051, 0x12);

        Word val = cpu.ReadWord (0x0050);

        Assert::AreEqual (static_cast<Word> (0x1234), val);
    }

    TEST_METHOD (BusRouting_WriteWord)
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
        cpu.InitForEmulation ();

        cpu.WriteWord (0x0060, 0xBEEF);

        Assert::AreEqual (static_cast<Byte> (0xEF), cpu.ReadByte (0x0060));
        Assert::AreEqual (static_cast<Byte> (0xBE), cpu.ReadByte (0x0061));
    }

    TEST_METHOD (ExecuteProgram_LdaSta_StoresResult)
    {
        // Program: LDA #$77, STA $0200, NOP (infinite loop)
        MemoryBus bus;
        RamDevice ram (0x0000, 0xBFFF);
        bus.AddDevice (&ram);

        std::vector<Byte> romData (0x3000, 0xEA);

        // Program at $D000
        romData[0x0000] = 0xA9;  // LDA #$77
        romData[0x0001] = 0x77;
        romData[0x0002] = 0x8D;  // STA $0200
        romData[0x0003] = 0x00;
        romData[0x0004] = 0x02;
        romData[0x0005] = 0x4C;  // JMP $D005
        romData[0x0006] = 0x05;
        romData[0x0007] = 0xD0;

        // Reset vector -> $D000
        romData[0x2FFC] = 0x00;
        romData[0x2FFD] = 0xD0;

        auto rom = RomDevice::CreateFromData (0xD000, 0xFFFF,
            romData.data (), romData.size ());
        bus.AddDevice (rom.get ());

        EmuCpu cpu (bus);
        cpu.InitForEmulation ();

        Assert::AreEqual (static_cast<Word> (0xD000), cpu.GetPC ());

        // Execute enough steps to run the LDA + STA
        for (int i = 0; i < 10; i++)
        {
            cpu.StepOne ();
        }

        Byte result = bus.ReadByte (0x0200);
        Assert::AreEqual (static_cast<Byte> (0x77), result);
    }

    TEST_METHOD (ExecuteProgram_LdxStx_StoresResult)
    {
        // LDX #$33, STX $0300
        MemoryBus bus;
        RamDevice ram (0x0000, 0xBFFF);
        bus.AddDevice (&ram);

        std::vector<Byte> romData (0x3000, 0xEA);

        romData[0x0000] = 0xA2;  // LDX #$33
        romData[0x0001] = 0x33;
        romData[0x0002] = 0x8E;  // STX $0300
        romData[0x0003] = 0x00;
        romData[0x0004] = 0x03;
        romData[0x0005] = 0x4C;  // JMP $D005
        romData[0x0006] = 0x05;
        romData[0x0007] = 0xD0;

        romData[0x2FFC] = 0x00;
        romData[0x2FFD] = 0xD0;

        auto rom = RomDevice::CreateFromData (0xD000, 0xFFFF,
            romData.data (), romData.size ());
        bus.AddDevice (rom.get ());

        EmuCpu cpu (bus);
        cpu.InitForEmulation ();

        for (int i = 0; i < 10; i++)
        {
            cpu.StepOne ();
        }

        Byte result = bus.ReadByte (0x0300);
        Assert::AreEqual (static_cast<Byte> (0x33), result);
    }

    TEST_METHOD (CycleTracking_IncrementsDuringExecution)
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
        cpu.InitForEmulation ();

        Assert::AreEqual (static_cast<uint64_t> (0), cpu.GetTotalCycles ());

        cpu.StepOne ();

        Assert::IsTrue (cpu.GetTotalCycles () > 0,
            L"Total cycles should increase after executing instructions");
    }

    TEST_METHOD (ResetCycles_ClearsCounter)
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
        cpu.InitForEmulation ();

        cpu.StepOne ();
        Assert::IsTrue (cpu.GetTotalCycles () > 0);

        cpu.ResetCycles ();
        Assert::AreEqual (static_cast<uint64_t> (0), cpu.GetTotalCycles ());
    }

    TEST_METHOD (InitForEmulation_SetsSPAndFlags)
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
        cpu.InitForEmulation ();

        Assert::AreEqual (static_cast<Byte> (0xFD), cpu.GetSP ());
        Assert::AreEqual (static_cast<Byte> (0), cpu.GetA ());
        Assert::AreEqual (static_cast<Byte> (0), cpu.GetX ());
        Assert::AreEqual (static_cast<Byte> (0), cpu.GetY ());
        Assert::AreEqual (static_cast<Word> (0xD000), cpu.GetPC ());
    }
};
