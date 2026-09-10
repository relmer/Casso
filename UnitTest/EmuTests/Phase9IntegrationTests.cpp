#include "Pch.h"
#include "TestMachine.h"
#include "FixtureProvider.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Phase9IntegrationTests
//
//  Phase 9 / T088. Verifies slot 6 ROM unshadow (audit C1) end-to-end on
//  the headless //e: when INTCXROM=0, reads in $C600-$C6FF return the
//  Disk2.rom bytes that the CxxxRomRouter holds for slot 6; when
//  INTCXROM=1, the same address falls through to internal ROM.
//
//  Each test attaches Disk2.rom explicitly via mmu->AttachSlotRom (6, ...)
//  rather than relying on the machine build -- the //e monitor's
//  auto-boot scan would otherwise pick up the Disk II signature and divert
//  cold-boot tests in other phases.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Phase9IntegrationTests)
{
public:

    static constexpr Word   kSlot6RomBase     = 0xC600;
    static constexpr Word   kIntCxRomOff      = 0xC006;
    static constexpr Word   kIntCxRomOn       = 0xC007;
    static constexpr Byte   kDisk2RomFirst    = 0xA2;     // first byte of Disk2.rom: LDX #$20

    static HRESULT BuildAndAttachSlot6 (MachineHost & machine)
    {
        HRESULT                hr = S_OK;
        std::vector<uint8_t>   slot6Rom;

        // The machine is built before it arrives, so the only step left that
        // can fail is loading the fixture -- and the attach is gated on it.
        hr = FixtureProvider().OpenFixture ("Disk2.rom", slot6Rom);

        if (SUCCEEDED (hr))
        {
            machine.GetMmu()->AttachSlotRom (6, std::move (slot6Rom));
        }

        return hr;
    }


    TEST_METHOD (Slot6Rom_Unshadowed_WhenIntCxRomOff)
    {
        TestMachine      machine ("Apple2e", TestMachine::Slots::Empty);
        HRESULT           hr;
        Byte              firstByte;

        hr = BuildAndAttachSlot6 (machine);
        AssertSucceeded (hr, L"BuildApple2e + slot 6 ROM attach must succeed");

        machine.GetMemoryBus().WriteByte (kIntCxRomOff, 0);

        firstByte = machine.GetMemoryBus().ReadByte (kSlot6RomBase);
        Assert::AreEqual (
            static_cast<int> (kDisk2RomFirst),
            static_cast<int> (firstByte),
            L"$C600 with INTCXROM=0 must surface Disk2.rom (LDX #$20 = $A2)");
    }


    TEST_METHOD (Slot6Rom_Hidden_WhenIntCxRomOn)
    {
        TestMachine      machine ("Apple2e", TestMachine::Slots::Empty);
        HRESULT           hr;
        Byte              slotByte;
        Byte              internalByte;

        hr = BuildAndAttachSlot6 (machine);
        AssertSucceeded (hr, L"BuildApple2e + slot 6 ROM attach must succeed");

        machine.GetMemoryBus().WriteByte (kIntCxRomOff, 0);
        slotByte = machine.GetMemoryBus().ReadByte (kSlot6RomBase);

        machine.GetMemoryBus().WriteByte (kIntCxRomOn, 0);
        internalByte = machine.GetMemoryBus().ReadByte (kSlot6RomBase);

        Assert::AreNotEqual (
            static_cast<int> (slotByte),
            static_cast<int> (internalByte),
            L"INTCXROM=1 must hide slot 6 ROM and reveal internal ROM");
    }
};


