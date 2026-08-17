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
};
