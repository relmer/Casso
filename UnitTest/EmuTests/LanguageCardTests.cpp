#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Devices/LanguageCard.h"
#include "Devices/AppleIIeMmu.h"
#include "Devices/RamDevice.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCardTests
//
//  Adversarial coverage of the audit-correct LC state machine
//  (audit C7/M6/M7/M8) plus aux-LC routing via AppleIIeMmu.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LanguageCardTests)
{
public:

    TEST_METHOD (PowerOnDefaultsToBank2WriteRamPrearmed)
    {
        MemoryBus    bus;
        LanguageCard lc (bus);

        Assert::IsTrue  (lc.IsBank2 (),    L"Power-on must select bank 2");
        Assert::IsFalse (lc.IsReadRam (),  L"Power-on must read ROM");
        Assert::IsTrue  (lc.IsWriteRam (), L"Power-on must pre-arm WRITERAM (audit M8)");
        Assert::AreEqual (0, lc.GetPreWriteCount (),
            L"Power-on pre-write count must be 0");
    }

    TEST_METHOD (ReadC080_EnablesRamRead)
    {
        MemoryBus    bus;
        LanguageCard lc (bus);

        lc.Read (0xC080);

        Assert::IsTrue (lc.IsReadRam (),
            L"$C080 must enable reading from LC RAM");
    }

    TEST_METHOD (ReadD000_RomVisibleByDefault)
    {
        MemoryBus    bus;
        LanguageCard lc (bus);

        vector<Byte> rom (0x3000, 0x00);
        rom[0x0000] = 0xAA;
        rom[0x1000] = 0xBB;
        lc.SetRomData (rom);

        LanguageCardBank bank (lc);

        Assert::AreEqual (static_cast<Byte> (0xAA), bank.Read (0xD000),
            L"Default: $D000 reads ROM data");
        Assert::AreEqual (static_cast<Byte> (0xBB), bank.Read (0xE000),
            L"Default: $E000 reads ROM data");
    }

    TEST_METHOD (ReadRamEnabled_ReturnsLCRam_NotRom)
    {
        MemoryBus    bus;
        LanguageCard lc (bus);

        vector<Byte> rom (0x3000, 0xEA);
        lc.SetRomData (rom);

        lc.Read (0xC080);

        LanguageCardBank bank (lc);

        Assert::AreEqual (static_cast<Byte> (0x00), bank.Read (0xD000),
            L"Read-RAM enabled: $D000 returns LC RAM (zero), not ROM");
    }

    TEST_METHOD (PreWriteRequiresAnyTwoConsecutiveOddReads_NotSameAddress)
    {
        // Audit M6: WRITERAM enables after ANY two odd reads,
        // not necessarily the same address. Casso previously required
        // identical addresses; this test pins the corrected behavior.
        MemoryBus    bus;
        LanguageCard lc (bus);

        lc.Read (0xC082);
        Assert::IsFalse (lc.IsWriteRam (),
            L"Even read must clear WRITERAM and the pre-write arm");

        lc.Read (0xC081);
        Assert::IsFalse (lc.IsWriteRam (),
            L"Single odd read does not enable WRITERAM");

        lc.Read (0xC083);
        Assert::IsTrue (lc.IsWriteRam (),
            L"Two odd reads at DIFFERENT addresses must enable WRITERAM");
    }

    TEST_METHOD (Audit_C81_thenC83_EnablesWrite)
    {
        // The exact cross-address case the old impl failed.
        MemoryBus    bus;
        LanguageCard lc (bus);

        lc.Read (0xC082);
        lc.Read (0xC081);
        lc.Read (0xC083);

        Assert::IsTrue (lc.IsWriteRam (),
            L"Audit M6: $C081 then $C083 must enable WRITERAM");
    }

    TEST_METHOD (DoubleRead_OddSwitch_EnablesWrite_SameAddress)
    {
        MemoryBus    bus;
        LanguageCard lc (bus);

        lc.Read (0xC082);
        lc.Read (0xC081);
        Assert::IsFalse (lc.IsWriteRam (),
            L"Single odd read must NOT enable WRITERAM");

        lc.Read (0xC081);
        Assert::IsTrue (lc.IsWriteRam (),
            L"Two odd reads of the same address must enable WRITERAM");
    }

    TEST_METHOD (EvenAccessClearsPreWrite)
    {
        MemoryBus    bus;
        LanguageCard lc (bus);

        lc.Read (0xC082);
        lc.Read (0xC081);

        lc.Read (0xC082);
        Assert::AreEqual (0, lc.GetPreWriteCount (),
            L"Even read must reset pre-write count");
        Assert::IsFalse (lc.IsWriteRam (),
            L"Even read must clear WRITERAM");
    }

    TEST_METHOD (WriteToOddSwitchResetsPreWriteButPreservesWriteRam)
    {
        // Audit M7: writes to LC switches reset the pre-write counter
        // ONLY; they must not clear an already-sticky WRITERAM bit.
        MemoryBus    bus;
        LanguageCard lc (bus);

        lc.Read  (0xC082);
        lc.Read  (0xC081);
        lc.Read  (0xC081);
        Assert::IsTrue (lc.IsWriteRam (), L"WRITERAM should be set");

        lc.Write (0xC081, 0x00);
        Assert::IsTrue (lc.IsWriteRam (),
            L"Audit M7: write to odd switch must NOT clear sticky WRITERAM");
        Assert::AreEqual (0, lc.GetPreWriteCount (),
            L"Write to odd switch must reset pre-write count");
    }

    TEST_METHOD (Bank2Selected_ByDefault)
    {
        MemoryBus    bus;
        LanguageCard lc (bus);

        lc.Read (0xC080);

        Assert::IsTrue (lc.IsBank2 (),
            L"Bank 2 should be selected by default");
    }

    TEST_METHOD (ReadC088_SelectsBank1)
    {
        MemoryBus    bus;
        LanguageCard lc (bus);

        lc.Read (0xC088);

        Assert::IsFalse (lc.IsBank2 (),
            L"$C088 should select bank 1 (not bank 2)");
    }

    TEST_METHOD (Bank1_WritesIsolatedFromBank2)
    {
        MemoryBus    bus;
        LanguageCard lc (bus);

        lc.Read (0xC082);
        lc.Read (0xC081);
        lc.Read (0xC081);
        lc.WriteRam (0xD000, 0xAA);

        lc.Read (0xC089);
        lc.Read (0xC089);
        lc.WriteRam (0xD000, 0xBB);

        lc.Read (0xC088);
        Byte bank1Val = lc.ReadRam (0xD000);

        lc.Read (0xC080);
        Byte bank2Val = lc.ReadRam (0xD000);

        Assert::AreEqual (static_cast<Byte> (0xBB), bank1Val,
            L"Bank 1 $D000 should contain $BB");
        Assert::AreEqual (static_cast<Byte> (0xAA), bank2Val,
            L"Bank 2 $D000 must remain $AA (banks isolated)");
    }

    TEST_METHOD (UpperRam_E000_FFFF_SharedAcrossBanks)
    {
        MemoryBus    bus;
        LanguageCard lc (bus);

        lc.Read (0xC082);
        lc.Read (0xC081);
        lc.Read (0xC081);
        lc.WriteRam (0xE000, 0xCC);

        lc.Read (0xC089);
        lc.Read (0xC089);

        Assert::AreEqual (static_cast<Byte> (0xCC), lc.ReadRam (0xE000),
            L"$E000 must be shared across banks");
    }

    TEST_METHOD (LanguageCardBank_Write_GoesToLCRam)
    {
        MemoryBus    bus;
        LanguageCard lc (bus);

        lc.Read (0xC082);
        lc.Read (0xC081);
        lc.Read (0xC081);

        LanguageCardBank bank (lc);
        bank.Write (0xE000, 0x42);

        Assert::AreEqual (static_cast<Byte> (0x42), lc.ReadRam (0xE000),
            L"LanguageCardBank::Write must route to LC RAM");
    }

    TEST_METHOD (LanguageCardBank_WriteDisabled_Ignores)
    {
        MemoryBus    bus;
        LanguageCard lc (bus);

        lc.Read (0xC082);
        Assert::IsFalse (lc.IsWriteRam (),
            L"Even read must clear WRITERAM");

        LanguageCardBank bank (lc);
        bank.Write (0xD000, 0xFF);

        lc.Read (0xC080);
        Assert::AreEqual (static_cast<Byte> (0x00), lc.ReadRam (0xD000),
            L"Write must be ignored when WRITERAM is disabled");
    }

    TEST_METHOD (SoftResetPreservesAllLcRamBanks)
    {
        // Audit C7: //e soft reset MUST preserve LC RAM contents.
        MemoryBus    bus;
        LanguageCard lc (bus);

        lc.Read (0xC082);
        lc.Read (0xC081);
        lc.Read (0xC081);
        lc.WriteRam (0xD000, 0xAA);
        lc.WriteRam (0xE000, 0xBB);

        lc.Read (0xC089);
        lc.Read (0xC089);
        lc.WriteRam (0xD000, 0xCC);

        lc.SoftReset ();

        Assert::IsTrue  (lc.IsBank2 (),
            L"SoftReset must restore power-on flag state (BANK2)");
        Assert::IsTrue  (lc.IsWriteRam (),
            L"SoftReset must restore power-on WRITERAM");
        Assert::IsFalse (lc.IsReadRam (),
            L"SoftReset must clear READRAM");

        lc.Read (0xC080);
        Assert::AreEqual (static_cast<Byte> (0xAA), lc.ReadRam (0xD000),
            L"Audit C7: soft reset must preserve bank 2 $D000");
        Assert::AreEqual (static_cast<Byte> (0xBB), lc.ReadRam (0xE000),
            L"Audit C7: soft reset must preserve high RAM");

        lc.Read (0xC088);
        Assert::AreEqual (static_cast<Byte> (0xCC), lc.ReadRam (0xD000),
            L"Audit C7: soft reset must preserve bank 1 $D000");
    }

    TEST_METHOD (Reset_ForwardsToSoftReset_PreservesRam)
    {
        MemoryBus    bus;
        LanguageCard lc (bus);

        lc.Read (0xC082);
        lc.Read (0xC081);
        lc.Read (0xC081);
        lc.WriteRam (0xD000, 0xFF);

        lc.Reset ();

        lc.Read (0xC080);
        Assert::AreEqual (static_cast<Byte> (0xFF), lc.ReadRam (0xD000),
            L"Reset (== SoftReset) must preserve LC RAM");
    }

    TEST_METHOD (ReadRom_WithNoRomData_Returns0xFF)
    {
        MemoryBus    bus;
        LanguageCard lc (bus);

        Assert::AreEqual (static_cast<Byte> (0xFF), lc.ReadRom (0xD000),
            L"ReadRom with no ROM data should return $FF");
    }

    TEST_METHOD (AltZpRoutesLcWindowToAuxBank)
    {
        // Aux LC routing: when MMU's ALTZP is set, LC window must back
        // to a separate aux RAM bank so writes there are isolated from
        // main-bank contents.
        MemoryBus    bus;
        RamDevice    mainRam (0x0000, 0xBFFF);
        AppleIIeMmu  mmu;
        LanguageCard lc (bus);

        HRESULT hr = mmu.Initialize (&bus, &mainRam, nullptr, nullptr, nullptr, nullptr);

        UNREFERENCED_PARAMETER (hr);

        lc.SetMmu (&mmu);

        lc.Read (0xC082);
        lc.Read (0xC081);
        lc.Read (0xC081);
        Assert::IsTrue (lc.IsWriteRam (), L"WRITERAM enabled");

        mmu.SetAltZp (false);
        lc.WriteRam (0xD000, 0x11);

        mmu.SetAltZp (true);
        lc.WriteRam (0xD000, 0x22);

        Assert::AreEqual (static_cast<Byte> (0x22), lc.ReadRam (0xD000),
            L"With ALTZP=1, LC window must route to aux bank ($22)");

        mmu.SetAltZp (false);
        Assert::AreEqual (static_cast<Byte> (0x11), lc.ReadRam (0xD000),
            L"With ALTZP=0, LC window must route to main bank ($11)");
    }

    TEST_METHOD (AltZpAuxAndMain_HighRam_AlsoIsolated)
    {
        MemoryBus    bus;
        RamDevice    mainRam (0x0000, 0xBFFF);
        AppleIIeMmu  mmu;
        LanguageCard lc (bus);

        HRESULT hr = mmu.Initialize (&bus, &mainRam, nullptr, nullptr, nullptr, nullptr);

        UNREFERENCED_PARAMETER (hr);

        lc.SetMmu (&mmu);

        lc.Read (0xC082);
        lc.Read (0xC081);
        lc.Read (0xC081);

        mmu.SetAltZp (false);
        lc.WriteRam (0xE000, 0x77);

        mmu.SetAltZp (true);
        lc.WriteRam (0xE000, 0x88);

        Assert::AreEqual (static_cast<Byte> (0x88), lc.ReadRam (0xE000),
            L"ALTZP=1 high RAM routes to aux");

        mmu.SetAltZp (false);
        Assert::AreEqual (static_cast<Byte> (0x77), lc.ReadRam (0xE000),
            L"ALTZP=0 high RAM routes to main");
    }
};
