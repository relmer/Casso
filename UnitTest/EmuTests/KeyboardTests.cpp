#include "Pch.h"
#include "Core/MemoryBus.h"
#include "Devices/AppleKeyboard.h"
#include "Devices/Apple2eKeyboard.h"
#include "Devices/Apple2eSoftSwitchBank.h"
#include "Devices/Apple2eMmu.h"
#include "Devices/RamDevice.h"
#include "Devices/IRomBankSwitch.h"

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
        Apple2eKeyboard iieKbd;
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
        Apple2eMmu             mmu;
        Apple2eSoftSwitchBank  sw  (&bus);
        Apple2eKeyboard        iieKbd (&bus);

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
        Apple2eSoftSwitchBank  sw  (&bus);
        Apple2eKeyboard        iieKbd (&bus);
        iieKbd.SetSoftSwitchSibling (&sw);

        iieKbd.Write (0xC00D, 0);

        Assert::IsTrue (sw.Is80ColMode (),
            L"Write to $C00D via keyboard should reach softswitch and enable 80COL");
    }

    // The Apple //c ROM-bank flip-flop lives at $C028, inside the $C000-$C063
    // window the keyboard front device owns. It must forward that address to
    // the soft-switch bank on both a write (STA) and a read (any access flips
    // the flop). Regression for the //c bank switch that stayed stuck on bank 0.
    TEST_METHOD (IIeKeyboard_ForwardsC028ToRomBankFlipFlop)
    {
        struct CountingRomBank : IRomBankSwitch
        {
            int toggles = 0;
            int resets  = 0;
            void ToggleRomBank() override { ++toggles; }
            void ResetRomBank  () override { ++resets;  }
        };

        MemoryBus              bus;
        Apple2eSoftSwitchBank  sw  (&bus);
        Apple2eKeyboard        iieKbd (&bus);
        CountingRomBank        romBank;

        sw.SetRomBankSwitch (&romBank);
        iieKbd.SetSoftSwitchSibling (&sw);

        iieKbd.Write (0xC028, 0);
        Assert::AreEqual (1, romBank.toggles,
            L"Write $C028 via keyboard should reach the ROM-bank flip-flop once");

        iieKbd.Read (0xC028);
        Assert::AreEqual (2, romBank.toggles,
            L"Read $C028 via keyboard should also flip the ROM-bank flip-flop");
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
        Apple2eKeyboard kbd;

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
        Apple2eKeyboard kbd;

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
        Apple2eKeyboard kbd;

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
        Apple2eKeyboard        kbd;
        Apple2eSoftSwitchBank  bank;

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
        Apple2eKeyboard        kbd;
        Apple2eSoftSwitchBank  bank;

        kbd .SetSoftSwitchSibling (&bank);
        bank.SetKeyboard          (&kbd);
        kbd.KeyPress ('K');

        kbd.Read (0xC011);

        Assert::IsFalse (kbd.IsStrobeClear (),
            L"Reading $C011 (BSRBANK2 status) must not clear strobe");
    }

    TEST_METHOD (C012ReadDoesNotClearStrobe)
    {
        Apple2eKeyboard        kbd;
        Apple2eSoftSwitchBank  bank;

        kbd .SetSoftSwitchSibling (&bank);
        bank.SetKeyboard          (&kbd);
        kbd.KeyPress ('M');

        kbd.Read (0xC012);

        Assert::IsFalse (kbd.IsStrobeClear (),
            L"Reading $C012 (BSRREADRAM status) must not clear strobe");
    }

    TEST_METHOD (C019ReadDoesNotClearStrobe)
    {
        Apple2eKeyboard        kbd;
        Apple2eSoftSwitchBank  bank;

        kbd .SetSoftSwitchSibling (&bank);
        bank.SetKeyboard          (&kbd);
        kbd.KeyPress ('Q');

        kbd.Read (0xC019);

        Assert::IsFalse (kbd.IsStrobeClear (),
            L"Reading $C019 (RDVBLBAR) must not clear strobe");
    }

    TEST_METHOD (C01EReadDoesNotClearStrobe)
    {
        Apple2eKeyboard        kbd;
        Apple2eSoftSwitchBank  bank;

        kbd .SetSoftSwitchSibling (&bank);
        bank.SetKeyboard          (&kbd);
        kbd.KeyPress ('Z');

        kbd.Read (0xC01E);

        Assert::IsFalse (kbd.IsStrobeClear (),
            L"Reading $C01E (RDALTCHAR) must not clear strobe");
    }

    TEST_METHOD (Audit_OpenClosedAppleNoLongerDeadCode)
    {
        // Pre-Phase-6: Apple2eKeyboard::GetEnd() was $C01F so reads of
        // $C061/$C062 never reached the device — the modifier code was
        // dead. T060 extends GetEnd() to $C063; this test asserts the
        // bus range now covers the modifier reads.
        Apple2eKeyboard kbd;

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
        Apple2eKeyboard kbd;

        Assert::AreEqual (static_cast<Byte> (0x00), kbd.Read (0xC063),
            L"$C063 with Shift released returns 0");

        kbd.SetShift (true);

        Byte val = kbd.Read (0xC063);
        Assert::IsTrue ((val & 0x80) != 0,
            L"Phase 6 / FR-013: Shift must be implemented at $C063");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Authentic //e keyboard auto-repeat (driven by AppleKeyboard::Tick
    //  in emulated CPU time). The shell suppresses the host OS repeat and
    //  arms a single character via BeginKeyRepeat; the device regenerates
    //  the real ~500ms delay then ~15 cps cadence, re-arming the $C000
    //  strobe, but only while the key remains physically down.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (AutoRepeat_NoRepeatBeforeDelay)
    {
        AppleKeyboard kbd;

        kbd.KeyPress      ('A');
        kbd.SetKeyDown    (true);
        kbd.BeginKeyRepeat('A');

        // Consume the initial press strobe and register the armed key.
        kbd.Read (0xC010);
        kbd.Tick (1);

        Assert::IsTrue (kbd.IsStrobeClear (),
            L"Registering the armed key must not re-arm the strobe");

        // Advance to just shy of the pre-repeat delay: still no repeat.
        kbd.Tick (AppleKeyboard::kKeyRepeatDelayCycles - 2);

        Assert::IsTrue (kbd.IsStrobeClear (),
            L"No repeat strobe should fire before the initial delay elapses");
    }

    TEST_METHOD (AutoRepeat_StrobeRearmsAfterDelay)
    {
        AppleKeyboard kbd;

        kbd.KeyPress      ('A');
        kbd.SetKeyDown    (true);
        kbd.BeginKeyRepeat('A');

        kbd.Read (0xC010);
        kbd.Tick (1);

        // Cross the initial delay threshold in one accumulation step.
        kbd.Tick (AppleKeyboard::kKeyRepeatDelayCycles);

        Assert::IsFalse (kbd.IsStrobeClear (),
            L"Strobe must re-arm once the initial repeat delay elapses");

        Byte val = kbd.Read (0xC000);
        Assert::AreEqual (static_cast<Byte> ('A'),
            static_cast<Byte> (val & 0x7F),
            L"Auto-repeat must re-latch the held key");
    }

    TEST_METHOD (AutoRepeat_SteadyRateAfterFirstRepeat)
    {
        AppleKeyboard kbd;

        kbd.KeyPress      ('A');
        kbd.SetKeyDown    (true);
        kbd.BeginKeyRepeat('A');

        kbd.Read (0xC010);
        kbd.Tick (1);

        // First repeat after the long initial delay.
        kbd.Tick (AppleKeyboard::kKeyRepeatDelayCycles);
        kbd.Read (0xC010);

        // The faster steady interval (shorter than the initial delay) now
        // governs subsequent repeats.
        Assert::IsTrue (
            AppleKeyboard::kKeyRepeatIntervalCycles <
            AppleKeyboard::kKeyRepeatDelayCycles,
            L"Steady repeat interval must be shorter than the initial delay");

        kbd.Tick (AppleKeyboard::kKeyRepeatIntervalCycles);

        Assert::IsFalse (kbd.IsStrobeClear (),
            L"Strobe must re-arm again after one steady repeat interval");
    }

    TEST_METHOD (AutoRepeat_StopsWhenKeyReleased)
    {
        AppleKeyboard kbd;

        kbd.KeyPress      ('A');
        kbd.SetKeyDown    (true);
        kbd.BeginKeyRepeat('A');

        kbd.Read (0xC010);
        kbd.Tick (1);
        kbd.Tick (AppleKeyboard::kKeyRepeatDelayCycles);
        kbd.Read (0xC010);

        // Physical release: any-key-down clears and the shell disarms the
        // repeat. No further repeats may fire, no matter how long we tick.
        kbd.SetKeyDown    (false);
        kbd.BeginKeyRepeat(0);

        kbd.Tick (AppleKeyboard::kKeyRepeatDelayCycles * 2);

        Assert::IsTrue (kbd.IsStrobeClear (),
            L"Releasing the key must stop auto-repeat");
    }

    TEST_METHOD (AutoRepeat_RequiresKeyPhysicallyDown)
    {
        AppleKeyboard kbd;

        // Armed but never marked physically down: nothing should repeat.
        kbd.KeyPress      ('A');
        kbd.BeginKeyRepeat('A');
        kbd.Read (0xC010);

        kbd.Tick (1);
        kbd.Tick (AppleKeyboard::kKeyRepeatDelayCycles * 2);

        Assert::IsTrue (kbd.IsStrobeClear (),
            L"Auto-repeat must not fire unless the key is physically held");
    }

    TEST_METHOD (AutoRepeat_DisarmStopsRepeat)
    {
        AppleKeyboard kbd;

        kbd.KeyPress      ('A');
        kbd.SetKeyDown    (true);
        kbd.BeginKeyRepeat('A');

        kbd.Read (0xC010);
        kbd.Tick (1);

        // Disarm (arm value 0) before the delay elapses: even with the key
        // still flagged down, no repeat should fire.
        kbd.BeginKeyRepeat(0);
        kbd.Tick (AppleKeyboard::kKeyRepeatDelayCycles * 2);

        Assert::IsTrue (kbd.IsStrobeClear (),
            L"Disarming auto-repeat must suppress further repeats");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Apple //c case switches: the 80/40 column switch (read at $C060,
    //  RD80SW, bit 7) and the keyboard-layout switch (Dvorak, applied to
    //  the typed character stream via MapTypedChar). Both are gated on
    //  SetApple2cMode — dormant on the //e.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (C060_OnIIe_ReadsFloatingZero)
    {
        // Without //c mode, $C060 is unowned — it reads the floating-bus 0,
        // exactly as before, so the //e is unaffected.
        Apple2eKeyboard kbd;

        kbd.SetEightyColumnSwitchIn (true);   // ignored off the //c

        Assert::AreEqual (static_cast<Byte> (0x00), kbd.Read (0xC060),
            L"$C060 must stay 0 on a machine that is not a //c");
    }

    TEST_METHOD (C060_OnIIc_SwitchOut_Reads40Columns)
    {
        // Switch out (up) = 40 columns = bit 7 CLEAR (PEEK 49248 < 128).
        Apple2eKeyboard kbd;

        kbd.SetApple2cMode          (true);
        kbd.SetEightyColumnSwitchIn (false);

        Assert::AreEqual (static_cast<Byte> (0x00), kbd.Read (0xC060),
            L"//c $C060 with the 80/40 switch out (40 cols) must read bit 7 clear");
        Assert::IsFalse (kbd.IsEightyColumnSwitchIn (),
            L"switch state accessor must report 'out'");
    }

    TEST_METHOD (C060_OnIIc_SwitchIn_Reads80Columns)
    {
        // Switch in (down) = 80 columns = bit 7 SET (PEEK 49248 >= 128).
        Apple2eKeyboard kbd;

        kbd.SetApple2cMode          (true);
        kbd.SetEightyColumnSwitchIn (true);

        Assert::AreEqual (static_cast<Byte> (0x80), kbd.Read (0xC060),
            L"//c $C060 with the 80/40 switch in (80 cols) must read bit 7 set");
        Assert::IsTrue (kbd.IsEightyColumnSwitchIn (),
            L"switch state accessor must report 'in'");
    }

    TEST_METHOD (Dvorak_TableMapsRepresentativeKeys)
    {
        // Spot-check the QWERTY-position -> Dvorak-character map across all
        // three letter rows plus the shifted punctuation and the bracket tail.
        Assert::AreEqual<Byte> ('o', Apple2eKeyboard::QwertyToDvorak ('s'), L"s->o");
        Assert::AreEqual<Byte> ('e', Apple2eKeyboard::QwertyToDvorak ('d'), L"d->e");
        Assert::AreEqual<Byte> ('u', Apple2eKeyboard::QwertyToDvorak ('f'), L"f->u");
        Assert::AreEqual<Byte> ('p', Apple2eKeyboard::QwertyToDvorak ('r'), L"r->p");
        Assert::AreEqual<Byte> ('j', Apple2eKeyboard::QwertyToDvorak ('c'), L"c->j");
        Assert::AreEqual<Byte> ('m', Apple2eKeyboard::QwertyToDvorak ('m'), L"m->m (unchanged)");
        Assert::AreEqual<Byte> ('R', Apple2eKeyboard::QwertyToDvorak ('O'), L"O->R");
        Assert::AreEqual<Byte> ('_', Apple2eKeyboard::QwertyToDvorak ('"'), L"double-quote->underscore");
        Assert::AreEqual<Byte> ('[', Apple2eKeyboard::QwertyToDvorak ('-'), L"minus->left-bracket");
        Assert::AreEqual<Byte> (']', Apple2eKeyboard::QwertyToDvorak ('='), L"equals->right-bracket");
    }

    TEST_METHOD (Dvorak_DigitsAndControlCodesPassThrough)
    {
        // Digits and control codes are in identical positions in both layouts.
        for (Byte ch = '0'; ch <= '9'; ++ch)
        {
            Assert::AreEqual (ch, Apple2eKeyboard::QwertyToDvorak (ch),
                L"digits must pass through the Dvorak map unchanged");
        }

        Assert::AreEqual<Byte> (0x0D, Apple2eKeyboard::QwertyToDvorak (0x0D), L"Return unchanged");
        Assert::AreEqual<Byte> (0x1B, Apple2eKeyboard::QwertyToDvorak (0x1B), L"Escape unchanged");
        Assert::AreEqual<Byte> (' ',  Apple2eKeyboard::QwertyToDvorak (' '),  L"Space unchanged");
    }

    TEST_METHOD (MapTypedChar_PassthroughWhenSwitchDisengaged)
    {
        // //c mode but the keyboard switch is out (QWERTY): no remap.
        Apple2eKeyboard kbd;

        kbd.SetApple2cMode          (true);
        kbd.SetKeyboardSwitchDvorak (false);

        Assert::AreEqual<Byte> ('s', kbd.MapTypedChar ('s'),
            L"QWERTY (switch out) must not remap the typed character");
    }

    TEST_METHOD (MapTypedChar_PassthroughOnIIeEvenWithSwitchSet)
    {
        // The switch flag is meaningless off the //c: never remap.
        Apple2eKeyboard kbd;

        kbd.SetKeyboardSwitchDvorak (true);   // no //c mode

        Assert::AreEqual<Byte> ('s', kbd.MapTypedChar ('s'),
            L"a non-//c keyboard must never Dvorak-remap");
    }

    TEST_METHOD (MapTypedChar_RemapsWhenSwitchEngagedOnIIc)
    {
        Apple2eKeyboard kbd;

        kbd.SetApple2cMode          (true);
        kbd.SetKeyboardSwitchDvorak (true);

        Assert::AreEqual<Byte> ('o', kbd.MapTypedChar ('s'),
            L"//c with the keyboard switch in must remap 's' to Dvorak 'o'");
    }

    TEST_METHOD (Dvorak_TypedThroughLatch_ReadsRemappedCharacter)
    {
        // End-to-end: the shell would feed MapTypedChar's result to KeyPress.
        // With the switch engaged, physically typing 'k' latches Dvorak 't'.
        Apple2eKeyboard kbd;

        kbd.SetApple2cMode          (true);
        kbd.SetKeyboardSwitchDvorak (true);

        kbd.KeyPress (kbd.MapTypedChar ('k'));

        Assert::AreEqual<Byte> ('T', static_cast<Byte> (kbd.Read (0xC000) & 0x7F),
            L"typing 'k' with the Dvorak switch in must latch 't' (upcased at $C000)");
    }
};
