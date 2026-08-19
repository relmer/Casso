#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/NibblizationLayer.h"

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



        // A .dsk is a plain sector image, so a track with no decodable
        // address fields (blank / unformatted bit stream) correctly
        // denibblizes to zeros for THAT track and leaves neighbors intact --
        // Denibblize returns S_OK rather than failing. This documents the
        // "missing sectors read back as zeros" behavior: it is intentional
        // for sector images (a blank disk is all zeros), not silent
        // corruption of a valid track.
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


    ////////////////////////////////////////////////////////////////////////
    //
    //  Partial decode reporting (GH #115).
    //
    //  Denibblize used to return S_OK over a track it had only partly
    //  decoded, and the flush path wrote that buffer over the user's file.
    //  The damage is not only the zeros: the scan for a missing data field
    //  runs on and finds the NEXT sector's, storing it under the sector
    //  number the address field gave -- so one point of damage produces one
    //  zeroed sector and one sector holding the wrong data, and the save
    //  reported success.
    //
    ////////////////////////////////////////////////////////////////////////

    // Turn one sector's DATA prolog (D5 AA AD) into an address prolog, so that
    // sector's data field can no longer be found. Returns how many byte-aligned
    // data prologs the track held, so a test asserts its own setup worked
    // rather than silently damaging nothing.
    //
    // That count is 8 on a 16-sector track, not 16, and the reason is worth
    // knowing before reading it as a bug: self-sync gap bytes occupy 10 bits
    // each, so the nibbles after an odd number of them sit off the byte
    // boundary. The decoder finds those by bit-level resync; a byte-wise search
    // like this one sees only the aligned half. Damaging one of them is still
    // exactly one damaged data field, which is all these tests need.
    int BreakOneDataField (DiskImage & img, int track)
    {
        vector<Byte> &  bits    = img.GetTrackBitsForWrite (track);
        int             prologs = 0;
        size_t          i       = 0;

        for (i = 0; i + 2 < bits.size(); i++)
        {
            if (bits[i] == 0xD5 && bits[i + 1] == 0xAA && bits[i + 2] == 0xAD)
            {
                prologs++;

                if (prologs == 1)
                {
                    bits[i + 2] = 0x96;
                }
            }
        }

        return prologs;
    }


    int CountDecoded (uint16_t mask)
    {
        int  count = 0;
        int  bit   = 0;

        for (bit = 0; bit < 16; bit++)
        {
            if ((mask & (1 << bit)) != 0)
            {
                count++;
            }
        }

        return count;
    }


    TEST_METHOD (Denibblize_CleanImage_ReportsEveryTrackComplete)
    {
        // The baseline the other cases are read against: a report that cannot
        // tell a clean image from a damaged one is worth nothing.
        DiskImage         img;
        vector<Byte>      raw = MakePinnedRandomImage (0xC0FFEEu);
        vector<Byte>      out;
        DenibblizeReport  report;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));
        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, out, report));

        Assert::AreEqual (35,  report.tracksPresent);
        Assert::AreEqual (35,  report.tracksComplete);
        Assert::AreEqual (0,   report.tracksPartial);
        Assert::AreEqual (0,   report.tracksUnformatted);
        Assert::AreEqual (560, report.sectorsDecoded, L"35 tracks x 16 sectors");
        Assert::AreEqual (0,   report.sectorsMissing);
        Assert::IsFalse  (report.HasPartialTrack());
        Assert::IsTrue   (raw == out, L"and the bytes must round-trip exactly");
    }


    TEST_METHOD (Denibblize_OneBrokenDataField_FailsAndNamesTheDamage)
    {
        // The report has to be specific enough to act on: which track, and how
        // much of it. "Something went wrong somewhere" would leave the caller
        // with the same choice it had before -- write the buffer or don't.
        DiskImage         img;
        vector<Byte>      raw     = MakePinnedRandomImage (0xC0FFEEu);
        vector<Byte>      out;
        DenibblizeReport  report;
        HRESULT           hr      = S_OK;
        const int         kTrack  = 7;
        int               prologs = 0;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));

        prologs = BreakOneDataField (img, kTrack);
        Assert::IsTrue (prologs > 0,
            L"precondition: the track must have held a data field to damage, or this "
            L"test passes by having broken nothing");

        hr = NibblizationLayer::Denibblize (img, DiskFormat::Dsk, out, report);

        Assert::IsTrue (FAILED (hr),
            L"a partly decoded track must not report success -- the caller writes "
            L"this buffer over the user's file");

        Assert::AreEqual (1,  report.tracksPartial,   L"exactly one track is partial");
        Assert::AreEqual (34, report.tracksComplete,  L"the other 34 are untouched");
        Assert::AreEqual (0,  report.tracksUnformatted);
        Assert::AreEqual (1,  report.sectorsMissing,
            L"one damaged data field costs one sector, not the rest of the track");
        Assert::AreEqual (559, report.sectorsDecoded);

        Assert::AreEqual (15, CountDecoded (report.decodedSectorMask[kTrack]),
            L"and the report must name WHICH track lost it");
    }


    TEST_METHOD (Denibblize_OneBrokenDataField_DoesNotMisfileTheNextSector)
    {
        // One point of damage costs exactly one sector.
        //
        // It used to cost two. The scan for the missing data field ran on to
        // the NEXT sector's, decoded it cleanly, and filed it under the number
        // this sector's address field gave -- so a second sector came back
        // holding plausible, wrong data. Zeros a reader might notice; wrong
        // data it will not. The scan now stops when it meets the next address
        // field and rewinds, so the good sector after a damaged one is neither
        // stolen nor lost.
        DiskImage         img;
        vector<Byte>      raw    = MakePinnedRandomImage (0xC0FFEEu);
        vector<Byte>      out;
        DenibblizeReport  report;
        HRESULT           hr     = S_OK;
        const int         kTrack = 7;
        const size_t      kTrkSz = 16 * 256;
        int               wrong  = 0;
        int               zeroed = 0;
        int               sector = 0;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));
        Assert::IsTrue (BreakOneDataField (img, kTrack) > 0,
            L"precondition: a data field was actually damaged");

        hr = NibblizationLayer::Denibblize (img, DiskFormat::Dsk, out, report);
        Assert::IsTrue (FAILED (hr));

        for (sector = 0; sector < 16; sector++)
        {
            size_t  base    = static_cast<size_t> (kTrack) * kTrkSz
                            + static_cast<size_t> (sector) * 256;
            bool    matches = true;
            bool    allZero = true;
            size_t  i       = 0;

            for (i = 0; i < 256; i++)
            {
                if (out[base + i] != raw[base + i]) { matches = false; }
                if (out[base + i] != 0)             { allZero = false; }
            }

            if (!matches) { wrong++;  }
            if (allZero)  { zeroed++; }
        }

        Assert::AreEqual (1, zeroed, L"the damaged sector comes back as zeros");
        Assert::AreEqual (1, wrong,
            L"and it is the ONLY sector that differs -- the sector after it must "
            L"keep its own data rather than being filed under the damaged one's "
            L"number");
        Assert::AreEqual (1, report.sectorsMissing,
            L"and the report must agree that exactly one sector was lost");
    }


    TEST_METHOD (Denibblize_UnformattedTrack_StillSucceeds)
    {
        // The line between the two states. A track that decodes NOTHING is
        // unformatted, and for a sector image that legitimately is zeros -- so
        // it must not be reported as damage, or every blank track in a
        // freshly created disk would refuse to save.
        DiskImage         img;
        vector<Byte>      raw    = MakePinnedRandomImage (0x5A5A5A5Au);
        vector<Byte>      out;
        DenibblizeReport  report;
        const int         kWiped = 5;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));

        img.ResizeTrack (kWiped, DiskImage::kDefaultTrackByteSize * 8);
        {
            vector<Byte> &  bits = img.GetTrackBitsForWrite (kWiped);

            std::fill (bits.begin(), bits.end(), static_cast<Byte> (0));
        }

        img.SetTrackBitCount (kWiped, DiskImage::kDefaultTrackByteSize * 8);

        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, out, report),
            L"a wholly unformatted track is blank media, not damage");

        Assert::AreEqual (1,  report.tracksUnformatted);
        Assert::AreEqual (0,  report.tracksPartial, L"nothing decoded means nothing was lost");
        Assert::AreEqual (34, report.tracksComplete);
        Assert::AreEqual (0,  report.sectorsMissing);
        Assert::AreEqual (0,  CountDecoded (report.decodedSectorMask[kWiped]));
    }


    TEST_METHOD (Denibblize_PartialTrack_BlocksTheFlushThatWouldWriteIt)
    {
        // The consequence that matters, through the path a user actually hits:
        // Serialize refuses, so the store's flush reports the loss and keeps
        // the image dirty instead of replacing a good file with a damaged one.
        DiskImage     img;
        vector<Byte>  raw = MakePinnedRandomImage (0xC0FFEEu);
        vector<Byte>  out;
        HRESULT       hr  = S_OK;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (raw, img));
        Assert::IsTrue (BreakOneDataField (img, 7) > 0,
            L"precondition: a data field was actually damaged");

        hr = img.Serialize (out);

        Assert::IsTrue (FAILED (hr),
            L"Serialize must carry the refusal, since that is what the flush path checks");
    }
};

