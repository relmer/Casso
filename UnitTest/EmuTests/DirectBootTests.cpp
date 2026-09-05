#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/DirectBootBuilder.h"
#include "Devices/Disk/Dos33Skeleton.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DirectBootTests
//
//  The shape of a disk that boots a binary with nothing else on it, and the
//  boundary of what such a disk can carry.
//
//  THE LAYOUT ORACLE IS APPLE'S, NOT OURS. A payload laid out in the wrong
//  sector order reads back perfectly through our own reader -- the same skew
//  applied on the way out and the way in is the identity -- and hands the
//  guest its pages shuffled. So the mapping is checked against the table DOS
//  3.3 carries inside its own boot sector, persisted below byte for byte from
//  a real System Master, rather than against the constant the builder used.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DirectBootTests)
{
public:
    //  DOS 3.3's interleave, the inverse of the table DOS carries in its own
    //  boot sector. Written out rather than read off a disk: what the two
    //  builder cases assert is the ORDER OUR BUILDER LAYS PAGES DOWN IN, and
    //  the interleave is only the input they need to say it. That our copy
    //  really does match Apple's is a different claim, and the inverse case
    //  below makes it against the skew persisted from a real master.
    static std::vector<int> PhysicalToLogical()
    {
        return std::vector<int> { 0, 7, 14, 6, 13, 5, 12, 4,
                                  11, 3, 10, 2, 9, 1, 8, 15 };
    }




    //  The skew DOS 3.3 itself boots through, copied byte for byte from a
    //  real System Master: file offset $4D-$5C of track 0 sector 0, indexed
    //  by DOS logical sector and giving the address-field number the drive
    //  presents that sector under.
    //
    //  CONSUMED BY BOOT0, NOT RWTS -- RWTS is not in memory yet. The boot ROM
    //  loads the sector at $0800, and the instruction at $0824 is
    //  `BD 4D 08  LDA $084D,X`, indexed by the sector counter at $08FF and
    //  stored to $3D -- the sector number for the drive ROM's read call --
    //  while boot0 pulls the rest of track 0 into descending pages. The skew
    //  exists so the next wanted sector arrives under the head about when the
    //  loader is ready for it, instead of a revolution later.
    //
    //  A static copy rather than a read off the master, by owner decision:
    //  the master is only a much more involved way to obtain the same sixteen
    //  persisted bytes, and reading it live is what kept this claim out of
    //  the unit suite.
    static constexpr Byte  kDos33SkewFromTheMaster[16] =
    {
        0x00, 0x0D, 0x0B, 0x09, 0x07, 0x05, 0x03, 0x01,
        0x0E, 0x0C, 0x0A, 0x08, 0x06, 0x04, 0x02, 0x0F,
    };

    static constexpr size_t  kSectorBytes  = (size_t) NibblizationLayer::kSectorByteSize;
    static constexpr size_t  kSectorsTrack = (size_t) NibblizationLayer::kSectorsPerTrack;

    //  What the boot ROM finds in the first byte of the sector it reads: the
    //  number of sectors to fetch before jumping to $0801. One means "this
    //  sector and no more".
    static constexpr Byte  kRomReadsOnlyTheLoader = 1;

    //  Where the built loader keeps what a build has to fill in. Restated
    //  from the assembly listing rather than read off the class.
    static constexpr size_t  kSectorsLeftAt = 0xF0;
    static constexpr size_t  kLoadPageAt    = 0xF1;
    static constexpr size_t  kTrackAt       = 0xF2;
    static constexpr size_t  kPhaseAt       = 0xF3;
    static constexpr size_t  kIndexAt       = 0xF4;
    static constexpr size_t  kReadOrderAt   = 0xE0;
    static constexpr size_t  kJumpOpcodeAt  = 0x53;
    static constexpr size_t  kEntryLowAt    = 0x54;
    static constexpr size_t  kEntryHighAt   = 0x55;

    //  The order the loader asks a track's sectors for: every other one, then
    //  the ones it skipped. Written out rather than computed, so that a builder
    //  whose construction drifted is caught by a table instead of agreeing with
    //  a second copy of its own arithmetic.
    static std::vector<int> ReadOrder()
    {
        return std::vector<int> { 0, 2, 4, 6, 8, 10, 12, 14,
                                  1, 3, 5, 7, 9, 11, 13, 15 };
    }

    static constexpr Byte    kJumpAbsolute  = 0x4C;

    static constexpr Word    kSomewhereSafe = 0x6000;

    //

    static std::vector<Byte> PayloadOfPages (size_t pages)
    {
        std::vector<Byte>  payload (pages * kSectorBytes, 0);
        size_t             page = 0;
        size_t             i    = 0;

        //  Every byte of a page carries that page's number, so a page landing
        //  in the wrong place is visible wherever it is looked at.
        for (page = 0; page < pages; page++)
        {
            for (i = 0; i < kSectorBytes; i++)
            {
                payload[page * kSectorBytes + i] = (Byte) page;
            }
        }

        return payload;
    }

    //  Two mappings, both of which have to be applied: the page read Nth is
    //  the one at the Nth sector of the read order, and that physical sector
    //  sits at its own logical place in the buffer.
    static size_t PayloadSectorOffset (const std::vector<int> & physicalToLogical, size_t page)
    {
        int  track    = DirectBootBuilder::kFirstPayloadTrack + (int) (page / kSectorsTrack);
        int  physical = ReadOrder()[page % kSectorsTrack];

        return Dos33Skeleton::GetSectorOffset (track, physicalToLogical[(size_t) physical]);
    }

    //  A vector nothing could produce, so "untouched" is a real assertion
    //  rather than one satisfied by an empty result.
    static std::vector<Byte> Sentinel()
    {
        return std::vector<Byte> (3, 0x5A);
    }


    //
    //  ------------------------------------------------------------------
    //  The mapping, against evidence the builder does not own.
    //  ------------------------------------------------------------------

    //

    TEST_METHOD (Dos33SkewOn16SectorDisk2Media_IsTheExactInverseOfOurInterleave)
    {
        //  Scoped to DOS 3.3 on 16-sector 5.25-inch Disk II media, which the
        //  Duodisk and the //c internal drive share, and said in the name
        //  because it is NOT a universal: a .po on identical media carries a
        //  different table, and the concept is meaningless for UniDisk 3.5,
        //  ProFile, or any block device.
        //
        //  Getting the interleave wrong is SILENT in the emulator -- every
        //  sector is still found, just a revolution later -- and only slow on
        //  real hardware, which is why the check is worth keeping at all.
        std::vector<bool>  seen (kSectorsTrack, false);
        size_t             logical = 0;
        int                skewed  = 0;



        for (logical = 0; logical < kSectorsTrack; logical++)
        {
            int  physical = (int) kDos33SkewFromTheMaster[logical];

            Assert::IsTrue (physical < (int) kSectorsTrack,
                L"every entry in DOS's own skew table must map to a sector on the track");

            Assert::IsFalse (seen[(size_t) physical],
                L"and must map to a different one, or it is not a permutation and cannot be "
                L"the mapping anything reads through");

            seen[(size_t) physical] = true;

            Assert::AreEqual ((int) logical,
                              NibblizationLayer::GetDosFileIndexForPhysicalSector (physical),
                L"the sector the drive presents at a physical position must be the one DOS "
                L"3.3's own boot table places there");

            if (physical != (int) logical)
            {
                skewed++;
            }
        }

        Assert::IsTrue (skewed > 0,
            L"and the two orders must actually differ, or every assertion above is "
            L"satisfied by a builder that ignores the skew entirely");
    }

    TEST_METHOD (Capacity_IsTheDistanceFromThePayloadToTheTopOfMemory)
    {
        //  $C000 is where memory stops. Restated rather than borrowed.
        constexpr size_t  kTopOfMemory     = 0xC000;
        constexpr size_t  kLowestPayload   = 0x0900;
        constexpr size_t  kBytesAtLowest   = kTopOfMemory - kLowestPayload;   // 46,848
        constexpr size_t  kSectorsAtLowest = kBytesAtLowest / kSectorBytes;   // 183



        Assert::AreEqual (kBytesAtLowest, DirectBootBuilder::GetCapacity (0x0900),
            L"a payload at the lowest address it may load at can fill memory to the top");

        Assert::AreEqual (kSectorsAtLowest, DirectBootBuilder::kMostSectors,
            L"which is one hundred and eighty-three sectors, and that is the number a "
            L"refusal has to be able to state");

        Assert::AreEqual (kTopOfMemory - 0x6000, DirectBootBuilder::GetCapacity (0x6000),
            L"and a payload loading higher can carry proportionally less");

        Assert::AreEqual (kSectorBytes, DirectBootBuilder::GetCapacity (0xBF00),
            L"down to the last page below the ceiling");
    }

    TEST_METHOD (Capacity_IsZeroForAnyAddressTheBootPathCannotLoadAt)
    {
        Assert::AreEqual (size_t (0), DirectBootBuilder::GetCapacity (0x0800),
            L"the loader's own page is not available to a payload");

        Assert::AreEqual (size_t (0), DirectBootBuilder::GetCapacity (0x08FF),
            L"nor is any part of it");

        Assert::AreEqual (size_t (0), DirectBootBuilder::GetCapacity (0x0300),
            L"nor is the boot ROM's decode table");

        Assert::AreEqual (size_t (0), DirectBootBuilder::GetCapacity (0xC000),
            L"and $C000 is not memory at all");
    }

    TEST_METHOD (Build_APayloadOfExactlyTheCapacity_IsBuilt)
    {
        std::vector<Byte>  payload (DirectBootBuilder::kLargestCapacity, 0xEA);
        std::vector<Byte>  built;
        std::string        refusal;
        DirectBootSpec     spec;



        spec.loadAddress  = 0x0900;
        spec.entryAddress = 0x0900;

        AssertSucceeded (DirectBootBuilder::Build (payload, spec, built, refusal),
            L"the largest payload the boot path can load must be buildable, or the stated "
            L"capacity is a number nothing can reach");

        Assert::AreEqual (std::string(), refusal, L"and must be refused for nothing");

        Assert::AreEqual (size_t (NibblizationLayer::kImageByteSize), built.size(),
            L"producing a whole 5.25-inch sector image");

        Assert::AreEqual ((int) DirectBootBuilder::kMostSectors, (int) built[kSectorsLeftAt],
            L"and the loader must be told to pull exactly that many sectors");
    }

    TEST_METHOD (Build_OneByteOverTheCapacity_IsRefusedWithTheCapacityStated)
    {
        std::vector<Byte>  payload (DirectBootBuilder::kLargestCapacity + 1, 0xEA);
        std::vector<Byte>  built   = Sentinel();
        std::string        refusal;
        DirectBootSpec     spec;



        spec.loadAddress  = 0x0900;
        spec.entryAddress = 0x0900;

        AssertFailed (DirectBootBuilder::Build (payload, spec, built, refusal),
            L"one byte past what the boot path can load must be refused");

        Assert::AreEqual (std::string ("the payload is 46849 bytes and a direct-boot image "
                                       "loading at $0900 can carry 46848 (183 sectors)"),
            refusal,
            L"and the refusal must state the capacity, not merely that there was too much "
            L"-- a caller told only 'too large' cannot decide what to do about it");

        Assert::IsTrue (built == Sentinel(),
            L"while the caller's buffer is left exactly as it was: a refused build hands "
            L"back nothing, rather than a partly-formed image somebody might commit");
    }

    TEST_METHOD (Build_ARefusalNamesExactlyOneReason)
    {
        std::vector<Byte>  payload (DirectBootBuilder::kLargestCapacity + 1, 0xEA);
        std::vector<Byte>  built;
        std::string        refusal;
        DirectBootSpec     spec;



        spec.loadAddress  = 0x0300;
        spec.entryAddress = 0x0300;

        AssertFailed (DirectBootBuilder::Build (payload, spec, built, refusal));

        //  Both an unusable address and an oversized payload are true of this
        //  call. Only the first is worth saying: two candidate explanations
        //  for one refusal is what leaves a reader guessing.
        Assert::AreEqual (std::string ("a direct-boot payload must load between $0900 and "
                                       "$BFFF. Page $08 contains the loader and $C000 is "
                                       "not memory. $0300 was requested"),
            refusal,
            L"the address is the reason; the size is a consequence of it");

        Assert::AreEqual (std::string::npos, refusal.find ('\n'),
            L"and it is one reason, not a list");
    }

    TEST_METHOD (Build_AnEmptyPayload_IsRefusedRatherThanProducingABlankDisk)
    {
        std::vector<Byte>  payload;
        std::vector<Byte>  built = Sentinel();
        std::string        refusal;
        DirectBootSpec     spec;



        AssertFailed (DirectBootBuilder::Build (payload, spec, built, refusal));

        Assert::AreEqual (std::string ("there is nothing to boot into: the payload is empty"),
            refusal);

        Assert::IsTrue (built == Sentinel(), L"and nothing is handed back");
    }

    TEST_METHOD (Build_AnEntryOutsideTheLoadedBytes_IsRefused)
    {
        std::vector<Byte>  payload = PayloadOfPages (1);
        std::vector<Byte>  built;
        std::string        refusal;
        DirectBootSpec     spec;



        spec.loadAddress  = kSomewhereSafe;
        spec.entryAddress = (Word) (kSomewhereSafe + kSectorBytes);

        AssertFailed (DirectBootBuilder::Build (payload, spec, built, refusal),
            L"an entry one byte past the payload would have the guest execute memory "
            L"nothing loaded");

        Assert::AreEqual (std::string ("the entry address $6100 is outside the payload, "
                                       "which occupies $6000 through $60FF"),
            refusal);
    }


    //
    //  ------------------------------------------------------------------
    //  What the image is made of.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (Build_PutsTheLoaderWhereTheBootRomLooks_AndTellsItToStopAfterIt)
    {
        std::vector<Byte>  payload = PayloadOfPages (3);
        std::vector<Byte>  built;
        std::string        refusal;
        DirectBootSpec     spec;



        spec.loadAddress  = kSomewhereSafe;
        spec.entryAddress = kSomewhereSafe;

        AssertSucceeded (DirectBootBuilder::Build (payload, spec, built, refusal));

        //  The boot ROM reads track 0's first address field into $0800 and
        //  then reads on for as long as the next sector number is below the
        //  byte it just landed at offset zero.
        Assert::AreEqual ((int) kRomReadsOnlyTheLoader, (int) built[0],
            L"so the loader sector must request itself from the ROM and nothing else -- a "
            L"larger number here would have the ROM overwrite the loader with payload");

        Assert::AreEqual (3, (int) built[kSectorsLeftAt],
            L"the loader is told how many sectors the payload occupies");

        Assert::AreEqual ((int) (kSomewhereSafe >> 8), (int) built[kLoadPageAt],
            L"and which page to start filling");

        Assert::AreEqual (0, (int) built[kTrackAt],
            L"starting from the track the boot ROM leaves the head on");

        Assert::AreEqual (0, (int) built[kPhaseAt],
            L"and the magnet the boot ROM leaves energized, which is what makes the first "
            L"step land on track 1 rather than somewhere near it");
    }

    TEST_METHOD (Build_AnEntryAwayFromTheLoadAddress_IsWhatTheLoaderJumpsTo)
    {
        std::vector<Byte>  payload = PayloadOfPages (2);
        std::vector<Byte>  built;
        std::string        refusal;
        DirectBootSpec     spec;
        Word               entry   = (Word) (kSomewhereSafe + 0x0123);



        spec.loadAddress  = kSomewhereSafe;
        spec.entryAddress = entry;

        AssertSucceeded (DirectBootBuilder::Build (payload, spec, built, refusal));

        Assert::AreEqual ((int) kJumpAbsolute, (int) built[kJumpOpcodeAt],
            L"the loader finishes with an absolute jump");

        Assert::AreEqual ((int) (entry & 0xFF), (int) built[kEntryLowAt],
            L"whose target is the entry that was requested");

        Assert::AreEqual ((int) (entry >> 8), (int) built[kEntryHighAt],
            L"both halves of it, and not the load address");

        Assert::AreNotEqual ((int) (spec.loadAddress & 0xFF), (int) built[kEntryLowAt],
            L"which this case can only claim because the two differ in that byte");
    }

    TEST_METHOD (Build_PlacesEachPageWhereTheDriveWillPresentItInTurn)
    {
        //  Twenty pages, so the payload runs off the end of one track and the
        //  loader has to step. A single-track case cannot tell a builder that
        //  seeks from one that does not.
        std::vector<int>   physicalToLogical = PhysicalToLogical();
        std::vector<Byte>  payload           = PayloadOfPages (20);
        std::vector<Byte>  built;
        std::string        refusal;
        DirectBootSpec     spec;
        size_t             page              = 0;
        int                displaced         = 0;



        spec.loadAddress  = kSomewhereSafe;
        spec.entryAddress = kSomewhereSafe;

        AssertSucceeded (DirectBootBuilder::Build (payload, spec, built, refusal));

        for (page = 0; page < 20; page++)
        {
            size_t  at       = PayloadSectorOffset (physicalToLogical, page);
            size_t  straight = Dos33Skeleton::GetSectorOffset (
                                   DirectBootBuilder::kFirstPayloadTrack
                                   + (int) (page / kSectorsTrack),
                                   (int) (page % kSectorsTrack));

            Assert::AreEqual ((int) page, (int) built[at],
                L"page N has to sit in the sector the drive hands back Nth, which is what "
                L"the loader requests -- not in the Nth sector of the buffer");

            if (at != straight)
            {
                displaced++;
            }
        }

        Assert::IsTrue (displaced > 0,
            L"and the two must differ for some page, or this case is satisfied by a "
            L"builder that wrote the payload straight down the buffer");
    }

    //  The loader reads the order out of its own sector, and the builder lays
    //  the pages down against that same order. They are two halves of one
    //  agreement and nothing but this case holds them together: a build that
    //  wrote one and not the other produces an image that loads at full speed
    //  and hands the guest its pages shuffled.
    TEST_METHOD (Build_TheOrderInTheLoaderIsTheOrderThePagesWereLaidDownIn)
    {
        std::vector<int>   expected = ReadOrder();
        std::vector<Byte>  payload  = PayloadOfPages (16);
        std::vector<Byte>  built;
        std::string        refusal;
        DirectBootSpec     spec;
        size_t             i        = 0;
        int                gap      = 0;



        spec.loadAddress  = kSomewhereSafe;
        spec.entryAddress = kSomewhereSafe;

        AssertSucceeded (DirectBootBuilder::Build (payload, spec, built, refusal));

        for (i = 0; i < kSectorsTrack; i++)
        {
            Assert::AreEqual (expected[i], (int) built[kReadOrderAt + i],
                L"the sixteen bytes the loader indexes are the read order itself");
        }

        //  Consecutive requests a sector apart is the ascending order this
        //  replaced, and it costs a revolution each. Two is what was measured
        //  to be both sufficient and the floor.
        gap = expected[1] - expected[0];

        Assert::AreEqual (2, gap,
            L"and consecutive requests must be two sectors apart, which is what makes a "
            L"track cost two revolutions instead of sixteen");

        Assert::AreEqual (0, (int) built[kIndexAt],
            L"the loader starts at the beginning of that order, and says so rather than "
            L"inheriting a zero from the buffer");
    }

    TEST_METHOD (Build_AnUnalignedLoadAddress_CarriesItsOwnLeadIn)
    {
        //  The ROM reads whole pages into page-aligned buffers, so a payload
        //  starting part-way into a page rides behind the bytes below it.
        constexpr size_t   kLeadIn = 0x80;

        std::vector<int>   physicalToLogical = PhysicalToLogical();
        std::vector<Byte>  payload (1, 0xAA);
        std::vector<Byte>  built;
        std::string        refusal;
        DirectBootSpec     spec;
        size_t             at      = 0;
        size_t             i       = 0;



        spec.loadAddress  = (Word) (kSomewhereSafe + kLeadIn);
        spec.entryAddress = spec.loadAddress;

        Assert::AreEqual (size_t (1),
            DirectBootBuilder::GetSectorsNeeded (spec.loadAddress, payload.size()),
            L"one byte behind a lead-in is still one sector");

        AssertSucceeded (DirectBootBuilder::Build (payload, spec, built, refusal));

        at = PayloadSectorOffset (physicalToLogical, 0);

        for (i = 0; i < kLeadIn; i++)
        {
            Assert::AreEqual (0, (int) built[at + i],
                L"the lead-in is filler and carries nothing of the payload");
        }

        Assert::AreEqual (0xAA, (int) built[at + kLeadIn],
            L"and the payload's first byte lands exactly where its load address falls "
            L"within the page");
    }

    TEST_METHOD (Build_LeavesNoFilesystemOnTheDiskAtAll)
    {
        std::vector<Byte>  payload = PayloadOfPages (2);
        std::vector<Byte>  built;
        std::string        refusal;
        DirectBootSpec     spec;
        size_t             i       = 0;
        size_t             vtocAt  = 0;
        int                nonZero = 0;



        spec.loadAddress  = kSomewhereSafe;
        spec.entryAddress = kSomewhereSafe;

        AssertSucceeded (DirectBootBuilder::Build (payload, spec, built, refusal));

        Assert::IsTrue (VolumeKind::Unknown == VolumeImage::DetectFilesystem (built),
            L"nothing on this disk is a filesystem -- that is the whole point of it, and "
            L"a builder that quietly formatted the rest would still boot");

        //  Track 17 is where DOS 3.3 keeps its volume table of contents and
        //  its catalog. On this disk it is empty space.
        vtocAt = Dos33Skeleton::GetSectorOffset (17, 0);

        for (i = 0; i < kSectorBytes * kSectorsTrack; i++)
        {
            if (built[vtocAt + i] != 0)
            {
                nonZero++;
            }
        }

        Assert::AreEqual (0, nonZero,
            L"and the track a catalog would live on is untouched");
    }

    TEST_METHOD (Build_ProducesAnImageThatDecodesBackThroughTheDriveUnchanged)
    {
        //  The cheap question, asked before any of the guest-visible cases
        //  start a processor: an image that cannot survive being encoded and
        //  read back by the drive is not worth booting.
        std::vector<Byte>   payload = PayloadOfPages (18);
        std::vector<Byte>   built;
        std::vector<Byte>   decoded;
        std::string         refusal;
        DirectBootSpec      spec;
        DiskImage           image;
        SectorDecodeReport  report;



        spec.loadAddress  = kSomewhereSafe;
        spec.entryAddress = kSomewhereSafe;

        AssertSucceeded (DirectBootBuilder::Build (payload, spec, built, refusal));

        AssertSucceeded (NibblizationLayer::NibblizeDsk (built, image));
        AssertSucceeded (NibblizationLayer::Denibblize (image, DiskFormat::Dsk, decoded, report));

        Assert::IsTrue (built == decoded,
            L"every sector the builder wrote must come back off the drive as it went on");
    }
};
