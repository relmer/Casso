#include "Pch.h"

#include "Core/MemoryBus.h"
#include "Devices/BlockDevice/AppleScsiCard.h"
#include "Machines/Apple2/Common/ExpansionRomBus.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleScsiCardTests
//
//  The Apple II SCSI Card's map: the 5380 and the card's ports in $C0n0-$C0nF,
//  the ROM's first page at $Cn00, banked RAM at $C800 and banked ROM at
//  $CC00, and the expansion ROM protocol.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleScsiCardTests)
{
public:

    static constexpr int   kSlot   = 7;
    static constexpr Word  kIoBase = 0xC0F0;

    //  ROM byte i of bank b is b, except each bank's first byte, which is
    //  $A0 plus the bank, so a read shows which bank answered.
    static std::vector<Byte> MakeRom()
    {
        std::vector<Byte>  rom (AppleScsiCard::kRomSize);



        for (size_t i = 0; i < rom.size(); i++)
        {
            rom[i] = (Byte) (((i % 0x400) == 0) ? 0xA0 + i / 0x400 : i / 0x400);
        }

        return rom;
    }


    //  THE 5380'S REGISTERS ARE AT $C0n0-$C0n7, the card's ID mask at $C0n9,
    //  and DRQ in bit 7 of $C0nE.
    TEST_METHOD (IoWindow)
    {
        MemoryBus      bus;
        AppleScsiCard  card (kSlot, bus);



        Assert::AreEqual ((Word) kIoBase,        card.GetStart());
        Assert::AreEqual ((Word) (kIoBase + 15), card.GetEnd());

        card.Write (kIoBase + 2, Ncr5380::kModeDma);
        Assert::AreEqual ((Byte) Ncr5380::kModeDma, card.Read (kIoBase + 2));

        Assert::AreEqual ((Byte) 0x80, card.Read (kIoBase + 9));
        Assert::AreEqual ((Byte) 0,    card.Read (kIoBase + 14), L"no DRQ with no transfer");
    }


    //  $C0nB RESETS THE 5380, and $C0nC and $C0nD have no effect.
    TEST_METHOD (ChipResetAndBlockModePorts)
    {
        MemoryBus      bus;
        AppleScsiCard  card (kSlot, bus);



        card.Write (kIoBase + 2, Ncr5380::kModeDma);
        card.Write (kIoBase + 12, 0);
        card.Write (kIoBase + 13, 0);
        Assert::AreEqual ((Byte) Ncr5380::kModeDma, card.Read (kIoBase + 2));

        card.Write (kIoBase + 11, 0);
        Assert::AreEqual ((Byte) 0, card.Read (kIoBase + 2));
    }


    //  THE SLOT PAGE IS THE ROM'S FIRST PAGE and selects the card; the bank
    //  latch's low bits pick the ROM bank at $CC00 and its next bits the RAM
    //  bank at $C800. Reset clears the latch.
    TEST_METHOD (BanksAndSlotPage)
    {
        MemoryBus      bus;
        AppleScsiCard  card (kSlot, bus);



        Assert::AreEqual (S_OK, card.SetFirmware (MakeRom()));

        Assert::AreEqual ((Byte) 0xA0, card.OnSlotPageAccess (0xC700, false, 0));
        Assert::IsTrue   (card.IsExpansionRomSelected());

        card.Write (kIoBase + 10, 0x35);
        Assert::AreEqual ((Byte) 0xA5, card.ReadExpansionRom (0xCC00), L"ROM bank 5");

        card.WriteExpansionRom (0xC810, 0x42);
        card.Write (kIoBase + 10, 0x05);
        Assert::AreEqual ((Byte) 0x00, card.ReadExpansionRom (0xC810), L"RAM bank 0 is a different 1 KB");
        card.Write (kIoBase + 10, 0x35);
        Assert::AreEqual ((Byte) 0x42, card.ReadExpansionRom (0xC810), L"RAM bank 3 kept the byte");

        card.WriteExpansionRom (0xCC00, 0x99);
        Assert::AreEqual ((Byte) 0xA5, card.ReadExpansionRom (0xCC00), L"the ROM ignores writes");

        card.SoftReset();
        Assert::AreEqual ((Byte) 0, card.GetBankLatch());
        Assert::IsFalse  (card.IsExpansionRomSelected());
    }


    //  ON THE EXPANSION BUS, $CF00-$CFFE READ THE CARD'S ROM and only $CFFF
    //  deselects it.
    TEST_METHOD (OnlyCfffDeselects)
    {
        MemoryBus        bus;
        AppleScsiCard    card (kSlot, bus);
        ExpansionRomBus  expansion (bus);



        Assert::AreEqual (S_OK, card.SetFirmware (MakeRom()));
        expansion.AddCard (&card);

        card.OnSlotPageAccess (0xC700, false, 0);
        card.Write (kIoBase + 10, 0x02);

        Assert::AreEqual ((Byte) 0x02, expansion.Read (0xCF10));
        Assert::IsTrue   (card.IsExpansionRomSelected());

        expansion.Write (0xC900, 0x77);
        Assert::AreEqual ((Byte) 0x77, expansion.Read (0xC900), L"the bus passes writes to the card's RAM");

        expansion.Read (0xCFFF);
        Assert::IsFalse  (card.IsExpansionRomSelected());
    }


    //  A ROM OF THE WRONG SIZE IS NOT TAKEN.
    TEST_METHOD (WrongSizeFirmware)
    {
        MemoryBus      bus;
        AppleScsiCard  card (kSlot, bus);
        HRESULT        hr = card.SetFirmware (std::vector<Byte> (100));



        Assert::IsTrue  (FAILED (hr));
        Assert::IsFalse (card.HasFirmware());
    }
};
