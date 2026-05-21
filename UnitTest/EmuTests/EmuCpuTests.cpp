#include "Pch.h"
#include "Core/MemoryBus.h"
#include "Core/EmuCpu.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Video/AppleTextMode.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  EmuCpuTests
//
//  Note: Cpu::StepOne reads opcodes from internal memory[] directly, and
//  CpuOperations::Store writes to internal memory[] directly.  Operand
//  fetching uses virtual ReadByte/ReadWord (routed through the bus).
//  Therefore tests must PokeByte the program into internal memory and
//  use PeekByte to verify store results.
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

        std::vector<Byte> romData (0x3000, 0xEA);
        romData[0x2FFC] = 0x00;
        romData[0x2FFD] = 0xD0;

        auto rom = RomDevice::CreateFromData (0xD000, 0xFFFF,
            romData.data (), romData.size ());
        bus.AddDevice (rom.get ());

        EmuCpu cpu (bus);

        // Copy ROM data into CPU internal memory[] for execution
        for (size_t i = 0; i < romData.size (); i++)
        {
            cpu.PokeByte (static_cast<Word> (0xD000 + i), romData[i]);
        }

        cpu.InitForEmulation ();

        // EmuCpu::WriteByte routes through bus to RAM
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

        // Copy ROM data into CPU internal memory[] for execution
        for (size_t i = 0; i < romData.size (); i++)
        {
            cpu.PokeByte (static_cast<Word> (0xD000 + i), romData[i]);
        }

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

        // Copy ROM data into CPU internal memory[] for execution
        for (size_t i = 0; i < romData.size (); i++)
        {
            cpu.PokeByte (static_cast<Word> (0xD000 + i), romData[i]);
        }

        cpu.InitForEmulation ();

        cpu.WriteWord (0x0060, 0xBEEF);

        Assert::AreEqual (static_cast<Byte> (0xEF), cpu.ReadByte (0x0060));
        Assert::AreEqual (static_cast<Byte> (0xBE), cpu.ReadByte (0x0061));
    }

    TEST_METHOD (ExecuteProgram_LdaSta_StoresResult)
    {
        // LDA #$77, STA $0200, JMP loop
        MemoryBus bus;
        RamDevice ram (0x0000, 0xBFFF);
        bus.AddDevice (&ram);

        std::vector<Byte> romData (0x3000, 0xEA);

        romData[0x0000] = 0xA9;  // LDA #$77
        romData[0x0001] = 0x77;
        romData[0x0002] = 0x8D;  // STA $0200
        romData[0x0003] = 0x00;
        romData[0x0004] = 0x02;
        romData[0x0005] = 0xEA;  // NOP
        romData[0x0006] = 0x4C;  // JMP $D006
        romData[0x0007] = 0x06;
        romData[0x0008] = 0xD0;

        romData[0x2FFC] = 0x00;
        romData[0x2FFD] = 0xD0;

        auto rom = RomDevice::CreateFromData (0xD000, 0xFFFF,
            romData.data (), romData.size ());
        bus.AddDevice (rom.get ());

        EmuCpu cpu (bus);

        // Load ROM into CPU internal memory (StepOne reads opcodes directly)
        for (size_t i = 0; i < romData.size (); i++)
        {
            cpu.PokeByte (static_cast<Word> (0xD000 + i), romData[i]);
        }

        cpu.InitForEmulation ();
        Assert::AreEqual (static_cast<Word> (0xD000), cpu.GetPC ());

        for (int i = 0; i < 100; i++)
        {
            cpu.StepOne ();
        }

        // Store writes to CPU internal memory
        Byte result = cpu.PeekByte (0x0200);
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
        romData[0x0005] = 0xEA;  // NOP
        romData[0x0006] = 0x4C;  // JMP $D006
        romData[0x0007] = 0x06;
        romData[0x0008] = 0xD0;

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

        for (int i = 0; i < 100; i++)
        {
            cpu.StepOne ();
        }

        Byte result = cpu.PeekByte (0x0300);
        Assert::AreEqual (static_cast<Byte> (0x33), result);
    }

    TEST_METHOD (PCAdvances_DuringExecution)
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
        Word startPC = cpu.GetPC ();

        cpu.StepOne ();

        Assert::IsTrue (cpu.GetPC () != startPC,
            L"PC should advance after executing a NOP");
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

        // Copy ROM data into CPU internal memory[] for execution
        for (size_t i = 0; i < romData.size (); i++)
        {
            cpu.PokeByte (static_cast<Word> (0xD000 + i), romData[i]);
        }

        cpu.InitForEmulation ();

        // ResetCycles should set counter to zero regardless of prior state
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

        // Copy ROM data into CPU internal memory[] for execution
        for (size_t i = 0; i < romData.size (); i++)
        {
            cpu.PokeByte (static_cast<Word> (0xD000 + i), romData[i]);
        }

        cpu.InitForEmulation ();

        Assert::AreEqual (static_cast<Byte> (0xFD), cpu.GetSP ());
        Assert::AreEqual (static_cast<Byte> (0), cpu.GetA ());
        Assert::AreEqual (static_cast<Byte> (0), cpu.GetX ());
        Assert::AreEqual (static_cast<Byte> (0), cpu.GetY ());
        Assert::AreEqual (static_cast<Word> (0xD000), cpu.GetPC ());
    }

    TEST_METHOD (GetMemory_ReturnsNonNull)
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

        Assert::IsNotNull (cpu.GetMemory (),
            L"GetMemory() must never return nullptr");
    }

    TEST_METHOD (PokeByte_VisibleViaGetMemory)
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

        cpu.PokeByte (0x0400, 0xA0);

        Assert::AreEqual (static_cast<Byte> (0xA0), cpu.GetMemory ()[0x0400],
            L"PokeByte should be visible via GetMemory()");
    }

    TEST_METHOD (WriteByte_SyncsToInternalMemory)
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

        // WriteByte should update BOTH internal memory and bus
        cpu.WriteByte (0x0300, 0x42);

        Assert::AreEqual (static_cast<Byte> (0x42), cpu.PeekByte (0x0300),
            L"WriteByte should sync to internal memory (PeekByte)");
    }

    TEST_METHOD (WriteByte_SyncsToBus)
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

        cpu.WriteByte (0x0300, 0x42);

        // Verify the bus (and thus the underlying RamDevice) received the write
        Assert::AreEqual (static_cast<Byte> (0x42), bus.ReadByte (0x0300),
            L"WriteByte should sync to bus (RamDevice)");
    }

    TEST_METHOD (StoreInstruction_WritesToMemory)
    {
        // LDA #$42, STA $0400, NOP, JMP loop
        MemoryBus bus;
        RamDevice ram (0x0000, 0xBFFF);
        bus.AddDevice (&ram);

        std::vector<Byte> romData (0x3000, 0xEA);

        romData[0x0000] = 0xA9;  // LDA #$42
        romData[0x0001] = 0x42;
        romData[0x0002] = 0x8D;  // STA $0400
        romData[0x0003] = 0x00;
        romData[0x0004] = 0x04;
        romData[0x0005] = 0xEA;  // NOP
        romData[0x0006] = 0x4C;  // JMP $D006
        romData[0x0007] = 0x06;
        romData[0x0008] = 0xD0;

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

        for (int i = 0; i < 10; i++)
        {
            cpu.StepOne ();
        }

        // Verify the store reached internal memory
        Assert::AreEqual (static_cast<Byte> (0x42), cpu.PeekByte (0x0400),
            L"STA $0400 should be visible via PeekByte");

        // Verify it's also visible via GetMemory()
        Assert::AreEqual (static_cast<Byte> (0x42), cpu.GetMemory ()[0x0400],
            L"STA $0400 should be visible via GetMemory()");
    }

    TEST_METHOD (StoreInstruction_VisibleToVideoRam)
    {
        // End-to-end test: execute STA $0400, then render text mode using
        // GetMemory() — verify framebuffer has non-black pixels.
        // This is the test that would have caught the green screen bug.
        MemoryBus bus;
        RamDevice ram (0x0000, 0xBFFF);
        bus.AddDevice (&ram);

        std::vector<Byte> romData (0x3000, 0xEA);

        romData[0x0000] = 0xA9;  // LDA #$C1  (normal 'A')
        romData[0x0001] = 0xC1;
        romData[0x0002] = 0x8D;  // STA $0400
        romData[0x0003] = 0x00;
        romData[0x0004] = 0x04;
        romData[0x0005] = 0xEA;  // NOP
        romData[0x0006] = 0x4C;  // JMP $D006
        romData[0x0007] = 0x06;
        romData[0x0008] = 0xD0;

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

        for (int i = 0; i < 10; i++)
        {
            cpu.StepOne ();
        }

        // Now render using GetMemory() — the way the real emulator does it
        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0xFF000000u);

        textMode.Render (cpu.GetMemory (), fb.data (), fbW, fbH);

        // Verify the character cell at $0400 (row 0, col 0) has green pixels
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
            L"STA $0400 followed by Render(GetMemory()) must produce green pixels");
    }
};
