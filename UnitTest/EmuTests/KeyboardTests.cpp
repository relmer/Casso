#include "../Casso65Emu/Pch.h"

#include <CppUnitTest.h>

#include "../Casso65Emu/Core/MemoryBus.h"
#include "../Casso65Emu/Devices/AppleKeyboard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  KeyboardTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (KeyboardTests)
{
public:

    TEST_METHOD (KeyPress_SetsBit7Strobe)
    {
        AppleKeyboard kbd;

        kbd.KeyPress ('A');
        Byte val = kbd.Read (0xC000);

        Assert::IsTrue ((val & 0x80) != 0);
        Assert::AreEqual (static_cast<Byte> ('A'), static_cast<Byte> (val & 0x7F));
    }

    TEST_METHOD (ReadC010_ClearsStrobe)
    {
        AppleKeyboard kbd;

        kbd.KeyPress ('B');
        kbd.Read (0xC010);

        Byte val = kbd.Read (0xC000);

        Assert::IsTrue ((val & 0x80) == 0);
    }

    TEST_METHOD (LowercaseInput_ConvertedToUppercase)
    {
        AppleKeyboard kbd;

        kbd.KeyPress ('a');
        Byte val = kbd.Read (0xC000);

        Assert::AreEqual (static_cast<Byte> ('A'), static_cast<Byte> (val & 0x7F));
    }

    TEST_METHOD (NoKeyPressed_NoBit7)
    {
        AppleKeyboard kbd;

        Byte val = kbd.Read (0xC000);

        Assert::IsTrue ((val & 0x80) == 0);
    }
};
