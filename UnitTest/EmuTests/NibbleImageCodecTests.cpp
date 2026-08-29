#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/NibbleImageCodec.h"
#include "Devices/Disk/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleImageCodecTests
//
//  Bytes to bit streams and back, for headerless nibble images.
//
//  THE LOAD AND THE DERIVATION ARE EXACT INVERSES on an untouched track, and
//  that is the invariant most of this file leans on. Byte-concatenation packs
//  eight bits per byte; the derivation shifts until the high bit sets, which
//  for a byte that has one takes exactly eight shifts. So a track that goes in
//  comes back out, byte for byte, and any drift shows up as a mismatch rather
//  than as a plausible-looking difference.
//
//  Every fixture is synthesized in memory. No .nib file exists in the tree and
//  none is downloaded; the round-trip images here are GCR-encoded from sector
//  data by the code already under test elsewhere.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (NibbleImageCodecTests)
{
public:

    //  A nibble image whose every byte is a legal nibble.
    //
    //  THE PERIOD IS 127 AND NOT 128 ON PURPOSE, and this cost a real defect
    //  to learn. The pattern was `0x80 | (i & 0x7F)`, period 128, and a track
    //  is 6,656 bytes -- exactly 52 periods. Rotating such a track by 128 is
    //  the identity, so a round-trip test over it could not see a rotation at
    //  all: deliberately breaking the rotation rule left every assertion green.
    //  127 is prime and does not divide either track size, so any rotation
    //  changes the bytes and the tests can tell.
    //
    //  The scattered sync bytes are what a gap-finding rotation keys on. The
    //  base pattern tops out at 0xFE and would otherwise contain none, which
    //  would leave the rotation untested for the opposite reason.
    static vector<Byte>  MakeImage (size_t trackSize)
    {
        vector<Byte>  raw (trackSize * NibbleImageCodec::kTrackCount, 0);
        size_t        i = 0;

        for (i = 0; i < raw.size(); i++)
        {
            raw[i] = static_cast<Byte> (0x80 | (i % 127));

            if ((i % 1000) == 3)
            {
                raw[i] = 0xFF;
            }
        }

        return raw;
    }



    TEST_METHOD (ResolveGeometry_AcceptsBothCirculatingSizes)
    {
        size_t   trackSize = 0;

        AssertSucceeded (NibbleImageCodec::ResolveGeometry (232960, trackSize));
        Assert::AreEqual ((size_t) 6656, trackSize, L"232,960 bytes is 35 tracks of 6,656");

        AssertSucceeded (NibbleImageCodec::ResolveGeometry (223440, trackSize));
        Assert::AreEqual ((size_t) 6384, trackSize, L"223,440 bytes is 35 tracks of 6,384");
    }



    TEST_METHOD (ResolveGeometry_RefusesEveryOtherLength)
    {
        //  Either side of both accepted totals, plus the sector-image size,
        //  which is the wrong-file case a user is most likely to hit.
        static constexpr size_t  kRejected[] = { 232959, 232961, 223439, 223441, 143360, 0 };

        size_t   trackSize = 0;
        size_t   count     = ARRAYSIZE (kRejected);
        size_t   i         = 0;
        HRESULT  hr        = S_OK;

        Assert::IsTrue (count > 0, L"the corpus must not be empty");

        for (i = 0; i < count; i++)
        {
            hr = NibbleImageCodec::ResolveGeometry (kRejected[i], trackSize);

            Assert::IsTrue (FAILED (hr), L"only the two circulating totals are a nibble image");
        }
    }



    TEST_METHOD (Load_PacksEveryTrackEightBitsPerByte)
    {
        DiskImage     img;
        vector<Byte>  raw   = MakeImage (NibbleImageCodec::kNibTrackSize);
        int           track = 0;

        AssertSucceeded (NibbleImageCodec::Load (raw, img));

        //  The count is asserted before anything loops over it, so a codec
        //  that produced nothing cannot pass by having nothing to check.
        Assert::AreEqual (NibbleImageCodec::kTrackCount, img.GetTrackCount(),
            L"a nibble image is 35 tracks");

        for (track = 0; track < NibbleImageCodec::kTrackCount; track++)
        {
            const vector<Byte>  &  bits   = img.GetTrackBits (track);
            size_t                 offset = static_cast<size_t> (track) * NibbleImageCodec::kNibTrackSize;

            Assert::AreEqual (NibbleImageCodec::kNibTrackSize * 8, img.GetTrackBitCount (track),
                L"every track is its block size in bits");

            Assert::AreEqual (0, memcmp (bits.data(), &raw[offset], NibbleImageCodec::kNibTrackSize),
                L"a track's packed bytes are the file's bytes for that track");
        }
    }



    TEST_METHOD (Load_AcceptsBytesWithTheHighBitClear)
    {
        DiskImage     img;
        vector<Byte>  raw (NibbleImageCodec::kNibImageSize, 0x00);

        //  Illegal on real media, present in real images, and refusing them
        //  would reject files that work.
        raw[0] = 0xD5;

        AssertSucceeded (NibbleImageCodec::Load (raw, img));
        Assert::IsTrue (img.GetTrackBitCount (0) > 0, L"a track of mostly zeros still loads");
    }



    TEST_METHOD (Load_RefusesAWrongSizedFile)
    {
        DiskImage     img;
        vector<Byte>  raw (1000, 0xFF);
        HRESULT       hr = NibbleImageCodec::Load (raw, img);

        Assert::IsTrue (FAILED (hr));
    }



    TEST_METHOD (HasAnyNibble_SeparatesBlankFromGarbage)
    {
        vector<Byte>  zeros (NibbleImageCodec::kNibImageSize, 0x00);
        vector<Byte>  ones  (NibbleImageCodec::kNibImageSize, 0xFF);

        Assert::IsFalse (NibbleImageCodec::HasAnyNibble (zeros),
            L"all-zero content assembles no nibble anywhere");
        Assert::IsTrue  (NibbleImageCodec::HasAnyNibble (ones));

        //  One high bit in the whole file is enough, which is the point: this
        //  test is deliberately weak because the format offers nothing else.
        zeros[NibbleImageCodec::kNibImageSize - 1] = 0x80;
        Assert::IsTrue (NibbleImageCodec::HasAnyNibble (zeros));
    }



    TEST_METHOD (Serialize_RoundTripsAnUntouchedImageExactly)
    {
        DiskImage     img;
        vector<Byte>  raw = MakeImage (NibbleImageCodec::kNibTrackSize);
        vector<Byte>  out;

        AssertSucceeded (NibbleImageCodec::Load (raw, img));
        AssertSucceeded (NibbleImageCodec::Serialize (img, raw, out));

        Assert::AreEqual (raw.size(), out.size(), L"the file keeps its length");
        Assert::AreEqual (0, memcmp (raw.data(), out.data(), raw.size()),
            L"an image nothing wrote to comes back byte for byte");
    }



    TEST_METHOD (Serialize_DerivationIsTheExactInverseOfTheLoad)
    {
        DiskImage     img;
        vector<Byte>  raw = MakeImage (NibbleImageCodec::kNibTrackSize);
        vector<Byte>  out;

        //  Passing no source bytes forces every track down the DERIVATION path
        //  rather than the copy path, which is the whole point: the previous
        //  test would pass on a codec whose derivation was broken, because it
        //  copies. Here nothing is copied.
        AssertSucceeded (NibbleImageCodec::Load (raw, img));
        AssertSucceeded (NibbleImageCodec::Serialize (img, vector<Byte>(), out));

        Assert::AreEqual (raw.size(), out.size());
        Assert::AreEqual (0, memcmp (raw.data(), out.data(), raw.size()),
            L"a track of legal nibbles derives back to exactly its own bytes");
    }



    TEST_METHOD (Serialize_RoundTripsTheSmallerTrackSizeToo)
    {
        DiskImage     img;
        vector<Byte>  raw = MakeImage (NibbleImageCodec::kNb2TrackSize);
        vector<Byte>  out;

        AssertSucceeded (NibbleImageCodec::Load (raw, img));
        AssertSucceeded (NibbleImageCodec::Serialize (img, raw, out));

        Assert::AreEqual (NibbleImageCodec::kNb2ImageSize, out.size());
        Assert::AreEqual (0, memcmp (raw.data(), out.data(), raw.size()));
    }



    TEST_METHOD (Serialize_LeavesCleanTracksAloneWhenOneIsDirtied)
    {
        DiskImage     img;
        vector<Byte>  raw         = MakeImage (NibbleImageCodec::kNibTrackSize);
        vector<Byte>  out;
        size_t        dirtyOffset = 5 * NibbleImageCodec::kNibTrackSize;
        int           track       = 0;

        AssertSucceeded (NibbleImageCodec::Load (raw, img));

        //  Flip one bit on track 5 through the public write path, so the
        //  dirty bookkeeping is the real one rather than a test fiction.
        img.WriteBit (5, 0, 0);

        AssertSucceeded (NibbleImageCodec::Serialize (img, raw, out));

        for (track = 0; track < NibbleImageCodec::kTrackCount; track++)
        {
            size_t  offset = static_cast<size_t> (track) * NibbleImageCodec::kNibTrackSize;

            if (track == 5)
            {
                continue;
            }

            Assert::AreEqual (0, memcmp (&raw[offset], &out[offset], NibbleImageCodec::kNibTrackSize),
                L"a track the guest did not write is copied, not re-derived");
        }

        Assert::AreEqual (NibbleImageCodec::kNibImageSize, out.size());
        Assert::IsTrue (dirtyOffset < out.size());
    }



    TEST_METHOD (Serialize_DerivedTrackNeverOverflowsItsBlock)
    {
        DiskImage     img;
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0xE7);
        vector<Byte>  out;

        //  Real GCR tracks, self-sync and all: 50,624 bits carrying 6,224
        //  bytes, which is the shape a guest write leaves behind and the case
        //  where the derived count falls well short of the 6,656-byte block.
        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, img));
        AssertSucceeded (NibbleImageCodec::Serialize (img, vector<Byte>(), out));

        Assert::AreEqual (NibbleImageCodec::kNibImageSize, out.size(),
            L"a file written from nothing takes the standard block size");
    }



    TEST_METHOD (Serialize_PadsAShortTrackWithSyncBytes)
    {
        DiskImage     img;
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0x00);
        vector<Byte>  out;
        size_t        i       = 0;
        size_t        syncRun = 0;
        size_t        longest = 0;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, img));
        AssertSucceeded (NibbleImageCodec::Serialize (img, vector<Byte>(), out));

        Assert::AreEqual (NibbleImageCodec::kNibImageSize, out.size());

        //  Every byte written must be a legal nibble, or the head stalls over
        //  the padding on the next mount.
        for (i = 0; i < NibbleImageCodec::kNibTrackSize; i++)
        {
            Assert::IsTrue ((out[i] & 0x80) != 0, L"padding must have the high bit set");

            syncRun = (out[i] == 0xFF) ? syncRun + 1 : 0;

            if (syncRun > longest)
            {
                longest = syncRun;
            }
        }

        Assert::IsTrue (longest > 0, L"a padded track ends in a run of sync bytes");
    }
};
