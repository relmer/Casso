#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Devices/RamDevice.h"
#include "Devices/AppleIIeMmu.h"
#include "Devices/AppleIIeSoftSwitchBank.h"
#include "Devices/AppleIIeKeyboard.h"
#include "Devices/LanguageCard.h"
#include "Video/VideoTiming.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SoftSwitchReadSurfaceTests
//
//  Phase 6 / T065 / FR-001 / FR-003 / FR-014 / audit §1.2 — full per-
//  address coverage of the //e $C011-$C01F status read surface. Each
//  test asserts:
//    (a) bit 7 reflects the canonical source device's state,
//    (b) bits 0-6 mirror the keyboard latch (floating-bus convention),
//    (c) the read does not clear the keyboard strobe (audit §1.2),
//    (d) the read does not perturb any other observable state.
//
//  Fixture wires the full //e back-end (bus, MMU, LC, bank, keyboard,
//  video timing) so the bank's ReadStatusRegister sees real sources.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    struct SurfaceFixture
    {
        MemoryBus              bus;
        RamDevice              mainRam;
        AppleIIeMmu            mmu;
        AppleIIeSoftSwitchBank bank;
        AppleIIeKeyboard       keyboard;
        LanguageCard           lc;
        VideoTiming            vt;

        SurfaceFixture ()
            : bus      (),
              mainRam  (0x0000, 0xBFFF),
              mmu      (),
              bank     (&bus),
              keyboard (&bus),
              lc       (bus),
              vt       ()
        {
            HRESULT  hr = S_OK;

            bank.SetMmu          (&mmu);
            bank.SetKeyboard     (&keyboard);
            bank.SetLanguageCard (&lc);
            bank.SetVideoTiming  (&vt);

            keyboard.SetSoftSwitchSibling (&bank);
            keyboard.SetMmu               (&mmu);
            keyboard.SetLanguageCard      (&lc);
            keyboard.SetVideoTiming       (&vt);

            lc.SetMmu (&mmu);

            hr = mmu.Initialize (&bus, &mainRam, nullptr, nullptr, nullptr, &bank);
            UNREFERENCED_PARAMETER (hr);
        }
    };

    void SetCanonical (SurfaceFixture & f, Word address, bool flag)
    {
        switch (address)
        {
            case 0xC011: // BSRBANK2
                f.lc.Read (flag ? 0xC083 : 0xC08B);
                break;
            case 0xC012: // BSRREADRAM
                f.lc.Read (flag ? 0xC080 : 0xC081);
                break;
            case 0xC013: f.mmu.SetRamRd      (flag); break;
            case 0xC014: f.mmu.SetRamWrt     (flag); break;
            case 0xC015: f.mmu.SetIntCxRom   (flag); break;
            case 0xC016: f.mmu.SetAltZp      (flag); break;
            case 0xC017: f.mmu.SetSlotC3Rom  (flag); break;
            case 0xC018: f.mmu.Set80Store    (flag); break;
            case 0xC019:
                // RDVBLBAR: bit7 = 1 during DISPLAY (NOT vblank).
                // flag=true => display => not in vblank => cycle low.
                if (flag)
                {
                    f.vt.Reset ();
                }
                else
                {
                    f.vt.Tick (VideoTiming::kVblankStartCycle);
                }
                break;
            case 0xC01A: f.bank.Read (flag ? 0xC051 : 0xC050); break; // RDTEXT
            case 0xC01B: f.bank.Read (flag ? 0xC053 : 0xC052); break; // RDMIXED
            case 0xC01C: f.bank.Read (flag ? 0xC055 : 0xC054); break; // RDPAGE2
            case 0xC01D: f.bank.Read (flag ? 0xC057 : 0xC056); break; // RDHIRES
            case 0xC01E: f.bank.Read (flag ? 0xC00F : 0xC00E); break; // RDALTCHAR
            case 0xC01F: f.bank.Read (flag ? 0xC00D : 0xC00C); break; // RD80VID
            default:     break;
        }
    }
}



TEST_CLASS (SoftSwitchReadSurfaceTests)
{
public:

    void AssertStatusRead (Word address, const wchar_t * label)
    {
        SurfaceFixture  f;
        Byte            valLowFlag    = 0;
        Byte            valHighFlag   = 0;

        // Latch a known key (bits 0-6 = 'X' & 0x7F = $58) so we can
        // verify the floating-bus low 7 bits and the strobe isolation.
        f.keyboard.KeyPress ('X');
        Assert::IsFalse (f.keyboard.IsStrobeClear (), label);

        // Flag = false. (a) bit 7 = 0 expected, (b) bits 0-6 = 'X'.
        SetCanonical (f, address, false);
        valLowFlag = f.bank.Read (address);

        Assert::AreEqual (static_cast<Byte> (0x00),
            static_cast<Byte> (valLowFlag & 0x80),
            L"Bit 7 must be clear when canonical source is false");
        Assert::AreEqual (static_cast<Byte> ('X'),
            static_cast<Byte> (valLowFlag & 0x7F),
            L"Bits 0-6 must mirror keyboard latch (floating bus)");

        // (c) read did not clear strobe.
        Assert::IsFalse (f.keyboard.IsStrobeClear (),
            L"Status read must NOT clear keyboard strobe (audit §1.2)");

        // Flag = true. (a) bit 7 = 1 expected.
        SetCanonical (f, address, true);
        valHighFlag = f.bank.Read (address);

        Assert::AreEqual (static_cast<Byte> (0x80),
            static_cast<Byte> (valHighFlag & 0x80),
            L"Bit 7 must be set when canonical source is true");
        Assert::AreEqual (static_cast<Byte> ('X'),
            static_cast<Byte> (valHighFlag & 0x7F),
            L"Bits 0-6 unchanged across canonical-source toggle");

        // (d) issuing the read multiple times does not perturb keyboard.
        f.bank.Read (address);
        f.bank.Read (address);
        Assert::IsFalse (f.keyboard.IsStrobeClear (),
            L"Repeated status reads must not perturb keyboard state");
        Assert::AreEqual (static_cast<Byte> ('X'),
            f.keyboard.GetLatchedKeyDataBits (),
            L"Keyboard latch data bits unchanged after status reads");
    }

    TEST_METHOD (BSRBANK2_C011)    { AssertStatusRead (0xC011, L"$C011"); }
    TEST_METHOD (BSRREADRAM_C012)  { AssertStatusRead (0xC012, L"$C012"); }
    TEST_METHOD (RDRAMRD_C013)     { AssertStatusRead (0xC013, L"$C013"); }
    TEST_METHOD (RDRAMWRT_C014)    { AssertStatusRead (0xC014, L"$C014"); }
    TEST_METHOD (RDINTCXROM_C015)  { AssertStatusRead (0xC015, L"$C015"); }
    TEST_METHOD (RDALTZP_C016)     { AssertStatusRead (0xC016, L"$C016"); }
    TEST_METHOD (RDSLOTC3ROM_C017) { AssertStatusRead (0xC017, L"$C017"); }
    TEST_METHOD (RD80STORE_C018)   { AssertStatusRead (0xC018, L"$C018"); }
    TEST_METHOD (RDVBLBAR_C019)    { AssertStatusRead (0xC019, L"$C019"); }
    TEST_METHOD (RDTEXT_C01A)      { AssertStatusRead (0xC01A, L"$C01A"); }
    TEST_METHOD (RDMIXED_C01B)     { AssertStatusRead (0xC01B, L"$C01B"); }
    TEST_METHOD (RDPAGE2_C01C)     { AssertStatusRead (0xC01C, L"$C01C"); }
    TEST_METHOD (RDHIRES_C01D)     { AssertStatusRead (0xC01D, L"$C01D"); }
    TEST_METHOD (RDALTCHAR_C01E)   { AssertStatusRead (0xC01E, L"$C01E"); }
    TEST_METHOD (RD80VID_C01F)     { AssertStatusRead (0xC01F, L"$C01F"); }
};
