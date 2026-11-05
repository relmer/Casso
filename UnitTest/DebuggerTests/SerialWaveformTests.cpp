#include "Pch.h"

#include "Debugger/SerialWaveform.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SerialWaveformTests
//
//  The bits the serial log draws for one byte, from the 6551's control and
//  command registers: a start bit, the word low bit first, any parity bit,
//  and the stop bits.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (SerialWaveformTests)
    {
    public:
        static constexpr Byte  kControl8N1 = 0x1E;     // 9600 baud, 8 bits, 1 stop bit
        static constexpr Byte  kControl7   = 0x3E;     // 7 bits
        static constexpr Byte  kControl5   = 0x7E;     // 5 bits
        static constexpr Byte  kTwoStop    = 0x80;
        static constexpr Byte  kNoParity   = 0x0B;
        static constexpr Byte  kOdd        = 0x2B;
        static constexpr Byte  kEven       = 0x6B;


        static std::string Levels (const std::vector<SerialBit> & bits)
        {
            std::string  text;



            for (const SerialBit & bit : bits)
            {
                text += bit.isMark ? '1' : '0';
            }

            return text;
        }


        TEST_METHOD (EightNoneOneIsTenBits)
        {
            std::vector<SerialBit>  bits;



            SerialWaveform::LayOut ('A', kControl8N1, kNoParity, bits);

            Assert::AreEqual (std::string ("0100000101"), Levels (bits), L"start, $41 low bit first, stop");
            Assert::IsTrue   (bits.front().kind == SerialBit::Kind::Start);
            Assert::IsTrue   (bits.back().kind  == SerialBit::Kind::Stop);
            Assert::AreEqual (10.0, SerialWaveform::GetCharacterBits (kControl8N1, kNoParity));
            Assert::AreEqual (9.0,  bits.back().start);
        }


        TEST_METHOD (SevenEvenOneHasAParityBit)
        {
            std::vector<SerialBit>  bits;



            SerialWaveform::LayOut ('A', kControl7, kEven, bits);

            Assert::AreEqual (std::string ("0100000101"), Levels (bits), L"$41 has two ones, so even parity is 0");
            Assert::IsTrue   (bits[8].kind == SerialBit::Kind::Parity);
        }


        TEST_METHOD (EightOddTwoHasParityAndTwoStopBits)
        {
            std::vector<SerialBit>  bits;



            SerialWaveform::LayOut (0x03, kControl8N1 | kTwoStop, kOdd, bits);

            Assert::AreEqual (std::string ("01100000011"), Levels (bits), L"$03 has two ones, so odd parity is 1");
            Assert::AreEqual (2.0, bits.back().length, L"one stop entry two bits long");
            Assert::AreEqual (12.0, SerialWaveform::GetCharacterBits (kControl8N1 | kTwoStop, kOdd));
        }


        TEST_METHOD (AFiveBitWordWithTwoStopBitsHasOneAndAHalf)
        {
            std::vector<SerialBit>  bits;



            SerialWaveform::LayOut (0x1F, kControl5 | kTwoStop, kNoParity, bits);

            Assert::AreEqual (std::string ("0111111"), Levels (bits), L"start, five ones, one stop bit");
            Assert::AreEqual (1.5, bits.back().length);
            Assert::AreEqual (7.5, SerialWaveform::GetCharacterBits (kControl5 | kTwoStop, kNoParity));
        }
    };
}
