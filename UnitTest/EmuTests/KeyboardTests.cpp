#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Devices/AppleKeyboard.h"
#include "Devices/AppleIIeKeyboard.h"
#include "Devices/AppleIIeSoftSwitchBank.h"
#include "Devices/AppleIIeMmu.h"
#include "Devices/RamDevice.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  KeyboardTests
//
//  Adversarial tests proving the keyboard actually works end-to-end.
//  Verifies ASCII encoding, strobe behavior, key overwrite semantics,
//  and bus integration.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (KeyboardTests)
{
public:

    TEST_METHOD (KeyPress_A_ReadsC1WithHighBit)
    {
        // Apple II: 'A' = $41. With strobe bit 7 set: $C1.
        AppleKeyboard kbd;
        kbd.KeyPress ('A');

        Byte val = kbd.Read (0xC000);

        Assert::AreEqual (static_cast<Byte> (0xC1), val,
            L"Press 'A' -> $C000 should return $C1 (ASCII $41 | $80)");
    }

    TEST_METHOD (ReadC010_ClearsStrobeBit7)
    {
        AppleKeyboard kbd;
        kbd.KeyPress ('B');

        // Read $C010 to clear strobe
        kbd.Read (0xC010);

        Byte val = kbd.Read (0xC000);

        Assert::AreEqual (static_cast<Byte> (0x42), val,
            L"After strobe clear, $C000 should return $42 (no high bit)");
        Assert::IsTrue ((val & 0x80) == 0,
            L"Bit 7 should be clear after reading $C010");
    }

    TEST_METHOD (SecondKeyPress_OverwritesFirst)
    {
        // Press 'X', don't clear strobe, press 'Y'.
        // $C000 should show 'Y' with strobe, not 'X'.
        AppleKeyboard kbd;
        kbd.KeyPress ('X');
        kbd.KeyPress ('Y');

        Byte val = kbd.Read (0xC000);

        Assert::AreEqual (static_cast<Byte> ('Y'), static_cast<Byte> (val & 0x7F),
            L"Second key press should overwrite first");
        Assert::IsTrue ((val & 0x80) != 0,
            L"Strobe should still be set after second key press");
    }

    TEST_METHOD (LowercaseConverted_ToUppercase)
    {
        AppleKeyboard kbd;
        kbd.KeyPress ('a');

        Byte val = kbd.Read (0xC000);

        Assert::AreEqual (static_cast<Byte> ('A'), static_cast<Byte> (val & 0x7F),
            L"Lowercase 'a' should be converted to uppercase 'A'");
    }

    TEST_METHOD (AllPrintableASCII_MapCorrectly)
    {
        // Verify all printable ASCII characters produce correct output
        for (Byte ch = 0x20; ch <= 0x5F; ch++)
        {
            AppleKeyboard kbd;
            kbd.KeyPress (ch);

            Byte val = kbd.Read (0xC000);
            Byte asciiPart = val & 0x7F;

            Assert::AreEqual (ch, asciiPart,
                std::format (L"Key ${:02X} should read back as ${:02X}, got ${:02X}",
                    ch, ch, asciiPart).c_str ());
            Assert::IsTrue ((val & 0x80) != 0,
                std::format (L"Key ${:02X} should have strobe set", ch).c_str ());
        }
    }

    TEST_METHOD (ReturnKey_Produces0D)
    {
        AppleKeyboard kbd;
        kbd.KeyPress (0x0D);  // Return/Enter

        Byte val = kbd.Read (0xC000);

        Assert::AreEqual (static_cast<Byte> (0x0D), static_cast<Byte> (val & 0x7F),
            L"Return key should produce $0D");
        Assert::IsTrue ((val & 0x80) != 0,
            L"Return key should have strobe set");
    }

    TEST_METHOD (EscapeKey_Produces1B)
    {
        AppleKeyboard kbd;
        kbd.KeyPress (0x1B);  // Escape

        Byte val = kbd.Read (0xC000);

        Assert::AreEqual (static_cast<Byte> (0x1B), static_cast<Byte> (val & 0x7F),
            L"Escape key should produce $1B");
    }

    TEST_METHOD (NoKeyPressed_NoBit7)
    {
        AppleKeyboard kbd;

        Byte val = kbd.Read (0xC000);

        Assert::IsTrue ((val & 0x80) == 0,
            L"No key pressed: bit 7 should be clear");
    }

    TEST_METHOD (StrobePersists_UntilC010Read)
    {
        AppleKeyboard kbd;
        kbd.KeyPress ('Z');

        // Multiple reads of $C000 should all have strobe set
        Byte val1 = kbd.Read (0xC000);
        Byte val2 = kbd.Read (0xC000);
        Byte val3 = kbd.Read (0xC000);

        Assert::IsTrue ((val1 & 0x80) != 0, L"1st read should have strobe");
        Assert::IsTrue ((val2 & 0x80) != 0, L"2nd read should have strobe");
        Assert::IsTrue ((val3 & 0x80) != 0, L"3rd read should have strobe");

        // Now clear and verify
        kbd.Read (0xC010);
        Byte val4 = kbd.Read (0xC000);
        Assert::IsTrue ((val4 & 0x80) == 0, L"After clear, no strobe");
    }

    TEST_METHOD (KeyPress_ViaBus_ReturnsCorrectValue)
    {
        // Prove keyboard works when accessed through MemoryBus
        MemoryBus bus;
        AppleKeyboard kbd;
        bus.AddDevice (&kbd);

        kbd.KeyPress ('H');
        Byte val = bus.ReadByte (0xC000);

        Assert::AreEqual (static_cast<Byte> (0xC8), val,
            L"'H' via bus should read $C8 at $C000");

        // Clear strobe via bus
        bus.ReadByte (0xC010);
        Byte val2 = bus.ReadByte (0xC000);
        Assert::IsTrue ((val2 & 0x80) == 0,
            L"Strobe should be clear after bus read of $C010");
    }

    TEST_METHOD (SetKeyDown_AffectsC010ReadValue)
    {
        AppleKeyboard kbd;
        kbd.KeyPress ('X');

        // Clear strobe
        kbd.Read (0xC010);

        // With key physically held, $C010 should show bit 7
        kbd.SetKeyDown (true);
        Byte val = kbd.Read (0xC010);
        Assert::IsTrue ((val & 0x80) != 0,
            L"$C010 should show bit 7 when key is physically held");

        kbd.SetKeyDown (false);
        Byte val2 = kbd.Read (0xC010);
        Assert::IsTrue ((val2 & 0x80) == 0,
            L"$C010 should not show bit 7 when key is released");
    }

    TEST_METHOD (IIeKeyboard_UpcastToBase_Works)
    {
        AppleIIeKeyboard iieKbd;
        AppleKeyboard * basePtr = &iieKbd;

        basePtr->KeyPress ('A');
        Byte val = basePtr->Read (0xC000);

        Assert::AreEqual (static_cast<Byte> (0xC1), val,
            L"IIe keyboard upcast should work identically to base");
    }

    TEST_METHOD (IIeKeyboard_ForwardsC001WriteToSoftSwitch)
    {
        MemoryBus              bus;
        RamDevice              mainRam (0x0000, 0xBFFF);
        AppleIIeMmu            mmu;
        AppleIIeSoftSwitchBank sw  (&bus);
        AppleIIeKeyboard       iieKbd (&bus);

        sw.SetMmu (&mmu);
        HRESULT hrInit = mmu.Initialize (&bus, &mainRam, nullptr, nullptr, nullptr, &sw);
        UNREFERENCED_PARAMETER (hrInit);

        iieKbd.SetSoftSwitchSibling (&sw);

        iieKbd.Write (0xC001, 0);

        Assert::IsTrue (sw.Is80Store (),
            L"Write to $C001 via keyboard should reach softswitch and enable 80STORE");
    }

    TEST_METHOD (IIeKeyboard_ForwardsC00DWriteToSoftSwitch)
    {
        MemoryBus              bus;
        AppleIIeSoftSwitchBank sw  (&bus);
        AppleIIeKeyboard       iieKbd (&bus);
        iieKbd.SetSoftSwitchSibling (&sw);

        iieKbd.Write (0xC00D, 0);

        Assert::IsTrue (sw.Is80ColMode (),
            L"Write to $C00D via keyboard should reach softswitch and enable 80COL");
    }

    TEST_METHOD (Reset_ClearsAllState)
    {
        AppleKeyboard kbd;
        kbd.KeyPress ('Z');
        kbd.SetKeyDown (true);

        kbd.Reset ();

        Byte val = kbd.Read (0xC000);

        Assert::AreEqual (static_cast<Byte> (0x00), val,
            L"After Reset, $C000 should return $00 (no key, no strobe)");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Phase 6 / T062 / FR-013, FR-014 / audit §1.2, §4 — modifier keys
    //  ($C061/$C062/$C063) and strobe-clear isolation. The //e keyboard
    //  forwards $C011-$C01F status reads to the soft-switch bank (T061
    //  ownership split); the bank reports bit 7 from the canonical
    //  source and bits 0-6 from the keyboard latch (read-only).
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (OpenAppleReadable_C061)
    {
        AppleIIeKeyboard kbd;

        Byte released = kbd.Read (0xC061);
        Assert::AreEqual (static_cast<Byte> (0x00), released,
            L"$C061 with Open Apple released returns 0");

        kbd.SetOpenApple (true);

        Byte pressed = kbd.Read (0xC061);
        Assert::IsTrue ((pressed & 0x80) != 0,
            L"$C061 bit 7 must be set when Open Apple is pressed");
    }

    TEST_METHOD (ClosedAppleReadable_C062)
    {
        AppleIIeKeyboard kbd;

        Byte released = kbd.Read (0xC062);
        Assert::AreEqual (static_cast<Byte> (0x00), released,
            L"$C062 with Closed Apple released returns 0");

        kbd.SetClosedApple (true);

        Byte pressed = kbd.Read (0xC062);
        Assert::IsTrue ((pressed & 0x80) != 0,
            L"$C062 bit 7 must be set when Closed Apple is pressed");
    }

    TEST_METHOD (ShiftReadable_C063)
    {
        AppleIIeKeyboard kbd;

        Byte released = kbd.Read (0xC063);
        Assert::AreEqual (static_cast<Byte> (0x00), released,
            L"$C063 with Shift released returns 0");

        kbd.SetShift (true);

        Byte pressed = kbd.Read (0xC063);
        Assert::IsTrue ((pressed & 0x80) != 0,
            L"$C063 bit 7 must be set when Shift is pressed");
    }

    TEST_METHOD (OnlyC010ClearsStrobe)
    {
        AppleIIeKeyboard       kbd;
        AppleIIeSoftSwitchBank bank;

        kbd .SetSoftSwitchSibling (&bank);
        bank.SetKeyboard          (&kbd);

        kbd.KeyPress ('A');
        Assert::IsFalse (kbd.IsStrobeClear (), L"Pre-condition: strobe set");

        // Read every status register $C011-$C01F — none should clear strobe.
        for (Word addr = 0xC011; addr <= 0xC01F; ++addr)
        {
            kbd.Read (addr);
            Assert::IsFalse (kbd.IsStrobeClear (),
                L"Reading $C011-$C01F must not clear strobe");
        }

        // Read $C010 — must clear strobe.
        kbd.Read (0xC010);
        Assert::IsTrue (kbd.IsStrobeClear (),
            L"Reading $C010 must clear strobe");
    }

    TEST_METHOD (C011ReadDoesNotClearStrobe)
    {
        AppleIIeKeyboard       kbd;
        AppleIIeSoftSwitchBank bank;

        kbd .SetSoftSwitchSibling (&bank);
        bank.SetKeyboard          (&kbd);
        kbd.KeyPress ('K');

        kbd.Read (0xC011);

        Assert::IsFalse (kbd.IsStrobeClear (),
            L"Reading $C011 (BSRBANK2 status) must not clear strobe");
    }

    TEST_METHOD (C012ReadDoesNotClearStrobe)
    {
        AppleIIeKeyboard       kbd;
        AppleIIeSoftSwitchBank bank;

        kbd .SetSoftSwitchSibling (&bank);
        bank.SetKeyboard          (&kbd);
        kbd.KeyPress ('M');

        kbd.Read (0xC012);

        Assert::IsFalse (kbd.IsStrobeClear (),
            L"Reading $C012 (BSRREADRAM status) must not clear strobe");
    }

    TEST_METHOD (C019ReadDoesNotClearStrobe)
    {
        AppleIIeKeyboard       kbd;
        AppleIIeSoftSwitchBank bank;

        kbd .SetSoftSwitchSibling (&bank);
        bank.SetKeyboard          (&kbd);
        kbd.KeyPress ('Q');

        kbd.Read (0xC019);

        Assert::IsFalse (kbd.IsStrobeClear (),
            L"Reading $C019 (RDVBLBAR) must not clear strobe");
    }

    TEST_METHOD (C01EReadDoesNotClearStrobe)
    {
        AppleIIeKeyboard       kbd;
        AppleIIeSoftSwitchBank bank;

        kbd .SetSoftSwitchSibling (&bank);
        bank.SetKeyboard          (&kbd);
        kbd.KeyPress ('Z');

        kbd.Read (0xC01E);

        Assert::IsFalse (kbd.IsStrobeClear (),
            L"Reading $C01E (RDALTCHAR) must not clear strobe");
    }

    TEST_METHOD (Audit_OpenClosedAppleNoLongerDeadCode)
    {
        // Pre-Phase-6: AppleIIeKeyboard::GetEnd() was $C01F so reads of
        // $C061/$C062 never reached the device — the modifier code was
        // dead. T060 extends GetEnd() to $C063; this test asserts the
        // bus range now covers the modifier reads.
        AppleIIeKeyboard kbd;

        Assert::AreEqual (static_cast<Word> (0xC063), kbd.GetEnd (),
            L"Phase 6 / audit §4 C3: keyboard GetEnd() must reach $C063");

        kbd.SetOpenApple   (true);
        kbd.SetClosedApple (true);

        Assert::IsTrue ((kbd.Read (0xC061) & 0x80) != 0,
            L"$C061 read must reach Open Apple state");
        Assert::IsTrue ((kbd.Read (0xC062) & 0x80) != 0,
            L"$C062 read must reach Closed Apple state");
    }

    TEST_METHOD (Audit_ShiftKeyImplemented)
    {
        // Phase 6 / FR-013: $C063 (Shift) must be a real read site,
        // not stub-zero. SetShift(true) must produce bit 7 = 1.
        AppleIIeKeyboard kbd;

        Assert::AreEqual (static_cast<Byte> (0x00), kbd.Read (0xC063),
            L"$C063 with Shift released returns 0");

        kbd.SetShift (true);

        Byte val = kbd.Read (0xC063);
        Assert::IsTrue ((val & 0x80) != 0,
            L"Phase 6 / FR-013: Shift must be implemented at $C063");
    }
};
