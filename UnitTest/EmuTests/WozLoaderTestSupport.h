#pragma once

#include "Pch.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WozLoaderTestSupport
//
//  What the WozLoaderTests test classes share: the images they mount, the
//  capture of a reported save failure, and the bays they use. Each class
//  derives from it beside TestClass.
//
////////////////////////////////////////////////////////////////////////////////

class WozLoaderTestSupport
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


    //  Zeroes TRK record `record`'s start block and block count, leaving an
    //  all-zero record when its count is zero, and zeroes the header CRC, so
    //  the load skips the file's checksum.
    void ZeroTrackRecord (vector<Byte> & woz, size_t record)
    {
        size_t  recordsOffset = 0;
        Byte *  rec           = nullptr;



        Assert::IsTrue (FindChunk (woz, "TRKS", recordsOffset) != 0, L"the image has a TRKS chunk");

        rec = woz.data() + recordsOffset + record * WozLoader::kV2TrkRecordSize;
        memset (rec, 0, 4);
        memset (woz.data() + 8, 0, 4);
    }


    //  `count` bit tracks of 512 bits, record N for track N, mapped at 4N-1,
    //  4N and 4N+1 as Applesauce maps them, or at 4N to 4N+3 as Casso does.
    static vector<WozSyntheticTrack> MakeTracks (int count, bool isApplesauce)
    {
        vector<WozSyntheticTrack>  tracks (count);
        int                        n      = 0;



        for (n = 0; n < count; n++)
        {
            tracks[n].data.assign (64, 0xFF);
            tracks[n].bitCount = 512;

            if (isApplesauce)
            {
                tracks[n].quarterTracks = (n > 0) ? vector<int> { 4 * n - 1, 4 * n, 4 * n + 1 } : vector<int> { 0, 1 };
            }
            else
            {
                tracks[n].quarterTracks = { 4 * n, 4 * n + 1, 4 * n + 2, 4 * n + 3 };
            }
        }

        return tracks;
    }


    //  Track 5's data is in TRK record 20, record 5 is an unmapped empty bit
    //  record, and track 20 has no record: TRK index 20 is taken, so track
    //  20's reservation takes the lowest free index, 5.
    static vector<WozSyntheticTrack> MakeTracksWithIndexTaken()
    {
        vector<WozSyntheticTrack>  tracks = MakeTracks (21, false);



        tracks[5].data.clear();
        tracks[5].bitCount = 0;
        tracks[5].quarterTracks.clear();

        tracks[20].quarterTracks = { 20, 21, 22, 23 };

        return tracks;
    }


    //  The chunk's payload, by its id.
    vector<Byte> GetChunkBytes (const vector<Byte> & woz, const char * wanted)
    {
        size_t    offset = 0;
        uint32_t  size   = FindChunk (woz, wanted, offset);



        return vector<Byte> (woz.begin() + offset, woz.begin() + offset + size);
    }


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
};
