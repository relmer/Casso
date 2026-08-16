#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/TrackWritability.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NibblizationTests
//
//  Phase 10 / FR-023 / audit §7. Validates the .DSK / .DO / .PO loaders
//  end-to-end: encode a synthetic 143360-byte sector image, then either
//  decode the bit stream back to bytes (round-trip identity) or assert
//  the prologue/epilogue framing landed in the expected positions.
//
//  No host fixture files are required — tests build raw images in memory.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (NibblizationTests)
{
public:

    static constexpr int      kImageSize = NibblizationLayer::kImageByteSize;
    static constexpr int      kPattern1  = 0x55;
    static constexpr int      kPattern2  = 0xAA;

    vector<Byte> MakeAllValueImage (Byte v)
    {
        return vector<Byte> (kImageSize, v);
    }

    vector<Byte> MakeAlternatingImage (Byte a, Byte b)
    {
        size_t         i   = 0;



        vector<Byte>   img (kImageSize, 0);

        for (i = 0; i < img.size(); i++)
        {
            img[i] = ((i & 1) == 0) ? a : b;
        }

        return img;
    }

    vector<Byte> MakePinnedRandomImage (uint32_t seed)
    {
        uint32_t  state = 0;
        size_t    i     = 0;



        vector<Byte>   img (kImageSize, 0);
        state = seed;

        for (Byte & byte : img)
        {
            state = state * 1664525u + 1013904223u;
            byte  = static_cast<Byte> ((state >> 24) & 0xFF);
        }

        return img;
    }

    TEST_METHOD (NibblizeDsk_AcceptsCorrectSizedImage)
    {
        DiskImage      img;
        vector<Byte>   raw = MakeAllValueImage (0);

        HRESULT   hr = NibblizationLayer::NibblizeDsk (raw, img);

        AssertSucceeded (hr, L"DSK nibblization must accept 143360-byte image");
        Assert::AreEqual (DiskImage::kDefaultTrackCount, img.GetTrackCount());
        Assert::IsTrue (img.GetTrackBitCount (0) > 0,
            L"Track 0 must have bit data after nibblization");
    }

    TEST_METHOD (NibblizeDsk_RejectsShortImage)
    {
        DiskImage      img;
        HRESULT        hr  = S_OK;
        vector<Byte>   raw (1024, 0);

        {
            // A wrong-sized image is a caller bug, so the guard asserts.
            UnitTestHelpers::ExpectedEhmAssert   expect;

            hr = NibblizationLayer::NibblizeDsk (raw, img);
        }

        AssertFailed (hr);
    }

    TEST_METHOD (DskRoundTripIdentity_AllZeros)
    {
        DiskImage      img;
        vector<Byte>   raw       = MakeAllValueImage (0);
        vector<Byte>   recovered;
        HRESULT        hrLoad    = NibblizationLayer::NibblizeDsk (raw, img);
        HRESULT        hrSave    = S_OK;

        AssertSucceeded (hrLoad);

        hrSave = NibblizationLayer::Denibblize (img, DiskFormat::Dsk, recovered);

        AssertSucceeded (hrSave);
        Assert::AreEqual (raw.size(), recovered.size());
        Assert::IsTrue   (raw == recovered, L"DSK round-trip identity (zeros)");
    }

    TEST_METHOD (DskRoundTripIdentity_AllPatternA5)
    {
        DiskImage      img;
        vector<Byte>   raw       = MakeAllValueImage (0xA5);
        vector<Byte>   recovered;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));
        AssertSucceeded (NibblizationLayer::Denibblize  (img, DiskFormat::Dsk, recovered));

        Assert::IsTrue (raw == recovered, L"DSK round-trip identity (0xA5)");
    }

    TEST_METHOD (DskRoundTripIdentity_Alternating)
    {
        DiskImage      img;
        vector<Byte>   raw       = MakeAlternatingImage (kPattern1, kPattern2);
        vector<Byte>   recovered;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));
        AssertSucceeded (NibblizationLayer::Denibblize  (img, DiskFormat::Dsk, recovered));

        Assert::IsTrue (raw == recovered, L"DSK round-trip identity (alternating)");
    }

    TEST_METHOD (DskRoundTripIdentity_PinnedPrng)
    {
        DiskImage      img;
        vector<Byte>   raw       = MakePinnedRandomImage (0xCA550001u);
        vector<Byte>   recovered;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));
        AssertSucceeded (NibblizationLayer::Denibblize  (img, DiskFormat::Dsk, recovered));

        Assert::IsTrue (raw == recovered, L"DSK round-trip identity (pinned PRNG)");
    }

    TEST_METHOD (DoRoundTripIdentity)
    {
        DiskImage      img;
        vector<Byte>   raw       = MakePinnedRandomImage (0xCA550002u);
        vector<Byte>   recovered;

        AssertSucceeded (NibblizationLayer::NibblizeDo (raw, img));
        Assert::IsTrue (img.GetSourceFormat() == DiskFormat::Do);
        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Do, recovered));

        Assert::IsTrue (raw == recovered, L".DO round-trip identity");
    }

    TEST_METHOD (PoRoundTripIdentity)
    {
        DiskImage      img;
        vector<Byte>   raw       = MakePinnedRandomImage (0xCA550003u);
        vector<Byte>   recovered;

        AssertSucceeded (NibblizationLayer::NibblizePo (raw, img));
        Assert::IsTrue (img.GetSourceFormat() == DiskFormat::Po);
        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Po, recovered));

        Assert::IsTrue (raw == recovered, L".PO round-trip identity");
    }

    TEST_METHOD (PoAndDskInterleavesDifferOnSameSourceBytes)
    {
        size_t  bits = 0;



        // Same flat 143360-byte source produces different nibble streams
        // when interpreted as DSK vs PO because the sector mapping differs.
        DiskImage      dskImg;
        DiskImage      poImg;
        vector<Byte>   raw    = MakePinnedRandomImage (0xCA550004u);
        bool           differ = false;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, dskImg));
        AssertSucceeded (NibblizationLayer::NibblizePo  (raw, poImg));

        // Compare track 1 bit streams (track 0 sectors all colocated for this
        // input but other tracks reorder differently).
        bits = dskImg.GetTrackBitCount (1);

        Assert::AreEqual (bits, poImg.GetTrackBitCount (1));

        for (size_t i = 0; i < bits; i++)
        {
            if (dskImg.ReadBit (1, i) != poImg.ReadBit (1, i))
            {
                differ = true;
                break;
            }
        }

        Assert::IsTrue (differ, L"DSK and PO must produce different bit streams for the same source");
    }

    TEST_METHOD (AddressFieldFraming_FirstSectorHasD5AA96)
    {
        DiskImage      img;
        vector<Byte>   raw = MakeAllValueImage (0);
        size_t         i   = 0;
        Byte           b0  = 0;
        Byte           b1  = 0;
        Byte           b2  = 0;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));

        // After 20 sync nibbles (10 bits each = 200 bits including the
        // mandatory 2-bit zero gap that real Disk II hardware writes
        // between sync nibbles), expect $D5 $AA $96 prologue.
        // Pack the next 24 bits MSB-first into three bytes.
        for (int n = 0; n < 24; n++)
        {
            Byte   bit = img.ReadBit (0, 200 + n);
            int    pos = n / 8;
            if (pos == 0) { b0 = static_cast<Byte> ((b0 << 1) | bit); }
            else if (pos == 1) { b1 = static_cast<Byte> ((b1 << 1) | bit); }
            else { b2 = static_cast<Byte> ((b2 << 1) | bit); }
        }

        Assert::AreEqual (static_cast<Byte> (0xD5), b0, L"Address prologue byte 0");
        Assert::AreEqual (static_cast<Byte> (0xAA), b1, L"Address prologue byte 1");
        Assert::AreEqual (static_cast<Byte> (0x96), b2, L"Address prologue byte 2");

        UNREFERENCED_PARAMETER (i);
    }

    TEST_METHOD (NibblizeThenDenibblizeProducesByteEqualOriginal)
    {
        // Aggregate test mirrors the tasks.md naming.
        DiskImage      img;
        vector<Byte>   raw       = MakePinnedRandomImage (0xDEADBEEFu);
        vector<Byte>   recovered;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));
        AssertSucceeded (NibblizationLayer::Denibblize  (img, DiskFormat::Dsk, recovered));
        Assert::IsTrue (raw == recovered);
    }

    TEST_METHOD (SerializeOnSyntheticImageMatchesDenibblize)
    {
        // DiskImage::Serialize must agree with NibblizationLayer::Denibblize.
        DiskImage      img;
        vector<Byte>   raw      = MakePinnedRandomImage (0xC0FFEE01u);
        vector<Byte>   viaSer;
        vector<Byte>   viaDirect;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));
        AssertSucceeded (img.Serialize (viaSer));
        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, viaDirect));

        Assert::IsTrue (viaSer == viaDirect);
        Assert::IsTrue (viaSer == raw);
    }

    TEST_METHOD (DskReformatRewrite_serializesToNewContentNotStale)
    {
        // A Print Shop "initialize data disk" (or any reformat) rewrites the
        // mounted image's tracks. The .dsk write-back (Serialize/Denibblize)
        // must reflect the NEW content, never the stale original -- the
        // scenario behind the reported "initialized data disk still shows the
        // old files" symptom. Because Denibblize scans for GCR address/data
        // markers (byte-sync), a re-nibblized (reformatted) track round-trips
        // regardless of the exact gap/sync layout the format produced, so a
        // standard DOS 3.3 format survives the flush; the earlier loss was
        // flush *timing*, addressed by motor-idle auto-flush.
        DiskImage      img;
        vector<Byte>   original  = MakePinnedRandomImage (0x11111111u);
        vector<Byte>   rewritten = MakePinnedRandomImage (0x22222222u);
        vector<Byte>   baseline;
        vector<Byte>   afterReformat;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (original, img));
        AssertSucceeded (img.Serialize (baseline));
        Assert::IsTrue (original == baseline, L"baseline round-trip");

        // Reformat: overwrite every track with different content.
        AssertSucceeded (NibblizationLayer::NibblizeDsk (rewritten, img));
        AssertSucceeded (img.Serialize (afterReformat));

        Assert::IsTrue  (rewritten == afterReformat,
            L"a reformatted .dsk must serialize to the new content");
        Assert::IsFalse (original == afterReformat,
            L"the stale original must not survive the reformat");
    }

    TEST_METHOD (Denibblize_UnformattedTrack_ZeroFillsThatTrackAndKeepsOthers)
    {
        bool  neighborOk = false;



        // A .dsk is a plain sector image, so a track with NO decodable
        // address fields anywhere -- a blank / unformatted bit stream --
        // correctly denibblizes to zeros for THAT track and leaves neighbors
        // intact, and Denibblize returns S_OK rather than failing. A blank
        // disk really is all zeros.
        //
        // This claim is confined to the WHOLLY UNFORMATTED case, which is what
        // this test wipes. It does NOT generalize to "missing sectors read back
        // as zeros": a track that yields some sectors and then fails has lost
        // data, and reporting that as zeros is exactly the silent corruption of
        // a valid track that GH #115 describes. The three tests below pin the
        // damaged cases; do not widen this comment to cover them.
        DiskImage      img;
        vector<Byte>   raw       = MakePinnedRandomImage (0x5A5A5A5Au);
        vector<Byte>   recovered;
        const size_t   trkBytes  = 16 * 256;
        const int      wiped     = 5;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));

        // Blank track 5's bit stream (no address fields left to decode).
        img.ResizeTrack (wiped, DiskImage::kDefaultTrackByteSize * 8);
        {
            vector<Byte> & b = img.GetTrackBitsForWrite (wiped);
            std::fill (b.begin(), b.end(), static_cast<Byte> (0));
        }

        img.SetTrackBitCount (wiped, DiskImage::kDefaultTrackByteSize * 8);

        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, recovered));

        // Wiped track -> all zeros.
        for (size_t i = 0; i < trkBytes; i++)
        {
            Assert::AreEqual (Byte (0), recovered[static_cast<size_t> (wiped) * trkBytes + i]);
        }

        // Adjacent track 4 -> unaffected, still matches the original.
        neighborOk = true;
        for (size_t i = 0; i < trkBytes; i++)
        {
            size_t  off = static_cast<size_t> (4) * trkBytes + i;
            if (recovered[off] != raw[off]) { neighborOk = false; break; }
        }

        Assert::IsTrue (neighborOk, L"a formatted neighbor track must be unaffected");
    }

    //  Byte offset of the Nth address prologue (D5 AA 96) in a track's packed
    //  bit stream. Nibbles are 8 bits packed MSB-first from offset 0, so the
    //  stream is byte-aligned and a plain byte scan finds them.
    static size_t FindAddressField (const vector<Byte> & bits, int which)
    {
        size_t  i     = 0;
        int     seen  = 0;
        size_t  found = SIZE_MAX;



        for (i = 0; i + 2 < bits.size() && found == SIZE_MAX; i++)
        {
            if (bits[i] == 0xD5 && bits[i + 1] == 0xAA && bits[i + 2] == 0x96)
            {
                if (seen == which)
                {
                    found = i;
                }

                seen++;
            }
        }

        return found;
    }

    //  The sector number an address field claims, decoded from its 4-and-4 pair.
    static Byte ReadFieldSector (const vector<Byte> & bits, size_t addrAt)
    {
        return static_cast<Byte> (((bits[addrAt + 7] << 1) | 1) & bits[addrAt + 8]);
    }

    //  Rewrites an address field's sector number, keeping the checksum honest so
    //  the header stays otherwise valid -- the point is to exercise the sector
    //  number, not to be rejected for a bad checksum.
    static void PatchFieldSector (vector<Byte> & bits, size_t addrAt, Byte volume, Byte track, Byte sector)
    {
        Byte  checksum = static_cast<Byte> (volume ^ track ^ sector);



        bits[addrAt + 7]  = static_cast<Byte> ((sector >> 1) | 0xAA);
        bits[addrAt + 8]  = static_cast<Byte> (sector | 0xAA);
        bits[addrAt + 9]  = static_cast<Byte> ((checksum >> 1) | 0xAA);
        bits[addrAt + 10] = static_cast<Byte> (checksum | 0xAA);
    }

    //  Writes a checksum that cannot be right for this header, leaving every
    //  other field intact.
    //
    //  Flipping a bit in the ENCODED byte is not good enough: 4-and-4 forces the
    //  odd bits to 1, so clearing one only changes the decoded value when that
    //  value's bit was already set -- a corruption that silently does nothing
    //  for half the sectors on a track. Writing an explicitly wrong value is
    //  unconditional.
    static void PatchFieldChecksum (vector<Byte> & bits, size_t addrAt, Byte volume, Byte track)
    {
        Byte  sector = ReadFieldSector (bits, addrAt);
        Byte  wrong  = static_cast<Byte> ((volume ^ track ^ sector) ^ 0xFF);



        bits[addrAt + 9]  = static_cast<Byte> ((wrong >> 1) | 0xAA);
        bits[addrAt + 10] = static_cast<Byte> (wrong | 0xAA);
    }

    //  Logical sectors the report says were recovered must still hold their
    //  original bytes: damage to one sector may not disturb any other.
    void AssertCoveredSectorsIntact (
        const vector<Byte>        & recovered,
        const vector<Byte>        & original,
        const SectorDecodeReport  & report,
        int                         track)
    {
        const size_t  kSectorBytes = NibblizationLayer::kSectorByteSize;
        int           sector       = 0;
        size_t        i            = 0;



        for (sector = 0; sector < NibblizationLayer::kSectorsPerTrack; sector++)
        {
            bool    covered = report.IsSectorRecovered (track, sector);
            size_t  base    = (static_cast<size_t> (track) * NibblizationLayer::kSectorsPerTrack
                            + static_cast<size_t> (sector)) * kSectorBytes;

            if (!covered)
            {
                continue;
            }

            for (i = 0; i < kSectorBytes; i++)
            {
                Assert::AreEqual (original[base + i], recovered[base + i],
                    L"a recovered sector must hold its original bytes");
            }
        }
    }

    TEST_METHOD (Denibblize_PartiallyDecodableTrack_ReportsDataLossAndDoesNotZeroTail)
    {
        // The defect this pins: the decoder used to abandon a track at its
        // first bad sector, leaving that sector AND every later one in scan
        // order as zeros, while reporting success. Corrupting one address
        // field's checksum mid-track must cost exactly that one sector.
        DiskImage           img;
        vector<Byte>        raw     = MakePinnedRandomImage (0x1234ABCDu);
        vector<Byte>        recovered;
        SectorDecodeReport  report;
        const int           kTrack  = 7;
        const int           kVictim = 5;   // physical position within the track

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));

        {
            vector<Byte> &  bits   = img.GetTrackBitsForWrite (kTrack);
            size_t          addrAt = FindAddressField (bits, kVictim);

            Assert::AreNotEqual (SIZE_MAX, addrAt, L"the track must carry address fields to corrupt");

            // Break only the checksum: the header still parses, and is refused
            // because it cannot be trusted rather than because it is unreadable.
            PatchFieldChecksum (bits, addrAt, NibblizationLayer::kDefaultVolume,
                                static_cast<Byte> (kTrack));
        }

        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, recovered, report));

        Assert::IsTrue (TrackDecodeOutcome::Partial == report.GetOutcome (kTrack),
            L"a track with an unusable header is damaged, not blank");
        Assert::IsTrue (report.HasDataLoss(), L"the report must say data was lost");
        Assert::AreEqual (1, report.GetUnrecoveredCount(),
            L"exactly one sector is lost -- the tail must survive");

        AssertCoveredSectorsIntact (recovered, raw, report, kTrack);

        // Neighbors are untouched, and a clean track is still Complete.
        Assert::IsTrue (TrackDecodeOutcome::Complete == report.GetOutcome (kTrack + 1),
            L"an undamaged neighbor must still read Complete");
    }

    TEST_METHOD (Denibblize_OutOfRangeSectorNumber_ReportsIncompleteCoverage)
    {
        // No decode FAILS here: the header is valid and its checksum agrees.
        // The sector number simply names a slot the geometry does not have, so
        // one logical sector is never filled. Coverage is what catches it.
        DiskImage           img;
        vector<Byte>        raw    = MakePinnedRandomImage (0x2468BDF0u);
        vector<Byte>        recovered;
        SectorDecodeReport  report;
        const int           kTrack = 9;
        const Byte          kBogus = 200;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));

        {
            vector<Byte> &  bits   = img.GetTrackBitsForWrite (kTrack);
            size_t          addrAt = FindAddressField (bits, 3);

            Assert::AreNotEqual (SIZE_MAX, addrAt, L"the track must carry address fields to corrupt");

            PatchFieldSector (bits, addrAt, NibblizationLayer::kDefaultVolume,
                              static_cast<Byte> (kTrack), kBogus);
        }

        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, recovered, report));

        Assert::IsTrue (TrackDecodeOutcome::Partial == report.GetOutcome (kTrack),
            L"an unreachable sector number leaves the track short");
        Assert::AreEqual (1, report.GetUnrecoveredCount(), L"exactly one slot goes unfilled");
        Assert::IsFalse (report.IsDuplicated (kTrack), L"nothing was written twice");

        AssertCoveredSectorsIntact (recovered, raw, report, kTrack);
    }

    TEST_METHOD (Denibblize_DuplicateSectorNumbers_ReportsIncompleteCoverage)
    {
        // Also no failure: two valid headers claim the same slot. The second
        // would overwrite the first, and some other slot goes unclaimed to pay
        // for it. Reported through the duplicate flag as well as coverage.
        DiskImage           img;
        vector<Byte>        raw    = MakePinnedRandomImage (0x0F0F5A5Au);
        vector<Byte>        recovered;
        SectorDecodeReport  report;
        const int           kTrack = 11;
        Byte                victim = 0;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));

        {
            vector<Byte> &  bits    = img.GetTrackBitsForWrite (kTrack);
            size_t          firstAt = FindAddressField (bits, 2);
            size_t          nextAt  = FindAddressField (bits, 3);

            Assert::AreNotEqual (SIZE_MAX, firstAt, L"the track must carry address fields to corrupt");
            Assert::AreNotEqual (SIZE_MAX, nextAt,  L"the track must carry a second address field");

            victim = ReadFieldSector (bits, firstAt);

            PatchFieldSector (bits, nextAt, NibblizationLayer::kDefaultVolume,
                              static_cast<Byte> (kTrack), victim);
        }

        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, recovered, report));

        Assert::IsTrue (TrackDecodeOutcome::Partial == report.GetOutcome (kTrack),
            L"a duplicated slot means some other slot went unclaimed");
        Assert::IsTrue (report.IsDuplicated (kTrack), L"the duplicate must be reported as such");
        Assert::AreEqual (1, report.GetUnrecoveredCount(), L"exactly one slot goes unfilled");
    }

    TEST_METHOD (Denibblize_ReportlessOverload_FailsOnDataLoss)
    {
        // The reportless signature must not be an escape hatch. It exists for
        // callers that do not want the detail, NOT for callers that want to
        // ignore it -- DiskImage::Serialize is the one production caller and
        // the flush path is precisely where silent truncation cost data.
        DiskImage           img;
        vector<Byte>        raw    = MakePinnedRandomImage (0x77778888u);
        vector<Byte>        recovered;
        HRESULT             hr     = S_OK;
        const int           kTrack = 4;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));

        {
            vector<Byte> &  bits   = img.GetTrackBitsForWrite (kTrack);
            size_t          addrAt = FindAddressField (bits, 6);

            Assert::AreNotEqual (SIZE_MAX, addrAt, L"the track must carry address fields to corrupt");

            PatchFieldChecksum (bits, addrAt, NibblizationLayer::kDefaultVolume,
                                static_cast<Byte> (kTrack));
        }

        hr = NibblizationLayer::Denibblize (img, DiskFormat::Dsk, recovered);

        Assert::IsTrue (FAILED (hr),
            L"the reportless overload must fail rather than hand back a truncated buffer");
    }

    TEST_METHOD (Denibblize_UnformattedTrack_StillSucceedsThroughReportlessOverload)
    {
        // The counterpart to the test above: a blank track is NOT data loss,
        // and refusing one would make blank and newly formatted media
        // unreadable. This is the distinction a bare "not all sectors decoded"
        // check would collapse.
        DiskImage       img;
        vector<Byte>    raw    = MakePinnedRandomImage (0x99AABBCCu);
        vector<Byte>    recovered;
        const int       kWiped = 5;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));

        img.ResizeTrack (kWiped, DiskImage::kDefaultTrackByteSize * 8);
        {
            vector<Byte> &  b = img.GetTrackBitsForWrite (kWiped);

            std::fill (b.begin(), b.end(), static_cast<Byte> (0));
        }

        img.SetTrackBitCount (kWiped, DiskImage::kDefaultTrackByteSize * 8);

        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, recovered));
    }

    TEST_METHOD (RenibblizeTracks_LeavesEveryOtherTrackBitIdentical)
    {
        // The whole argument for a targeted rewrite: a small edit must not
        // resynthesize the rest of the disk. Re-encoding everything would
        // discard timing, sync, and weak bits on tracks nothing was written to.
        DiskImage       img;
        vector<Byte>    raw      = MakePinnedRandomImage (0xC0FFEE01u);
        vector<Byte>    edited;
        vector<Byte>    before;
        const int       kTouched = 12;
        const int       kIntact  = 13;
        int             tracks[] = { kTouched };

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));

        before = img.GetTrackBits (kIntact);
        edited = raw;

        // Change a byte that lives on the touched track only.
        edited[static_cast<size_t> (kTouched) * 16 * NibblizationLayer::kSectorByteSize] ^= 0xFF;

        AssertSucceeded (NibblizationLayer::RenibblizeTracks (edited, DiskFormat::Dsk, tracks, img));

        {
            const vector<Byte> &  after     = img.GetTrackBits (kIntact);
            size_t                i         = 0;
            bool                  identical = after.size() == before.size();

            for (i = 0; identical && i < before.size(); i++)
            {
                identical = after[i] == before[i];
            }

            Assert::IsTrue (identical, L"an untouched track's bits must be byte-identical");
        }
    }

    TEST_METHOD (RenibblizeTracks_TouchedTrackCarriesTheEdit)
    {
        // The other half: the track that was named must actually change, and
        // the image must still denibblize cleanly afterwards.
        DiskImage           img;
        vector<Byte>        raw      = MakePinnedRandomImage (0x5EED1234u);
        vector<Byte>        edited;
        vector<Byte>        recovered;
        SectorDecodeReport  report;
        const int           kTouched = 4;
        int                 tracks[] = { kTouched };
        size_t              at       = static_cast<size_t> (kTouched) * 16
                                     * NibblizationLayer::kSectorByteSize;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));

        edited      = raw;
        edited[at] ^= 0xFF;

        AssertSucceeded (NibblizationLayer::RenibblizeTracks (edited, DiskFormat::Dsk, tracks, img));
        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, recovered, report));

        Assert::AreEqual (edited[at], recovered[at], L"the edit must survive the re-encode");
        Assert::IsFalse (report.HasDataLoss(), L"a re-encoded image must still decode cleanly");
        Assert::IsTrue (TrackDecodeOutcome::Complete == report.GetOutcome (kTouched),
            L"the rewritten track must read back Complete");
    }

    TEST_METHOD (TrackWritability_CleanImage_EveryTrackWritable)
    {
        DiskImage           img;
        vector<Byte>        raw = MakePinnedRandomImage (0x11223344u);
        vector<Byte>        recovered;
        SectorDecodeReport  report;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));
        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, recovered, report));

        {
            TrackWritability  writability = TrackWritability::Evaluate (img, report);

            Assert::IsTrue (writability.IsImageWritable(), L"a standard image must be writable");
            Assert::IsTrue (writability.IsTrackWritable (0),  L"track 0 must be writable");
            Assert::IsTrue (writability.IsTrackWritable (34), L"the last track must be writable");
        }
    }

    TEST_METHOD (TrackWritability_DamagedTrack_RefusesOnlyThatTrack)
    {
        // Refusal is per track. A track the operation never touches must not
        // block a write elsewhere on the disk -- that is the difference between
        // protecting data and refusing to work.
        DiskImage           img;
        vector<Byte>        raw       = MakePinnedRandomImage (0x99887766u);
        vector<Byte>        recovered;
        SectorDecodeReport  report;
        const int           kDamaged  = 6;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));

        {
            vector<Byte> &  bits   = img.GetTrackBitsForWrite (kDamaged);
            size_t          addrAt = FindAddressField (bits, 2);

            Assert::AreNotEqual (SIZE_MAX, addrAt, L"the track must carry address fields to corrupt");

            PatchFieldChecksum (bits, addrAt, NibblizationLayer::kDefaultVolume,
                                static_cast<Byte> (kDamaged));
        }

        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, recovered, report));

        {
            TrackWritability  writability = TrackWritability::Evaluate (img, report);
            int               elsewhere[] = { kDamaged - 1, kDamaged + 1 };
            int               includes[]  = { kDamaged - 1, kDamaged };

            Assert::IsTrue  (writability.IsImageWritable(),
                L"one damaged track does not condemn the whole image");
            Assert::IsFalse (writability.IsTrackWritable (kDamaged),
                L"the damaged track itself must be refused");
            Assert::IsTrue  (writability.AreTracksWritable (elsewhere),
                L"a write that avoids the damage must be allowed");
            Assert::IsFalse (writability.AreTracksWritable (includes),
                L"a write that needs the damaged track must be refused");
        }
    }

    TEST_METHOD (TrackWritability_HalfTrackData_RefusesTheWholeImage)
    {
        // Data between whole tracks has nowhere to go in a sector image, and
        // the loss would be silent. Checked before any track is examined.
        DiskImage           img;
        vector<Byte>        raw = MakePinnedRandomImage (0xABCDEF01u);
        vector<Byte>        recovered;
        SectorDecodeReport  report;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));
        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, recovered, report));

        // Point a half-track position at a slot that is not its whole track,
        // which is what a bit-stream capture of a protected disk looks like.
        img.SetQuarterTrackSlot (10, 7);

        {
            TrackWritability  writability = TrackWritability::Evaluate (img, report);
            bool              hasReason   = !writability.GetImageRefusalReason().empty();

            Assert::IsFalse (writability.IsImageWritable(),
                L"an image holding data between tracks must be refused outright");
            Assert::IsTrue  (hasReason, L"the refusal must say why");
            Assert::IsFalse (writability.IsTrackWritable (0),
                L"the whole-image refusal outranks any per-track answer");
        }
    }
};

