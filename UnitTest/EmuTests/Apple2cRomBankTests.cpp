#include "Pch.h"

#include "Core/MemoryBus.h"
#include "Devices/LanguageCard.h"
#include "Devices/Apple2eMmu.h"
#include "Devices/Apple2eSoftSwitchBank.h"
#include "Devices/Apple2cRomBank.h"
#include "Devices/IRomBankSwitch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cRomBankTests
//
//  Unit-tests the Apple //c 32K firmware banking against SYNTHETIC 16K bank
//  images -- no real (copyrighted) //c ROM required, so this runs in CI. The
//  full-ROM cold boot is a separate, ROM-gated integration test.
//
//  Covers: initial bank 0, $C028-driven toggle across $D000-$FFFF, reset to
//  bank 0, and that the //e soft-switch bank forwards $C028 reads AND writes
//  to the IRomBankSwitch hook (and $C028 is inert without one).
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr Word    kLcWindow    = 0xD000;
    constexpr size_t  kBankSize    = 0x4000;      // $C000-$FFFF
    constexpr size_t  kLcOffset    = kLcWindow - 0xC000;  // $1000

    // Build a 16K bank image whose $D000 byte is `marker` (so the active bank
    // is observable via LanguageCard::ReadRom).
    std::vector<Byte> MakeBank (Byte marker)
    {
        std::vector<Byte>   image (kBankSize, 0x00);
        image[kLcOffset] = marker;                 // $D000
        image[kBankSize - 3] = 0x34;               // arbitrary $FFFD
        image[kBankSize - 4] = 0x12;               // arbitrary $FFFC
        return image;
    }

    // Minimal IRomBankSwitch spy for the soft-switch forwarding test.
    struct SpyRomBank : IRomBankSwitch
    {
        int  toggles = 0;
        int  resets  = 0;
        void ToggleRomBank () override { ++toggles; }
        void ResetRomBank  () override { ++resets; }
    };
}


TEST_CLASS (Apple2cRomBankTests)
{
public:

    TEST_METHOD (AppliesBank0OnLoad)
    {
        MemoryBus        bus;
        LanguageCard     lc (bus);
        Apple2eMmu       mmu;
        Apple2cRomBank   romBank (lc, mmu);

        romBank.SetBankImages (MakeBank (0xA0), MakeBank (0xB1));

        Assert::AreEqual<int>  (0,    romBank.CurrentBank ());
        Assert::AreEqual<Byte> (0xA0, lc.ReadRom (kLcWindow));
    }

    TEST_METHOD (ToggleFlipsVisibleBank)
    {
        MemoryBus        bus;
        LanguageCard     lc (bus);
        Apple2eMmu       mmu;
        Apple2cRomBank   romBank (lc, mmu);

        romBank.SetBankImages (MakeBank (0xA0), MakeBank (0xB1));

        romBank.ToggleRomBank ();
        Assert::AreEqual<int>  (1,    romBank.CurrentBank ());
        Assert::AreEqual<Byte> (0xB1, lc.ReadRom (kLcWindow));

        romBank.ToggleRomBank ();
        Assert::AreEqual<int>  (0,    romBank.CurrentBank ());
        Assert::AreEqual<Byte> (0xA0, lc.ReadRom (kLcWindow));
    }

    TEST_METHOD (ResetRestoresBank0)
    {
        MemoryBus        bus;
        LanguageCard     lc (bus);
        Apple2eMmu       mmu;
        Apple2cRomBank   romBank (lc, mmu);

        romBank.SetBankImages (MakeBank (0xA0), MakeBank (0xB1));

        romBank.ToggleRomBank ();                  // -> bank 1
        Assert::AreEqual<int> (1, romBank.CurrentBank ());

        romBank.ResetRomBank ();                   // -> bank 0
        Assert::AreEqual<int>  (0,    romBank.CurrentBank ());
        Assert::AreEqual<Byte> (0xA0, lc.ReadRom (kLcWindow));
    }

    TEST_METHOD (SoftSwitchForwardsC028AccessAndReset)
    {
        Apple2eSoftSwitchBank   ss (nullptr);
        SpyRomBank              spy;

        ss.SetRomBankSwitch (&spy);

        ss.Read (0xC028);                          // any read flips
        Assert::AreEqual (1, spy.toggles);

        ss.Write (0xC028, 0x00);                    // write forwards through Read
        Assert::AreEqual (2, spy.toggles);

        ss.Reset ();                                // /RESET -> bank 0
        Assert::AreEqual (1, spy.resets);
    }

    TEST_METHOD (C028InertWithoutRomBankHook)
    {
        Apple2eSoftSwitchBank   ss (nullptr);       // //e: no ROM banking

        // Must not crash / must be a harmless no-op with no hook attached.
        ss.Read  (0xC028);
        ss.Write (0xC028, 0x00);
        ss.Reset ();
    }
};
