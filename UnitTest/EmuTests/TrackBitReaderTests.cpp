#include "Pch.h"

#include "Machines/Apple2/Common/TrackBitReader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TrackBitReaderTests
//
//  Nibble and bit reads over bit buffers built in memory.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TrackBitReaderTests)
{
public:

    static TrackBitReader::TrackBits MakeBits (const vector<Byte> & bytes, size_t bitCount)
    {
        TrackBitReader::TrackBits  src;



        src.bits     = &bytes;
        src.bitCount = bitCount;

        return src;
    }



    TEST_METHOD (NibblesEndWhereTheHighBitSets)
    {
        //  D5, two zero bits, then AA: the zeros are skipped, as a sync
        //  byte's tail is.
        vector<Byte>               bytes  = { 0xD5, 0x2A, 0x80 };
        TrackBitReader::TrackBits  src    = MakeBits (bytes, 18);
        size_t                     bitPos = 0;



        Assert::AreEqual ((int) 0xD5, (int) TrackBitReader::ReadNibbleAt (src, bitPos));
        Assert::AreEqual ((size_t) 8, bitPos);
        Assert::AreEqual ((int) 0xAA, (int) TrackBitReader::ReadNibbleAt (src, bitPos));
        Assert::AreEqual ((size_t) 18, bitPos);
    }



    TEST_METHOD (ANibbleAcrossTheEndContinuesAtTheStart)
    {
        //  The last four bits are 1101 and the first four 0101: D5 read
        //  from bit 12 of a 16-bit track.
        vector<Byte>               bytes  = { 0x50, 0x0D };
        TrackBitReader::TrackBits  src    = MakeBits (bytes, 16);
        size_t                     bitPos = 12;



        Assert::AreEqual ((int) 0xD5, (int) TrackBitReader::ReadNibbleAt (src, bitPos));
        Assert::AreEqual ((size_t) 20, bitPos);
    }



    TEST_METHOD (ATrackWithNoHighBitGivesZero)
    {
        vector<Byte>               bytes  (8, 0);
        TrackBitReader::TrackBits  src    = MakeBits (bytes, 64);
        TrackBitReader::TrackBits  none;
        size_t                     bitPos = 0;



        Assert::AreEqual ((int) 0, (int) TrackBitReader::ReadNibbleAt (src, bitPos));
        Assert::IsTrue (bitPos > 64, L"one revolution read");

        bitPos = 0;
        Assert::AreEqual ((int) 0, (int) TrackBitReader::ReadNibbleAt (none, bitPos), L"no bits");
        Assert::AreEqual ((size_t) 0, bitPos);
    }



    TEST_METHOD (GetBitWrapsAtTheBitCount)
    {
        vector<Byte>               bytes = { 0x80, 0x01 };
        TrackBitReader::TrackBits  src   = MakeBits (bytes, 16);
        TrackBitReader::TrackBits  none;



        Assert::AreEqual ((int) 1, (int) TrackBitReader::GetBit (src, 0));
        Assert::AreEqual ((int) 0, (int) TrackBitReader::GetBit (src, 1));
        Assert::AreEqual ((int) 1, (int) TrackBitReader::GetBit (src, 15));
        Assert::AreEqual ((int) 1, (int) TrackBitReader::GetBit (src, 16), L"bit 16 is bit 0");
        Assert::AreEqual ((int) 0, (int) TrackBitReader::GetBit (none, 3));
    }



    TEST_METHOD (UnpackBitsGivesOneBitPerEntry)
    {
        vector<Byte>     packed   = { 0xA5, 0xF0 };
        vector<uint8_t>  bits;
        vector<uint8_t>  expected = { 0, 1, 0, 1, 1, 1, 1, 1 };



        TrackBitReader::UnpackBits (packed, 4, 12, bits);

        Assert::IsTrue (bits == expected);
    }
};
