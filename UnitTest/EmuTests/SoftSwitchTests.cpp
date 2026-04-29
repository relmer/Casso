#include "../Casso65Emu/Pch.h"

#include <CppUnitTest.h>

#include "../Casso65Emu/Devices/AppleSoftSwitchBank.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SoftSwitchTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SoftSwitchTests)
{
public:

    TEST_METHOD (Default_TextMode)
    {
        AppleSoftSwitchBank sw;

        Assert::IsFalse (sw.IsGraphicsMode ());
        Assert::IsFalse (sw.IsHiresMode ());
    }

    TEST_METHOD (ReadC050_SetsGraphicsMode)
    {
        AppleSoftSwitchBank sw;

        sw.Read (0xC050);

        Assert::IsTrue (sw.IsGraphicsMode ());
    }

    TEST_METHOD (ReadC051_ClearsGraphicsMode)
    {
        AppleSoftSwitchBank sw;
        sw.Read (0xC050);
        sw.Read (0xC051);

        Assert::IsFalse (sw.IsGraphicsMode ());
    }

    TEST_METHOD (ReadC053_SetsMixedMode)
    {
        AppleSoftSwitchBank sw;

        sw.Read (0xC053);

        Assert::IsTrue (sw.IsMixedMode ());
    }

    TEST_METHOD (ReadC055_SetsPage2)
    {
        AppleSoftSwitchBank sw;

        sw.Read (0xC055);

        Assert::IsTrue (sw.IsPage2 ());
    }

    TEST_METHOD (ReadC057_SetsHiresMode)
    {
        AppleSoftSwitchBank sw;

        sw.Read (0xC057);

        Assert::IsTrue (sw.IsHiresMode ());
    }

    TEST_METHOD (Reset_ClearsAllFlags)
    {
        AppleSoftSwitchBank sw;
        sw.Read (0xC050);
        sw.Read (0xC053);
        sw.Read (0xC055);
        sw.Read (0xC057);

        sw.Reset ();

        Assert::IsFalse (sw.IsGraphicsMode ());
        Assert::IsFalse (sw.IsMixedMode ());
        Assert::IsFalse (sw.IsPage2 ());
        Assert::IsFalse (sw.IsHiresMode ());
    }
};
