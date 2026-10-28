#include "Pch.h"
#include "WozLoaderTestSupport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WozLoaderMetadataTests
//
//  What a save keeps of a WOZ it did not write: META, the INFO fields it does
//  not own, chunks it cannot parse, and the structure every file it writes
//  satisfies.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WozLoaderMetadataTests), public WozLoaderTestSupport
{
public:

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
            L"a disk Casso authored records that in creator");
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
        Assert::AreEqual (Byte (2),  info[0],  L"the emitted container is v2, so INFO must hold 2");
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
        Assert::IsTrue (img.IsImageWriteProtected(), L"precondition: the source is marked protected");

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
};
