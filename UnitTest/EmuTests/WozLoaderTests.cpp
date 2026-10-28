#include "Pch.h"
#include "WozLoaderTestSupport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WozLoaderTests
//
//  Phase 10 / FR-022. Validates the chunked WOZ loader against synthetic
//  in-memory v2 images. No host fixture file is required — every test
//  builds its own bytes via WozLoader::BuildSyntheticV2 (the same helper
//  is used by DiskImageStoreTests).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WozLoaderTests), public WozLoaderTestSupport
{
public:

    TEST_METHOD (LoadRejectsTruncatedFile)
    {
        DiskImage  img;
        HRESULT    hr  = S_OK;
        vector<Byte>  bytes (4, 0);

        hr = WozLoader::Load (bytes, img);

        AssertFailed (hr);
    }


    TEST_METHOD (LoadRejectsBadSignature)
    {
        DiskImage  img;
        HRESULT    hr  = S_OK;
        vector<Byte>  bytes (32, 0);

        bytes[0] = 'W'; bytes[1] = 'O'; bytes[2] = 'Z'; bytes[3] = '9';

        hr = WozLoader::Load (bytes, img);

        AssertFailed (hr, L"Unknown WOZ version magic must be rejected");
    }


    TEST_METHOD (LoadRejectsMissingMandatoryChunks)
    {
        HRESULT  hr = S_OK;



        // Only a header — no INFO/TMAP/TRKS chunks.
        DiskImage     img;
        vector<Byte>  bytes (12, 0);

        bytes[0] = 'W'; bytes[1] = 'O'; bytes[2] = 'Z'; bytes[3] = '2';
        bytes[4] = 0xFF; bytes[5] = 0x0A; bytes[6] = 0x0D; bytes[7] = 0x0A;

        hr = WozLoader::Load (bytes, img);

        AssertFailed (hr);
    }


    TEST_METHOD (LoadAcceptsSyntheticV2)
    {
        DiskImage     img;
        vector<Byte>  bitStream = MakeBitStream();
        vector<Byte>  woz;
        HRESULT       hr        = S_OK;

        AssertSucceeded (WozLoader::BuildSyntheticV2 (
            1, false, bitStream, kTestBitCount, woz));

        hr = WozLoader::Load (woz, img);

        AssertSucceeded (hr, L"Synthetic v2 WOZ must load");
        Assert::IsTrue (img.GetSourceFormat() == DiskFormat::Woz);
        Assert::AreEqual (kTestBitCount, img.GetTrackBitCount (0),
            L"Track 0 bit count must match TRK record");
    }


    TEST_METHOD (LoadHonorsWriteProtectFlagInInfoChunk)
    {
        DiskImage     img;
        vector<Byte>  bitStream = MakeBitStream();
        vector<Byte>  woz;

        AssertSucceeded (WozLoader::BuildSyntheticV2 (
            1, true, bitStream, kTestBitCount, woz));

        AssertSucceeded (WozLoader::Load (woz, img));
        Assert::IsTrue (img.IsWriteProtected(),
            L"INFO chunk write_protected=1 must surface on the DiskImage");
    }


    TEST_METHOD (LoadCopiesBitStreamFaithfully)
    {
        DiskImage     img;
        vector<Byte>  bitStream = MakeBitStream();
        vector<Byte>  woz;
        size_t        i         = 0;
        size_t        byteCount = (kTestBitCount + 7) / 8;
        size_t        diff      = 0;

        AssertSucceeded (WozLoader::BuildSyntheticV2 (
            1, false, bitStream, kTestBitCount, woz));
        AssertSucceeded (WozLoader::Load (woz, img));

        for (i = 0; i < byteCount; i++)
        {
            Byte   actual = 0;
            int    b      = 0;

            for (b = 0; b < 8; b++)
            {
                Byte   bit = img.ReadBit (0, i * 8 + b);
                actual = static_cast<Byte> ((actual << 1) | (bit & 1));
            }

            if (actual != bitStream[i])
            {
                diff++;
            }
        }

        Assert::AreEqual (size_t (0), diff,
            L"WOZ bit stream must round-trip into DiskImage byte-for-byte");
    }


    TEST_METHOD (LoadRejectsMalformedChunkSize)
    {
        DiskImage     img;
        vector<Byte>  bitStream = MakeBitStream();
        vector<Byte>  woz;
        HRESULT       hr        = S_OK;

        AssertSucceeded (WozLoader::BuildSyntheticV2 (
            1, false, bitStream, kTestBitCount, woz));

        // Corrupt the INFO chunk size to overflow the file.
        woz[12 + 4] = 0xFF;
        woz[12 + 5] = 0xFF;
        woz[12 + 6] = 0xFF;
        woz[12 + 7] = 0x00;

        hr = WozLoader::Load (woz, img);

        AssertFailed (hr, L"Chunk size that runs past EOF must be rejected");
    }


    TEST_METHOD (LoadIgnoresOptionalMetaChunk)
    {
        HRESULT  hr = S_OK;



        // Append a META chunk after the build helper output — the loader
        // must silently ignore it (not fail).
        DiskImage     img;
        vector<Byte>  bitStream = MakeBitStream();
        vector<Byte>  woz;

        AssertSucceeded (WozLoader::BuildSyntheticV2 (
            1, false, bitStream, kTestBitCount, woz));

        // Append META with empty payload.
        woz.push_back ('M');
        woz.push_back ('E');
        woz.push_back ('T');
        woz.push_back ('A');
        woz.push_back (0); woz.push_back (0); woz.push_back (0); woz.push_back (0);

        hr = WozLoader::Load (woz, img);

        AssertSucceeded (hr, L"META is optional, must not cause load failure");
    }


    TEST_METHOD (BuildSyntheticV2_ProducesAtLeastFourBlocks)
    {
        vector<Byte>  woz;



        // Sanity: smallest synthetic still has header + INFO + TMAP + TRKS
        // payload + 1 block of bit data.
        vector<Byte>  bitStream (8, 0);

        AssertSucceeded (WozLoader::BuildSyntheticV2 (1, false, bitStream, 64, woz));
        Assert::IsTrue (woz.size() >= 4 * WozLoader::kV2BlockSize);

        // Signature bytes intact.
        Assert::AreEqual (static_cast<Byte> ('W'), woz[0]);
        Assert::AreEqual (static_cast<Byte> ('O'), woz[1]);
        Assert::AreEqual (static_cast<Byte> ('Z'), woz[2]);
        Assert::AreEqual (static_cast<Byte> ('2'), woz[3]);
    }


    TEST_METHOD (Serialize_RoundTripsBitStreamByteForByte)
    {
        DiskImage     src;
        DiskImage     reloaded;
        vector<Byte>  woz;
        vector<Byte>  reserialized;

        AssertSucceeded (WozLoader::BuildSyntheticV2 (
            1, false, MakeBitStream(), kTestBitCount, woz));
        AssertSucceeded (WozLoader::Load (woz, src));

        AssertSucceeded (WozLoader::Serialize (src, reserialized));
        AssertSucceeded (WozLoader::Load (reserialized, reloaded));

        Assert::AreEqual (kTestBitCount, reloaded.GetTrackBitCount (0));
        Assert::AreEqual (size_t (0), TrackByteDiff (src, reloaded, 0, kTestBitCount),
            L"Serialize->Load must reproduce track 0 bit-for-bit");
    }


    TEST_METHOD (Serialize_ReflectsGuestWrites)
    {
        DiskImage     src;
        DiskImage     reloaded;
        vector<Byte>  woz;
        vector<Byte>  reserialized;
        const size_t  flippedBit = 200;   // inside track 0, clear of the sync marker

        AssertSucceeded (WozLoader::BuildSyntheticV2 (
            1, false, MakeBitStream(), kTestBitCount, woz));
        AssertSucceeded (WozLoader::Load (woz, src));

        // Bit starts at 1 (0xFF fill); the guest writes a 0.
        Assert::AreEqual (Byte (1), src.ReadBit (0, flippedBit));
        src.WriteBit (0, flippedBit, 0);

        AssertSucceeded (WozLoader::Serialize (src, reserialized));
        AssertSucceeded (WozLoader::Load (reserialized, reloaded));

        Assert::AreEqual (Byte (0), reloaded.ReadBit (0, flippedBit),
            L"A guest WriteBit must survive Serialize->Load (the old bug lost it)");
        Assert::AreEqual (Byte (1), reloaded.ReadBit (0, flippedBit + 1),
            L"Neighboring bits must be untouched");
    }


    TEST_METHOD (Serialize_PreservesWriteProtectFlag)
    {
        DiskImage     wp;
        DiskImage     rw;
        DiskImage     wpReloaded;
        DiskImage     rwReloaded;
        vector<Byte>  wpWoz;
        vector<Byte>  rwWoz;
        vector<Byte>  wpOut;
        vector<Byte>  rwOut;

        AssertSucceeded (WozLoader::BuildSyntheticV2 (1, true,  MakeBitStream(), kTestBitCount, wpWoz));
        AssertSucceeded (WozLoader::BuildSyntheticV2 (1, false, MakeBitStream(), kTestBitCount, rwWoz));
        AssertSucceeded (WozLoader::Load (wpWoz, wp));
        AssertSucceeded (WozLoader::Load (rwWoz, rw));

        AssertSucceeded (WozLoader::Serialize (wp, wpOut));
        AssertSucceeded (WozLoader::Serialize (rw, rwOut));
        AssertSucceeded (WozLoader::Load (wpOut, wpReloaded));
        AssertSucceeded (WozLoader::Load (rwOut, rwReloaded));

        Assert::IsTrue  (wpReloaded.IsWriteProtected(), L"write-protect must survive serialization");
        Assert::IsFalse (rwReloaded.IsWriteProtected(), L"a writable disk must not gain protection");
    }


    TEST_METHOD (Serialize_WritesValidHeaderCrc)
    {
        DiskImage     src;
        vector<Byte>  woz;
        vector<Byte>  out;

        AssertSucceeded (WozLoader::BuildSyntheticV2 (
            1, false, MakeBitStream(), kTestBitCount, woz));
        AssertSucceeded (WozLoader::Load (woz, src));
        AssertSucceeded (WozLoader::Serialize (src, out));

        uint32_t   stored   = static_cast<uint32_t> (out[8])
                            | (static_cast<uint32_t> (out[9])  << 8)
                            | (static_cast<uint32_t> (out[10]) << 16)
                            | (static_cast<uint32_t> (out[11]) << 24);
        uint32_t   expected = Crc32Ref (out.data() + WozLoader::kHeaderSize,
                                        out.size() - WozLoader::kHeaderSize);

        Assert::AreEqual (expected, stored, L"Header CRC32 must cover all post-header bytes");
        Assert::AreNotEqual (uint32_t (0), stored, L"A populated image must not emit a zero CRC");
    }


    TEST_METHOD (Serialize_MultiTrack_RoundTripsEachTrackAndTmap)
    {
        DiskImage     src;
        DiskImage     reloaded;
        vector<Byte>  out;
        const size_t  bits = 4096;

        src.SetSourceFormat  (DiskFormat::Woz);
        src.ClearQuarterTrackMap();
        src.EnsureTrackSlots (3);
        FillTrack (src, 0, bits, 0x10);
        FillTrack (src, 1, bits, 0x40);
        FillTrack (src, 2, bits, 0x90);

        AssertSucceeded (WozLoader::Serialize (src, out));
        AssertSucceeded (WozLoader::Load (out, reloaded));

        for (int slot = 0; slot < 3; slot++)
        {
            Assert::AreEqual (bits, reloaded.GetTrackBitCount (slot));
            Assert::AreEqual (size_t (0), TrackByteDiff (src, reloaded, slot, bits),
                L"Each populated track must round-trip");
            // The quarter-track map must resolve each track's phases back to it.
            Assert::AreEqual (slot, reloaded.ResolveQuarterTrack (slot * 4));
        }
    }


    TEST_METHOD (Serialize_HalfTrackMap_RoundTripsQuarterTrackResolution)
    {
        // Non-whole-track TMAP (a copy-protection layout): qt 0 -> slot 0,
        // qt 2 -> slot 1 (a half-track). Serialize must rebuild the TMAP from
        // the quarter-track map so resolution survives.
        DiskImage     src;
        DiskImage     reloaded;
        vector<Byte>  out;
        const size_t  bits = 2048;

        src.SetSourceFormat  (DiskFormat::Woz);
        src.ClearQuarterTrackMap();
        src.EnsureTrackSlots (2);

        src.ResizeTrack (0, bits);
        { vector<Byte> & b = src.GetTrackBitsForWrite (0); for (size_t i = 0; i < b.size(); i++) { b[i] = static_cast<Byte> (0x10 + (i & 0x1F)); } }
        src.SetTrackBitCount (0, bits);

        src.ResizeTrack (1, bits);
        { vector<Byte> & b = src.GetTrackBitsForWrite (1); for (size_t i = 0; i < b.size(); i++) { b[i] = static_cast<Byte> (0x50 + (i & 0x1F)); } }
        src.SetTrackBitCount (1, bits);

        src.SetQuarterTrackSlot (0, 0);
        src.SetQuarterTrackSlot (2, 1);

        AssertSucceeded (WozLoader::Serialize (src, out));
        AssertSucceeded (WozLoader::Load (out, reloaded));

        Assert::AreEqual (0, reloaded.ResolveQuarterTrack (0), L"qt0 -> slot 0");
        Assert::AreEqual (1, reloaded.ResolveQuarterTrack (2), L"qt2 -> slot 1 (half-track)");
        Assert::AreEqual (size_t (0), TrackByteDiff (src, reloaded, 0, bits));
        Assert::AreEqual (size_t (0), TrackByteDiff (src, reloaded, 1, bits));
    }


    TEST_METHOD (Serialize_GapSlot_EmptyTrackPreserved)
    {
        // Slots 0 and 2 populated, slot 1 left empty (an unformatted track
        // between two formatted ones). The empty slot must stay empty and the
        // populated ones must round-trip.
        DiskImage     src;
        DiskImage     reloaded;
        vector<Byte>  out;
        const size_t  bits = 2048;

        src.SetSourceFormat  (DiskFormat::Woz);
        src.ClearQuarterTrackMap();
        src.EnsureTrackSlots (3);
        FillTrack (src, 0, bits, 0x10);
        FillTrack (src, 2, bits, 0x90);   // slot 1 intentionally left empty

        AssertSucceeded (WozLoader::Serialize (src, out));
        AssertSucceeded (WozLoader::Load (out, reloaded));

        Assert::AreEqual (size_t (0), reloaded.GetTrackBitCount (1), L"gap slot must stay empty");
        Assert::AreEqual (bits, reloaded.GetTrackBitCount (0));
        Assert::AreEqual (bits, reloaded.GetTrackBitCount (2));
        Assert::AreEqual (size_t (0), TrackByteDiff (src, reloaded, 0, bits));
        Assert::AreEqual (size_t (0), TrackByteDiff (src, reloaded, 2, bits));
    }


    TEST_METHOD (Serialize_ViaDiskImageDispatch_ProducesReloadableWoz)
    {
        // Exercise the actual flush entry point: DiskImage::Serialize's WOZ
        // arm (not WozLoader::Serialize directly) must route to the writer.
        DiskImage     src;
        DiskImage     reloaded;
        vector<Byte>  woz;
        vector<Byte>  out;

        AssertSucceeded (WozLoader::BuildSyntheticV2 (
            1, false, MakeBitStream(), kTestBitCount, woz));
        AssertSucceeded (WozLoader::Load (woz, src));

        AssertSucceeded (src.Serialize (out));
        AssertSucceeded (WozLoader::Load (out, reloaded));

        Assert::AreEqual (kTestBitCount, reloaded.GetTrackBitCount (0));
        Assert::AreEqual (size_t (0), TrackByteDiff (src, reloaded, 0, kTestBitCount));
    }


    TEST_METHOD (Serialize_FromV1Source_EmitsReloadableV2)
    {
        // A v1 WOZ loads, and Serialize re-emits a v2 image (we always write
        // v2) whose track data still round-trips.
        DiskImage     src;
        DiskImage     reloaded;
        vector<Byte>  v1;
        vector<Byte>  out;

        BuildSyntheticV1 (MakeBitStream(), kTestBitCount, v1);
        AssertSucceeded (WozLoader::Load (v1, src));
        Assert::AreEqual (kTestBitCount, src.GetTrackBitCount (0), L"v1 source must load");

        AssertSucceeded (WozLoader::Serialize (src, out));
        Assert::AreEqual (static_cast<Byte> ('2'), out[3], L"output signature must be WOZ2");

        AssertSucceeded (WozLoader::Load (out, reloaded));
        Assert::AreEqual (kTestBitCount, reloaded.GetTrackBitCount (0));
        Assert::AreEqual (size_t (0), TrackByteDiff (src, reloaded, 0, kTestBitCount),
            L"v1 -> v2 serialize must preserve the track bit stream");
    }


    TEST_METHOD (Serialize_TrksChunkSizeSpansRecordsAndPayload)
    {
        DiskImage     src;
        vector<Byte>  out;
        const size_t  bits          = 4096;
        size_t        recordsOffset = 0;
        uint32_t      declared      = 0;

        src.SetSourceFormat  (DiskFormat::Woz);
        src.ClearQuarterTrackMap();
        src.EnsureTrackSlots (3);
        FillTrack (src, 0, bits, 0x10);
        FillTrack (src, 1, bits, 0x40);
        FillTrack (src, 2, bits, 0x90);

        AssertSucceeded (WozLoader::Serialize (src, out));

        declared = FindChunk (out, "TRKS", recordsOffset);

        Assert::AreEqual (static_cast<uint32_t> (TrksSpanFromRecords (out, recordsOffset)), declared,
            L"TRKS size must span the record table plus the payload its records claim");
        Assert::AreEqual (out.size(), recordsOffset + declared,
            L"TRKS is the last chunk, so its span must reach exactly end-of-file");
        Assert::IsTrue (declared > WozLoader::kV2TrkRecordCount * WozLoader::kV2TrkRecordSize,
            L"A populated image's TRKS must be larger than the bare record table");
    }


    TEST_METHOD (Serialize_StrictChunkWalkReachesEndOfFile)
    {
        DiskImage       src;
        vector<Byte>    out;
        vector<string>  ids;

        src.SetSourceFormat  (DiskFormat::Woz);
        src.ClearQuarterTrackMap();
        src.EnsureTrackSlots (2);
        FillTrack (src, 0, 4096, 0x10);
        FillTrack (src, 1, 4096, 0x40);

        AssertSucceeded (WozLoader::Serialize (src, out));

        ids = StrictChunkWalk (out);

        Assert::AreEqual (size_t (3), ids.size(), L"A strict walk must reach INFO, TMAP, TRKS and stop at EOF");
        Assert::AreEqual (string ("INFO"), ids[0]);
        Assert::AreEqual (string ("TMAP"), ids[1]);
        Assert::AreEqual (string ("TRKS"), ids[2]);
    }


    TEST_METHOD (Serialize_ChunkAfterTrksIsReachable)
    {
        // The point of the size fix: a chunk written after TRKS -- META is
        // the one that matters -- must be reachable by walking the table,
        // not just by scanning for its magic.
        DiskImage       src;
        vector<Byte>    out;
        vector<string>  ids;
        const Byte      meta[] = { 'M', 'E', 'T', 'A', 0, 0, 0, 0 };

        src.SetSourceFormat  (DiskFormat::Woz);
        src.ClearQuarterTrackMap();
        src.EnsureTrackSlots (1);
        FillTrack (src, 0, 4096, 0x10);

        AssertSucceeded (WozLoader::Serialize (src, out));
        out.insert (out.end(), meta, meta + sizeof (meta));

        ids = StrictChunkWalk (out);

        Assert::AreEqual (size_t (4), ids.size(), L"A strict walk must reach the chunk after TRKS");
        Assert::AreEqual (string ("META"), ids[3],
            L"TRKS must not swallow or orphan the chunk that follows it");
    }


    TEST_METHOD (BuildSyntheticV2_TrksChunkSizeSpansRecordsAndPayload)
    {
        vector<Byte>    woz;
        vector<string>  ids;
        size_t          recordsOffset = 0;
        uint32_t        declared      = 0;

        AssertSucceeded (WozLoader::BuildSyntheticV2 (
            1, false, MakeBitStream(), kTestBitCount, woz));

        declared = FindChunk (woz, "TRKS", recordsOffset);

        Assert::AreEqual (static_cast<uint32_t> (TrksSpanFromRecords (woz, recordsOffset)), declared,
            L"The synthetic builder must size TRKS the same way the real writer does");
        Assert::AreEqual (woz.size(), recordsOffset + declared,
            L"TRKS must span the synthetic image's payload to end-of-file");

        ids = StrictChunkWalk (woz);

        Assert::AreEqual (size_t (3), ids.size());
        Assert::AreEqual (string ("TRKS"), ids[2]);
    }


    TEST_METHOD (Load_CrcMismatch_LoadsAnywayAndFlagsTheImage)
    {
        DiskImage     src;
        DiskImage     damaged;
        vector<Byte>  woz;
        vector<Byte>  out;

        AssertSucceeded (WozLoader::BuildSyntheticV2 (
            1, false, MakeBitStream(), kTestBitCount, woz));
        AssertSucceeded (WozLoader::Load (woz, src));
        AssertSucceeded (WozLoader::Serialize (src, out));   // stamps a real CRC

        // Corrupt one byte of track data, leaving the stored CRC describing
        // what the file USED to be -- exactly what a damaged dump looks like.
        out[out.size() - 1] = static_cast<Byte> (out[out.size() - 1] ^ 0xFF);

        AssertSucceeded (WozLoader::Load (out, damaged),
            L"a CRC mismatch must not refuse the image");
        Assert::IsTrue (damaged.HasSourceCrcMismatch(),
            L"the mismatch must be recorded so a later flush can warn about it");
        Assert::AreEqual (kTestBitCount, damaged.GetTrackBitCount (0),
            L"the readable content must still load");
    }


    TEST_METHOD (Load_ValidCrc_LeavesTheImageUnflagged)
    {
        DiskImage     src;
        DiskImage     reloaded;
        vector<Byte>  woz;
        vector<Byte>  out;

        AssertSucceeded (WozLoader::BuildSyntheticV2 (
            1, false, MakeBitStream(), kTestBitCount, woz));
        AssertSucceeded (WozLoader::Load (woz, src));
        AssertSucceeded (WozLoader::Serialize (src, out));
        AssertSucceeded (WozLoader::Load (out, reloaded));

        Assert::IsFalse (reloaded.HasSourceCrcMismatch(),
            L"our own writer's output must validate against its own stored CRC");
    }


    TEST_METHOD (Load_ZeroCrc_SkipsValidationPerTheFormat)
    {
        // A stored CRC of zero means "none computed" and must not be read as
        // a mismatch -- BuildSyntheticV2 writes zero, as do real tools that
        // decline to checksum.
        DiskImage     img;
        vector<Byte>  woz;

        AssertSucceeded (WozLoader::BuildSyntheticV2 (
            1, false, MakeBitStream(), kTestBitCount, woz));
        Assert::AreEqual (uint32_t (0),
            static_cast<uint32_t> (woz[8]) | (static_cast<uint32_t> (woz[9]) << 8)
            | (static_cast<uint32_t> (woz[10]) << 16) | (static_cast<uint32_t> (woz[11]) << 24),
            L"precondition: the synthetic builder stores no CRC");

        AssertSucceeded (WozLoader::Load (woz, img));

        Assert::IsFalse (img.HasSourceCrcMismatch(),
            L"a zero CRC is 'not computed', not 'does not match'");
    }


    TEST_METHOD (SetWriteProtectFlag_ChangesOnlyTheFlagAndTheChecksum)
    {
        vector<Byte>  woz;
        vector<Byte>  original;
        size_t        infoOffset = 0;
        size_t        i          = 0;
        size_t        flagAt     = 0;
        size_t        diffs      = 0;
        bool          crcMoved   = false;

        // An image shaped like a real preservation dump: rich INFO fields and
        // a META chunk, both of which the rebuild path had to be taught to
        // keep and which this path must not depend on having been taught.
        BuildSyntheticV2WithChunks (MakeBitStream(), kTestBitCount,
                                    "Passport.py by 4am (2019-02-17)",
                                    { { "META", SampleMeta() } }, woz);

        FindChunk (woz, "INFO", infoOffset);
        Assert::IsTrue (infoOffset != 0, L"precondition: the image has an INFO chunk");
        flagAt   = infoOffset + 2;
        original = woz;

        AssertSucceeded (WozLoader::SetWriteProtectFlag (woz, false));

        Assert::AreEqual (original.size(), woz.size(), L"a one-byte edit must not resize the file");

        for (i = 0; i < original.size(); i++)
        {
            if (original[i] == woz[i])
            {
                continue;
            }

            diffs++;

            if (i >= 8 && i < 12)
            {
                crcMoved = true;
                continue;
            }

            Assert::AreEqual (flagAt, i,
                L"the only byte outside the header checksum that may change is the flag");
        }

        Assert::IsTrue (crcMoved, L"the stored checksum must be recomputed");
        Assert::AreEqual (Byte (0), woz[flagAt], L"the flag must actually be cleared");

        // Said explicitly, because it is the thing that used to break:
        Assert::IsTrue (ChunkPayload (woz, "META") == AsBytes (SampleMeta()),
            L"META must be untouched -- this path never reads it");
        Assert::AreEqual (string ("Passport.py by 4am (2019-02-17)"),
            string (reinterpret_cast<const char *> (woz.data() + infoOffset + 5), 31),
            L"and so must the creator field");
    }


    TEST_METHOD (SetWriteProtectFlag_LeavesTheFileValidatingAgainstItsOwnChecksum)
    {
        vector<Byte>  woz;
        DiskImage     reloaded;
        uint32_t      stored = 0;

        BuildSyntheticV2WithChunks (MakeBitStream(), kTestBitCount, "Applesauce v1.0.6",
                                    { { "META", SampleMeta() } }, woz);

        AssertSucceeded (WozLoader::SetWriteProtectFlag (woz, true));

        stored = static_cast<uint32_t> (woz[8])
               | (static_cast<uint32_t> (woz[9]) << 8)
               | (static_cast<uint32_t> (woz[10]) << 16)
               | (static_cast<uint32_t> (woz[11]) << 24);

        Assert::AreNotEqual (uint32_t (0), stored,
            L"the checksum must be recomputed, not zeroed -- zero means 'not computed', "
            L"which would silently retire the file's own damage check");
        Assert::AreEqual (Crc32Ref (woz.data() + WozLoader::kHeaderSize,
                                    woz.size() - WozLoader::kHeaderSize), stored,
            L"and it must match the patched contents");

        AssertSucceeded (WozLoader::Load (woz, reloaded));
        Assert::IsFalse (reloaded.HasSourceCrcMismatch(),
            L"so loading the patched file must not report damage");
        Assert::IsTrue (reloaded.GetWriteProtectInfo().imageFlag,
            L"and the flag must read back as set");
    }


    TEST_METHOD (SetWriteProtectFlag_OnAV1File_DoesNotUpgradeTheContainer)
    {
        // An edit of one field is not a reason to rewrite the file in a newer
        // format. Serialize converts v1 to v2 by design; this must not.
        vector<Byte>  woz;

        BuildSyntheticV1 (MakeBitStream(), kTestBitCount, woz);

        AssertSucceeded (WozLoader::SetWriteProtectFlag (woz, true));

        Assert::AreEqual (string ("WOZ1"),
            string (reinterpret_cast<const char *> (woz.data()), 4),
            L"a v1 file must stay v1 -- this edits a byte, it does not convert");
        Assert::AreEqual (Byte (1), woz[WozLoader::kHeaderSize + kChunkHeader + 2],
            L"and the flag must still be set");
        Assert::AreEqual (Byte (1), woz[WozLoader::kHeaderSize + kChunkHeader + 0],
            L"INFO must still hold version 1");
    }


    TEST_METHOD (SetWriteProtectFlag_RejectsBytesThatAreNotAWoz)
    {
        vector<Byte>  notAWoz (600, 0x42);
        vector<Byte>  original;
        vector<Byte>  truncated (4, 0);
        HRESULT       hr = S_OK;

        original = notAWoz;

        hr = WozLoader::SetWriteProtectFlag (notAWoz, true);

        Assert::IsTrue (FAILED (hr), L"a file with no WOZ signature must be refused");
        Assert::IsTrue (notAWoz == original,
            L"and refusing must leave every byte alone rather than half-patching it");

        hr = WozLoader::SetWriteProtectFlag (truncated, true);
        Assert::IsTrue (FAILED (hr), L"bytes too short to hold a header must be refused");
    }


    TEST_METHOD (SetWriteProtectFlag_RejectsAWozWithNoInfoChunk)
    {
        // Degraded input must fail rather than patch a guessed offset. A
        // fixed offset would "work" on every well-formed file and quietly
        // corrupt a malformed one.
        vector<Byte>  woz;
        vector<Byte>  original;
        HRESULT       hr = S_OK;

        BuildSyntheticV2WithChunks (MakeBitStream(), kTestBitCount, "Applesauce v1.0.6", {}, woz);

        // Rename INFO so the walk cannot find it, leaving the chunk sizes and
        // everything else intact.
        memcpy (woz.data() + WozLoader::kHeaderSize, "XXXX", 4);
        original = woz;

        hr = WozLoader::SetWriteProtectFlag (woz, true);

        Assert::IsTrue (FAILED (hr), L"no INFO chunk means there is no flag to set");
        Assert::IsTrue (woz == original, L"and nothing may be written on the way to failing");
    }


    TEST_METHOD (Describe_AnswersNothingForBytesThatAreNotAWoz)
    {
        WozLoader::Description  woz;
        vector<Byte>            notAWoz (200, 0x42);

        //  Nothing here fails: the caller has already been told the image could
        //  not be used, and a second error code adds nothing it can act on.
        WozLoader::Describe (notAWoz, woz);

        Assert::IsFalse (woz.isWoz,   L"a non-WOZ is reported as one, not guessed at");
        Assert::AreEqual (0, woz.wozVersion);
        Assert::AreEqual (size_t (0), woz.meta.size());
    }


    TEST_METHOD (Describe_ReadsWhatTheImageSaysAboutItself_FromInfoTmapAndMeta)
    {
        WozLoader::Description  woz;
        vector<Byte>            image;

        //  No trailing newline on the last pair, deliberately: real images are
        //  written both ways and a reader that needs one loses whichever key
        //  the writer happened to put last.
        BuildChunksOnlyV2 (2, WozLoader::kDiskType525, true, WozLoader::kBootSector16,
                           "Applesauce v1.2.5",
                           "title\tKarateka\n"
                           "publisher\tBroderbund Software\n"
                           "requires_ram\t48K",
                           8, image);

        WozLoader::Describe (image, woz);

        Assert::IsTrue (woz.isWoz,             L"the signature is recognized");
        Assert::AreEqual (2, woz.wozVersion,   L"and its version");
        Assert::AreEqual (2, woz.infoVersion,  L"INFO reports its own version separately");
        Assert::IsTrue (woz.writeProtected,    L"the write-protect flag is read");
        Assert::IsTrue (woz.synchronized,      L"so is synchronized");
        Assert::IsTrue (woz.cleaned,           L"and cleaned");
        Assert::IsTrue (woz.hasBootSectorFormat, L"a v2 INFO carries the boot-sector field");
        Assert::AreEqual (WozLoader::kBootSector16, woz.bootSectorFormat);

        Assert::AreEqual (std::string ("Applesauce v1.2.5"), woz.creator,
                          L"the creator field loses its padding, not its content");

        Assert::AreEqual (8, woz.quarterTracksWithData, L"TMAP positions holding data");
        Assert::AreEqual (8, woz.trackSlotsWithData,    L"and the distinct slots behind them");

        Assert::AreEqual (size_t (3), woz.meta.size(), L"every META pair, the last one included");
        Assert::AreEqual (std::string ("title"),     woz.meta[0].key);
        Assert::AreEqual (std::string ("Karateka"),  woz.meta[0].value);
        Assert::AreEqual (std::string ("requires_ram"), woz.meta[2].key);
        Assert::AreEqual (std::string ("48K"),          woz.meta[2].value);
    }


    TEST_METHOD (Describe_RecordsAV1InfoAsHavingNoBootSectorField_RatherThanAnUnknownOne)
    {
        WozLoader::Description  woz;
        vector<Byte>            image;

        //  Version 1 of the INFO chunk stops before that field. Reporting it as
        //  the "unknown" value would say the image answered when it did not,
        //  and a reader cannot tell those two apart afterwards.
        BuildChunksOnlyV2 (1, WozLoader::kDiskType525, false, WozLoader::kBootSector13,
                           "Passport.py by 4am", "title\tSpace Quarks\n", 4, image);

        WozLoader::Describe (image, woz);

        Assert::AreEqual (1, woz.infoVersion);
        Assert::IsFalse (woz.hasBootSectorFormat,
                         L"a v1 INFO has no boot-sector field to report");
    }
};
