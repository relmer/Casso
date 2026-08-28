#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FixtureProvider.h"
#include "HeadlessHost.h"
#include "MachineIdle.h"
#include "TextScreenScraper.h"
#include "Devices/Disk/Dos33Volume.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/ProDosSkeleton.h"
#include "Devices/Disk/TrackWritability.h"
#include "Devices/Disk/VolumeImage.h"
#include "Devices/Disk/WozLoader.h"
#include "Devices/Disk2Controller.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CrossFormatWriteTests
//
//  Writing an edited volume back into every container it can have come from.
//
//  WHY THIS IS NOT THE EXTRACTION SUITE WITH THE ARROWS REVERSED. A reorder
//  applied wrongly but CONSISTENTLY is invisible to a round trip: the file is
//  written through the wrong table and read through the same wrong table, so
//  every byte comes back and the image is garbage only on a real machine. A
//  round trip is therefore not the check here -- it is the thing that hides the
//  defect, and this file was written after finding exactly that defect live.
//
//  So the ordering claims are settled against evidence that owes nothing to the
//  code under test:
//
//    * the PUBLISHED on-disk layouts, read at absolute file offsets -- a ProDOS
//      volume directory begins at byte 1024 of a ProDOS-ordered image and a
//      DOS 3.3 VTOC at byte 69632 of a DOS-ordered one, whatever this codebase
//      believes;
//    * the block map the ProDOS reader uses, which is a DIFFERENT table in a
//      different file and is corroborated by real ProDOS volumes stored in DOS
//      sector order;
//    * a real 6502 booting the image we wrote.
//
//  Everything that re-reads what was written does so through the DRIVE -- the
//  container is nibblized as the hardware would see it and decoded with the DOS
//  interleave -- never through the reorder that wrote it.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (CrossFormatWriteTests)
{
public:

    static constexpr const char *  kDos33Disk  = "Disks/Merlin-proDos2.23.dsk";
    static constexpr const char *  kProDosDisk = "Disks/Merlin-proProdos2.33-a.dsk";

    //  Published ProDOS layout. A block is 512 bytes and a ProDOS-ordered image
    //  stores block N at byte N * 512; the volume directory is block 2, and its
    //  header entry starts four bytes into that block, past the two link words.
    static constexpr size_t  kProDosBlockBytes   = 512;
    static constexpr int     kVolumeDirBlock     = 2;
    static constexpr size_t  kDirFirstEntry      = 0x04;
    static constexpr Byte    kVolumeDirStorage   = 0xF0;
    static constexpr size_t  kHdrEntryLength     = 0x1F;
    static constexpr size_t  kHdrEntriesPerBlock = 0x20;
    static constexpr size_t  kHdrTotalBlocks     = 0x25;

    //  Published DOS 3.3 layout. The VTOC is track 17 sector 0, which in a
    //  DOS-ordered image is byte 69632.
    static constexpr size_t  kVtocOffset         = 17 * 16 * 256;
    static constexpr size_t  kVtocCatalogTrack   = 0x01;
    static constexpr size_t  kVtocDosVersion     = 0x03;
    static constexpr size_t  kVtocVolumeNumber   = 0x06;
    static constexpr size_t  kVtocMaxPairs       = 0x27;
    static constexpr size_t  kVtocTrackCount     = 0x34;
    static constexpr size_t  kVtocSectorCount    = 0x35;
    static constexpr size_t  kVtocBytesLow       = 0x36;
    static constexpr size_t  kVtocBytesHigh      = 0x37;

    static constexpr const char *  kPlacedName = "TSTPROG";
    static constexpr size_t         kPlacedSize = 512;
    static constexpr Word           kPlacedAddr = 0x6000;

    //  The catalog track every DOS 3.3 write has to update, so a write that
    //  lands on exactly one track is not a case this file can use.
    static constexpr int  kCatalogTrack = 17;

    //  Boot tracks: allocated on every DOS 3.3 volume, so the allocator never
    //  hands them out and a write never lands on them.
    static constexpr int  kBlankedTrack  = 0;
    static constexpr int  kDamagedTrack  = 2;

    //  Ceiling and sampling interval for the cold boot. Not run-until-idle:
    //  this disk boots into the assembler's own menu rather than to a prompt,
    //  so the idle rule never fires on it.
    static constexpr uint64_t  kBootCycles = 40'000'000ULL;
    static constexpr uint64_t  kBootSlice  =  2'000'000ULL;

    static constexpr Word  kBootRomEntry = 0xC600;
    static constexpr Word  kIntCxRomOff  = 0xC006;

    //  What the guest itself puts on screen once it has loaded off the image.
    //  Asserting on this rather than on "the screens matched" is what stops
    //  two identically blank machines from passing.
    static constexpr const char *  kMerlinMenuLine = "Catalog";

    struct Container
    {
        std::string   path;
        vector<Byte>  bytes;
    };


    vector<Byte> LoadFixture (const char * relativePath)
    {
        FixtureProvider  fixtures;
        vector<Byte>     bytes;

        AssertSucceeded (fixtures.OpenFixture (relativePath, bytes));
        Assert::AreEqual (size_t (NibblizationLayer::kImageByteSize), bytes.size(),
            L"a fixture volume must be a full sector image");

        return bytes;
    }


    //  One DOS-logical buffer as each of the four containers, BEFORE any edit.
    //  These are the "file on disk" that Save is asked to replace.
    std::vector<Container> MakeAllFourContainers (const vector<Byte> & sectors)
    {
        std::vector<Container>  all;
        DiskImage               img;
        vector<Byte>            po;
        vector<Byte>            woz;

        VolumeImage::DosLogicalToProDosFile (sectors, po);

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, img));
        AssertSucceeded (WozLoader::Serialize (img, woz));

        all.push_back (Container { "image.dsk", sectors });
        all.push_back (Container { "image.do",  sectors });
        all.push_back (Container { "image.po",  po      });
        all.push_back (Container { "image.woz", woz     });

        return all;
    }


    //  Re-reads a container the way the DRIVE would: lay its bytes down as
    //  physical sectors, then decode with the DOS interleave. Deliberately not
    //  VolumeImage::Load -- for a .po that would apply the same reorder that
    //  wrote the file, and a shared mistake would cancel out.
    vector<Byte> ReadBackThroughTheDrive (const std::string & path, const vector<Byte> & bytes)
    {
        DiskImage           img;
        SectorDecodeReport  report;
        vector<Byte>        sectors;
        bool                isWoz = path.ends_with (".woz");
        bool                isPo  = path.ends_with (".po");

        if (isWoz)
        {
            AssertSucceeded (WozLoader::Load (bytes, img));
        }
        else if (isPo)
        {
            AssertSucceeded (NibblizationLayer::NibblizePo (bytes, img));
        }
        else
        {
            AssertSucceeded (NibblizationLayer::NibblizeDsk (bytes, img));
        }

        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, sectors, report));

        return sectors;
    }


    FilePayload MakeBinaryPayload()
    {
        FilePayload  payload;
        size_t       i = 0;

        payload.type           = Dos33Volume::kTypeBinary;
        payload.loadAddress    = kPlacedAddr;
        payload.hasLoadAddress = true;

        payload.bytes.resize (kPlacedSize);

        for (i = 0; i < kPlacedSize; i++)
        {
            payload.bytes[i] = (Byte) ((i * 7 + 3) & 0xFF);
        }

        return payload;
    }


    //  The complete post-edit buffer for a volume: one binary placed.
    vector<Byte> PlaceTheBinary (const vector<Byte> & sectors)
    {
        Dos33Volume   volume (sectors);
        vector<Byte>  edited;

        AssertSucceeded (volume.Write (FilePath::Parse (kPlacedName), MakeBinaryPayload(), edited));

        return edited;
    }


    static bool Contains (const vector<int> & tracks, int track)
    {
        return std::find (tracks.begin(), tracks.end(), track) != tracks.end();
    }


    //  Byte offset of the Nth address prologue in a packed track bit stream.
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


    //  Writes a checksum that cannot be right for this header, which costs the
    //  track exactly one sector and makes it Partial. Flipping a bit in the
    //  ENCODED byte would not do: 4-and-4 forces the odd bits to 1, so clearing
    //  one is a no-op for half the sectors on a track.
    static void PatchFieldChecksum (vector<Byte> & bits, size_t addrAt, Byte volume, Byte track)
    {
        Byte  sector = (Byte) (((bits[addrAt + 7] << 1) | 1) & bits[addrAt + 8]);
        Byte  wrong  = (Byte) ((volume ^ track ^ sector) ^ 0xFF);

        bits[addrAt + 9]  = (Byte) ((wrong >> 1) | 0xAA);
        bits[addrAt + 10] = (Byte) (wrong | 0xAA);
    }


    //  A WOZ of the given sectors with one track's first address field broken,
    //  so that track decodes Partial and is refused for writing.
    vector<Byte> MakeWozWithDamagedTrack (const vector<Byte> & sectors, int track)
    {
        DiskImage     img;
        vector<Byte>  woz;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, img));

        {
            vector<Byte> &  bits   = img.GetTrackBitsForWrite (track);
            size_t          addrAt = FindAddressField (bits, 0);

            Assert::AreNotEqual (SIZE_MAX, addrAt, L"the track must carry address fields to corrupt");

            PatchFieldChecksum (bits, addrAt, NibblizationLayer::kDefaultVolume, (Byte) track);
        }

        AssertSucceeded (WozLoader::Serialize (img, woz));

        return woz;
    }


    //  ------------------------------------------------------------------
    //  Absolute on-disk layout. These are the gates for the sector order,
    //  and none of them reads anything back.
    //  ------------------------------------------------------------------

    TEST_METHOD (AProDosVolumeWrittenAsPo_PutsItsVolumeDirectoryWhereProDosDefinesIt)
    {
        // THE gate for the .po write. A ProDOS-ordered image stores block N at
        // byte N * 512, so the volume directory is at byte 1024 and says so in
        // its own header. Nothing here consults a Casso constant, and nothing
        // here reads the file back -- which is the point. A wrong reorder is
        // self-consistent under a round trip and produces a file no ProDOS
        // would mount; only an absolute assertion against the published layout
        // can tell.
        //
        // This test FAILED against the table that shipped before it, which put
        // the volume directory a full nine sectors away from where ProDOS looks.
        vector<Byte>  sectors = LoadFixture (kProDosDisk);
        vector<Byte>  po;
        std::string   reason;
        std::string   name;
        size_t        header  = (size_t) kVolumeDirBlock * kProDosBlockBytes + kDirFirstEntry;
        size_t        nameLen = 0;
        size_t        i       = 0;
        Word          blocks  = 0;

        AssertSucceeded (VolumeImage::Save (sectors, "merlin.po", sectors, po, reason));

        Assert::AreEqual (size_t (NibblizationLayer::kImageByteSize), po.size(),
            L"a ProDOS-ordered image is the same length as the sectors it holds");

        Assert::AreEqual ((int) kVolumeDirStorage, (int) (po[header] & 0xF0),
            L"byte 1028 of a ProDOS-ordered image is the volume directory's storage type");

        nameLen = (size_t) (po[header] & 0x0F);
        Assert::AreEqual (size_t (6), nameLen, L"and its low nibble is the volume name's length");

        for (i = 0; i < nameLen; i++)
        {
            name.push_back ((char) po[header + 1 + i]);
        }

        Assert::AreEqual (std::string ("MERLIN"), name,
            L"the volume this disk carries must name itself at the offset ProDOS reads");

        // The rest of the header, still at absolute offsets, so a buffer that
        // happened to spell MERLIN would not be enough.
        Assert::AreEqual (0x27, (int) po[header + kHdrEntryLength],
            L"a ProDOS directory entry is 39 bytes");
        Assert::AreEqual (0x0D, (int) po[header + kHdrEntriesPerBlock],
            L"and thirteen of them fit in a block");

        blocks = (Word) (po[header + kHdrTotalBlocks] | (po[header + kHdrTotalBlocks + 1] << 8));
        Assert::AreEqual (280, (int) blocks, L"a 5.25-inch ProDOS volume is 280 blocks");
    }

    TEST_METHOD (AProDosVolumeWrittenAsPo_AgreesBlockForBlockWithTheReadersBlockMap)
    {
        // The second independent witness, and the cheaper one to keep honest.
        // ProDosSkeleton owns the block-to-sector map the readers address real
        // ProDOS volumes through, and those volumes are stored in DOS sector
        // order -- so that table has been corroborated against disks written in
        // 1985. The reorder must agree with it for every one of the 280 blocks,
        // not merely for the directory.
        //
        // It is a DIFFERENT table from the one the writer composes, which is
        // what makes this worth running at all: two statements of the same fact
        // that were derived separately.
        vector<Byte>  sectors = LoadFixture (kProDosDisk);
        vector<Byte>  po;
        std::string   reason;
        int           block   = 0;
        size_t        i       = 0;

        AssertSucceeded (VolumeImage::Save (sectors, "merlin.po", sectors, po, reason));

        for (block = 0; block < ProDosSkeleton::kTotalBlocks; block++)
        {
            for (i = 0; i < kProDosBlockBytes; i++)
            {
                Byte  fromFile = po[(size_t) block * kProDosBlockBytes + i];
                Byte  fromMap  = sectors[ProDosSkeleton::BlockByteOffset (block, i)];

                if (fromFile != fromMap)
                {
                    Assert::Fail (L"a ProDOS-ordered file must hold every block where the "
                                  L"reader's own block map says that block lives");
                }
            }
        }
    }

    TEST_METHOD (ADos33VolumeWrittenAsDsk_PutsItsVtocWhereDosDefinesIt)
    {
        // The counterpart for the other sector order, and the reason .dsk and
        // .do are both carried: they are the same layout under two names, so
        // the extension must not trigger a reorder for one of them. Byte 69632
        // is track 17 sector 0 and nothing else.
        vector<Byte>  sectors = LoadFixture (kDos33Disk);
        vector<Byte>  asDsk;
        vector<Byte>  asDo;
        std::string   reason;

        AssertSucceeded (VolumeImage::Save (sectors, "merlin.dsk", sectors, asDsk, reason));
        AssertSucceeded (VolumeImage::Save (sectors, "merlin.do",  sectors, asDo,  reason));

        Assert::IsTrue (asDsk == asDo, L".dsk and .do are the same layout under two names");

        Assert::AreEqual (17,  (int) asDsk[kVtocOffset + kVtocCatalogTrack],
            L"the VTOC names track 17 as the catalog track");
        Assert::AreEqual (3,   (int) asDsk[kVtocOffset + kVtocDosVersion],
            L"and DOS release 3");
        Assert::AreEqual (254, (int) asDsk[kVtocOffset + kVtocVolumeNumber],
            L"and volume 254, which is what this disk's own catalog listing says");
        Assert::AreEqual (122, (int) asDsk[kVtocOffset + kVtocMaxPairs],
            L"and 122 track/sector pairs to a list sector");
        Assert::AreEqual (35,  (int) asDsk[kVtocOffset + kVtocTrackCount]);
        Assert::AreEqual (16,  (int) asDsk[kVtocOffset + kVtocSectorCount]);
        Assert::AreEqual (0,   (int) asDsk[kVtocOffset + kVtocBytesLow]);
        Assert::AreEqual (1,   (int) asDsk[kVtocOffset + kVtocBytesHigh],
            L"and 256 bytes to a sector");
    }


    //  ------------------------------------------------------------------
    //  The four-format write matrix.
    //  ------------------------------------------------------------------

    TEST_METHOD (AFilePlacedThroughEveryContainer_ComesBackFromEveryContainer)
    {
        // Extraction across formats is settled elsewhere; this is the write
        // side, which needs its own matrix because a mistake in the outbound
        // reorder is silent. Every container is re-read through the drive, so
        // the .po case is decided by the hardware interleave rather than by the
        // reorder that wrote it.
        vector<Byte>            sectors    = LoadFixture (kDos33Disk);
        std::vector<Container>  containers = MakeAllFourContainers (sectors);
        FilePayload             payload    = MakeBinaryPayload();
        size_t                  c          = 0;

        Assert::AreEqual (size_t (4), containers.size(), L"four containers, or this proves nothing");

        for (c = 0; c < containers.size(); c++)
        {
            SectorDecodeReport  report;
            vector<Byte>        loaded;
            vector<Byte>        edited;
            vector<Byte>        written;
            vector<Byte>        back;
            FilePayload         readBack;
            std::string         reason;

            AssertSucceeded (VolumeImage::Load (containers[c].bytes, containers[c].path,
                                                loaded, report));

            Assert::IsTrue (loaded == sectors, L"every container starts from the same volume");

            edited = PlaceTheBinary (loaded);

            AssertSucceeded (VolumeImage::Save (containers[c].bytes, containers[c].path,
                                                edited, written, reason));

            Assert::IsTrue (reason.empty(), L"a write that succeeded has nothing to refuse");
            Assert::IsFalse (written == containers[c].bytes,
                L"the container must actually differ afterwards, or nothing was written");

            back = ReadBackThroughTheDrive (containers[c].path, written);

            Assert::IsTrue (back == edited,
                L"what the drive reads off the written container must be the edited volume, "
                L"byte for byte -- including every sector the edit did not touch");

            {
                Dos33Volume  volume (back);

                AssertSucceeded (volume.Read (FilePath::Parse (kPlacedName), readBack));
            }

            Assert::IsTrue (readBack.bytes == payload.bytes,
                L"and the placed file must read back as the bytes placed");
            Assert::AreEqual (kPlacedAddr, readBack.loadAddress);
        }
    }


    //  ------------------------------------------------------------------
    //  What a bit-stream write must NOT disturb.
    //  ------------------------------------------------------------------

    TEST_METHOD (WritingToAWoz_LeavesEveryUntouchedTracksBitsIdentical)
    {
        // The trap this test has to avoid: a WOZ built by nibblizing a sector
        // image re-encodes to itself, so an implementation that rewrote all
        // thirty-five tracks would pass a bit-comparison anyway. So one track
        // is blanked first. A whole-image re-encode would replace those zeros
        // with a full standard track -- loudly, and on a track the edit never
        // named.
        vector<Byte>        sectors   = LoadFixture (kDos33Disk);
        vector<Byte>        woz;
        vector<Byte>        prior;
        vector<Byte>        edited;
        vector<Byte>        written;
        vector<int>         changed;
        std::string         reason;
        DiskImage           source;
        DiskImage           before;
        DiskImage           after;
        SectorDecodeReport  report;
        int                 track     = 0;
        size_t              differing = 0;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, source));

        {
            vector<Byte> &  bits    = source.GetTrackBitsForWrite (kBlankedTrack);
            bool            hadBits = std::any_of (bits.begin(), bits.end(),
                                                   [] (Byte b) { return b != 0; });

            Assert::IsTrue (hadBits,
                L"the sentinel track must start out carrying nibbles, or blanking it proves nothing");

            std::fill (bits.begin(), bits.end(), (Byte) 0);
        }

        AssertSucceeded (WozLoader::Serialize (source, woz));
        AssertSucceeded (VolumeImage::Load (woz, "image.woz", prior, report));

        edited = PlaceTheBinary (prior);

        VolumeImage::ChangedTracks (prior, edited, changed);

        Assert::IsTrue (changed.size() > 0, L"the edit must have changed something");
        Assert::IsFalse (Contains (changed, kBlankedTrack),
            L"the sentinel track must be one the edit did not touch");

        AssertSucceeded (VolumeImage::Save (woz, "image.woz", edited, written, reason));

        AssertSucceeded (WozLoader::Load (woz,     before));
        AssertSucceeded (WozLoader::Load (written, after));

        for (track = 0; track < NibblizationLayer::kTrackCount; track++)
        {
            bool  touched = Contains (changed, track);

            if (touched)
            {
                differing++;
                continue;
            }

            Assert::AreEqual (before.GetTrackBitCount (track), after.GetTrackBitCount (track),
                L"an untouched track's bit count must not move");

            Assert::IsTrue (before.GetTrackBits (track) == after.GetTrackBits (track),
                L"an untouched track's packed bits must be byte-identical");
        }

        Assert::AreEqual (changed.size(), differing, L"every changed track must have been visited");

        {
            const vector<Byte> &  bits  = after.GetTrackBits (kBlankedTrack);
            bool                  blank = std::all_of (bits.begin(), bits.end(),
                                                       [] (Byte b) { return b == 0; });

            Assert::IsTrue (blank,
                L"a track carrying nothing a re-encode could reproduce must come back untouched");
        }
    }

    TEST_METHOD (AnUnwritableTrackTheWriteNeverTouches_DoesNotBlockIt_AndKeepsItsBits)
    {
        // Refusal is per track for a reason: a protected or damaged track
        // elsewhere on the disk is not this write's business. It must not
        // become one, and its bits must survive -- rewriting it as standard
        // sectors is exactly the silent loss SC-008 forbids.
        vector<Byte>        sectors = LoadFixture (kDos33Disk);
        vector<Byte>        woz     = MakeWozWithDamagedTrack (sectors, kDamagedTrack);
        vector<Byte>        prior;
        vector<Byte>        edited;
        vector<Byte>        written;
        vector<int>         changed;
        std::string         reason;
        SectorDecodeReport  report;
        DiskImage           before;
        DiskImage           after;

        AssertSucceeded (VolumeImage::Load (woz, "image.woz", prior, report));

        Assert::IsTrue (TrackDecodeOutcome::Partial == report.GetOutcome (kDamagedTrack),
            L"the damaged track must be Partial, or this case is not the one intended");

        edited = PlaceTheBinary (prior);

        VolumeImage::ChangedTracks (prior, edited, changed);

        Assert::IsFalse (Contains (changed, kDamagedTrack),
            L"the damaged track must be one the write does not need");

        AssertSucceeded (VolumeImage::Save (woz, "image.woz", edited, written, reason));

        Assert::IsTrue (reason.empty(), L"damage the write avoids is not a refusal");

        AssertSucceeded (WozLoader::Load (woz,     before));
        AssertSucceeded (WozLoader::Load (written, after));

        Assert::IsTrue (before.GetTrackBits (kDamagedTrack) == after.GetTrackBits (kDamagedTrack),
            L"and the bits it could not decode must still be there afterwards");
    }


    //  ------------------------------------------------------------------
    //  Refusal, and the fact that it is whole-operation.
    //  ------------------------------------------------------------------

    TEST_METHOD (AWriteThatNeedsAPartialTrack_IsRefusedWholeAndCommitsNothing)
    {
        // Skipping the offending track and committing the rest is WORSE than
        // refusing: it produces a half-applied edit and reports success. So the
        // damaged track here is one the write genuinely needs, alongside at
        // least one track that is perfectly writable -- and the assertion is
        // that NOTHING comes back, not merely that an error did.
        vector<Byte>        sectors = LoadFixture (kDos33Disk);
        vector<Byte>        clean;
        vector<Byte>        damaged;
        vector<Byte>        pristine;
        vector<Byte>        prior;
        vector<Byte>        edited;
        vector<Byte>        written;
        vector<int>         firstPass;
        vector<int>         changed;
        std::string         reason;
        SectorDecodeReport  report;
        DiskImage           img;
        HRESULT             hr      = S_OK;
        int                 victim  = -1;
        size_t              i       = 0;
        bool                spans   = false;

        // Pass one on a clean image, purely to learn which tracks the write
        // needs. Damaging one of them first would change where the allocator
        // puts the file, and the case would stop being the case.
        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, img));
        AssertSucceeded (WozLoader::Serialize (img, clean));
        AssertSucceeded (VolumeImage::Load (clean, "image.woz", prior, report));

        edited = PlaceTheBinary (prior);

        VolumeImage::ChangedTracks (prior, edited, firstPass);

        for (i = 0; i < firstPass.size() && victim < 0; i++)
        {
            if (firstPass[i] != kCatalogTrack)
            {
                victim = firstPass[i];
            }
        }

        Assert::IsTrue (victim >= 0, L"the write must land somewhere other than the catalog track");
        Assert::IsTrue (firstPass.size() >= 2,
            L"the refusal must span more than the damaged track, or whole-operation says nothing");

        // Pass two: the same volume with that track made Partial. The VTOC is
        // untouched, so the allocator makes the same choices.
        damaged  = MakeWozWithDamagedTrack (sectors, victim);
        pristine = damaged;

        AssertSucceeded (VolumeImage::Load (damaged, "image.woz", prior, report));

        Assert::IsTrue (TrackDecodeOutcome::Partial == report.GetOutcome (victim),
            L"the track the write needs must be the damaged one");

        edited = PlaceTheBinary (prior);

        VolumeImage::ChangedTracks (prior, edited, changed);

        Assert::IsTrue (Contains (changed, victim), L"and the write must still need it");

        for (i = 0; i < changed.size(); i++)
        {
            if (changed[i] != victim)
            {
                spans = true;
            }
        }

        Assert::IsTrue (spans,
            L"at least one OTHER track must be changed and writable, so that skipping the "
            L"damaged one would have produced a file rather than nothing");

        hr = VolumeImage::Save (damaged, "image.woz", edited, written, reason);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED), hr,
            L"an unwritable track the write needs refuses the write");

        Assert::AreEqual (size_t (0), written.size(),
            L"and produces NOTHING -- a partial commit would be an image carrying half an edit");

        Assert::IsTrue (reason.find ("track " + std::to_string (victim)) != std::string::npos,
            L"the refusal must name the track it is about");

        Assert::IsTrue (damaged == pristine, L"and the image it was handed is untouched");
    }

    TEST_METHOD (AnImageHoldingDataBetweenTracks_RefusesEveryWriteOutright)
    {
        // The whole-image refusal outranks any per-track answer, so a write
        // that would have been fine on every track it needs is still refused.
        // Nothing else can be true of a disk with data between tracks: a
        // sector-level rewrite has nowhere to put it.
        vector<Byte>        sectors = LoadFixture (kDos33Disk);
        vector<Byte>        woz;
        vector<Byte>        prior;
        vector<Byte>        edited;
        vector<Byte>        written;
        std::string         reason;
        SectorDecodeReport  report;
        DiskImage           img;
        HRESULT             hr      = S_OK;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, img));

        // A half-track position resolving to a slot that is not its whole
        // track: what a capture of a protected disk looks like.
        img.SetQuarterTrackSlot (10, 7);

        AssertSucceeded (WozLoader::Serialize (img, woz));
        AssertSucceeded (VolumeImage::Load (woz, "image.woz", prior, report));

        edited = PlaceTheBinary (prior);

        hr = VolumeImage::Save (woz, "image.woz", edited, written, reason);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED), hr);
        Assert::AreEqual (size_t (0), written.size(), L"nothing may be produced");

        // The reason has to be the IMAGE's, not a track's. Merely asserting
        // that something was said passes when the whole-image check is deleted:
        // the per-track answer inherits the image verdict, so the write is still
        // refused and only the explanation degrades -- to one that blames a
        // track for a property of the disk. Measured by mutation.
        Assert::IsTrue (reason.find ("quarter-track") != std::string::npos,
            L"the refusal must be about the image, not about some track it picked");

        Assert::IsTrue (reason.find ("does not decode") == std::string::npos,
            L"and must not blame an individual track: this is settled before any is examined");
    }


    //  ------------------------------------------------------------------
    //  A real 6502 reading what we wrote.
    //  ------------------------------------------------------------------

    //  Every track's bit stream, as the drive would see it, for an image
    //  mounted through the store the emulator itself uses.
    void AssertTheSameDiskSurface (const vector<Byte> & sectors, const vector<Byte> & po)
    {
        DiskImageStore  fromDsk;
        DiskImageStore  fromPo;
        DiskImage     * dskImage = nullptr;
        DiskImage     * poImage  = nullptr;
        int             track    = 0;

        AssertSucceeded (fromDsk.MountFromBytes (6, 0, "merlin.dsk", DiskFormat::Dsk, sectors));
        AssertSucceeded (fromPo.MountFromBytes  (6, 0, "merlin.po",  DiskFormat::Po,  po));

        dskImage = fromDsk.GetImage (6, 0);
        poImage  = fromPo.GetImage  (6, 0);

        Assert::IsNotNull (dskImage);
        Assert::IsNotNull (poImage);

        Assert::AreEqual (dskImage->GetTrackCount(), poImage->GetTrackCount());

        for (track = 0; track < NibblizationLayer::kTrackCount; track++)
        {
            Assert::AreEqual (dskImage->GetTrackBitCount (track), poImage->GetTrackBitCount (track),
                L"every track must be the same length on both");

            Assert::IsTrue (dskImage->GetTrackBits (track) == poImage->GetTrackBits (track),
                L"and carry the same nibbles -- a wrong reorder puts sectors at the wrong "
                L"physical addresses, which is invisible to any round trip and fatal here");
        }
    }

    TEST_METHOD (AVolumeWrittenAsPo_PresentsTheSameDiskSurfaceAsTheVolumeItCameFrom)
    {
        // Mounted through the store the emulator itself uses, a ProDOS-ordered
        // image and the DOS-ordered image it was written from must produce the
        // SAME bit stream on every track. That is the whole guest-visible
        // claim in one sentence: the drive cannot tell the two apart, so no
        // program running on the machine can either.
        vector<Byte>  sectors = LoadFixture (kDos33Disk);
        vector<Byte>  po;
        std::string   reason;

        AssertSucceeded (VolumeImage::Save (sectors, "merlin.po", sectors, po, reason));

        AssertTheSameDiskSurface (sectors, po);
    }

    //  Boots a mounted image, stopping as soon as `until` appears on screen.
    //  Returns the screen; `outCycles` is what it cost to get there, which is
    //  kBootCycles exactly when the guest never arrived.
    //
    //  Sliced rather than run flat out because a machine reading a mis-ordered
    //  disk executes whatever it loaded, and every illegal opcode it hits is a
    //  logged line -- measured at over a gigabyte for one 40-megacycle run.
    std::string BootAndScrape (const std::string  & name,
                               DiskFormat           format,
                               const vector<Byte> & bytes,
                               const std::string  & until,
                               uint64_t           & outCycles)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        DiskImage    * img    = nullptr;
        std::string    joined;
        uint64_t       spent  = 0;

        AssertSucceeded (host.BuildApple2eWithDisk2 (core), L"BuildApple2eWithDisk2 must succeed");

        core.PowerCycle();

        AssertSucceeded (core.diskStore->MountFromBytes (6, 0, name, format, bytes),
            L"MountFromBytes must succeed");

        img = core.diskStore->GetImage (6, 0);
        Assert::IsNotNull (img, L"the mounted image must be present");
        core.diskController->SetExternalDisk (0, img);

        core.bus->WriteByte (kIntCxRomOff, 0);
        core.cpu->SetPC (kBootRomEntry);

        while (spent < kBootCycles && joined.find (until) == std::string::npos)
        {
            std::vector<std::string>  rows;

            core.RunCycles (kBootSlice);
            spent += kBootSlice;

            rows   = TextScreenScraper::Scrape (core);
            joined = "";

            for (const std::string & row : rows)
            {
                joined += row;
                joined += "\n";
            }
        }

        outCycles = spent;

        return joined;
    }

    TEST_METHOD (AVolumeWrittenAsPo_BootsToTheSameScreenAsTheVolumeItCameFrom)
    {
        // The guest-visible half. A real 6502 runs the boot ROM and then DOS's
        // own RWTS against the ProDOS-ordered image this code wrote; the same
        // volume as a DOS-ordered image is the control. A reorder that is wrong
        // but self-consistent puts every sector at the wrong physical address,
        // and the machine simply does not get where it was going.
        //
        // The control screen is required to carry real content, because two
        // blank screens compare equal and would prove nothing at all.
        vector<Byte>  sectors   = LoadFixture (kDos33Disk);
        vector<Byte>  po;
        std::string   reason;
        std::string   fromDsk;
        std::string   fromPo;
        uint64_t      dskCycles = 0;
        uint64_t      poCycles  = 0;

        AssertSucceeded (VolumeImage::Save (sectors, "merlin.po", sectors, po, reason));

        // Checked before either machine is started. It is the same claim this
        // test makes, stated cheaply, and putting it first means a mis-ordered
        // image fails here in milliseconds rather than by running a 6502
        // through forty megacycles of whatever it managed to load.
        AssertTheSameDiskSurface (sectors, po);

        fromDsk = BootAndScrape ("merlin.dsk", DiskFormat::Dsk, sectors, kMerlinMenuLine, dskCycles);

        Assert::IsTrue (fromDsk.find (kMerlinMenuLine) != std::string::npos,
            L"the control boot must actually load the assembler off the disk and show its "
            L"menu -- two blank screens would compare equal and prove nothing");

        fromPo = BootAndScrape ("merlin.po", DiskFormat::Po, po, kMerlinMenuLine, poCycles);

        Assert::IsTrue (fromPo.find (kMerlinMenuLine) != std::string::npos,
            L"and the ProDOS-ordered image this code wrote must get the guest to the same "
            L"place: a wrong reorder puts every sector at the wrong physical address and "
            L"the machine never arrives");

        Assert::AreEqual (fromDsk, fromPo, L"screen for screen");
        Assert::AreEqual (dskCycles, poCycles);
    }
};
