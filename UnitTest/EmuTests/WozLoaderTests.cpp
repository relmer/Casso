#include "Pch.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/WozLoader.h"

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




TEST_CLASS (WozLoaderTests)
{
public:

    static constexpr size_t  kTestBitCount = 51200;   // ~6400 bytes / track 0

    vector<Byte> MakeBitStream()
    {
        vector<Byte>   bits ((kTestBitCount + 7) / 8, 0xFF);

        // Drop in a $D5 $AA $96 marker at offset 160 bits (after 20 sync bytes)
        // so AddressFieldFraming-style probes can find it.
        bits[20] = 0xD5;
        bits[21] = 0xAA;
        bits[22] = 0x96;

        return bits;
    }


    // Independent reference implementation of the WOZ header CRC (standard
    // reflected CRC-32) so the Serialize tests can check the writer's CRC
    // against a second source rather than trusting the code under test.
    uint32_t Crc32Ref (const Byte * data, size_t len)
    {
        uint32_t   crc = 0xFFFFFFFFu;

        for (size_t i = 0; i < len; i++)
        {
            crc ^= data[i];
            for (int b = 0; b < 8; b++)
            {
                crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
            }
        }

        return ~crc;
    }


    // Populate `slot` of a from-scratch WOZ DiskImage with a distinct,
    // recognizable bit pattern and map its four quarter-tracks to it.
    void FillTrack (DiskImage & img, int slot, size_t bitCount, Byte fill)
    {
        size_t  byteCount = 0;



        img.ResizeTrack (slot, bitCount);

        vector<Byte> &  buf       = img.GetTrackBitsForWrite (slot);
        byteCount = (bitCount + 7) / 8;

        for (size_t i = 0; i < byteCount && i < buf.size(); i++)
        {
            buf[i] = static_cast<Byte> (fill + (i & 0x1F));
        }

        img.SetTrackBitCount (slot, bitCount);
        for (int q = 0; q < 4; q++)
        {
            img.SetQuarterTrackSlot (slot * 4 + q, slot);
        }
    }


    // Compare track `slot`'s packed bytes across two images bit-for-bit.
    size_t TrackByteDiff (const DiskImage & a, const DiskImage & b, int slot, size_t bitCount)
    {
        size_t   byteCount = (bitCount + 7) / 8;
        size_t   diff      = 0;

        for (size_t i = 0; i < byteCount; i++)
        {
            Byte   av = 0;
            Byte   bv = 0;

            for (int bit = 0; bit < 8; bit++)
            {
                av = static_cast<Byte> ((av << 1) | (a.ReadBit (slot, i * 8 + bit) & 1));
                bv = static_cast<Byte> ((bv << 1) | (b.ReadBit (slot, i * 8 + bit) & 1));
            }

            if (av != bv)
            {
                diff++;
            }
        }

        return diff;
    }


    // Minimal single-track (track 0) WOZ *v1* image, to exercise the loader's
    // v1 path and confirm Serialize re-emits a reloadable v2 from a v1 source.
    // v1 layout: header(12) + INFO(8+60) + TMAP(8+160) + TRKS(8 + 6656/track);
    // each v1 TRK record is 6656 bytes with the bit stream at offset 0 and the
    // bit count (LE16) at offset 6648.
    void BuildSyntheticV1 (const vector<Byte> & trackZeroBits, size_t bitCount, vector<Byte> & out)
    {
        const size_t  kRec   = WozLoader::kV1TrackRecordSize;       // 6656
        const size_t  kTrks  = kRec;                                 // one track
        size_t        pos    = 0;
        size_t        trk    = 0;
        size_t        nbytes = (bitCount + 7) / 8;
        const Byte    sig[8] = { 'W', 'O', 'Z', '1', 0xFF, 0x0A, 0x0D, 0x0A };

        out.assign (12 + (8 + 60) + (8 + 160) + (8 + kTrks), 0);

        memcpy (out.data(), sig, 8);                                // header (CRC left 0)
        pos = 12;

        memcpy (out.data() + pos, "INFO", 4);
        out[pos + 4]     = 60;                                       // chunk size (LE)
        out[pos + 8 + 0] = 1;                                        // INFO version 1
        out[pos + 8 + 1] = 1;                                        // disk type 5.25"
        out[pos + 8 + 2] = 0;                                        // not write protected
        pos += 8 + 60;

        memcpy (out.data() + pos, "TMAP", 4);
        out[pos + 4] = 160;                                          // chunk size (LE)
        for (int qt = 0; qt < 160; qt++) { out[pos + 8 + qt] = 0xFF; }
        out[pos + 8 + 0] = 0;                                        // qt 0,1,3 -> whole track 0
        out[pos + 8 + 1] = 0;
        out[pos + 8 + 3] = 0;
        pos += 8 + 160;

        memcpy (out.data() + pos, "TRKS", 4);
        out[pos + 4] = static_cast<Byte> (kTrks & 0xFF);             // chunk size (LE16 fits)
        out[pos + 5] = static_cast<Byte> ((kTrks >> 8) & 0xFF);
        trk = pos + 8;
        for (size_t i = 0; i < nbytes && i < trackZeroBits.size(); i++)
        {
            out[trk + i] = trackZeroBits[i];
        }

        out[trk + 6648] = static_cast<Byte> (bitCount & 0xFF);       // bit count (LE16)
        out[trk + 6649] = static_cast<Byte> ((bitCount >> 8) & 0xFF);
    }

    // Walk the chunk table the way a CONFORMANT parser does: each chunk's
    // declared size must land exactly on the next chunk id, and the last must
    // land exactly on end-of-file. Casso's own Load stops at the first thing
    // that is not a chunk instead, and that tolerance is what hid a malformed
    // TRKS size -- so these guards must not lean on it.
    //
    // Returns the ids reached in order; a step that lands anywhere other than
    // a chunk id or EOF appends "!!" and stops.
    vector<string> StrictChunkWalk (const vector<Byte> & woz)
    {
        vector<string>   ids;
        size_t           pos = WozLoader::kHeaderSize;

        while (pos < woz.size())
        {
            uint32_t   size = 0;
            bool       isId = (pos + 8 <= woz.size());

            for (size_t i = 0; isId && i < 4; i++)
            {
                isId = (woz[pos + i] >= 'A' && woz[pos + i] <= 'Z');
            }

            if (!isId)
            {
                ids.push_back ("!!");
                break;
            }

            size = static_cast<uint32_t> (woz[pos + 4])
                 | (static_cast<uint32_t> (woz[pos + 5]) << 8)
                 | (static_cast<uint32_t> (woz[pos + 6]) << 16)
                 | (static_cast<uint32_t> (woz[pos + 7]) << 24);

            if (pos + 8 + size > woz.size())
            {
                ids.push_back ("!!");
                break;
            }

            ids.push_back (string (reinterpret_cast<const char *> (woz.data() + pos), 4));
            pos += 8 + size;
        }

        return ids;
    }


    // Locate a chunk by stepping the table from the header. Returns its
    // declared size and, via dataOffsetOut, where its payload starts.
    uint32_t FindChunk (const vector<Byte> & woz, const char * wanted, size_t & dataOffsetOut)
    {
        size_t   pos = WozLoader::kHeaderSize;

        dataOffsetOut = 0;

        while (pos + 8 <= woz.size())
        {
            uint32_t   size = static_cast<uint32_t> (woz[pos + 4])
                            | (static_cast<uint32_t> (woz[pos + 5]) << 8)
                            | (static_cast<uint32_t> (woz[pos + 6]) << 16)
                            | (static_cast<uint32_t> (woz[pos + 7]) << 24);

            if (memcmp (woz.data() + pos, wanted, 4) == 0)
            {
                dataOffsetOut = pos + 8;
                return size;
            }

            pos += 8 + size;
        }

        return 0;
    }


    // The byte span the TRK records themselves describe: the record table
    // plus every block the records claim, derived from the records rather
    // than from the file size, so it is an independent expectation.
    size_t TrksSpanFromRecords (const vector<Byte> & woz, size_t recordsOffset)
    {
        size_t   endBlock = WozLoader::kV2FirstDataBlock;

        for (size_t slot = 0; slot < WozLoader::kV2TrkRecordCount; slot++)
        {
            const Byte *  rec        = woz.data() + recordsOffset + slot * WozLoader::kV2TrkRecordSize;
            size_t        startBlock = static_cast<size_t> (rec[0]) | (static_cast<size_t> (rec[1]) << 8);
            size_t        blockCount = static_cast<size_t> (rec[2]) | (static_cast<size_t> (rec[3]) << 8);

            if (blockCount != 0 && startBlock + blockCount > endBlock)
            {
                endBlock = startBlock + blockCount;
            }
        }

        return WozLoader::kV2TrkRecordCount * WozLoader::kV2TrkRecordSize
             + (endBlock - WozLoader::kV2FirstDataBlock) * WozLoader::kV2BlockSize;
    }


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


    ////////////////////////////////////////////////////////////////////////
    //
    //  GH #88 follow-up -- WozLoader::Serialize (write-back). The WOZ arm of
    //  DiskImage::Serialize used to return the untouched source bytes, so
    //  guest writes were silently discarded on flush. These lock down the
    //  real serializer: faithful round-trip, guest writes reflected,
    //  write-protect preserved, a valid header CRC, and multi-track output.
    //
    ////////////////////////////////////////////////////////////////////////

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


    ////////////////////////////////////////////////////////////////////////
    //
    //  TRKS chunk size. Both writers sized TRKS to the 160-record table
    //  alone (1280 bytes), leaving the block-aligned bit streams outside the
    //  chunk they belong to. A conformant parser then adds 1280 to the chunk
    //  start, lands in the middle of track data, sees no valid id and stops
    //  -- so every chunk after TRKS was unreachable to every tool but ours.
    //  Casso read its own output back only because Load stops at the first
    //  non-chunk, which is exactly the state a short TRKS size produces.
    //
    ////////////////////////////////////////////////////////////////////////

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


    ////////////////////////////////////////////////////////////////////////
    //
    //  Header CRC on load. Serialize always stamps a freshly computed CRC
    //  and Load never checked the stored one, so a damaged image opened
    //  silently and was written back correctly checksummed -- laundering the
    //  damage into a file nothing could flag. Load now validates and records
    //  the result WITHOUT refusing the image: being able to open a damaged
    //  preservation dump is the point of the tool.
    //
    ////////////////////////////////////////////////////////////////////////

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
};

