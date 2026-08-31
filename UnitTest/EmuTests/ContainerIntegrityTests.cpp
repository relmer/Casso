#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/BlankDiskBuilder.h"
#include "Devices/Disk/MountDiagnosis.h"
#include "Devices/Disk/NibbleImageCodec.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/SectorDecodeReport.h"
#include "Devices/Disk/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ContainerIntegrityTests
//
//  Every container this tool writes, made, written to, read back, written to
//  again, and checked in full -- the second write's data AND the first's AND
//  the parts nothing touched.
//
//  WHY BIT PATTERNS AND NOT A FILL. The 6-and-2 encoder is dense bit
//  manipulation, not a copy. A sector byte is split across two nibbles: its
//  high six bits go to one, and its low two -- REVERSED, bit 0 to position 1
//  and bit 1 to position 0 -- share a nibble with the low bits of the bytes 86
//  and 172 positions later. Three source bytes feed one encoded nibble.
//
//  That makes several single-bit mistakes possible, and each hides from a
//  different fill:
//
//    * the two-bit reversal done wrong swaps bits 0 and 1 of every byte, which
//      a uniform fill CANNOT show, because reversing two equal bits is a no-op;
//    * a byte's low bits pairing with the wrong group's high bits (i, i+86 or
//      i+172 confused) needs bytes that differ from each other to show at all;
//    * the MSB-first bit packing can be off by a position, which shows on a
//      pattern with a single bit set and nowhere else;
//    * a sector or track landing in the wrong slot needs data that says where
//      it came from.
//
//  So the patterns below are chosen against those four, in the spirit of a
//  memory test: walking ones, walking zeros, alternating halves, and a byte
//  that encodes its own address. A fill of 0x00 or 0xE7 -- which the other
//  suites here use -- passes all four defects.
//
//  ONE PATTERN PER REGION rather than one per pass, so a single round trip
//  carries all of them and the suite does not pay for a GCR encode per pattern.
//
//  Everything is in memory. No file is opened: the containers are built,
//  rendered and re-read as byte vectors.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ContainerIntegrityTests)
{
public:

    static constexpr int  kSectorSize    = NibblizationLayer::kSectorByteSize;
    static constexpr int  kSectorsPerTrk = NibblizationLayer::kSectorsPerTrack;
    static constexpr int  kTracks        = NibblizationLayer::kTrackCount;

    //  What a container is called, and how big its tracks are when that is a
    //  choice. Kept beside the test rather than derived, because the point is
    //  to sweep containers INDEPENDENTLY of the code that lists them -- a bug
    //  that drops one from the builder's list would otherwise drop it here too.
    struct ContainerCase
    {
        const char  *  path;
        DiskFormat     format;
        size_t         nibbleTrackSize;
    };

    static std::vector<ContainerCase>  AllContainers()
    {
        return {
            { "t.dsk", DiskFormat::Dsk, 0 },
            { "t.do",  DiskFormat::Do,  0 },
            { "t.po",  DiskFormat::Po,  0 },
            { "t.woz", DiskFormat::Woz, 0 },
            { "t.nib", DiskFormat::Nib, NibbleImageCodec::kNibTrackSize },
            { "t.nb2", DiskFormat::Nib, NibbleImageCodec::kNb2TrackSize },
        };
    }



    //
    //  The patterns. Each takes the byte's absolute offset in the volume so a
    //  misplaced sector carries the wrong values rather than plausible ones.
    //

    //  One bit set, marching. Isolates a bit-packing position error: only this
    //  shows WHICH bit moved.
    static Byte  WalkingOnes (size_t offset)
    {
        return static_cast<Byte> (1u << (offset % 8));
    }

    //  Its complement. A stuck-high bit hides in walking ones and shows here.
    static Byte  WalkingZeros (size_t offset)
    {
        return static_cast<Byte> (~(1u << (offset % 8)));
    }

    //  Adjacent bits differ everywhere, which is what catches the low-two-bit
    //  reversal: 0xAA and 0x55 are each other's bit-pair swap.
    static Byte  Alternating (size_t offset)
    {
        return static_cast<Byte> (((offset & 1) != 0) ? 0x55 : 0xAA);
    }

    //  The byte says where it lives. Catches a sector in the wrong slot and the
    //  86/172 group confusion, neither of which changes any single bit's value
    //  -- they move whole bytes, so only content that varies with position can
    //  show it. Mixed high and low so both halves of the 6-and-2 split differ.
    static Byte  AddressInData (size_t offset)
    {
        return static_cast<Byte> (((offset >> 5) & 0xF8) ^ (offset & 0xFF));
    }

    //  Every value in the alphabet, so no legal byte goes unencoded.
    static Byte  AllValues (size_t offset)
    {
        return static_cast<Byte> (offset & 0xFF);
    }

    //  Position WITHIN a sector, and never a multiple of four.
    //
    //  It changes at every 256-byte edge, which is where an off-by-one in
    //  sector indexing shows, and it carries non-zero low bits everywhere --
    //  the two bits the encoder treats separately from the other six, which no
    //  other pattern here guarantees at a sector boundary.
    //
    //  WHAT IT DOES NOT COVER, recorded so nobody assumes otherwise. The
    //  encoder's third-group guard, `i + 172 < 256`, was the reason this
    //  pattern was written: a `<=` there reads sectorData[256], one past the
    //  volume for the last sector. That mutation was tried and this sweep did
    //  NOT catch it, for a structural reason -- the decoder maps the third
    //  group to encoded[0..83] and never reads encoded[84]'s bits at all, so
    //  the stray read lands where nothing looks. The out-of-bounds access is
    //  real and the data round-trips perfectly regardless.
    //
    //  No round-trip test can find that, and -- measured, not assumed -- an
    //  address sanitizer does not either. A volume is 143,360 bytes, which is
    //  exactly 35 pages, so the allocation ends flush on a page boundary and
    //  ASAN's large-allocation path has nowhere to put a redzone; the stray
    //  read lands in addressable memory. The same read one byte past a
    //  SIXTEEN-byte vector is caught instantly, which is how that was pinned
    //  down. Catching this one needs a guard-page allocator, or a buffer
    //  deliberately allocated one byte long and offset so its end falls
    //  mid-block. A reader remains the cheapest instrument.
    static Byte  SectorEdges (size_t offset)
    {
        size_t  withinSector = offset % (size_t) kSectorSize;

        return static_cast<Byte> (((withinSector * 3) | 1) & 0xFF);
    }



    using PatternFn = Byte (*) (size_t);

    struct Region
    {
        int          firstTrack;
        int          trackCount;
        PatternFn    pattern;
        const char * name;
    };

    //  Tracks 1..16 and 18..33, leaving track 0 (boot) and 17 (the DOS
    //  catalog) as the builder made them -- so the untouched-data check has
    //  something structured to compare, not just more of the same pattern.
    static std::vector<Region>  FirstPass()
    {
        return {
            { 1,  4, &WalkingOnes,   "walking ones"   },
            { 5,  4, &WalkingZeros,  "walking zeros"  },
            { 9,  4, &Alternating,   "alternating"    },
            { 13, 2, &AddressInData, "address in data"},
            { 15, 2, &SectorEdges,   "sector edges"   },
        };
    }

    //  A second write, landing elsewhere, so the first pass's data has to
    //  survive a whole further round trip beside it.
    static std::vector<Region>  SecondPass()
    {
        return {
            { 18, 6, &AllValues,     "all values"     },
            { 24, 5, &AddressInData, "address in data"},
            { 29, 5, &SectorEdges,   "sector edges"   },
        };
    }



    static void  ApplyRegion (std::vector<Byte> & sectors, const Region & r)
    {
        size_t  first = (size_t) r.firstTrack * kSectorsPerTrk * kSectorSize;
        size_t  count = (size_t) r.trackCount * kSectorsPerTrk * kSectorSize;
        size_t  i     = 0;

        for (i = 0; i < count; i++)
        {
            sectors[first + i] = r.pattern (first + i);
        }
    }



    static void  AssertRegion (const std::vector<Byte>  & sectors,
                               const Region             & r,
                               const ContainerCase      & c)
    {
        size_t  first = (size_t) r.firstTrack * kSectorsPerTrk * kSectorSize;
        size_t  count = (size_t) r.trackCount * kSectorsPerTrk * kSectorSize;
        size_t  i     = 0;

        for (i = 0; i < count; i++)
        {
            if (sectors[first + i] == r.pattern (first + i))
            {
                continue;
            }

            //  Report the first wrong byte with everything needed to place it:
            //  a bare "not equal" over 143,360 bytes says nothing about which
            //  of the four defects above is present.
            std::wstringstream  msg;

            msg << L"container " << c.path
                << L", pattern " << r.name
                << L", volume offset " << (first + i)
                << L" (track "  << ((first + i) / (kSectorsPerTrk * kSectorSize))
                << L", byte "   << ((first + i) % (kSectorsPerTrk * kSectorSize))
                << L"): expected 0x" << std::hex << (int) r.pattern (first + i)
                << L" got 0x" << (int) sectors[first + i];

            Assert::Fail (msg.str().c_str());
        }
    }



    //  One container, built, written twice, and checked in full each time.
    static void  ExerciseContainer (const ContainerCase & c)
    {
        BlankDiskSpec       spec;
        std::vector<Byte>   blank;
        std::vector<Byte>   afterFirst;
        std::vector<Byte>   afterSecond;
        std::vector<Byte>   sectors;
        std::vector<Byte>   edited;
        std::vector<Byte>   readBack;
        SectorDecodeReport  report;
        std::string         refusal;

        spec.format          = c.format;
        spec.contents        = BlankDiskContents::Unformatted;
        spec.nibbleTrackSize = c.nibbleTrackSize;

        AssertSucceeded (BlankDiskBuilder::Build (spec, BootPayload(), blank),
            L"the container must be buildable before anything is written to it");
        Assert::IsFalse (blank.empty());

        AssertSucceeded (VolumeImage::Load (blank, c.path, sectors, report));
        Assert::AreEqual ((size_t) NibblizationLayer::kImageByteSize, sectors.size(),
            L"every container decodes to one volume's worth of sectors");

        //  First write.
        edited = sectors;

        for (const Region & r : FirstPass())
        {
            ApplyRegion (edited, r);
        }

        AssertSucceeded (VolumeImage::Save (blank, c.path, edited, afterFirst, refusal),
            L"the first write must be renderable back into the container");
        Assert::AreEqual (std::string(), refusal);
        Assert::AreEqual (blank.size(), afterFirst.size(),
            L"a write must not change the container's size");

        AssertSucceeded (VolumeImage::Load (afterFirst, c.path, readBack, report));

        for (const Region & r : FirstPass())
        {
            AssertRegion (readBack, r, c);
        }

        //  Second write, elsewhere, over what the first one left.
        edited = readBack;

        for (const Region & r : SecondPass())
        {
            ApplyRegion (edited, r);
        }

        AssertSucceeded (VolumeImage::Save (afterFirst, c.path, edited, afterSecond, refusal),
            L"the second write must be renderable too");
        Assert::AreEqual (std::string(), refusal);

        AssertSucceeded (VolumeImage::Load (afterSecond, c.path, readBack, report));

        //  BOTH passes, which is the whole point: a container that loses the
        //  older data while storing the newer looks perfectly healthy to a
        //  test that only checks what it just wrote.
        for (const Region & r : FirstPass())
        {
            AssertRegion (readBack, r, c);
        }

        for (const Region & r : SecondPass())
        {
            AssertRegion (readBack, r, c);
        }

        //  And the whole volume, byte for byte, against what it should be --
        //  so a stray write outside every region is caught as well.
        Assert::AreEqual (edited.size(), readBack.size());
        Assert::AreEqual (0, memcmp (edited.data(), readBack.data(), edited.size()),
            L"the volume must match the edit exactly, including the untouched parts");
    }



    TEST_METHOD (EveryContainer_SurvivesTwoWritesWithEveryBitPattern)
    {
        std::vector<ContainerCase>  containers = AllContainers();

        //  A sweep over an empty list passes while checking nothing.
        Assert::IsTrue (containers.size() >= 5,
            L"every writable container must be exercised");

        for (const ContainerCase & c : containers)
        {
            ExerciseContainer (c);
        }
    }



    TEST_METHOD (DataCanCarryALongerSyncRunThanTheGapBetweenSectors)
    {
        //  THE MEASUREMENT BEHIND THE GAP RULE, kept because the rule looks
        //  arbitrary without it. $FF is a legal 6-and-2 nibble, so sector DATA
        //  encodes to runs of it -- and alternating $AA/$55 source bytes make a
        //  run inside the first data field far longer than the twenty-byte gap
        //  between sectors. A padding rule that picks the longest run therefore
        //  pads into the middle of a sector and destroys it, which is what this
        //  file caught on its first run.
        //
        //  If this ever stops being true the gap rule could be simplified; the
        //  number is asserted so that would be noticed rather than assumed.
        DiskImage     img;
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0);
        vector<Byte>  derived;
        size_t        bitPos  = 0;
        size_t        bits    = 0;
        size_t        run     = 0;
        size_t        bestRun = 0;
        size_t        i       = 0;
        Byte          nib     = 0;

        for (i = 0; i < sectors.size(); i++)
        {
            sectors[i] = Alternating (i);
        }

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, img));

        bits = img.GetTrackBitCount (9);

        while (bitPos < bits)
        {
            nib = NibblizationLayer::ReadNibbleAt (img, 9, bitPos);

            if (nib == 0)
            {
                break;
            }

            derived.push_back (nib);
        }

        Assert::IsTrue (derived.size() > 0, L"the track must derive something");

        for (i = 0; i < derived.size(); i++)
        {
            run = (derived[i] == 0xFF) ? run + 1 : 0;

            if (run > bestRun)
            {
                bestRun = run;
            }
        }

        Assert::IsTrue (bestRun > 20,
            L"data encodes to a sync run longer than the inter-sector gap, "
            L"which is why the gap is found by what follows it and not by length");
    }



    TEST_METHOD (NoEncodedDataCanImpersonateAFieldMark)
    {
        //  $D5 OPENS EVERY FIELD AND MUST APPEAR NOWHERE ELSE. The 6-and-2
        //  alphabet excludes it deliberately, and the address header's 4-and-4
        //  encoding cannot produce it either -- every 4-and-4 byte is
        //  `x | $AA`, and $D5 does not contain $AA's bits. So a track holds
        //  exactly thirty-two: sixteen address prologues and sixteen data ones.
        //
        //  This is the property every resync in the decoder rests on. If a
        //  future edit let $D5 into the data alphabet, a sector's own contents
        //  could impersonate the start of the next field and the decoder would
        //  sync to garbage -- so the count is asserted rather than assumed, on
        //  the patterns most likely to produce awkward encodings.
        PatternFn  patterns[] = { &Alternating, &AllValues, &SectorEdges, &WalkingOnes };

        for (PatternFn pattern : patterns)
        {
            DiskImage     img;
            vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0);
            vector<Byte>  derived;
            size_t        bitPos = 0;
            size_t        bits   = 0;
            size_t        i      = 0;
            int           marks  = 0;
            Byte          nib    = 0;

            for (i = 0; i < sectors.size(); i++)
            {
                sectors[i] = pattern (i);
            }

            AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, img));

            bits = img.GetTrackBitCount (5);

            while (bitPos < bits)
            {
                nib = NibblizationLayer::ReadNibbleAt (img, 5, bitPos);

                if (nib == 0)
                {
                    break;
                }

                derived.push_back (nib);
            }

            Assert::IsTrue (derived.size() > 0, L"the track must derive something");

            for (i = 0; i < derived.size(); i++)
            {
                if (derived[i] == NibblizationLayer::kProlog0)
                {
                    marks++;
                }
            }

            Assert::AreEqual (32, marks,
                L"a track must hold exactly sixteen address and sixteen data marks; "
                L"any other count means encoded data is impersonating a field start");
        }
    }



    TEST_METHOD (EveryWritableContainerIsInThisSweep)
    {
        //  The list above is deliberately hand-written, so this is what keeps
        //  it honest: a container the builder learns to write and nobody adds
        //  here would otherwise be silently unexercised.
        const DiskFormat  *         writable = nullptr;
        size_t                      count    = 0;
        size_t                      i        = 0;
        std::vector<ContainerCase>  cases    = AllContainers();

        writable = BlankDiskBuilder::GetWritableContainers (count);

        Assert::IsTrue (count > 0, L"the builder must write something");

        for (i = 0; i < count; i++)
        {
            bool  covered = false;

            for (const ContainerCase & c : cases)
            {
                covered = covered || (c.format == writable[i]);
            }

            Assert::IsTrue (covered,
                L"a container the builder writes is not exercised by the integrity sweep");
        }
    }
};
