#include "../Casso65EmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DeviceTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DeviceTests)
{
public:

    TEST_METHOD (RamDevice_ReadWriteRoundtrip)
    {
        RamDevice ram (0x0000, 0x00FF);

        ram.Write (0x0042, 0xAB);
        Byte val = ram.Read (0x0042);

        Assert::AreEqual (static_cast<Byte> (0xAB), val);
    }

    TEST_METHOD (RamDevice_InitializedToZero)
    {
        RamDevice ram (0x1000, 0x1FFF);

        for (Word addr = 0x1000; addr <= 0x10FF; addr++)
        {
            Assert::AreEqual (static_cast<Byte> (0), ram.Read (addr));
        }
    }

    TEST_METHOD (RamDevice_Reset_ZeroFills)
    {
        RamDevice ram (0x0000, 0x00FF);
        ram.Write (0x0050, 0xFF);
        ram.Reset ();

        Assert::AreEqual (static_cast<Byte> (0), ram.Read (0x0050));
    }

    TEST_METHOD (RomDevice_ReadsReturnData)
    {
        Byte data[] = { 0xEA, 0x60, 0x00 };
        auto rom = RomDevice::CreateFromData (0xFFFC, 0xFFFE, data, 3);

        Assert::AreEqual (static_cast<Byte> (0xEA), rom->Read (0xFFFC));
        Assert::AreEqual (static_cast<Byte> (0x60), rom->Read (0xFFFD));
    }

    TEST_METHOD (RomDevice_WritesAreIgnored)
    {
        Byte data[] = { 0xAA };
        auto rom = RomDevice::CreateFromData (0xD000, 0xD000, data, 1);

        rom->Write (0xD000, 0x55);

        Assert::AreEqual (static_cast<Byte> (0xAA), rom->Read (0xD000));
    }

    TEST_METHOD (RamDevice_GetStartEnd)
    {
        RamDevice ram (0x2000, 0x3FFF);

        Assert::AreEqual (static_cast<Word> (0x2000), ram.GetStart ());
        Assert::AreEqual (static_cast<Word> (0x3FFF), ram.GetEnd ());
    }

    TEST_METHOD (RomDevice_CreateFromFile_MissingFile_ReturnsNull)
    {
        std::string error;
        auto rom = RomDevice::CreateFromFile (0xD000, 0xFFFF, "nonexistent_rom.bin", error);

        Assert::IsNull (rom.get ());
        Assert::IsFalse (error.empty ());
    }
};
