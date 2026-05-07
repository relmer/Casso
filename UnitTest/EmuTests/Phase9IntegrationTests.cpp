#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "HeadlessHost.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


namespace
{
    static constexpr Word   kSlot6RomBase     = 0xC600;
    static constexpr Word   kIntCxRomOff      = 0xC006;
    static constexpr Word   kIntCxRomOn       = 0xC007;
    static constexpr Byte   kDisk2RomFirst    = 0xA2;     // first byte of disk2.rom: LDX #$20
}





////////////////////////////////////////////////////////////////////////////////
//
//  Phase9IntegrationTests
//
//  Phase 9 / T088. Verifies slot 6 ROM unshadow (audit C1) end-to-end on
//  the headless //e: when INTCXROM=0, reads in $C600-$C6FF return the
//  disk2.rom bytes that the CxxxRomRouter holds for slot 6; when
//  INTCXROM=1, the same address falls through to internal ROM.
//
//  Each test attaches disk2.rom explicitly via mmu->AttachSlotRom (6, ...)
//  rather than relying on HeadlessHost::BuildAppleIIe -- the //e monitor's
//  auto-boot scan would otherwise pick up the Disk II signature and divert
//  cold-boot tests in other phases.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Phase9IntegrationTests)
{
public:

    static HRESULT BuildAndAttachSlot6 (HeadlessHost & host, EmulatorCore & core)
    {
        HRESULT                hr;
        std::vector<uint8_t>   slot6Rom;

        hr = host.BuildAppleIIe (core);
        if (FAILED (hr))
        {
            return hr;
        }

        hr = core.fixtures->OpenFixture ("disk2.rom", slot6Rom);
        if (FAILED (hr))
        {
            return hr;
        }

        core.mmu->AttachSlotRom (6, std::move (slot6Rom));
        return S_OK;
    }


    TEST_METHOD (Slot6Rom_Unshadowed_WhenIntCxRomOff)
    {
        HeadlessHost      host;
        EmulatorCore      core;
        HRESULT           hr;
        Byte              firstByte;

        hr = BuildAndAttachSlot6 (host, core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildAppleIIe + slot 6 ROM attach must succeed");

        core.bus->WriteByte (kIntCxRomOff, 0);

        firstByte = core.bus->ReadByte (kSlot6RomBase);
        Assert::AreEqual (
            static_cast<int> (kDisk2RomFirst),
            static_cast<int> (firstByte),
            L"$C600 with INTCXROM=0 must surface disk2.rom (LDX #$20 = $A2)");
    }


    TEST_METHOD (Slot6Rom_Hidden_WhenIntCxRomOn)
    {
        HeadlessHost      host;
        EmulatorCore      core;
        HRESULT           hr;
        Byte              slotByte;
        Byte              internalByte;

        hr = BuildAndAttachSlot6 (host, core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildAppleIIe + slot 6 ROM attach must succeed");

        core.bus->WriteByte (kIntCxRomOff, 0);
        slotByte = core.bus->ReadByte (kSlot6RomBase);

        core.bus->WriteByte (kIntCxRomOn, 0);
        internalByte = core.bus->ReadByte (kSlot6RomBase);

        Assert::AreNotEqual (
            static_cast<int> (slotByte),
            static_cast<int> (internalByte),
            L"INTCXROM=1 must hide slot 6 ROM and reveal internal ROM");
    }
};

