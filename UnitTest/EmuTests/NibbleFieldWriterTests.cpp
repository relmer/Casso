#include "Pch.h"

#include "Devices/Disk/DiskFieldFormat.h"
#include "Machines/Apple2/Common/NibbleFieldWriter.h"
#include "Machines/Apple2/Common/NibbleFormat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleFieldWriterTests
//
//  The bits each writer packs, read back eight at a time from known offsets.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (NibbleFieldWriterTests)
{
public:

    static constexpr size_t  kNibbleBits   = 8;
    static constexpr size_t  kSyncBits     = kNibbleBits + NibbleFieldWriter::kSyncTailZeros;
    static constexpr size_t  kBufferBytes  = 1024;



    static Byte GetByteAt (const vector<Byte> & bits, size_t bitPos)
    {
        Byte    value = 0;
        size_t  i     = 0;



        for (i = 0; i < kNibbleBits; i++)
        {
            value = static_cast<Byte> ((value << 1) | ((bits[(bitPos + i) >> 3] >> (7 - ((bitPos + i) & 7))) & 1));
        }

        return value;
    }



    static void AssertSyncRun (const vector<Byte> & bits, int count)
    {
        int  i = 0;



        for (i = 0; i < count; i++)
        {
            Assert::AreEqual ((int) 0xFF, (int) GetByteAt (bits, i * kSyncBits));
            Assert::AreEqual ((int) 0,    (int) GetByteAt (bits, i * kSyncBits + 2) & 0x03, L"two zero bits after each sync byte");
        }
    }



    static void AssertNibbles (const vector<Byte> & bits, size_t from, const vector<Byte> & expected)
    {
        size_t  i = 0;



        for (i = 0; i < expected.size(); i++)
        {
            Assert::AreEqual ((int) expected[i], (int) GetByteAt (bits, from + i * kNibbleBits), std::format (L"nibble {}", i).c_str());
        }
    }



    TEST_METHOD (PackNibbleBitsWritesMsbFirstAtAnyOffset)
    {
        vector<Byte>  bits      (4, 0);
        size_t        bitOffset = 3;



        NibbleFieldWriter::PackNibbleBits (bits, bitOffset, 0xD5);

        Assert::AreEqual ((size_t) 11, bitOffset);
        Assert::AreEqual ((int) 0xD5, (int) GetByteAt (bits, 3));
        Assert::AreEqual ((int) 0x1A, (int) bits[0]);
        Assert::AreEqual ((int) 0xA0, (int) bits[1]);
    }



    TEST_METHOD (ASyncNibbleIsTenBits)
    {
        vector<Byte>  bits      (4, 0);
        size_t        bitOffset = 0;



        NibbleFieldWriter::PackSyncNibbleBits (bits, bitOffset);
        NibbleFieldWriter::PackSyncNibbleBits (bits, bitOffset);

        Assert::AreEqual (2 * kSyncBits, bitOffset);
        AssertSyncRun (bits, 2);
    }



    TEST_METHOD (AnAddressFieldIsSyncPrologue44AndEpilogue)
    {
        constexpr Byte  kVolume = 254;
        constexpr Byte  kTrack  = 17;
        constexpr Byte  kSector = 9;



        vector<Byte>  bits      (kBufferBytes, 0);
        vector<Byte>  expected  = { 0xD5, 0xAA, 0x96 };
        size_t        bitOffset = 0;
        size_t        start     = NibbleFieldWriter::kAddrPrologueGap * kSyncBits;
        Byte          odd       = 0;
        Byte          even      = 0;



        NibbleFieldWriter::AppendAddressField (bits, bitOffset, kVolume, kTrack, kSector);

        for (Byte value : { kVolume, kTrack, kSector, static_cast<Byte> (kVolume ^ kTrack ^ kSector) })
        {
            DiskFieldFormat::Encode44 (value, odd, even);
            expected.push_back (odd);
            expected.push_back (even);
        }

        expected.insert (expected.end(), { 0xDE, 0xAA, 0xEB });

        Assert::AreEqual (start + expected.size() * kNibbleBits, bitOffset);
        AssertSyncRun (bits, NibbleFieldWriter::kAddrPrologueGap);
        AssertNibbles (bits, start, expected);
    }



    TEST_METHOD (ADataFieldIsSyncPrologueSixAndTwoAndEpilogue)
    {
        vector<Byte>  sector    (NibbleFormat::kSectorByteSize, 0);
        vector<Byte>  bits      (kBufferBytes, 0);
        vector<Byte>  expected  = { 0xD5, 0xAA, 0xAD };
        vector<Byte>  body;
        size_t        bitOffset = 0;
        size_t        start     = NibbleFieldWriter::kDataPrologueGap * kSyncBits;
        size_t        i         = 0;
        Byte          checksum  = 0;



        for (i = 0; i < sector.size(); i++)
        {
            sector[i] = static_cast<Byte> (i * 3 + 1);
        }

        NibbleFieldWriter::AppendDataField (bits, bitOffset, sector.data());

        DiskFieldFormat::EncodeData (DiskFieldKind::Sixteen, sector, body, checksum);
        expected.insert (expected.end(), body.begin(), body.end());
        expected.push_back (checksum);
        expected.insert (expected.end(), { 0xDE, 0xAA, 0xEB });

        Assert::AreEqual (NibbleFormat::kDataNibbleCount + 6, expected.size());
        Assert::AreEqual (start + expected.size() * kNibbleBits, bitOffset);
        AssertSyncRun (bits, NibbleFieldWriter::kDataPrologueGap);
        AssertNibbles (bits, start, expected);
    }
};
