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


    //  A track longer than the turn with weak-bit stretches, as Karateka's
    //  track 2.5, is fitted by shortening its longest runs of zero bits, each
    //  left long enough to read as weak; its bytes stay as they are.
    TEST_METHOD (FitToTurn_ShortensWeakStretches_PastTheTurn)
    {
        Bits               bits;
        RawWriteStream     stream;
        std::vector<Byte>  values;



        MakePackedTrack (bits);

        for (size_t at : { 1000, 2000, 4000 })
        {
            bits.insert (bits.begin() + (ptrdiff_t) (at * 8), 100, 0);
        }

        RawWriteStream::FromBits (bits, stream);
        values = stream.values;

        Assert::IsTrue (stream.FitToTurn (bits.size() - 150));
        Assert::IsTrue (stream.GetCellCount() <= bits.size() - 150 + 80);
        Assert::IsTrue (std::search (stream.values.begin(), stream.values.end(), values.begin(), values.end()) != stream.values.end());

        for (Byte zeros : stream.zeros)
        {
            Assert::IsTrue (zeros <= 2 || zeros >= 16);
        }
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


    //  Writes a D5 AA sector of 16 bytes at `at`, its bytes after the prolog
    //  from `seed`.
    static void PutSector (std::vector<Byte> & ioValues, size_t at, uint32_t seed)
    {
        ioValues[at]     = 0xD5;
        ioValues[at + 1] = 0xAA;

        for (size_t i = 2; i < 16; i++)
        {
            seed               = seed * 1103515245 + 12345;
            ioValues[at + i]   = (Byte) (0x96 | (seed >> 16));
        }
    }


    //  A quarter position that is the track beside it read from another
    //  starting point, with noise between its sectors, is an off-center read.
    //  A half track with a sector the tracks beside don't have, as Karateka's
    //  track 2.5, isn't, even though the rest of it is its neighbors bleeding
    //  through.
    TEST_METHOD (IsOffCenterRead_IsTheTrackBesideButNotAHalfTracksSector)
    {
        RawWriteStream  whole;
        RawWriteStream  offCenter;
        RawWriteStream  halfTrack;



        whole.values.assign (6000, 0xAB);

        for (size_t sector = 0; sector < 10; sector++)
        {
            PutSector (whole.values, 100 + sector * 500, (uint32_t) sector);
        }

        offCenter.values.assign (whole.values.begin() + 1234, whole.values.end());
        offCenter.values.insert (offCenter.values.end(), whole.values.begin(), whole.values.begin() + 1234);

        for (size_t at = 0; at < offCenter.values.size(); at += 200)
        {
            offCenter.values[at] = 0xFF;
        }

        halfTrack.values = whole.values;
        PutSector (halfTrack.values, 2300, 99);

        Assert::IsTrue  (RawWriteStream::IsOffCenterRead (offCenter, nullptr, &whole));
        Assert::IsTrue  (RawWriteStream::IsOffCenterRead (offCenter, &whole, nullptr));
        Assert::IsFalse (RawWriteStream::IsOffCenterRead (halfTrack, &whole, &whole));
        Assert::IsFalse (RawWriteStream::IsOffCenterRead (offCenter, nullptr, nullptr));
    }


    //  As Karateka's tracks 2 to 3: a quarter position that is track 2 read off
    //  center is dropped, half track 2.5 with a sector of its own is kept, and
    //  track 3, only 2.5's edge, becomes an erase of sync bytes written ahead
    //  of 2.5.
    TEST_METHOD (DropOffCenterReads_ErasesAHalfTracksEdgeBeforeIt)
    {
        std::vector<RawWriteTrack>  tracks (4);
        std::vector<Byte>           two (6000, 0xAB);
        std::vector<Byte>           half (6000, 0xAD);



        for (size_t sector = 0; sector < 10; sector++)
        {
            PutSector (two,  100 + sector * 500, (uint32_t) sector);
            PutSector (half, 300 + sector * 500, (uint32_t) (sector + 50));
        }

        for (size_t at = 0; at < half.size(); at += 37)
        {
            half[at] = (Byte) (0x96 + at % 50);
        }

        tracks[0].position      = 8;
        tracks[0].stream.values = two;
        tracks[1].position      = 9;
        tracks[1].stream.values.assign (two.begin() + 700, two.end());
        tracks[1].stream.values.insert (tracks[1].stream.values.end(), two.begin(), two.begin() + 700);
        tracks[2].position      = 10;
        tracks[2].stream.values = half;
        tracks[3].position      = 12;
        tracks[3].stream.values.assign (half.begin() + 1000, half.begin() + 3000);

        for (RawWriteTrack & track : tracks)
        {
            track.stream.zeros.assign (track.stream.values.size(), 0);
        }

        std::erase_if (tracks[3].stream.values, [] (Byte value) { return value == 0xD5; });
        tracks[3].stream.zeros.resize (tracks[3].stream.values.size());

        RawWriteStream::DropOffCenterReads (tracks);

        Assert::AreEqual ((size_t) 3, tracks.size());
        Assert::AreEqual (8,  tracks[0].position);
        Assert::AreEqual (12, tracks[1].position);
        Assert::AreEqual (10, tracks[2].position);
        Assert::IsTrue (std::all_of (tracks[1].stream.values.begin(), tracks[1].stream.values.end() - 1, [] (Byte value) { return value == 0xFF; }));
        Assert::AreNotEqual ((Byte) 0xFF, tracks[1].stream.values.back());
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
