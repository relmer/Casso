#include "../Casso65Emu/Pch.h"

#include <CppUnitTest.h>

#include "../Casso65Emu/Core/MemoryBus.h"
#include "../Casso65Emu/Devices/LanguageCard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCardTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LanguageCardTests)
{
public:

    TEST_METHOD (InitialState_RomVisible)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        Assert::IsFalse (lc.IsReadRam ());
        Assert::IsFalse (lc.IsWriteRam ());
    }

    TEST_METHOD (ReadC080_EnablesRamRead)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        lc.Read (0xC080);

        Assert::IsTrue (lc.IsReadRam ());
    }

    TEST_METHOD (DoubleReadC081_EnablesRamWrite)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        lc.Read (0xC081);
        lc.Read (0xC081);

        Assert::IsTrue (lc.IsWriteRam ());
    }

    TEST_METHOD (SingleReadC081_DoesNotEnableWrite)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        lc.Read (0xC081);

        Assert::IsFalse (lc.IsWriteRam ());
    }

    TEST_METHOD (Bank2Selected_ByDefault)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        lc.Read (0xC080);

        Assert::IsTrue (lc.IsBank2 ());
    }

    TEST_METHOD (ReadC088_SelectsBank1)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        lc.Read (0xC088);

        Assert::IsFalse (lc.IsBank2 ());
    }

    TEST_METHOD (WriteRam_RequiresWriteEnable)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        // Enable write
        lc.Read (0xC081);
        lc.Read (0xC081);

        // Write to Language Card RAM
        lc.WriteRam (0xD000, 0xAB);
        Byte val = lc.ReadRam (0xD000);

        Assert::AreEqual (static_cast<Byte> (0xAB), val);
    }

    TEST_METHOD (Reset_ClearsState)
    {
        MemoryBus bus;
        LanguageCard lc (bus);

        lc.Read (0xC081);
        lc.Read (0xC081);
        lc.WriteRam (0xD000, 0xFF);

        lc.Reset ();

        Assert::IsFalse (lc.IsReadRam ());
        Assert::IsFalse (lc.IsWriteRam ());
        Assert::AreEqual (static_cast<Byte> (0), lc.ReadRam (0xD000));
    }
};
