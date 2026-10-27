#include "Pch.h"

#include "Machines/Apple2/Common/SscSwitches.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace SscSwitchesTestNs
{
    // Floating-bus bytes with every undriven bit set, then clear, so a test
    // checks both the switch bits and the bits the buffers do not drive.
    static constexpr Byte  kFloatHigh = 0xFF;
    static constexpr Byte  kFloatLow  = 0x00;




    ////////////////////////////////////////////////////////////////////////////
    //
    //  SscSwitchesTests
    //
    //  The SSC's DIP switches as the firmware reads them, against Apple's
    //  Installation and Operating Manual, table A-7, and the switch tables on
    //  Apple's reference card.
    //
    //  Each readable switch is checked at its bit in both positions, one at a
    //  time from all-OFF, so a swapped pair of bits fails rather than
    //  masking each other.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SscSwitchesTests)
    {
    public:
        TEST_METHOD (EachSw1SwitchReadsAtItsBit)
        {
            // SW1-1..SW1-6 -> bits 7, 6, 5, 4, 1, 0. SW1-7 has no bit.
            constexpr int  kBit[] = { 7, 6, 5, 4, 1, 0 };



            for (int n = 1; n <= 6; ++n)
            {
                SscSwitches  sw;     // all OFF
                Byte         allOff = sw.GetDipSwitch1Byte (kFloatLow);
                Byte         oneOn  = 0;



                sw.SetOn (SscBank::Sw1, n, true);
                oneOn = sw.GetDipSwitch1Byte (kFloatLow);

                Assert::AreEqual<Byte> (static_cast<Byte> (1 << kBit[n - 1]), static_cast<Byte> (allOff ^ oneOn),
                                        format (L"SW1-{} toggles exactly bit {}", n, kBit[n - 1]).c_str());
                Assert::IsTrue ((oneOn & (1 << kBit[n - 1])) == 0, format (L"SW1-{} ON reads 0", n).c_str());
            }
        }


        TEST_METHOD (EachSw2SwitchReadsAtItsBit)
        {
            // SW2-1..SW2-5 -> bits 7, 5, 3, 2, 1. SW2-6 and SW2-7 have no bit.
            constexpr int  kBit[] = { 7, 5, 3, 2, 1 };



            for (int n = 1; n <= 5; ++n)
            {
                SscSwitches  sw;
                Byte         allOff = sw.GetDipSwitch2Byte (true, kFloatLow);
                Byte         oneOn  = 0;



                sw.SetOn (SscBank::Sw2, n, true);
                oneOn = sw.GetDipSwitch2Byte (true, kFloatLow);

                Assert::AreEqual<Byte> (static_cast<Byte> (1 << kBit[n - 1]), static_cast<Byte> (allOff ^ oneOn),
                                        format (L"SW2-{} toggles exactly bit {}", n, kBit[n - 1]).c_str());
            }
        }


        TEST_METHOD (UnreadSwitchesChangeNoRegisterBit)
        {
            SscSwitches  sw;
            Byte         sw1Before = sw.GetDipSwitch1Byte (kFloatLow);
            Byte         sw2Before = sw.GetDipSwitch2Byte (true, kFloatLow);



            sw.SetOn (SscBank::Sw1, 7, true);
            sw.SetOn (SscBank::Sw2, 6, true);
            sw.SetOn (SscBank::Sw2, 7, true);

            Assert::AreEqual (sw1Before, sw.GetDipSwitch1Byte (kFloatLow));
            Assert::AreEqual (sw2Before, sw.GetDipSwitch2Byte (true, kFloatLow));
        }


        TEST_METHOD (UndrivenBitsComeFromTheFloatingBus)
        {
            SscSwitches  sw;



            // DIPSW1 bits 2-3 and DIPSW2 bits 4 and 6 are not driven.
            Assert::AreEqual<Byte> (0x0C, static_cast<Byte> (sw.GetDipSwitch1Byte (kFloatHigh) & 0x0C));
            Assert::AreEqual<Byte> (0x00, static_cast<Byte> (sw.GetDipSwitch1Byte (kFloatLow)  & 0x0C));
            Assert::AreEqual<Byte> (0x50, static_cast<Byte> (sw.GetDipSwitch2Byte (true, kFloatHigh) & 0x50));
            Assert::AreEqual<Byte> (0x00, static_cast<Byte> (sw.GetDipSwitch2Byte (true, kFloatLow)  & 0x50));
        }


        TEST_METHOD (CtsIsBitZeroOfDipSwitch2)
        {
            SscSwitches  sw;



            Assert::AreEqual<Byte> (0, static_cast<Byte> (sw.GetDipSwitch2Byte (true,  kFloatLow) & 1), L"CTS true reads 0");
            Assert::AreEqual<Byte> (1, static_cast<Byte> (sw.GetDipSwitch2Byte (false, kFloatLow) & 1), L"CTS false reads 1");
        }


        TEST_METHOD (ModeFromSw15AndSw16)
        {
            SscSwitches  sw;



            sw.SetOn (SscBank::Sw1, 5, true);
            sw.SetOn (SscBank::Sw1, 6, true);
            Assert::IsTrue (sw.GetMode() == SscMode::Communications);

            sw.SetOn (SscBank::Sw1, 5, false);
            Assert::IsTrue (sw.GetMode() == SscMode::Printer);

            sw.SetOn (SscBank::Sw1, 5, true);
            sw.SetOn (SscBank::Sw1, 6, false);
            Assert::IsTrue (sw.GetMode() == SscMode::SicP8);

            sw.SetOn (SscBank::Sw1, 5, false);
            Assert::IsTrue (sw.GetMode() == SscMode::SicP8A);
        }


        TEST_METHOD (BaudCodeMatchesTheReferenceCard)
        {
            // From the reference card: SW1-1..SW1-4 for 19,200, 9,600, 300.
            struct Row { bool on[4]; int code; };
            const Row  rows[] =
            {
                { { false, false, false, false }, 15 },     // 19,200
                { { false, false, false, true  }, 14 },     // 9,600
                { { true,  false, false, true  }, 6  },     // 300
                { { true,  true,  true,  true  }, 0  },     // undefined on the switches
            };



            for (const Row & row : rows)
            {
                SscSwitches  sw;

                for (int n = 1; n <= 4; ++n)
                {
                    sw.SetOn (SscBank::Sw1, n, row.on[n - 1]);
                }

                Assert::AreEqual (row.code, sw.GetBaudCode());
            }
        }


        TEST_METHOD (DefaultIsCommunications9600InterruptsOff)
        {
            SscSwitches  sw = SscSwitches::MakeDefault();



            Assert::IsTrue (sw.GetMode() == SscMode::Communications);
            Assert::AreEqual (14, sw.GetBaudCode());
            Assert::IsFalse (sw.IsIrqConnected());
            Assert::AreEqual (string ("off off off on on on on"), sw.GetBankText (SscBank::Sw1));
            Assert::AreEqual (string ("on on off on off off off"), sw.GetBankText (SscBank::Sw2));
        }


        TEST_METHOD (Sw26ConnectsTheIrq)
        {
            SscSwitches  sw;



            Assert::IsFalse (sw.IsIrqConnected());
            sw.SetOn (SscBank::Sw2, 6, true);
            Assert::IsTrue (sw.IsIrqConnected());
        }


        TEST_METHOD (BankTextRoundTripsAndRejectsMalformed)
        {
            SscSwitches  sw = SscSwitches::MakeDefault();



            Assert::IsTrue (sw.TryParseBankText (SscBank::Sw1, "ON on Off off ON off on"));
            Assert::AreEqual (string ("on on off off on off on"), sw.GetBankText (SscBank::Sw1));

            Assert::IsFalse (sw.TryParseBankText (SscBank::Sw1, "on on off"), L"too few");
            Assert::IsFalse (sw.TryParseBankText (SscBank::Sw1, "on on off off on off on on"), L"too many");
            Assert::IsFalse (sw.TryParseBankText (SscBank::Sw1, "on on off off up off on"), L"not on or off");
            Assert::AreEqual (string ("on on off off on off on"), sw.GetBankText (SscBank::Sw1),
                              L"a failed parse does not alter the bank");
        }
    };
}
