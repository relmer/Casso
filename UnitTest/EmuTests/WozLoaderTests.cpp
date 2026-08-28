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

    //  A WOZ v2 header with an INFO, a TMAP and a META chunk and nothing else.
    //
    //  Describe reads chunks and never touches track data, so an image with no
    //  TRKS at all is a legitimate subject for it -- and building one keeps the
    //  test about what the chunks say rather than about track encoding.
    void BuildChunksOnlyV2 (Byte           infoVersion,
                            Byte           diskType,
                            bool           writeProtected,
                            Byte           bootSectorFormat,
                            const char *   creator,
                            const char *   metaText,
                            int            quarterTracksWithData,
                            vector<Byte> & out)
    {
        const Byte  sig[8]     = { 'W', 'O', 'Z', '2', 0xFF, 0x0A, 0x0D, 0x0A };
        size_t      metaLength = strlen (metaText);
        int         qt         = 0;

        out.assign (sig, sig + 8);
        out.insert (out.end(), 4, 0);                       // header CRC, unchecked here

        // INFO
        out.insert (out.end(), { 'I', 'N', 'F', 'O' });
        out.insert (out.end(), { 60, 0, 0, 0 });

        {
            vector<Byte>  info (WozLoader::kInfoChunkSize, 0);

            info[WozLoader::kInfoOffsetVersion]        = infoVersion;
            info[WozLoader::kInfoOffsetDiskType]       = diskType;
            info[WozLoader::kInfoOffsetWriteProtected] = writeProtected ? 1 : 0;
            info[WozLoader::kInfoOffsetSynchronized]   = 1;
            info[WozLoader::kInfoOffsetCleaned]        = 1;

            memset (info.data() + WozLoader::kInfoOffsetCreator, ' ',
                    WozLoader::kInfoCreatorLength);
            memcpy (info.data() + WozLoader::kInfoOffsetCreator, creator, strlen (creator));

            info[WozLoader::kInfoOffsetBootSectorFormat] = bootSectorFormat;

            out.insert (out.end(), info.begin(), info.end());
        }

        // TMAP: the first `quarterTracksWithData` positions map to distinct
        // slots, so the two counts differ and each is checked on its own.
        out.insert (out.end(), { 'T', 'M', 'A', 'P' });
        out.insert (out.end(), { 160, 0, 0, 0 });

        for (qt = 0; qt < 160; qt++)
        {
            out.push_back (qt < quarterTracksWithData ? static_cast<Byte> (qt) : Byte (0xFF));
        }

        // META
        out.insert (out.end(), { 'M', 'E', 'T', 'A' });
        out.push_back (static_cast<Byte> (metaLength         & 0xFF));
        out.push_back (static_cast<Byte> ((metaLength >>  8) & 0xFF));
        out.push_back (static_cast<Byte> ((metaLength >> 16) & 0xFF));
        out.push_back (static_cast<Byte> ((metaLength >> 24) & 0xFF));
        out.insert (out.end(), metaText, metaText + metaLength);
    }


    static constexpr size_t  kTestBitCount = 51200;   // ~6400 bytes / track 0
    static constexpr size_t  kChunkHeader  = 8;       // 4-byte id + 4-byte size
    static constexpr size_t  kCreatorSize  = 32;      // INFO creator field, space-padded

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


    ////////////////////////////////////////////////////////////////////////
    //
    //  Metadata retention. Serialize rebuilds the file from the live track
    //  model, which is what makes guest writes survive -- and is also why
    //  everything the model does not hold was being dropped. A round trip
    //  deleted META outright and replaced most of INFO with "unknown", so
    //  every WOZ that passed through a flush came out degraded. The tests
    //  below are byte-level: what the writer emits for a field it does not
    //  own must equal what the reader was handed.
    //
    ////////////////////////////////////////////////////////////////////////

    // A WOZ v2 image whose INFO fields carry distinctive values, followed by
    // an arbitrary list of trailing chunks, so the retention tests assert
    // against bytes they chose rather than against the writer's own defaults.
    void BuildSyntheticV2WithChunks (
        const vector<Byte>                   &  trackZeroBits,
        size_t                                  bitCount,
        const char                           *  creator,
        const vector<pair<string, string>>   &  trailing,
        vector<Byte>                         &  out)
    {
        size_t   i      = 0;
        size_t   info   = 0;
        size_t   blocks = 0;

        AssertSucceeded (WozLoader::BuildSyntheticV2 (1, true, trackZeroBits, bitCount, out));

        info   = WozLoader::kHeaderSize + kChunkHeader;
        blocks = (((bitCount + 7) / 8) + WozLoader::kV2BlockSize - 1) / WozLoader::kV2BlockSize;

        out[info + 3]  = 1;                            // synchronized
        out[info + 4]  = 1;                            // cleaned
        memset (out.data() + info + 5, ' ', kCreatorSize);
        memcpy (out.data() + info + 5, creator, strlen (creator));
        out[info + 37] = 1;                            // disk sides
        out[info + 38] = 1;                            // boot sector format: 16-sector
        out[info + 39] = 32;                           // optimal bit timing
        out[info + 40] = 63;                           // compatible hardware (LE16)
        out[info + 42] = 48;                           // required RAM in K (LE16)

        // Largest track is Casso-owned, so the source has to state it
        // correctly or a byte-for-byte round trip is not the right
        // expectation -- the writer would rightly overwrite a wrong value.
        out[info + 44] = static_cast<Byte> (blocks        & 0xFF);
        out[info + 45] = static_cast<Byte> ((blocks >> 8) & 0xFF);

        for (i = 0; i < trailing.size(); i++)
        {
            const string &  id      = trailing[i].first;
            const string &  payload = trailing[i].second;
            size_t          size    = payload.size();

            Assert::AreEqual (size_t (4), id.size(), L"a chunk id is four bytes");

            out.insert (out.end(), id.begin(), id.end());
            out.push_back (static_cast<Byte> (size         & 0xFF));
            out.push_back (static_cast<Byte> ((size >>  8) & 0xFF));
            out.push_back (static_cast<Byte> ((size >> 16) & 0xFF));
            out.push_back (static_cast<Byte> ((size >> 24) & 0xFF));
            out.insert (out.end(), payload.begin(), payload.end());
        }
    }


    // The payload bytes of the named chunk, located by walking the table.
    // Empty when the chunk is absent, which for these tests is exactly the
    // failure being guarded against -- so callers assert non-empty before
    // comparing rather than comparing two empties and passing.
    vector<Byte> ChunkPayload (const vector<Byte> & woz, const char * wanted)
    {
        size_t     dataOffset = 0;
        uint32_t   size       = FindChunk (woz, wanted, dataOffset);

        if (dataOffset == 0)
        {
            return vector<Byte> ();
        }

        return vector<Byte> (woz.begin() + static_cast<ptrdiff_t> (dataOffset),
                             woz.begin() + static_cast<ptrdiff_t> (dataOffset + size));
    }


    // The META payload of a real preservation dump: tab-delimited key/value
    // rows, LF-terminated, UTF-8 without a BOM.
    string SampleMeta()
    {
        string   meta;

        meta += "title\tThe Print Shop Color\n";
        meta += "publisher\tBroderbund Software\n";
        meta += "developer\tDavid Balsam|Martin Kahn\n";
        meta += "copyright\t1986\n";
        meta += "language\tEnglish\n";
        meta += "requires_machine\t2|2+|2e|2e+|2c|2gs\n";
        meta += "image_date\t2019-01-17T18:46:44.070Z\n";
        meta += "contributor\t4am\n";

        return meta;
    }


    vector<Byte> AsBytes (const string & s)
    {
        return vector<Byte> (s.begin(), s.end());
    }


    // Offset of the first differing byte, or a.size() when the two match.
    // A length mismatch reports the shorter length, which is where reading
    // them together stops being meaningful.
    size_t FirstDifference (const vector<Byte> & a, const vector<Byte> & b)
    {
        size_t   shared = a.size() < b.size() ? a.size() : b.size();
        size_t   i      = 0;

        for (i = 0; i < shared; i++)
        {
            if (a[i] != b[i])
            {
                return i;
            }
        }

        return shared;
    }


    TEST_METHOD (Serialize_PreservesMetaChunkByteForByte)
    {
        DiskImage     img;
        vector<Byte>  woz;
        vector<Byte>  out;
        string        meta = SampleMeta();
        vector<Byte>  before;
        vector<Byte>  after;

        BuildSyntheticV2WithChunks (MakeBitStream(), kTestBitCount,
                                    "Applesauce v1.0.6", { { "META", meta } }, woz);

        before = ChunkPayload (woz, "META");
        Assert::AreEqual (meta.size(), before.size(), L"precondition: the source carries META");

        AssertSucceeded (WozLoader::Load      (woz, img));
        AssertSucceeded (WozLoader::Serialize (img, out));

        after = ChunkPayload (out, "META");

        Assert::AreEqual (before.size(), after.size(),
            L"a round trip must not drop or resize META -- it holds the title, publisher, "
            L"copyright and imaging provenance, none of which the track model can rebuild");
        Assert::IsTrue (before == after, L"META must come back byte for byte");
    }


    TEST_METHOD (Serialize_PreservesInfoFieldsItDoesNotOwn)
    {
        DiskImage     img;
        vector<Byte>  woz;
        vector<Byte>  out;
        vector<Byte>  before;
        vector<Byte>  after;

        BuildSyntheticV2WithChunks (MakeBitStream(), kTestBitCount,
                                    "Passport.py by 4am (2019-02-17)", {}, woz);

        before = ChunkPayload (woz, "INFO");
        Assert::AreEqual (WozLoader::kInfoChunkSize, before.size());

        AssertSucceeded (WozLoader::Load      (woz, img));
        AssertSucceeded (WozLoader::Serialize (img, out));

        after = ChunkPayload (out, "INFO");
        Assert::AreEqual (WozLoader::kInfoChunkSize, after.size());

        // Everything but the four fields Casso owns -- version, disk type,
        // write protect and largest track -- must survive untouched. Creator
        // matters most: it is the only record of which tool imaged the disk.
        Assert::AreEqual (string ("Passport.py by 4am (2019-02-17)"),
            string (reinterpret_cast<const char *> (after.data() + 5), 31),
            L"creator must stay the source's on a disk Casso only edited");

        Assert::AreEqual (Byte (1),  after[3],  L"synchronized must survive");
        Assert::AreEqual (Byte (1),  after[4],  L"cleaned must survive");
        Assert::AreEqual (Byte (1),  after[37], L"disk sides must survive");
        Assert::AreEqual (Byte (1),  after[38], L"boot sector format must survive");
        Assert::AreEqual (Byte (32), after[39], L"optimal bit timing must survive");
        Assert::AreEqual (Byte (63), after[40], L"compatible hardware must survive");
        Assert::AreEqual (Byte (48), after[42], L"required RAM must survive");
    }


    TEST_METHOD (Serialize_StampsCassoAsCreatorOnlyOnADiskItAuthored)
    {
        DiskImage     authored;
        vector<Byte>  out;
        vector<Byte>  info;
        string        creator;

        // No retained source INFO means Casso made this disk, and only then
        // does its own name belong in the creator field.
        authored.SetSourceFormat (DiskFormat::Woz);
        authored.ClearQuarterTrackMap();
        authored.EnsureTrackSlots (1);
        FillTrack (authored, 0, 4096, 0x10);

        AssertSucceeded (WozLoader::Serialize (authored, out));

        info = ChunkPayload (out, "INFO");
        Assert::AreEqual (WozLoader::kInfoChunkSize, info.size());

        creator = string (reinterpret_cast<const char *> (info.data() + 5), kCreatorSize);

        Assert::AreEqual (size_t (0), creator.rfind ("Casso ", 0),
            L"a disk Casso authored says so in creator");
        Assert::IsTrue (creator.size() > 6 && creator[6] != ' ',
            L"the stamp carries a version after the name, as the format's own example does");
        Assert::AreEqual (string ("                "), creator.substr (kCreatorSize - 16),
            L"creator is space-padded to 32 bytes, not null-terminated");
    }


    TEST_METHOD (Serialize_RetainsAChunkItCannotParse)
    {
        // Retention must not reduce to a list of ids Casso happens to know.
        // A chunk it has never heard of has to survive too -- and must not
        // end the chunk walk, or everything after it is lost as well.
        DiskImage     img;
        vector<Byte>  woz;
        vector<Byte>  out;
        string        writ = string ("\x01\x02\x03\x04", 4);
        string        meta = SampleMeta();

        BuildSyntheticV2WithChunks (MakeBitStream(), kTestBitCount, "Applesauce v1.0.6",
                                    { { "WRIT", writ }, { "META", meta } }, woz);

        AssertSucceeded (WozLoader::Load      (woz, img));
        AssertSucceeded (WozLoader::Serialize (img, out));

        Assert::IsTrue (ChunkPayload (out, "WRIT") == AsBytes (writ),
            L"an unmodeled chunk must round-trip verbatim");
        Assert::IsTrue (ChunkPayload (out, "META") == AsBytes (meta),
            L"a chunk sitting after an unrecognized one must not be lost with it");

        {
            vector<string>  ids = StrictChunkWalk (out);

            Assert::AreEqual (size_t (5), ids.size(),
                L"a strict walk must reach INFO, TMAP, TRKS, WRIT, META and then EOF");
            Assert::AreEqual (string ("WRIT"), ids[3]);
            Assert::AreEqual (string ("META"), ids[4]);
        }
    }


    TEST_METHOD (Serialize_FromV1Source_KeepsCreatorAndFillsTheV2OnlyFields)
    {
        // A v1 INFO ends after the creator string, so its later fields are
        // zero. Emitting a v2 container means filling the two with no legal
        // zero -- disk sides and bit timing -- while keeping everything v1
        // did say.
        DiskImage     img;
        vector<Byte>  woz;
        vector<Byte>  out;
        vector<Byte>  info;
        size_t        srcInfo = WozLoader::kHeaderSize + kChunkHeader;

        BuildSyntheticV1 (MakeBitStream(), kTestBitCount, woz);

        memset (woz.data() + srcInfo + 5, ' ', kCreatorSize);
        memcpy (woz.data() + srcInfo + 5, "Passport.py by 4am", 18);
        woz[srcInfo + 3] = 1;                                     // synchronized
        woz[srcInfo + 4] = 1;                                     // cleaned

        AssertSucceeded (WozLoader::Load      (woz, img));
        AssertSucceeded (WozLoader::Serialize (img, out));

        info = ChunkPayload (out, "INFO");
        Assert::AreEqual (WozLoader::kInfoChunkSize, info.size());

        Assert::AreEqual (string ("Passport.py by 4am"),
            string (reinterpret_cast<const char *> (info.data() + 5), 18),
            L"a v1 source's creator must survive the upgrade to v2");
        Assert::AreEqual (Byte (1),  info[3],  L"synchronized existed in v1 and must survive");
        Assert::AreEqual (Byte (1),  info[4],  L"cleaned existed in v1 and must survive");
        Assert::AreEqual (Byte (2),  info[0],  L"the emitted container is v2, so INFO must say 2");
        Assert::AreEqual (Byte (1),  info[37], L"disk sides has no legal zero");
        Assert::AreEqual (Byte (32), info[39], L"optimal bit timing has no legal zero");
        Assert::AreEqual (Byte (0),  info[38],
            L"boot sector format stays 'unknown' -- v1 never said, and guessing is worse");
    }


    TEST_METHOD (Serialize_OwnedInfoFieldsWinOverTheSource)
    {
        // Retention must not go so far as to re-emit stale geometry: the
        // fields Casso derives from the live model have to overwrite whatever
        // the source INFO said.
        DiskImage     img;
        vector<Byte>  woz;
        vector<Byte>  out;
        vector<Byte>  info;
        size_t        srcInfo = WozLoader::kHeaderSize + kChunkHeader;

        BuildSyntheticV2WithChunks (MakeBitStream(), kTestBitCount, "Applesauce v1.0.6", {}, woz);

        woz[srcInfo + 44] = 0xEE;                                 // a lie about largest track
        woz[srcInfo + 45] = 0xEE;

        AssertSucceeded (WozLoader::Load (woz, img));
        Assert::IsTrue (img.IsImageWriteProtected(), L"precondition: the source says protected");

        img.SetImageWriteProtected (false);
        AssertSucceeded (WozLoader::Serialize (img, out));

        info = ChunkPayload (out, "INFO");

        Assert::AreEqual (Byte (0), info[2],
            L"write protect comes from the live image, never from the retained bytes");
        Assert::IsTrue (info[44] != 0xEE || info[45] != 0xEE,
            L"largest track is derived from the emitted geometry, not retained");
    }


    TEST_METHOD (Serialize_WholeFileRoundTripsByteForByte)
    {
        // The strongest statement retention can make: hand the writer an
        // image the reader just parsed and get the identical file back. A
        // flush of an unmodified disk should be a no-op at the byte level.
        DiskImage     img;
        vector<Byte>  woz;
        vector<Byte>  out;

        BuildSyntheticV2WithChunks (MakeBitStream(), kTestBitCount, "Applesauce v1.0.6",
                                    { { "META", SampleMeta() } }, woz);

        // Casso stamps a header CRC and the synthetic builder leaves it zero,
        // so fill it in first or the comparison trips on those four bytes.
        {
            uint32_t   crc = Crc32Ref (woz.data() + WozLoader::kHeaderSize,
                                       woz.size() - WozLoader::kHeaderSize);

            woz[8]  = static_cast<Byte> (crc         & 0xFF);
            woz[9]  = static_cast<Byte> ((crc >>  8) & 0xFF);
            woz[10] = static_cast<Byte> ((crc >> 16) & 0xFF);
            woz[11] = static_cast<Byte> ((crc >> 24) & 0xFF);
        }

        AssertSucceeded (WozLoader::Load      (woz, img));
        AssertSucceeded (WozLoader::Serialize (img, out));

        Assert::AreEqual (woz.size(), out.size(),
            L"a clean round trip must not change the file size");

        // Report WHERE it first diverged: "byte 56 differs" names the INFO
        // field that was dropped, where a bare false says only that
        // something somewhere changed.
        {
            size_t    at      = FirstDifference (woz, out);
            wstring   message = L"a clean round trip must not change a single byte; first "
                                L"difference at offset " + to_wstring (at);

            Assert::AreEqual (woz.size(), at, message.c_str());
        }
    }


    TEST_METHOD (Serialize_GuestWriteSurvivesAlongsideTheMetadata)
    {
        // Both halves must hold at once. The writer that preserved metadata
        // discarded guest writes, and the fix for that discarded the
        // metadata; neither alone is correct.
        DiskImage     img;
        vector<Byte>  woz;
        vector<Byte>  out;
        DiskImage     reloaded;
        size_t        bitIndex = 64;

        BuildSyntheticV2WithChunks (MakeBitStream(), kTestBitCount, "Applesauce v1.0.6",
                                    { { "META", SampleMeta() } }, woz);

        AssertSucceeded (WozLoader::Load (woz, img));

        img.SetImageWriteProtected (false);
        img.WriteBit (0, bitIndex, img.ReadBit (0, bitIndex) ? 0 : 1);

        AssertSucceeded (WozLoader::Serialize (img, out));
        AssertSucceeded (WozLoader::Load      (out, reloaded));

        Assert::AreEqual (img.ReadBit (0, bitIndex), reloaded.ReadBit (0, bitIndex),
            L"the guest write must survive the flush");
        Assert::IsTrue (ChunkPayload (out, "META") == AsBytes (SampleMeta()),
            L"and the metadata must survive that same flush");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Structural invariants over the writer's output.
    //
    //  Every WOZ test used to feed the writer's output back through the
    //  reader, which proves the two agree -- not that the file is right. A
    //  short TRKS size survived exactly that way: the writer emitted it and
    //  the reader tolerated it. These guards read the bytes against the
    //  format's own rules instead, without consulting the model that
    //  produced them, and across a table of image shapes rather than the
    //  single shape a hand-written test happens to pick.
    //
    ////////////////////////////////////////////////////////////////////////

    static constexpr int  kShapeCount = 6;

    // Populate `img` with shape `shapeId` and return its name for failure
    // messages. The shapes span what the writer has to lay out correctly:
    // nothing at all, one track, a full disk, a hole in the middle, a
    // half-track map, and tracks of differing lengths.
    const wchar_t * BuildShape (int shapeId, DiskImage & img)
    {
        int   slot = 0;

        img.SetSourceFormat (DiskFormat::Woz);
        img.ClearQuarterTrackMap();

        switch (shapeId)
        {
            case 0:
                // No populated slot anywhere: every TRK record and every
                // TMAP entry must still be well-formed and empty.
                img.EnsureTrackSlots (1);
                return L"no tracks";

            case 1:
                img.EnsureTrackSlots (1);
                FillTrack (img, 0, 4096, 0x10);
                return L"one short track";

            case 2:
                img.EnsureTrackSlots (35);
                for (slot = 0; slot < 35; slot++)
                {
                    FillTrack (img, slot, kTestBitCount, static_cast<Byte> (0x10 + slot));
                }

                return L"thirty-five full tracks";

            case 3:
                // A hole: slot 1 stays empty between two populated slots, so
                // block assignment must not assume the slots are contiguous.
                img.EnsureTrackSlots (3);
                FillTrack (img, 0, kTestBitCount, 0x10);
                FillTrack (img, 2, kTestBitCount, 0x90);
                return L"gap slot between populated tracks";

            case 4:
                img.EnsureTrackSlots (2);
                FillTrack (img, 0, kTestBitCount, 0x10);
                FillTrack (img, 1, kTestBitCount, 0x40);
                img.SetQuarterTrackSlot (2, 1);
                return L"half-track quarter-track map";

            case 5:
                // Differing lengths, so block counts differ per record and a
                // shared-stride bug cannot hide behind uniform tracks.
                img.EnsureTrackSlots (4);
                FillTrack (img, 0, 4096,  0x10);
                FillTrack (img, 1, 51200, 0x40);
                FillTrack (img, 2, 600,   0x70);
                FillTrack (img, 3, 40000, 0x90);
                return L"tracks of differing lengths";

            default:
                Assert::Fail (L"unknown shape id");
                return L"";
        }
    }


    // The first packed byte of a track's bit stream, MSB first. Used to tell
    // which block a stream was actually read from.
    Byte FirstByteOfTrack (const DiskImage & img, int slot)
    {
        Byte   value = 0;
        int    bit   = 0;

        for (bit = 0; bit < 8; bit++)
        {
            value = static_cast<Byte> ((value << 1) | (img.ReadBit (slot, static_cast<size_t> (bit)) & 1));
        }

        return value;
    }


    // Assert every structural promise a WOZ v2 file makes. Reads the bytes
    // only -- the DiskImage that produced them is deliberately not consulted,
    // because agreeing with the model is what the old round-trip tests
    // already proved, and it is not the same thing as being a valid file.
    void AssertStructuralInvariants (const vector<Byte> & woz, const wchar_t * shape)
    {
        vector<string>                 ids;
        vector<pair<size_t, size_t>>   claimed;
        size_t                         infoOffset = 0;
        size_t                         tmapOffset = 0;
        size_t                         trksOffset = 0;
        uint32_t                       infoSize   = 0;
        uint32_t                       tmapSize   = 0;
        uint32_t                       trksSize   = 0;
        uint32_t                       storedCrc  = 0;
        size_t                         slot       = 0;
        size_t                         qt         = 0;
        size_t                         blockEnd   = WozLoader::kV2FirstDataBlock;
        wstring                        where      = wstring (L" [shape: ") + shape + L"]";

        Assert::IsTrue (woz.size() > WozLoader::kHeaderSize,
            (wstring (L"file is nothing but a header") + where).c_str());
        Assert::AreEqual (string ("WOZ2"),
            string (reinterpret_cast<const char *> (woz.data()), 4),
            (wstring (L"the writer always emits v2") + where).c_str());

        storedCrc = static_cast<uint32_t> (woz[8])
                  | (static_cast<uint32_t> (woz[9]) << 8)
                  | (static_cast<uint32_t> (woz[10]) << 16)
                  | (static_cast<uint32_t> (woz[11]) << 24);

        Assert::AreEqual (Crc32Ref (woz.data() + WozLoader::kHeaderSize,
                                    woz.size() - WozLoader::kHeaderSize), storedCrc,
            (wstring (L"the header CRC must cover every byte after the header") + where).c_str());

        // A strict walk has to reach end-of-file. Landing anywhere else means
        // some chunk size is wrong and a later chunk is unreachable.
        ids = StrictChunkWalk (woz);
        Assert::IsTrue (ids.size() >= 3,
            (wstring (L"INFO, TMAP and TRKS are all mandatory") + where).c_str());

        for (slot = 0; slot < ids.size(); slot++)
        {
            Assert::AreNotEqual (string ("!!"), ids[slot],
                (wstring (L"a strict chunk walk must reach EOF, not stall mid-file") + where).c_str());
        }

        infoSize = FindChunk (woz, "INFO", infoOffset);
        tmapSize = FindChunk (woz, "TMAP", tmapOffset);
        trksSize = FindChunk (woz, "TRKS", trksOffset);

        Assert::AreEqual (static_cast<uint32_t> (WozLoader::kInfoChunkSize), infoSize,
            (wstring (L"INFO is a fixed 60 bytes") + where).c_str());
        Assert::AreEqual (static_cast<uint32_t> (WozLoader::kTmapChunkSize), tmapSize,
            (wstring (L"TMAP is a fixed 160 bytes") + where).c_str());
        Assert::IsTrue (trksOffset != 0,
            (wstring (L"TRKS must be present") + where).c_str());
        Assert::IsTrue (trksSize >= WozLoader::kV2TrkRecordCount * WozLoader::kV2TrkRecordSize,
            (wstring (L"TRKS must hold all 160 records") + where).c_str());

        // Each record: a block region inside the file, large enough for the
        // bits it claims, overlapping no other record.
        for (slot = 0; slot < WozLoader::kV2TrkRecordCount; slot++)
        {
            const Byte *  rec        = woz.data() + trksOffset + slot * WozLoader::kV2TrkRecordSize;
            size_t        startBlock = static_cast<size_t> (rec[0]) | (static_cast<size_t> (rec[1]) << 8);
            size_t        blockCount = static_cast<size_t> (rec[2]) | (static_cast<size_t> (rec[3]) << 8);
            size_t        bitCount   = static_cast<size_t> (rec[4])
                                     | (static_cast<size_t> (rec[5]) << 8)
                                     | (static_cast<size_t> (rec[6]) << 16)
                                     | (static_cast<size_t> (rec[7]) << 24);
            size_t        first      = 0;
            size_t        last       = 0;
            size_t        other      = 0;

            if (blockCount == 0 && bitCount == 0)
            {
                Assert::AreEqual (size_t (0), startBlock,
                    (wstring (L"an empty record must be entirely zero") + where).c_str());
                continue;
            }

            Assert::IsTrue (blockCount != 0 && bitCount != 0,
                (wstring (L"a record must claim both blocks and bits, or neither") + where).c_str());
            Assert::IsTrue (startBlock >= WozLoader::kV2FirstDataBlock,
                (wstring (L"track data cannot start inside the header blocks") + where).c_str());
            Assert::IsTrue (blockCount * WozLoader::kV2BlockSize >= (bitCount + 7) / 8,
                (wstring (L"the claimed blocks must hold the claimed bits") + where).c_str());

            first = startBlock * WozLoader::kV2BlockSize;
            last  = first + blockCount * WozLoader::kV2BlockSize;

            Assert::IsTrue (last <= woz.size(),
                (wstring (L"a record's blocks must lie inside the file") + where).c_str());

            for (other = 0; other < claimed.size(); other++)
            {
                Assert::IsTrue (last <= claimed[other].first || first >= claimed[other].second,
                    (wstring (L"two records must not claim the same block") + where).c_str());
            }

            claimed.push_back (make_pair (first, last));

            if (startBlock + blockCount > blockEnd)
            {
                blockEnd = startBlock + blockCount;
            }
        }

        // TRKS spans its records plus every block they claim: the payload is
        // the chunk's own content, not trailing data sitting after it.
        Assert::AreEqual (
            static_cast<uint32_t> (WozLoader::kV2TrkRecordCount * WozLoader::kV2TrkRecordSize
                                   + (blockEnd - WozLoader::kV2FirstDataBlock) * WozLoader::kV2BlockSize),
            trksSize,
            (wstring (L"TRKS size must span its records and their blocks") + where).c_str());

        Assert::AreEqual (size_t (0), (trksOffset + trksSize) % WozLoader::kV2BlockSize,
            (wstring (L"the block region must end on a block boundary") + where).c_str());

        // Every TMAP entry names either an empty slot or a populated record.
        for (qt = 0; qt < WozLoader::kTmapChunkSize; qt++)
        {
            Byte          entry = woz[tmapOffset + qt];
            const Byte *  rec   = nullptr;

            if (entry == 0xFF)
            {
                continue;
            }

            Assert::IsTrue (entry < WozLoader::kV2TrkRecordCount,
                (wstring (L"a TMAP entry must index a real record") + where).c_str());

            rec = woz.data() + trksOffset + static_cast<size_t> (entry) * WozLoader::kV2TrkRecordSize;

            Assert::IsTrue (rec[2] != 0 || rec[3] != 0,
                (wstring (L"a mapped quarter-track must point at a record holding data") + where).c_str());
        }
    }


    TEST_METHOD (Serialize_OutputSatisfiesStructuralInvariants_AcrossImageShapes)
    {
        int   shapeId = 0;

        Assert::IsTrue (kShapeCount > 0, L"a shape table with no shapes checks nothing");

        for (shapeId = 0; shapeId < kShapeCount; shapeId++)
        {
            DiskImage        img;
            vector<Byte>     out;
            const wchar_t *  shape = BuildShape (shapeId, img);

            AssertSucceeded (WozLoader::Serialize (img, out));
            AssertStructuralInvariants (out, shape);
        }
    }


    TEST_METHOD (Serialize_InputTheWriterDidNotProduce_StaysStructurallyValid)
    {
        // Legal geometry the writer would never emit itself: records out of
        // order, a gap between the blocks they claim, and a chunk after them.
        // Reading only our own output back is how a writer bug hides, so the
        // reader has to be handed something it did not author.
        DiskImage     img;
        vector<Byte>  woz;
        vector<Byte>  out;
        size_t        trks    = 0;
        size_t        tmap    = 0;
        string        meta    = SampleMeta();
        const size_t  kSlotA  = 1;
        const size_t  kSlotB  = 0;
        const size_t  kBlockA = 3;
        const size_t  kBlockB = 9;            // deliberately not adjacent to A
        const size_t  kBlocks = 2;
        const size_t  kBits   = 4096;

        woz.assign ((kBlockB + kBlocks) * WozLoader::kV2BlockSize, 0);

        memcpy (woz.data(), "WOZ2\xFF\x0A\x0D\x0A", 8);

        {
            size_t   pos = WozLoader::kHeaderSize;

            memcpy (woz.data() + pos, "INFO", 4);
            woz[pos + 4]      = static_cast<Byte> (WozLoader::kInfoChunkSize);
            woz[pos + 8 + 0]  = 2;                               // INFO version 2
            woz[pos + 8 + 1]  = 1;                               // disk type 5.25"
            woz[pos + 8 + 4]  = 1;                               // cleaned
            memset (woz.data() + pos + 8 + 5, ' ', kCreatorSize);
            memcpy (woz.data() + pos + 8 + 5, "Applesauce v1.2.5", 17);
            woz[pos + 8 + 37] = 1;                               // disk sides
            woz[pos + 8 + 39] = 32;                              // optimal bit timing
            pos += kChunkHeader + WozLoader::kInfoChunkSize;

            memcpy (woz.data() + pos, "TMAP", 4);
            woz[pos + 4] = static_cast<Byte> (WozLoader::kTmapChunkSize);
            tmap = pos + kChunkHeader;
            memset (woz.data() + tmap, 0xFF, WozLoader::kTmapChunkSize);
            woz[tmap + 0] = static_cast<Byte> (kSlotB);
            woz[tmap + 4] = static_cast<Byte> (kSlotA);
            pos += kChunkHeader + WozLoader::kTmapChunkSize;

            memcpy (woz.data() + pos, "TRKS", 4);
            trks = pos + kChunkHeader;
        }

        {
            // Slot 1's blocks sit BEFORE slot 0's, with a six-block hole
            // between the two runs.
            Byte *   recA = woz.data() + trks + kSlotA * WozLoader::kV2TrkRecordSize;
            Byte *   recB = woz.data() + trks + kSlotB * WozLoader::kV2TrkRecordSize;

            recA[0] = static_cast<Byte> (kBlockA);
            recA[2] = static_cast<Byte> (kBlocks);
            recA[4] = static_cast<Byte> (kBits        & 0xFF);
            recA[5] = static_cast<Byte> ((kBits >> 8) & 0xFF);

            recB[0] = static_cast<Byte> (kBlockB);
            recB[2] = static_cast<Byte> (kBlocks);
            recB[4] = static_cast<Byte> (kBits        & 0xFF);
            recB[5] = static_cast<Byte> ((kBits >> 8) & 0xFF);

            // A distinct leading byte per block, so a stream read from the
            // wrong block is visible rather than merely wrong-length.
            woz[kBlockA * WozLoader::kV2BlockSize] = 0xD5;
            woz[kBlockB * WozLoader::kV2BlockSize] = 0xAA;
        }

        {
            size_t   trksSize = WozLoader::kV2TrkRecordCount * WozLoader::kV2TrkRecordSize
                              + (kBlockB + kBlocks - WozLoader::kV2FirstDataBlock)
                                * WozLoader::kV2BlockSize;

            woz[trks - 4] = static_cast<Byte> (trksSize         & 0xFF);
            woz[trks - 3] = static_cast<Byte> ((trksSize >>  8) & 0xFF);
            woz[trks - 2] = static_cast<Byte> ((trksSize >> 16) & 0xFF);
        }

        {
            Byte     header[8] = { 'M', 'E', 'T', 'A', 0, 0, 0, 0 };
            size_t   metaSize  = meta.size();

            header[4] = static_cast<Byte> (metaSize         & 0xFF);
            header[5] = static_cast<Byte> ((metaSize >>  8) & 0xFF);
            header[6] = static_cast<Byte> ((metaSize >> 16) & 0xFF);
            header[7] = static_cast<Byte> ((metaSize >> 24) & 0xFF);

            woz.insert (woz.end(), header, header + sizeof (header));
            woz.insert (woz.end(), meta.begin(), meta.end());
        }

        AssertSucceeded (WozLoader::Load (woz, img),
            L"a legal image with unusual geometry must load");

        Assert::AreEqual (kBits, img.GetTrackBitCount (static_cast<int> (kSlotA)));
        Assert::AreEqual (kBits, img.GetTrackBitCount (static_cast<int> (kSlotB)));
        Assert::AreEqual (Byte (0xD5), FirstByteOfTrack (img, static_cast<int> (kSlotA)),
            L"slot 1's stream must come from ITS block, not simply the first data block");
        Assert::AreEqual (Byte (0xAA), FirstByteOfTrack (img, static_cast<int> (kSlotB)),
            L"slot 0's stream must come from its own, later block");

        AssertSucceeded (WozLoader::Serialize (img, out));

        AssertStructuralInvariants (out, L"hand-built v2, records out of order across a block gap");

        Assert::IsTrue (ChunkPayload (out, "META") == AsBytes (meta),
            L"META must survive an image the writer did not lay out");
        Assert::AreEqual (string ("Applesauce v1.2.5"),
            string (reinterpret_cast<const char *> (ChunkPayload (out, "INFO").data() + 5), 17),
            L"so must the creator of an image the writer did not lay out");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  SetWriteProtectFlag -- the write-protect flag as a one-byte edit.
    //
    //  The flag lives in the file (INFO byte 2), so changing it has to write.
    //  The only writer available used to be Serialize, the full
    //  rebuild-from-model path, so a menu click relaid out an entire image to
    //  carry one bit -- and on a preservation dump that was pure loss. These
    //  tests pin the narrower guarantee: the bytes this function never reads
    //  are bytes it cannot damage, which holds whether or not the rebuild
    //  path retains every field correctly.
    //
    ////////////////////////////////////////////////////////////////////////

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
            L"INFO must still say version 1");
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

