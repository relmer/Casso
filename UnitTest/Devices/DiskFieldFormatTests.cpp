#include "Pch.h"

#include "Devices/Disk/DiskFieldFormat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormatTests
//
//  4-and-4, 6-and-2 and 5-and-3. The expected nibbles in the *Vector tests
//  were worked out by hand from the published descriptions of each format, not
//  produced by Casso's encoder, so these tests do not rest on the code they
//  check:
//
//    5-and-3 (Beneath Apple DOS, chapter 3; Apple's 13-sector read/write
//    routines of May 1978): bytes go in 51 groups of five, B0-B4, plus byte
//    255. Group i is stored at index j = 50 - i. The primary buffer holds each
//    byte's top five bits in five areas of 51 (B0 to B4), byte 255's at index
//    255. The secondary buffer holds three areas of 51: area a holds byte a's
//    low three bits in bits 4-2, bit (2 - a) of B3 in bit 1 and of B4 in bit
//    0; byte 255's low three bits are its last value, index 153. On disk: the
//    secondary buffer from index 153 down to 0, then the primary from 0 up.
//    Each nibble is the table entry for a value XOR the value before it; the
//    checksum nibble is the entry for the last value.
//
//    6-and-2 (Understanding the Apple IIe, chapter 9; DOS 3.3): 86 values of
//    low bit pairs, then 256 values of top six bits. Value i holds byte i's
//    two low bits, swapped, in bits 1-0.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskFieldFormatTests)
{
public:

    static constexpr int   kSectorBytes = DiskFieldFormat::kSectorBytes;
    static constexpr Byte  kT53_00      = 0xAB;
    static constexpr Byte  kT53_01      = 0xAD;
    static constexpr Byte  kT53_07      = 0xBA;
    static constexpr Byte  kT53_1C      = 0xFB;
    static constexpr Byte  kT53_1F      = 0xFF;
    static constexpr Byte  kT62_00      = 0x96;
    static constexpr Byte  kT62_01      = 0x97;
    static constexpr Byte  kT62_02      = 0x9A;



    static void Encode (DiskFieldKind kind, const std::array<Byte, kSectorBytes> & bytes, std::vector<Byte> & body, Byte & checksum)
    {
        DiskFieldFormat::EncodeData (kind, bytes, body, checksum);
    }



    static void AssertAllExcept (const std::vector<Byte> & body, Byte fill, std::initializer_list<std::pair<int, Byte>> exceptions)
    {
        std::vector<Byte>  expected (body.size(), fill);
        size_t             i        = 0;



        for (const auto & e : exceptions)
        {
            expected[e.first] = e.second;
        }

        for (i = 0; i < body.size(); i++)
        {
            Assert::AreEqual (static_cast<int> (expected[i]), static_cast<int> (body[i]), std::format (L"nibble {}", i).c_str());
        }
    }



    TEST_METHOD (FourAndFourRoundTripsEveryByte)
    {
        Byte  odd  = 0;
        Byte  even = 0;
        int   v    = 0;



        for (v = 0; v < 256; v++)
        {
            DiskFieldFormat::Encode44 (static_cast<Byte> (v), odd, even);

            Assert::IsTrue ((odd & 0xAA) == 0xAA && (even & 0xAA) == 0xAA, L"every other bit is a clock bit");
            Assert::AreEqual (v, static_cast<int> (DiskFieldFormat::Decode44 (odd, even)));
        }
    }



    TEST_METHOD (MarksAreTheStandardOnes)
    {
        Assert::AreEqual (std::string ("D5 AA 96"), DiskFieldFormat::GetAddressPrologue (DiskFieldKind::Sixteen).ToText());
        Assert::AreEqual (std::string ("D5 AA B5"), DiskFieldFormat::GetAddressPrologue (DiskFieldKind::Thirteen).ToText());
        Assert::AreEqual (std::string ("D5 AA AD"), DiskFieldFormat::GetDataPrologue().ToText());
        Assert::AreEqual (std::string ("DE AA"),    DiskFieldFormat::GetEpilogue().ToText());
        Assert::AreEqual (342, DiskFieldFormat::GetBodyLength (DiskFieldKind::Sixteen));
        Assert::AreEqual (410, DiskFieldFormat::GetBodyLength (DiskFieldKind::Thirteen));
    }



    TEST_METHOD (TranslateTablesAreDistinctAndReadable)
    {
        for (DiskFieldKind kind : { DiskFieldKind::Sixteen, DiskFieldKind::Thirteen })
        {
            std::set<Byte>  seen;
            int             size = DiskFieldFormat::GetTableSize (kind);
            int             i    = 0;
            Byte            nib  = 0;



            for (i = 0; i < size; i++)
            {
                nib = DiskFieldFormat::TranslateValue (kind, static_cast<Byte> (i));

                Assert::IsTrue ((nib & 0x80) != 0, L"high bit set");
                Assert::IsTrue (seen.insert (nib).second, L"no repeated entry");
                Assert::AreEqual (i, static_cast<int> (DiskFieldFormat::TranslateNibble (kind, nib)));
                Assert::IsTrue (nib != 0xD5 && nib != 0xAA, L"mark bytes are reserved");
            }
        }
    }



    TEST_METHOD (FiveAndThreeAllZeroVector)
    {
        std::array<Byte, kSectorBytes>  bytes    = {};
        std::vector<Byte>               body;
        Byte                            checksum = 0;



        Encode (DiskFieldKind::Thirteen, bytes, body, checksum);

        Assert::AreEqual (410, static_cast<int> (body.size()));
        AssertAllExcept (body, kT53_00, {});
        Assert::AreEqual (static_cast<int> (kT53_00), static_cast<int> (checksum));
    }



    TEST_METHOD (FiveAndThreeFirstByteVector)
    {
        //  Byte 0 = $FF is group 0, index 50. Primary area 0 index 50 = $1F,
        //  written at 154 + 50 = 204. Secondary area 0 index 50 = $1C (low
        //  bits 111 in bits 4-2), written at 153 - 50 = 103. Each nonzero value
        //  shows in its own nibble and the next one (value XOR its neighbor).
        std::array<Byte, kSectorBytes>  bytes    = {};
        std::vector<Byte>               body;
        Byte                            checksum = 0;



        bytes[0] = 0xFF;

        Encode (DiskFieldKind::Thirteen, bytes, body, checksum);

        AssertAllExcept (body, kT53_00, { { 103, kT53_1C }, { 104, kT53_1C }, { 204, kT53_1F }, { 205, kT53_1F } });
        Assert::AreEqual (static_cast<int> (kT53_00), static_cast<int> (checksum));
    }



    TEST_METHOD (FiveAndThreeLastByteVector)
    {
        //  Byte 255 = $FF: its low bits 111 are secondary index 153, written
        //  first; its top bits $1F are primary index 255, written last, so the
        //  checksum nibble is the entry for $1F.
        std::array<Byte, kSectorBytes>  bytes    = {};
        std::vector<Byte>               body;
        Byte                            checksum = 0;



        bytes[255] = 0xFF;

        Encode (DiskFieldKind::Thirteen, bytes, body, checksum);

        AssertAllExcept (body, kT53_00, { { 0, kT53_07 }, { 1, kT53_07 }, { 409, kT53_1F } });
        Assert::AreEqual (static_cast<int> (kT53_1F), static_cast<int> (checksum));
    }



    TEST_METHOD (FiveAndThreeFifthByteBitVector)
    {
        //  Byte 4 = $04 is bit 2 of B4 in group 0: secondary area 0, bit 0,
        //  index 50, written at 103. Its top five bits are zero.
        std::array<Byte, kSectorBytes>  bytes    = {};
        std::vector<Byte>               body;
        Byte                            checksum = 0;



        bytes[4] = 0x04;

        Encode (DiskFieldKind::Thirteen, bytes, body, checksum);

        AssertAllExcept (body, kT53_00, { { 103, kT53_01 }, { 104, kT53_01 } });
    }



    TEST_METHOD (SixAndTwoVectors)
    {
        std::array<Byte, kSectorBytes>  bytes    = {};
        std::vector<Byte>               body;
        Byte                            checksum = 0;



        Encode (DiskFieldKind::Sixteen, bytes, body, checksum);
        AssertAllExcept (body, kT62_00, {});
        Assert::AreEqual (static_cast<int> (kT62_00), static_cast<int> (checksum));

        //  Byte 0 = $01: its swapped low pair is 10, value 0 = 2.
        bytes[0] = 0x01;
        Encode (DiskFieldKind::Sixteen, bytes, body, checksum);
        AssertAllExcept (body, kT62_00, { { 0, kT62_02 }, { 1, kT62_02 } });

        //  Byte 0 = $04: top six bits 1, value 86.
        bytes[0] = 0x04;
        Encode (DiskFieldKind::Sixteen, bytes, body, checksum);
        AssertAllExcept (body, kT62_00, { { 86, kT62_01 }, { 87, kT62_01 } });
    }



    TEST_METHOD (EveryByteRoundTripsInBothFormats)
    {
        std::array<Byte, kSectorBytes>  bytes    = {};
        std::vector<Byte>               body;
        Byte                            checksum = 0;
        DataFieldDecode                 decode;
        uint32_t                        seed     = 0x1234567;
        int                             pass     = 0;
        int                             i        = 0;



        for (DiskFieldKind kind : { DiskFieldKind::Sixteen, DiskFieldKind::Thirteen })
        {
            for (pass = 0; pass < 64; pass++)
            {
                for (i = 0; i < kSectorBytes; i++)
                {
                    seed     = seed * 1103515245 + 12345;
                    bytes[i] = (pass == 0) ? static_cast<Byte> (i) : static_cast<Byte> (seed >> 16);
                }

                Encode (kind, bytes, body, checksum);
                DiskFieldFormat::DecodeData (kind, body, checksum, decode);

                Assert::IsTrue (decode.isChecksumGood);
                Assert::IsTrue (decode.invalidNibbles.empty());
                Assert::IsTrue (decode.bytes == bytes);
            }
        }
    }



    TEST_METHOD (ABadNibbleIsCountedAndDecodingGoesOn)
    {
        std::array<Byte, kSectorBytes>  bytes    = {};
        std::vector<Byte>               body;
        Byte                            checksum = 0;
        DataFieldDecode                 decode;
        int                             i        = 0;



        //  Bytes 0-255 make nibbles 10 and 11 stand for $3F and $2A, so taking
        //  both as 0 leaves the chain off by $15 and the checksum fails.
        for (i = 0; i < kSectorBytes; i++)
        {
            bytes[i] = static_cast<Byte> (i);
        }

        Encode (DiskFieldKind::Sixteen, bytes, body, checksum);

        body[10] = 0xD5;
        body[11] = 0xD5;

        DiskFieldFormat::DecodeData (DiskFieldKind::Sixteen, body, checksum, decode);

        Assert::AreEqual (1, static_cast<int> (decode.invalidNibbles.size()));
        Assert::AreEqual (static_cast<int> (0xD5), static_cast<int> (decode.invalidNibbles[0].value));
        Assert::AreEqual (2, decode.invalidNibbles[0].count);
        Assert::IsFalse (decode.isChecksumGood);
        Assert::IsTrue (decode.isStoredValid, L"the checksum nibble itself was not touched");
    }



    TEST_METHOD (AChecksumOfAllZerosAndAllOnesIsStable)
    {
        std::array<Byte, kSectorBytes>  bytes    = {};
        std::vector<Byte>               body;
        Byte                            checksum = 0;
        DataFieldDecode                 decode;



        bytes.fill (0xFF);

        for (DiskFieldKind kind : { DiskFieldKind::Sixteen, DiskFieldKind::Thirteen })
        {
            Encode (kind, bytes, body, checksum);
            DiskFieldFormat::DecodeData (kind, body, checksum, decode);

            Assert::IsTrue (decode.isChecksumGood);
            Assert::AreEqual (static_cast<int> (decode.storedChecksum), static_cast<int> (decode.computedChecksum));
        }
    }
};
