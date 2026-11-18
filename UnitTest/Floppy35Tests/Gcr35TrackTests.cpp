#include "Pch.h"

#include "Devices/Floppy35/Gcr35Track.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Gcr35TrackTests
//
//  The 3.5-inch GCR format: speed zones, block numbering, the sector codec
//  and its checksums, and tracks built and decoded back.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Gcr35TrackTests)
{
public:

    static std::vector<Byte> MakeBytes (size_t count, uint32_t seed)
    {
        std::vector<Byte>  bytes (count);
        uint32_t           state = seed;



        for (Byte & b : bytes)
        {
            state = state * 1103515245u + 12345u;
            b     = (Byte) (state >> 16);
        }

        return bytes;
    }


    TEST_METHOD (SectorsPerTrack_FiveZones)
    {
        Assert::AreEqual (12, Gcr35Track::GetSectorsPerTrack (0));
        Assert::AreEqual (12, Gcr35Track::GetSectorsPerTrack (15));
        Assert::AreEqual (11, Gcr35Track::GetSectorsPerTrack (16));
        Assert::AreEqual (10, Gcr35Track::GetSectorsPerTrack (32));
        Assert::AreEqual (9,  Gcr35Track::GetSectorsPerTrack (48));
        Assert::AreEqual (8,  Gcr35Track::GetSectorsPerTrack (79));
    }


    //  800K disks number side 0 then side 1 on each track; 400K disks have
    //  one side.
    TEST_METHOD (FirstBlock_TrackBySideBySector)
    {
        Assert::AreEqual (0u,    Gcr35Track::GetFirstBlock (0, 0, 2));
        Assert::AreEqual (12u,   Gcr35Track::GetFirstBlock (0, 1, 2));
        Assert::AreEqual (24u,   Gcr35Track::GetFirstBlock (1, 0, 2));
        Assert::AreEqual (384u,  Gcr35Track::GetFirstBlock (16, 0, 2));
        Assert::AreEqual (12u,   Gcr35Track::GetFirstBlock (1, 0, 1));
        Assert::AreEqual (1584u, Gcr35Track::GetFirstBlock (79, 0, 2));
        Assert::AreEqual (1592u, Gcr35Track::GetFirstBlock (79, 1, 2));
    }


    TEST_METHOD (Sector_RoundTrips)
    {
        std::vector<Byte>  bytes = MakeBytes (Gcr35Track::kSectorBytes, 7);
        std::vector<Byte>  nibbles;
        std::vector<Byte>  decoded;



        Gcr35Track::EncodeSector (bytes, nibbles);

        Assert::AreEqual (Gcr35Track::kDataNibbles + Gcr35Track::kSumNibbles, nibbles.size());
        Assert::IsTrue (Gcr35Track::TryDecodeSector (nibbles, decoded));
        Assert::IsTrue (decoded == bytes);
    }


    //  A changed nibble fails a checksum: either the running sums or the
    //  stored ones disagree.
    TEST_METHOD (Sector_CorruptNibbleFailsChecksum)
    {
        std::vector<Byte>  bytes = MakeBytes (Gcr35Track::kSectorBytes, 11);
        std::vector<Byte>  nibbles;
        std::vector<Byte>  decoded;



        Gcr35Track::EncodeSector (bytes, nibbles);
        nibbles[300] = (nibbles[300] == 0x96) ? (Byte) 0x97 : (Byte) 0x96;

        Assert::IsFalse (Gcr35Track::TryDecodeSector (nibbles, decoded));
    }


    //  The checksum group's top-bits nibble has c3's top bits in bits 4-5,
    //  the way the first byte of each data group does. A sector encoded
    //  the other way fails the checksum in Apple's firmware.
    TEST_METHOD (Sector_ChecksumNibbleOrder)
    {
        std::vector<Byte>  bytes (Gcr35Track::kSectorBytes, 0);
        std::vector<Byte>  nibbles;
        std::vector<Byte>  swapped;
        std::vector<Byte>  decoded;



        bytes[0] = 0xFF;
        bytes[1] = 0x80;
        Gcr35Track::EncodeSector (bytes, nibbles);

        swapped = nibbles;
        std::swap (swapped[Gcr35Track::kDataNibbles + 1], swapped[Gcr35Track::kDataNibbles + 3]);

        Assert::IsTrue  (Gcr35Track::TryDecodeSector (nibbles, decoded));
        Assert::IsFalse (Gcr35Track::TryDecodeSector (swapped, decoded));
    }


    TEST_METHOD (Track_BuildsAndDecodesEverySector)
    {
        for (int track : { 0, 20, 79 })
        {
            int                sectors = Gcr35Track::GetSectorsPerTrack (track);
            std::vector<Byte>  data    = MakeBytes ((size_t) sectors * Gcr35Track::kDataBytes, (uint32_t) track + 1);
            std::vector<Byte>  bits;
            std::vector<Byte>  decoded;
            std::vector<bool>  found;



            Gcr35Track::BuildTrack (track, 1, Gcr35Track::kFormatTwoSides, data, bits);
            Assert::AreEqual (Gcr35Track::GetTrackBits (track), bits.size());

            Gcr35Track::DecodeTrack (track, 1, bits, decoded, found);

            Assert::AreEqual ((size_t) sectors, (size_t) std::count (found.begin(), found.end(), true));
            Assert::IsTrue (decoded == data);
        }
    }


    //  A track read from any point decodes, since the sync groups bring the
    //  bit stream back into step.
    TEST_METHOD (Track_DecodesFromAnyRotation)
    {
        std::vector<Byte>  data = MakeBytes (12 * Gcr35Track::kDataBytes, 3);
        std::vector<Byte>  bits;
        std::vector<Byte>  decoded;
        std::vector<bool>  found;



        Gcr35Track::BuildTrack (0, 0, Gcr35Track::kFormatTwoSides, data, bits);
        std::rotate (bits.begin(), bits.begin() + 12345, bits.end());

        Gcr35Track::DecodeTrack (0, 0, bits, decoded, found);

        Assert::AreEqual ((size_t) 12, (size_t) std::count (found.begin(), found.end(), true));
        Assert::IsTrue (decoded == data);
    }


    //  The other side's track has its sectors' address fields marked side 1,
    //  so it doesn't decode as side 0.
    TEST_METHOD (Track_WrongSideFindsNothing)
    {
        std::vector<Byte>  data (12 * Gcr35Track::kDataBytes, 0x5A);
        std::vector<Byte>  bits;
        std::vector<Byte>  decoded;
        std::vector<bool>  found;



        Gcr35Track::BuildTrack (0, 1, Gcr35Track::kFormatTwoSides, data, bits);
        Gcr35Track::DecodeTrack (0, 0, bits, decoded, found);

        Assert::AreEqual ((size_t) 0, (size_t) std::count (found.begin(), found.end(), true));
    }
};
