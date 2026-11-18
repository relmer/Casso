#include "Pch.h"

#include "Apple3TestFixture.h"
#include "Devices/Via6522.h"
#include "Machines/Apple3/Apple3Keyboard.h"
#include "Machines/Apple3/Apple3SystemVias.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3KeyboardTests
//
//  The keyboard: the stroke for every character the encoder can send,
//  checked against the table on SVC page 8.6; the special keys; the KA and
//  KB ports; the strobe; the repeat rates; and the interrupt on the E VIA.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3KeyboardTests)
{
public:

    static constexpr Word  kKa      = 0xC000;
    static constexpr Word  kKb      = 0xC008;
    static constexpr Word  kClear   = 0xC010;
    static constexpr Byte  kStrobe  = 0x80;
    static constexpr Byte  kAllUp   = Apple3Keyboard::kKbShift | Apple3Keyboard::kKbControl | Apple3Keyboard::kKbAlphaLock |
                                      Apple3Keyboard::kKbApple1 | Apple3Keyboard::kKbApple2;


    //  SVC page 8.6, every key that sends a code and is not a special key:
    //  alone, with Shift, with Control, with both. Shift-Control-6 is
    //  printed $53; the table here has $1E.
    struct KeyRow
    {
        Byte  alone;
        Byte  shift;
        Byte  control;
        Byte  both;
    };

    static std::vector<KeyRow> GetTable()
    {
        std::vector<KeyRow>  rows =
        {
            { 0x31, 0x21, 0x31, 0x31 }, { 0x32, 0x40, 0x32, 0x00 }, { 0x33, 0x23, 0x33, 0x23 },
            { 0x34, 0x24, 0x34, 0x24 }, { 0x35, 0x25, 0x35, 0x25 }, { 0x36, 0x5E, 0x36, 0x1E },
            { 0x37, 0x26, 0x37, 0x26 }, { 0x38, 0x2A, 0x38, 0x2A }, { 0x39, 0x28, 0x39, 0x28 },
            { 0x30, 0x29, 0x30, 0x29 }, { 0x2D, 0x5F, 0x2D, 0x1F }, { 0x3D, 0x2B, 0x3D, 0x2B },
            { 0x5C, 0x7C, 0x7F, 0x1C }, { 0x5B, 0x7B, 0x1B, 0x1B }, { 0x5D, 0x7D, 0x1D, 0x1D },
            { 0x60, 0x7E, 0x60, 0x7E }, { 0x3B, 0x3A, 0x3B, 0x3A }, { 0x27, 0x22, 0x27, 0x22 },
            { 0x0D, 0x0D, 0x0D, 0x0D }, { 0x2C, 0x3C, 0x2C, 0x3C }, { 0x2E, 0x3E, 0x2E, 0x3E },
            { 0x2F, 0x3F, 0x2F, 0x3F },
        };



        for (Byte letter = 'A'; letter <= 'Z'; letter++)
        {
            rows.push_back ({ letter, letter, (Byte) (letter - 0x40), (Byte) (letter - 0x40) });
        }

        return rows;
    }


    //  The character a key and its modifiers send, with SOS's case rule
    //  for letters: lowercase unless Shift is down.
    static int GetTypedChar (const std::vector<KeyRow> & table, const Apple3Keyboard::Stroke & stroke)
    {
        int  typed = -1;



        for (const KeyRow & row : table)
        {
            if (row.alone == stroke.code)
            {
                typed = stroke.isControl ? (stroke.isShift ? row.both : row.control)
                                         : (stroke.isShift ? row.shift : row.alone);

                if (!stroke.isControl && !stroke.isShift && row.alone >= 'A' && row.alone <= 'Z')
                {
                    typed = row.alone + 0x20;
                }
            }
        }

        return typed;
    }


    TEST_METHOD (EveryCharacter_HasAStrokeThatSendsIt)
    {
        std::vector<KeyRow>  table = GetTable();



        for (int c = 0; c < 0x80; c++)
        {
            Apple3Keyboard::Stroke  stroke = Apple3Keyboard::GetStrokeForChar ((Byte) c);
            std::wstring            label  = std::format (L"character ${:02X}", c);



            Assert::IsTrue (stroke.isValid, label.c_str());

            if (c == ' ')
            {
                Assert::AreEqual ((Byte) 0xA0, stroke.code, label.c_str());
            }
            else
            {
                Assert::AreEqual (c, GetTypedChar (table, stroke), label.c_str());
            }
        }
    }


    TEST_METHOD (TypedLetter_LatchesTheUppercaseCode_AndShiftFollowsTheCase)
    {
        Apple3Keyboard  keyboard;



        keyboard.PressKey ('a');
        Assert::AreEqual ((Byte) (0x41 | kStrobe), keyboard.Read (kKa));
        Assert::AreEqual (kAllUp, keyboard.Read (kKb));

        keyboard.PressKey ('A');
        Assert::AreEqual ((Byte) (0x41 | kStrobe), keyboard.Read (kKa));
        Assert::AreEqual ((Byte) (kAllUp & ~Apple3Keyboard::kKbShift), keyboard.Read (kKb));
    }


    TEST_METHOD (TypedControlCharacter_ReportsControl)
    {
        Apple3Keyboard  keyboard;



        keyboard.PressKey (0x08);

        Assert::AreEqual ((Byte) (0x08 | kStrobe), keyboard.Read (kKa));
        Assert::AreEqual ((Byte) (kAllUp & ~Apple3Keyboard::kKbControl), keyboard.Read (kKb),
                          L"Control-H is not the left arrow: bit 7 is clear");
    }


    TEST_METHOD (SpecialKeys_LatchTheirCodesWithBit7OnTheKbPort)
    {
        struct Case { AppleSpecialKey key; Byte code; };

        Apple3Keyboard  keyboard;



        for (const Case & each : { Case { AppleSpecialKey::Left,   0x08 }, Case { AppleSpecialKey::Right, 0x15 },
                                   Case { AppleSpecialKey::Up,     0x0B }, Case { AppleSpecialKey::Down,  0x0A },
                                   Case { AppleSpecialKey::Tab,    0x09 }, Case { AppleSpecialKey::Escape, 0x1B } })
        {
            Assert::AreEqual (each.code, keyboard.PressSpecialKey (each.key));
            Assert::AreEqual ((Byte) (each.code | kStrobe), keyboard.Read (kKa));
            Assert::AreEqual ((Byte) (kAllUp | Apple3Keyboard::kKbSpecial), keyboard.Read (kKb));
        }
    }


    TEST_METHOD (Delete_IsNotAKey)
    {
        Apple3Keyboard  keyboard;



        Assert::AreEqual ((Byte) 0, keyboard.PressSpecialKey (AppleSpecialKey::Delete));
        Assert::AreEqual ((Byte) 0, (Byte) (keyboard.Read (kKa) & kStrobe));
    }


    TEST_METHOD (Space_IsASpecialKey)
    {
        Apple3Keyboard  keyboard;



        keyboard.PressKey (' ');

        Assert::AreEqual ((Byte) (0x20 | kStrobe), keyboard.Read (kKa));
        Assert::AreEqual ((Byte) (kAllUp | Apple3Keyboard::kKbSpecial), keyboard.Read (kKb));
    }


    TEST_METHOD (KeypadStroke_LatchesItsCode)
    {
        Apple3Keyboard  keyboard;



        keyboard.PressStroke ({ 0xB5, false, false, true });

        Assert::AreEqual ((Byte) (0x35 | kStrobe), keyboard.Read (kKa));
        Assert::AreEqual ((Byte) (kAllUp | Apple3Keyboard::kKbSpecial), keyboard.Read (kKb));
    }


    TEST_METHOD (KbPort_ReportsEachModifierLow)
    {
        Apple3Keyboard  keyboard;



        keyboard.SetKeyDown (true);
        Assert::AreEqual ((Byte) (kAllUp | Apple3Keyboard::kKbAnyKey), keyboard.Read (kKb));
        keyboard.SetKeyDown (false);

        keyboard.SetShift (true);
        Assert::AreEqual ((Byte) (kAllUp & ~Apple3Keyboard::kKbShift), keyboard.Read (kKb));
        keyboard.SetShift (false);

        keyboard.SetControl (true);
        Assert::AreEqual ((Byte) (kAllUp & ~Apple3Keyboard::kKbControl), keyboard.Read (kKb));
        keyboard.SetControl (false);

        keyboard.SetAlphaLock (true);
        Assert::AreEqual ((Byte) (kAllUp & ~Apple3Keyboard::kKbAlphaLock), keyboard.Read (kKb));
        keyboard.SetAlphaLock (false);

        keyboard.SetApple1 (true);
        Assert::AreEqual ((Byte) (kAllUp & ~Apple3Keyboard::kKbApple1), keyboard.Read (kKb));
        keyboard.SetApple1 (false);

        keyboard.SetApple2 (true);
        Assert::AreEqual ((Byte) (kAllUp & ~Apple3Keyboard::kKbApple2), keyboard.Read (kKb));
    }


    TEST_METHOD (Strobe_IsClearedByC01x_AndKaRepeatsThroughC007)
    {
        Apple3Keyboard  keyboard;



        keyboard.PressKey ('Q');

        Assert::AreEqual ((Byte) (0x51 | kStrobe), keyboard.Read (0xC007));
        Assert::AreEqual ((Byte) (0x51 | kStrobe), keyboard.Read (kKa), L"A KA read doesn't clear the flag");

        keyboard.Read (0xC01F);
        Assert::AreEqual ((Byte) 0x51, keyboard.Read (kKa));
    }


    TEST_METHOD (Repeat_IsTenPerSecond_OrThirtyWithApple2)
    {
        Apple3Keyboard  keyboard;
        Byte            fired = 0;



        keyboard.PressKey ('Z');
        keyboard.BeginKeyRepeat ('Z');
        keyboard.SetKeyDown (true);

        keyboard.TickAutoRepeat (0);
        keyboard.TickAutoRepeat (AppleKeyboard::kKeyRepeatDelayUs);
        keyboard.Read (kClear);

        fired = keyboard.TickAutoRepeat (99'999);
        Assert::AreEqual ((Byte) 0, fired);

        fired = keyboard.TickAutoRepeat (1);
        Assert::AreEqual ((Byte) 'Z', fired);

        keyboard.SetApple2 (true);

        fired = keyboard.TickAutoRepeat (33'332);
        Assert::AreEqual ((Byte) 0, fired);

        fired = keyboard.TickAutoRepeat (1);
        Assert::AreEqual ((Byte) 'Z', fired);
    }


    TEST_METHOD (Repeat_KeepsTheSpecialKeysBit)
    {
        Apple3Keyboard  keyboard;



        keyboard.PressSpecialKey (AppleSpecialKey::Left);
        keyboard.BeginKeyRepeat (0x08);
        keyboard.SetKeyDown (true);

        keyboard.TickAutoRepeat (0);
        keyboard.TickAutoRepeat (AppleKeyboard::kKeyRepeatDelayUs);

        Assert::AreEqual ((Byte) (0x08 | kStrobe), keyboard.Read (kKa));
        Assert::AreEqual (Apple3Keyboard::kKbSpecial, (Byte) (keyboard.Read (kKb) & Apple3Keyboard::kKbSpecial));
    }


    TEST_METHOD (DataReady_InterruptsThroughTheEViaCa2)
    {
        struct Rig : Apple3TestFixture
        {
            Apple3SystemVias  systemVias;
            Apple3Keyboard    keyboard;

            Rig() : systemVias (mmu) {}
        };

        Rig  rig;



        rig.keyboard.AttachVias (&rig.systemVias);
        rig.systemVias.GetE().WriteRegister (Via6522::kRegPcr, (Byte) (Via6522::kC2InFallingIndependent << Via6522::kPcrCa2Shift));

        rig.keyboard.PressKey ('K');
        rig.keyboard.UpdateInterruptLine();
        Assert::AreEqual ((int) Via6522::kIrqCa2, (int) (rig.systemVias.GetE().GetIfr() & Via6522::kIrqCa2));

        rig.systemVias.GetE().WriteRegister (Via6522::kRegIfr, Via6522::kIrqCa2);
        rig.keyboard.Read (kClear);

        rig.keyboard.PressKey ('L');
        rig.keyboard.UpdateInterruptLine();
        Assert::AreEqual ((int) Via6522::kIrqCa2, (int) (rig.systemVias.GetE().GetIfr() & Via6522::kIrqCa2),
                          L"Clearing the flag raises the line, so the next key is a new edge");
    }
};
