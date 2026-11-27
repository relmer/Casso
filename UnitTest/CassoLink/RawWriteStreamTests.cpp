#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/Session/RawWriteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStreamTests
//
//  A WOZ track's bits and a NIB track's nibbles as the bytes and zero bits
//  the raw write module writes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RawWriteStreamTests)
{
public:

    using Bits = std::vector<uint8_t>;

    //  Nibbles that don't repeat within a track.
    static void FillRandom (std::vector<Byte> & ioNibbles)
    {
        uint32_t  seed = 12345;



        for (Byte & nibble : ioNibbles)
        {
            seed   = seed * 1103515245 + 12345;
            nibble = (Byte) (0x96 | (seed >> 16));
        }
    }


    static void AppendByte (Byte value, size_t zeros, Bits & ioBits)
    {
        for (int bit = 7; bit >= 0; bit--)
        {
            ioBits.push_back ((uint8_t) ((value >> bit) & 1));
        }

        ioBits.insert (ioBits.end(), zeros, 0);
    }


    //  Nibbles, 9- and 10-bit sync bytes and a weak-bit run longer than one
    //  byte's zeros come back bit for bit.
    TEST_METHOD (FromBits_WritesEveryBitBack)
    {
        Bits            bits;
        Bits            written;
        RawWriteStream  stream;



        for (size_t i = 0; i + 1 < RawWriteStream::kSyncRun; i++)
        {
            AppendByte (0xFF, 2, bits);
        }

        AppendByte (0xFF, 1, bits);
        AppendByte (0xD5, 0, bits);
        AppendByte (0xAA, 0, bits);
        AppendByte (0x96, 0, bits);
        bits.insert (bits.end(), RawWriteStream::kMaxZeros * 2 + 13, 0);
        AppendByte (0xEB, 0, bits);

        RawWriteStream::FromBits (bits, stream);
        stream.ToBits (written);

        Assert::IsTrue  (written.size() >= bits.size() && written.size() < bits.size() + RawWriteStream::kBitsPerByte);
        Assert::IsTrue  (std::equal (bits.begin(), bits.end(), written.begin()));
        Assert::IsFalse (std::any_of (written.begin() + bits.size(), written.end(), [] (uint8_t bit) { return bit != 0; }));
        Assert::AreEqual (written.size(), stream.GetCellCount());
        Assert::AreEqual ((Byte) 2, stream.zeros[0]);
        Assert::AreEqual ((Byte) 1, stream.zeros[RawWriteStream::kSyncRun - 1]);
        Assert::AreEqual ((Byte) 0xD5, stream.values[RawWriteStream::kSyncRun]);
    }


    static constexpr size_t  kPackedTrack   = 6000;
    static constexpr size_t  kPackedGroupAt = 3000;
    static constexpr size_t  kPackedApart   = 40;

    //  A track with no sync run: a lone prolog and sync byte, then a group of
    //  prologs with sector data right up to each one.
    static void MakePackedTrack (Bits & outBits)
    {
        std::vector<Byte>  nibbles (kPackedTrack, 0xBD);



        nibbles[499]  = 0xFF;
        nibbles[500]  = 0xD5;
        nibbles[501]  = 0xAA;
        nibbles[502]  = 0xAA;

        for (size_t sector = 0; sector < 4; sector++)
        {
            nibbles[kPackedGroupAt + kPackedApart * sector]     = 0xD5;
            nibbles[kPackedGroupAt + kPackedApart * sector + 1] = 0xAA;
            nibbles[kPackedGroupAt + kPackedApart * sector + 2] = 0xAA;
        }

        outBits.clear();

        for (size_t i = 0; i < kPackedTrack; i++)
        {
            AppendByte (nibbles[i], i == 499 ? 2 : 0, outBits);
        }
    }


    //  A packed track with no run of kSyncRun sync bytes, as Karateka's, starts
    //  at the first prolog of its densest group, not at a lone sync byte or in
    //  the sector data before the prolog.
    TEST_METHOD (FromBits_WithNoSyncGap_StartsAtTheGroupOfPrologs)
    {
        Bits            bits;
        RawWriteStream  stream;



        MakePackedTrack (bits);

        RawWriteStream::FromBits (bits, stream);

        Assert::AreEqual (kPackedTrack, stream.values.size());
        Assert::AreEqual ((Byte) 0xD5, stream.values[0]);
        Assert::AreEqual ((Byte) 0xAA, stream.values[2]);
        Assert::AreEqual ((Byte) 0xD5, stream.values[kPackedApart]);
        Assert::AreEqual ((Byte) 0xBD, stream.values.back());
    }


    //  A packed track shorter than the turn gets sync bytes in front of its
    //  prolog, enough for the end of the write to overlap them, and the image's
    //  bytes follow whole.
    TEST_METHOD (FitToTurn_WithNoSyncGap_PutsSyncBytesBeforeTheProlog)
    {
        Bits            bits;
        RawWriteStream  stream;
        size_t          syncs = 0;



        MakePackedTrack (bits);
        RawWriteStream::FromBits (bits, stream);

        Assert::IsTrue (stream.FitToTurn (bits.size() + 4));

        while (stream.values[syncs] == 0xFF)
        {
            Assert::AreEqual ((Byte) RawWriteStream::kSyncZeros, stream.zeros[syncs]);
            syncs++;
        }

        Assert::IsTrue   (syncs * (RawWriteStream::kBitsPerByte + RawWriteStream::kSyncZeros) >= 80);
        Assert::AreEqual (kPackedTrack + syncs, stream.values.size());
        Assert::AreEqual ((Byte) 0xD5, stream.values[syncs]);
        Assert::AreEqual ((Byte) 0xBD, stream.values.back());
    }


    //  A packed track longer than the turn can't be fitted, since only sync
    //  bytes are dropped.
    TEST_METHOD (FitToTurn_WithNoSyncGap_FailsPastTheTurn)
    {
        Bits            bits;
        RawWriteStream  stream;



        MakePackedTrack (bits);
        RawWriteStream::FromBits (bits, stream);

        Assert::IsFalse (stream.FitToTurn (bits.size() - 100));
    }


    //  A track whose bits don't end on a byte is padded with zero bits.
    TEST_METHOD (FromBits_PadsThePartLastByte)
    {
        Bits            bits = { 1, 0, 1 };
        RawWriteStream  stream;



        RawWriteStream::FromBits (bits, stream);

        Assert::AreEqual ((size_t) 1, stream.values.size());
        Assert::AreEqual ((Byte) 0xA0, stream.values[0]);
    }


    //  A NIB track read past its revolution is cut where it repeats, and a
    //  run of $FF is written as sync bytes, a lone $FF as a nibble.
    TEST_METHOD (FromNibbles_CutsTheRepeat_AndWritesSyncRunsAsSyncBytes)
    {
        std::vector<Byte>  revolution (6000);
        std::vector<Byte>  track;
        RawWriteStream     stream;



        FillRandom (revolution);

        std::fill_n (revolution.begin() + 100, 6, (Byte) 0xFF);
        revolution[106] = 0x96;
        revolution[300] = 0xFF;
        revolution[299] = 0x96;
        revolution[301] = 0x97;

        track = revolution;
        track.insert (track.end(), revolution.begin(), revolution.begin() + 656);

        RawWriteStream::FromNibbles (track, stream);

        Assert::AreEqual (revolution.size(), stream.values.size());
        Assert::AreEqual ((Byte) RawWriteStream::kSyncZeros, stream.zeros[100]);
        Assert::AreEqual ((Byte) RawWriteStream::kSyncZeros, stream.zeros[105]);
        Assert::AreEqual ((Byte) 0, stream.zeros[106]);
        Assert::AreEqual ((Byte) 0, stream.zeros[300]);
    }


    //  A track that doesn't repeat, as an NB2 track, is written whole.
    TEST_METHOD (FromNibbles_KeepsATrackThatDoesntRepeat)
    {
        std::vector<Byte>  track (6384);
        RawWriteStream     stream;



        FillRandom (track);

        RawWriteStream::FromNibbles (track, stream);

        Assert::AreEqual (track.size(), stream.values.size());
    }
};
