#include "Pch.h"

#include "Core/MemoryBus.h"
#include "Core/MemoryDevice.h"
#include "Devices/Floppy35/Apple35ControllerCard.h"
#include "Devices/Floppy35/DeviceCpu.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FlatMemory
//
//  64K of memory that counts the reads the device serves.
//
////////////////////////////////////////////////////////////////////////////////

class FlatMemory : public MemoryDevice
{
public:
    Byte  Read     (Word address) override { reads++; return bytes[address]; }
    void  Write    (Word address, Byte value) override { bytes[address] = value; }
    Word  GetStart() const override { return 0x0000; }
    Word  GetEnd() const override { return 0xFFFF; }
    void  Reset() override {}

    std::vector<Byte>  bytes = std::vector<Byte> (0x10000, 0);
    size_t             reads = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Apple35ControllerCardTests
//
//  The card's 65C02 on its separate bus, and the windows the Apple has into
//  the card's RAM.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple35ControllerCardTests)
{
public:

    static constexpr int       kSlot       = 5;
    static constexpr uint32_t  kHostHz     = 1'023'000;
    static constexpr uint64_t  kCardHz     = 2'045'454;
    static constexpr size_t    kRomSize    = 0x8000;
    static constexpr size_t    kResetLow   = 0x7FFC;


    //  A ROM whose code, at $8000, stores `value` to `address` and loops.
    static std::vector<Byte> MakeRom (Word address, Byte value)
    {
        std::vector<Byte>  rom (kRomSize, 0xEA);
        const Byte         code[] = { 0xA9, value, 0x8D, (Byte) address, (Byte) (address >> 8), 0x4C, 0x05, 0x80 };



        std::copy (std::begin (code), std::end (code), rom.begin());
        rom[kResetLow]     = 0x00;
        rom[kResetLow + 1] = 0x80;

        return rom;
    }


    TEST_METHOD (DeviceCpu_RunsFromTheResetVector)
    {
        FlatMemory  memory;
        DeviceCpu   cpu;



        memory.bytes[0xFFFC] = 0x00;
        memory.bytes[0xFFFD] = 0x90;
        memory.bytes[0x9000] = 0xA9;
        memory.bytes[0x9001] = 0x5A;
        memory.bytes[0x9002] = 0x85;
        memory.bytes[0x9003] = 0x10;

        Assert::AreEqual (S_OK, cpu.Initialize (memory));
        cpu.Reset();
        Assert::AreEqual ((Word) 0x9000, cpu.GetPC());

        cpu.RunUntil (cpu.GetCycles() + 5);

        Assert::AreEqual ((Byte) 0x5A, memory.bytes[0x10], L"RAM below $C000 is the device's, not the CPU's array");
        Assert::IsTrue (cpu.GetCycles() >= 5);
    }


    //  A mapped page is served from memory directly; an unmapped one goes
    //  to the device.
    TEST_METHOD (DeviceCpu_MappedPagesSkipTheDevice)
    {
        FlatMemory         memory;
        DeviceCpu          cpu;
        std::vector<Byte>  page (0x100, 0xEA);
        size_t             before = 0;



        memory.bytes[0xFFFC] = 0x00;
        memory.bytes[0xFFFD] = 0x40;

        Assert::AreEqual (S_OK, cpu.Initialize (memory));
        cpu.MapPages (0x40, 1, page.data(), nullptr);
        cpu.Reset();

        before = memory.reads;
        cpu.RunUntil (cpu.GetCycles() + 40);

        Assert::AreEqual (before, memory.reads, L"the NOPs come from the mapped page");
    }


    //  The card's CPU runs at 2.046 MHz against the Apple's clock, brought up
    //  to date by the Apple's accesses; its slot page is RAM $7B00-$7BFF.
    TEST_METHOD (Card_CpuFollowsTheHostClock)
    {
        MemoryBus              bus;
        Apple35ControllerCard  card (kSlot, bus);
        uint64_t               hostCycles = 0;
        uint64_t               start      = 0;



        Assert::AreEqual (S_OK, card.SetFirmware (MakeRom (0x7B01, 0x20)));
        card.SetHostClock (&hostCycles, kHostHz);
        start = card.GetCardCpu().GetCycles();

        hostCycles = kHostHz / 10;
        Assert::AreEqual ((Byte) 0x20, card.OnSlotPageAccess (0xC501, false, 0));

        Assert::IsTrue (card.GetCardCpu().GetCycles() - start >= kCardHz / 10 - 8);
        Assert::IsTrue (card.GetCardCpu().GetCycles() - start <= kCardHz / 10 + 8);
    }


    TEST_METHOD (Card_SlotPageIgnoresWrites)
    {
        MemoryBus              bus;
        Apple35ControllerCard  card (kSlot, bus);



        card.GetCardMemory().GetRam()[0x7B10] = 0x11;
        card.OnSlotPageAccess (0xC510, true, 0x99);

        Assert::AreEqual ((Byte) 0x11, card.GetCardMemory().GetRam()[0x7B10]);
    }


    //  Any access to $C0n0-$C0nF latches the bank; $C800-$CBFF is card RAM at
    //  the bank times $400 and $CC00-$CFFE is card RAM $7C00-$7FFE.
    TEST_METHOD (Card_ExpansionWindows)
    {
        MemoryBus              bus;
        Apple35ControllerCard  card (kSlot, bus);
        std::vector<Byte>    & ram = card.GetCardMemory().GetRam();



        ram[0x0C05] = 0x33;
        ram[0x7C07] = 0x44;

        card.Read (0xC0D3);
        Assert::AreEqual ((Byte) 3, card.GetBank());
        Assert::AreEqual ((Byte) 0x33, card.ReadExpansionRom (0xC805));
        Assert::AreEqual ((Byte) 0x44, card.ReadExpansionRom (0xCC07));

        card.Write (0xC0D1, 0);
        card.WriteExpansionRom (0xC810, 0x55);
        Assert::AreEqual ((Byte) 0x55, ram[0x0410]);
    }


    //  The SWIM, head select and LED are in the card CPU's page $0A.
    TEST_METHOD (CardMemory_IoPage)
    {
        Apple35CardMemory  memory;



        memory.Write (0x0A81, 0);
        Assert::IsTrue (memory.IsLedOn());
        memory.Write (0x0A80, 0);
        Assert::IsFalse (memory.IsLedOn());

        memory.Read (0x0A41);
        Assert::AreEqual (1, memory.GetDrive (0).GetSide());
        Assert::AreEqual (1, memory.GetDrive (1).GetSide());

        memory.Read (0x0A09);
        Assert::IsTrue (memory.GetIwm().IsEnabled(), L"$0A09 is the SWIM's motor-on switch");
    }


    TEST_METHOD (CardMemory_RejectsAWrongSizeRom)
    {
        Apple35CardMemory  memory;



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH), memory.SetFirmware (std::vector<Byte> (0x4000, 0)));
        Assert::IsFalse (memory.HasFirmware());
    }
};
