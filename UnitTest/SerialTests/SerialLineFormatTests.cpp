#include "Pch.h"

#include "Devices/Acia6551.h"
#include "Devices/Serial/SerialLineFormat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace SerialLineFormatTestNs
{
    ////////////////////////////////////////////////////////////////////////////
    //
    //  SerialLineFormatTests
    //
    //  The format a 6551's control and command registers select, as a COM
    //  port is configured from it and as the chip times each character.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SerialLineFormatTests)
    {
    public:
        TEST_METHOD (EveryBaudCodeHasTheDatasheetRate)
        {
            const double  expected[] = { 115200.0, 50.0,   75.0,   109.92, 134.58, 150.0,  300.0,  600.0,
                                         1200.0,   1800.0, 2400.0, 3600.0, 4800.0, 7200.0, 9600.0, 19200.0 };
            int           code       = 0;



            for (code = 0; code < (int) size (expected); code++)
            {
                SerialLineFormat  format = SerialLineFormat::FromRegisters ((Byte) code, 0);

                Assert::AreEqual (expected[code], format.baud, 0.001, std::format (L"code {}", code).c_str());
            }
        }


        TEST_METHOD (WordLengthFollowsBitsSixAndFive)
        {
            Assert::AreEqual (8, SerialLineFormat::FromRegisters (0x00, 0).wordBits);
            Assert::AreEqual (7, SerialLineFormat::FromRegisters (0x20, 0).wordBits);
            Assert::AreEqual (6, SerialLineFormat::FromRegisters (0x40, 0).wordBits);
            Assert::AreEqual (5, SerialLineFormat::FromRegisters (0x60, 0).wordBits);
        }


        TEST_METHOD (ParityFollowsTheCommandRegister)
        {
            Assert::IsTrue (SerialParity::None  == SerialLineFormat::FromRegisters (0, 0x00).parity);
            Assert::IsTrue (SerialParity::None  == SerialLineFormat::FromRegisters (0, 0xC0).parity, L"PME clear disables parity");
            Assert::IsTrue (SerialParity::Odd   == SerialLineFormat::FromRegisters (0, 0x20).parity);
            Assert::IsTrue (SerialParity::Even  == SerialLineFormat::FromRegisters (0, 0x60).parity);
            Assert::IsTrue (SerialParity::Mark  == SerialLineFormat::FromRegisters (0, 0xA0).parity);
            Assert::IsTrue (SerialParity::Space == SerialLineFormat::FromRegisters (0, 0xE0).parity);
        }


        TEST_METHOD (StopBitsFollowTheDatasheetTable)
        {
            Assert::IsTrue (SerialStopBits::One        == SerialLineFormat::FromRegisters (0x00, 0x00).stopBits);
            Assert::IsTrue (SerialStopBits::Two        == SerialLineFormat::FromRegisters (0x80, 0x00).stopBits);
            Assert::IsTrue (SerialStopBits::OneAndHalf == SerialLineFormat::FromRegisters (0xE0, 0x00).stopBits, L"5 bits, no parity");
            Assert::IsTrue (SerialStopBits::Two        == SerialLineFormat::FromRegisters (0xE0, 0x20).stopBits, L"5 bits with parity");
            Assert::IsTrue (SerialStopBits::One        == SerialLineFormat::FromRegisters (0x80, 0x20).stopBits, L"8 bits with parity");
            Assert::IsTrue (SerialStopBits::Two        == SerialLineFormat::FromRegisters (0xA0, 0x20).stopBits, L"7 bits with parity");
        }


        TEST_METHOD (HalfBitsCountStartWordParityAndStop)
        {
            Assert::AreEqual (20, SerialLineFormat::FromRegisters (0x00, 0x00).GetHalfBitsPerCharacter(), L"8N1");
            Assert::AreEqual (22, SerialLineFormat::FromRegisters (0x00, 0x20).GetHalfBitsPerCharacter(), L"8O1");
            Assert::AreEqual (22, SerialLineFormat::FromRegisters (0x80, 0x20).GetHalfBitsPerCharacter(), L"8O with SBN set is 8O1");
            Assert::AreEqual (15, SerialLineFormat::FromRegisters (0xE0, 0x00).GetHalfBitsPerCharacter(), L"5N1.5");
            Assert::AreEqual (20, SerialLineFormat::FromRegisters (0xA0, 0x00).GetHalfBitsPerCharacter(), L"7N2");
        }


        TEST_METHOD (TheChipTimesEightBitsWithParityAndSbnAsOneStopBit)
        {
            Acia6551  acia (0xC0A8);



            acia.SetCpuClockHz (1000000);
            acia.Write (0xC0A8 + Acia6551::kRegControl, 0x8E);      // 9600 baud, 8 bits, SBN set
            acia.Write (0xC0A8 + Acia6551::kRegCommand, 0x2B);      // odd parity, DTR, TIC off

            Assert::AreEqual (1000000.0 * 11.0 / 9600.0, acia.GetTransmitCharacterCycles(), 0.01);
        }
    };
}
