#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/NibbleImageCodec.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/DirectBootBuilder.h"
#include "Devices/Disk/SectorDecodeReport.h"
#include "GuestSession.h"
#include "HeadlessHost.h"
#include "MachineIdle.h"
#include "TextScreenScraper.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleImageCodecTests
//
//  Bytes to bit streams and back, for headerless nibble images.
//
//  THE LOAD AND THE DERIVATION ARE EXACT INVERSES on an untouched track, and
//  that is the invariant most of this file leans on. Byte-concatenation packs
//  eight bits per byte; the derivation shifts until the high bit sets, which
//  for a byte that has one takes exactly eight shifts. So a track that goes in
//  comes back out, byte for byte, and any drift shows up as a mismatch rather
//  than as a plausible-looking difference.
//
//  Every fixture is synthesized in memory. No .nib file exists in the tree and
//  none is downloaded; the round-trip images here are GCR-encoded from sector
//  data by the code already under test elsewhere.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (NibbleImageCodecTests)
{
public:

    //  A nibble image whose every byte is a legal nibble.
    //
    //  THE PERIOD IS 127 AND NOT 128 ON PURPOSE, and this cost a real defect
    //  to learn. The pattern was `0x80 | (i & 0x7F)`, period 128, and a track
    //  is 6,656 bytes -- exactly 52 periods. Rotating such a track by 128 is
    //  the identity, so a round-trip test over it could not see a rotation at
    //  all: deliberately breaking the rotation rule left every assertion green.
    //  127 is prime and does not divide either track size, so any rotation
    //  changes the bytes and the tests can tell.
    //
    //  The scattered sync bytes are what a gap-finding rotation keys on. The
    //  base pattern tops out at 0xFE and would otherwise contain none, which
    //  would leave the rotation untested for the opposite reason.
    static vector<Byte>  MakeImage (size_t trackSize)
    {
        vector<Byte>  raw (trackSize * NibbleImageCodec::kTrackCount, 0);
        size_t        i = 0;

        for (i = 0; i < raw.size(); i++)
        {
            raw[i] = static_cast<Byte> (0x80 | (i % 127));

            if ((i % 1000) == 3)
            {
                raw[i] = 0xFF;
            }
        }

        return raw;
    }



    //  Turns a track's bit stream so it begins `byBits` further round the
    //  circle. A real nibble image has an arbitrary rotational origin -- the
    //  tool that captured it started wherever the head happened to be -- and
    //  the encoder in this tree always starts at a sync gap, so without this
    //  no fixture here ever puts a field across the seam.
    static void  RotateTrackBits (DiskImage & img, int track, size_t byBits)
    {
        size_t        bits = img.GetTrackBitCount (track);
        vector<Byte>  turned ((bits + 7) / 8, 0);
        size_t        i    = 0;
        Byte          bit  = 0;

        for (i = 0; i < bits; i++)
        {
            bit = img.ReadBit (track, (i + byBits) % bits);

            if (bit != 0)
            {
                turned[i >> 3] = static_cast<Byte> (turned[i >> 3] | (1 << (7 - (i & 7))));
            }
        }

        img.ResizeTrack (track, bits);
        memcpy (img.GetTrackBitsForWrite (track).data(), turned.data(), turned.size());
        img.SetTrackBitCount (track, bits);
    }



    static constexpr Word      kBootRomEntry = 0xC600;
    static constexpr Word      kIntCxRomOff  = 0xC006;
    static constexpr int       kSlot6        = 6;
    static constexpr int       kDrive1       = 0;

    //  A ceiling, not a budget: RunUntilIdle stops as soon as the machine
    //  settles. A DOS 3.3 cold boot settles in about 12M and this does far
    //  less, so reaching this number means the boot never happened.
    static constexpr uint64_t  kBootCeiling  = 30'000'000ULL;

    TEST_METHOD (Boot_ARealMachineReadsTheContainerOffTheDrive)
    {
        //  THE CLAIM A USER WOULD MAKE, checked by a 6502 rather than by this
        //  code's own opinion of itself. Everything else here compares the
        //  codec against itself; this boots the container through the Disk II
        //  the way the machine does, so the bit stream has to be right in a
        //  way no round trip can establish.
        //
        //  A direct-boot payload rather than a DOS 3.3 disk: it needs no
        //  master image, so the test runs the same everywhere instead of
        //  skipping on a checkout that has none.
        HeadlessHost              host;
        EmulatorCore              core;
        DiskImage                 built;
        DiskImage               * mounted = nullptr;
        vector<Byte>              payload;
        vector<Byte>              sectors;
        vector<Byte>              nibFile;
        std::string               refusal;
        DirectBootSpec            spec;
        std::vector<std::string>  rows;
        bool                      printed = false;
        int                       i       = 0;

        //  Writes NIBOK across the top-left of the text page, then spins.
        //
        //  FIVE DISTINCT GLYPHS AND NOT ONE. The first version of this stored a
        //  single 'X' and looked for that letter anywhere on a 40x24 screen --
        //  which passed even with the codec deliberately broken, because some
        //  X is on the screen either way. A word that nothing else writes is
        //  what makes the assertion about this payload.
        //
        //      LDA #ch / STA $0400+n, five times, then JMP to itself.
        {
            static const Byte  kGlyphs[] = { 0xCE, 0xC9, 0xC2, 0xCF, 0xCB };   // N I B O K
            Byte               column    = 0;

            for (column = 0; column < (Byte) std::size (kGlyphs); column++)
            {
                payload.push_back (0xA9);  payload.push_back (kGlyphs[column]);
                payload.push_back (0x8D);  payload.push_back (column);  payload.push_back (0x04);
            }

            //  JMP to the JMP: the payload loads at $0900 and this is its 26th byte.
            payload.push_back (0x4C);  payload.push_back (0x19);  payload.push_back (0x09);
        }

        spec.loadAddress  = 0x0900;
        spec.entryAddress = 0x0900;

        AssertSucceeded (DirectBootBuilder::Build (payload, spec, sectors, refusal));
        Assert::AreEqual (std::string(), refusal);

        //  Through the nibble container, not around it.
        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, built));
        AssertSucceeded (NibbleImageCodec::Build (built, NibbleImageCodec::kNibTrackSize, nibFile));
        Assert::AreEqual (NibbleImageCodec::kNibImageSize, nibFile.size());

        AssertSucceeded (host.BuildApple2eWithDisk2 (core));
        core.PowerCycle();

        //  Drive 1 is index 0 and INTCXROM-off is $C006. Getting either wrong
        //  leaves the boot ROM seeking an empty drive forever, which does not
        //  fail -- it just never goes idle and spends the whole ceiling.
        AssertSucceeded (core.diskStore->MountFromBytes (kSlot6, kDrive1, "boot.nib",
                                                         DiskFormat::Nib, nibFile));

        mounted = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (mounted, L"the nibble image must mount");
        core.diskController->SetExternalDisk (kDrive1, mounted);

        core.bus->WriteByte (kIntCxRomOff, 0);
        core.cpu->SetPC (kBootRomEntry);

        MachineIdle::RunUntilIdle (core, kBootCeiling);

        rows = TextScreenScraper::Scrape40 (*core.bus, TextScreenScraper::kTextPage1);

        Assert::IsTrue (rows.size() > 0, L"the text page must have been scraped");

        for (i = 0; i < (int) rows.size() && !printed; i++)
        {
            printed = rows[i].find ("NIBOK") != std::string::npos;
        }

        Assert::IsTrue (printed,
            L"the boot ROM must have read the payload off the nibble image and run it");
    }



    TEST_METHOD (ResolveGeometry_AcceptsBothCirculatingSizes)
    {
        size_t   trackSize = 0;

        AssertSucceeded (NibbleImageCodec::ResolveGeometry (232960, trackSize));
        Assert::AreEqual ((size_t) 6656, trackSize, L"232,960 bytes is 35 tracks of 6,656");

        AssertSucceeded (NibbleImageCodec::ResolveGeometry (223440, trackSize));
        Assert::AreEqual ((size_t) 6384, trackSize, L"223,440 bytes is 35 tracks of 6,384");
    }



    TEST_METHOD (ResolveGeometry_RefusesEveryOtherLength)
    {
        //  Either side of both accepted totals, plus the sector-image size,
        //  which is the wrong-file case a user is most likely to hit.
        static constexpr size_t  kRejected[] = { 232959, 232961, 223439, 223441, 143360, 0 };

        size_t   trackSize = 0;
        size_t   count     = std::size (kRejected);
        size_t   i         = 0;
        HRESULT  hr        = S_OK;

        Assert::IsTrue (count > 0, L"the corpus must not be empty");

        for (i = 0; i < count; i++)
        {
            hr = NibbleImageCodec::ResolveGeometry (kRejected[i], trackSize);

            Assert::IsTrue (FAILED (hr), L"only the two circulating totals are a nibble image");
        }
    }



    TEST_METHOD (Load_PacksEveryTrackEightBitsPerByte)
    {
        DiskImage     img;
        vector<Byte>  raw   = MakeImage (NibbleImageCodec::kNibTrackSize);
        int           track = 0;

        AssertSucceeded (NibbleImageCodec::Load (raw, img));

        //  The count is asserted before anything loops over it, so a codec
        //  that produced nothing cannot pass by having nothing to check.
        Assert::AreEqual (NibbleImageCodec::kTrackCount, img.GetTrackCount(),
            L"a nibble image is 35 tracks");

        for (track = 0; track < NibbleImageCodec::kTrackCount; track++)
        {
            const vector<Byte>  &  bits   = img.GetTrackBits (track);
            size_t                 offset = static_cast<size_t> (track) * NibbleImageCodec::kNibTrackSize;

            Assert::AreEqual (NibbleImageCodec::kNibTrackSize * 8, img.GetTrackBitCount (track),
                L"every track is its block size in bits");

            Assert::AreEqual (0, memcmp (bits.data(), &raw[offset], NibbleImageCodec::kNibTrackSize),
                L"a track's packed bytes are the file's bytes for that track");
        }
    }



    TEST_METHOD (Load_AcceptsBytesWithTheHighBitClear)
    {
        DiskImage     img;
        vector<Byte>  raw (NibbleImageCodec::kNibImageSize, 0x00);

        //  Illegal on real media, present in real images, and refusing them
        //  would reject files that work.
        raw[0] = 0xD5;

        AssertSucceeded (NibbleImageCodec::Load (raw, img));
        Assert::IsTrue (img.GetTrackBitCount (0) > 0, L"a track of mostly zeros still loads");
    }



    TEST_METHOD (Load_RefusesAWrongSizedFile)
    {
        DiskImage     img;
        vector<Byte>  raw (1000, 0xFF);
        HRESULT       hr = NibbleImageCodec::Load (raw, img);

        Assert::IsTrue (FAILED (hr));
    }



    TEST_METHOD (HasAnyNibble_SeparatesBlankFromGarbage)
    {
        vector<Byte>  zeros (NibbleImageCodec::kNibImageSize, 0x00);
        vector<Byte>  ones  (NibbleImageCodec::kNibImageSize, 0xFF);

        Assert::IsFalse (NibbleImageCodec::HasAnyNibble (zeros),
            L"all-zero content assembles no nibble anywhere");
        Assert::IsTrue  (NibbleImageCodec::HasAnyNibble (ones));

        //  One high bit in the whole file is enough, which is the point: this
        //  test is deliberately weak because the format offers nothing else.
        zeros[NibbleImageCodec::kNibImageSize - 1] = 0x80;
        Assert::IsTrue (NibbleImageCodec::HasAnyNibble (zeros));
    }



    TEST_METHOD (Serialize_RoundTripsAnUntouchedImageExactly)
    {
        DiskImage     img;
        vector<Byte>  raw = MakeImage (NibbleImageCodec::kNibTrackSize);
        vector<Byte>  out;

        AssertSucceeded (NibbleImageCodec::Load (raw, img));
        AssertSucceeded (NibbleImageCodec::Serialize (img, raw, out));

        Assert::AreEqual (raw.size(), out.size(), L"the file keeps its length");
        Assert::AreEqual (0, memcmp (raw.data(), out.data(), raw.size()),
            L"an image nothing wrote to comes back byte for byte");
    }



    TEST_METHOD (Serialize_DerivationIsTheExactInverseOfTheLoad)
    {
        DiskImage     img;
        vector<Byte>  raw = MakeImage (NibbleImageCodec::kNibTrackSize);
        vector<Byte>  out;

        //  Passing no source bytes forces every track down the DERIVATION path
        //  rather than the copy path, which is the whole point: the previous
        //  test would pass on a codec whose derivation was broken, because it
        //  copies. Here nothing is copied.
        AssertSucceeded (NibbleImageCodec::Load (raw, img));
        AssertSucceeded (NibbleImageCodec::Serialize (img, vector<Byte>(), out));

        Assert::AreEqual (raw.size(), out.size());
        Assert::AreEqual (0, memcmp (raw.data(), out.data(), raw.size()),
            L"a track of legal nibbles derives back to exactly its own bytes");
    }



    TEST_METHOD (Serialize_RoundTripsTheSmallerTrackSizeToo)
    {
        DiskImage     img;
        vector<Byte>  raw = MakeImage (NibbleImageCodec::kNb2TrackSize);
        vector<Byte>  out;

        AssertSucceeded (NibbleImageCodec::Load (raw, img));
        AssertSucceeded (NibbleImageCodec::Serialize (img, raw, out));

        Assert::AreEqual (NibbleImageCodec::kNb2ImageSize, out.size());
        Assert::AreEqual (0, memcmp (raw.data(), out.data(), raw.size()));
    }



    TEST_METHOD (Serialize_LeavesCleanTracksAloneWhenOneIsDirtied)
    {
        DiskImage     img;
        vector<Byte>  raw         = MakeImage (NibbleImageCodec::kNibTrackSize);
        vector<Byte>  out;
        size_t        dirtyOffset = 5 * NibbleImageCodec::kNibTrackSize;
        int           track       = 0;

        AssertSucceeded (NibbleImageCodec::Load (raw, img));

        //  Flip one bit on track 5 through the public write path, so the
        //  dirty bookkeeping is the real one rather than a test fiction.
        img.WriteBit (5, 0, 0);

        AssertSucceeded (NibbleImageCodec::Serialize (img, raw, out));

        for (track = 0; track < NibbleImageCodec::kTrackCount; track++)
        {
            size_t  offset = static_cast<size_t> (track) * NibbleImageCodec::kNibTrackSize;

            if (track == 5)
            {
                continue;
            }

            Assert::AreEqual (0, memcmp (&raw[offset], &out[offset], NibbleImageCodec::kNibTrackSize),
                L"a track the guest did not write is copied, not re-derived");
        }

        Assert::AreEqual (NibbleImageCodec::kNibImageSize, out.size());
        Assert::IsTrue (dirtyOffset < out.size());
    }



    TEST_METHOD (Serialize_DerivedTrackNeverOverflowsItsBlock)
    {
        DiskImage     img;
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0xE7);
        vector<Byte>  out;

        //  Real GCR tracks, self-sync and all: 50,624 bits carrying 6,224
        //  bytes, which is the shape a guest write leaves behind and the case
        //  where the derived count falls well short of the 6,656-byte block.
        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, img));
        AssertSucceeded (NibbleImageCodec::Serialize (img, vector<Byte>(), out));

        Assert::AreEqual (NibbleImageCodec::kNibImageSize, out.size(),
            L"a file written from nothing takes the standard block size");
    }



    TEST_METHOD (RoundTrip_ARealDiskSurvivesTheContainerAtSectorLevel)
    {
        //  THE END-TO-END CLAIM, and the one a user would recognize: a disk
        //  goes into a nibble image and comes back out with every sector
        //  intact. Sectors -> GCR -> nib bytes -> GCR -> sectors, with the
        //  comparison at the two ends. Every step is the production path.
        DiskImage           written;
        DiskImage           reloaded;
        vector<Byte>        sectors (NibblizationLayer::kImageByteSize, 0);
        vector<Byte>        nibFile;
        vector<Byte>        recovered;
        SectorDecodeReport  report;
        size_t              i = 0;

        //  Varied rather than a constant fill: a sector landing in the wrong
        //  place is invisible when every sector holds the same byte.
        for (i = 0; i < sectors.size(); i++)
        {
            sectors[i] = static_cast<Byte> ((i * 7 + (i / 256)) & 0xFF);
        }

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, written));
        AssertSucceeded (NibbleImageCodec::Serialize (written, vector<Byte>(), nibFile));

        Assert::AreEqual (NibbleImageCodec::kNibImageSize, nibFile.size(),
            L"the container is a whole nibble image");

        AssertSucceeded (NibbleImageCodec::Load (nibFile, reloaded));
        AssertSucceeded (NibblizationLayer::Denibblize (reloaded, DiskFormat::Dsk, recovered, report));

        //  Assert there was something to decode before comparing it, or a
        //  codec that produced an empty disk would compare equal to nothing.
        Assert::AreEqual (NibblizationLayer::kTrackCount, report.GetTrackCount(),
            L"every track must have been examined");

        Assert::AreEqual (sectors.size(), recovered.size());
        Assert::AreEqual (0, memcmp (sectors.data(), recovered.data(), sectors.size()),
            L"every sector survives the trip through the nibble container");
    }



    TEST_METHOD (Serialize_KeepsAllSixteenSectorsMarkersOnATrack)
    {
        //  The padding-placement proof. A rotate-and-pad that landed inside a
        //  field would destroy one, and the sector count is how that shows.
        //
        //  ALL SIXTEEN ARE FINDABLE BY A BYTE SEARCH HERE, unlike in a packed
        //  bit stream where self-sync bytes occupy ten cells and leave half the
        //  prologues off the byte boundary. In a nibble image every nibble IS a
        //  byte, which is the one place the format is easier to inspect.
        DiskImage     img;
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0x5A);
        vector<Byte>  nibFile;
        size_t        i        = 0;
        int           addrs    = 0;
        int           datas    = 0;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, img));
        AssertSucceeded (NibbleImageCodec::Serialize (img, vector<Byte>(), nibFile));

        for (i = 0; i + 2 < NibbleImageCodec::kNibTrackSize; i++)
        {
            if (nibFile[i] == 0xD5 && nibFile[i + 1] == 0xAA && nibFile[i + 2] == 0x96)
            {
                addrs++;
            }

            if (nibFile[i] == 0xD5 && nibFile[i + 1] == 0xAA && nibFile[i + 2] == 0xAD)
            {
                datas++;
            }
        }

        Assert::AreEqual (16, addrs, L"all sixteen address prologues survive the padding");
        Assert::AreEqual (16, datas, L"all sixteen data prologues survive the padding");
    }



    TEST_METHOD (Serialize_PaddingMissesTheFieldsWhenTheSeamIsInside_One)
    {
        //  THE CASE THE ROTATION EXISTS FOR, and the only one that can tell
        //  whether it works. Every track this tree encodes begins at a sync
        //  gap, so the derivation seam already sits somewhere harmless and
        //  padding appended there destroys nothing -- which means the other
        //  tests here pass just as happily with the rotation disabled. Turning
        //  the bit stream first puts a data field across the seam, which is
        //  what a nibble image captured by another tool looks like.
        //
        //  Sector-level comparison rather than a marker count: a splice
        //  through the middle of a data field can leave both prologues intact
        //  and still lose the sector.
        DiskImage           written;
        DiskImage           reloaded;
        vector<Byte>        sectors (NibblizationLayer::kImageByteSize, 0);
        vector<Byte>        nibFile;
        vector<Byte>        recovered;
        SectorDecodeReport  report;
        size_t              i     = 0;
        int                 track = 0;

        for (i = 0; i < sectors.size(); i++)
        {
            sectors[i] = static_cast<Byte> ((i * 11 + (i / 256)) & 0xFF);
        }

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, written));

        //  Far enough in to land inside a field rather than in the gap the
        //  encoder starts with.
        for (track = 0; track < NibbleImageCodec::kTrackCount; track++)
        {
            RotateTrackBits (written, track, 1500);
        }

        AssertSucceeded (NibbleImageCodec::Serialize (written, vector<Byte>(), nibFile));
        AssertSucceeded (NibbleImageCodec::Load (nibFile, reloaded));
        AssertSucceeded (NibblizationLayer::Denibblize (reloaded, DiskFormat::Dsk, recovered, report));

        Assert::AreEqual (NibblizationLayer::kTrackCount, report.GetTrackCount(),
            L"every track must have been examined");
        Assert::AreEqual (0, memcmp (sectors.data(), recovered.data(), sectors.size()),
            L"padding must land in the gap, not in the field the seam runs through");
    }



    TEST_METHOD (Serialize_KeepsAnEditMadeByTheBulkRewriter)
    {
        //  THE SILENT WRITE LOSS, PINNED. `disk put` on a nibble image
        //  reported success and changed nothing: the file went into the sector
        //  buffer, RenibblizeTracks re-encoded the affected tracks through
        //  GetTrackBitsForWrite -- which bypasses WriteBit and so recorded
        //  nothing -- and this serializer, seeing every track clean, copied all
        //  thirty-five back out of the original bytes over the top of the edit.
        //
        //  It was invisible for as long as the only bit-stream writer rebuilt
        //  every track regardless of dirty state. Nothing about the WOZ path
        //  exercises it, which is why no existing test caught it and a hand
        //  round-trip through the command line did.
        DiskImage           img;
        DiskImage           reloaded;
        vector<Byte>        sectors (NibblizationLayer::kImageByteSize, 0x00);
        vector<Byte>        edited;
        vector<Byte>        original;
        vector<Byte>        rewritten;
        vector<Byte>        recovered;
        SectorDecodeReport  report;
        vector<int>         changed;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, img));
        AssertSucceeded (NibbleImageCodec::Build (img, NibbleImageCodec::kNibTrackSize, original));

        //  Reload exactly as a command would, then edit one sector.
        AssertSucceeded (NibbleImageCodec::Load (original, reloaded));

        edited = sectors;
        edited[3 * 16 * NibblizationLayer::kSectorByteSize] = 0x5A;
        changed.push_back (3);

        AssertSucceeded (NibblizationLayer::RenibblizeTracks (edited, DiskFormat::Dsk,
                                                              changed, reloaded));
        AssertSucceeded (NibbleImageCodec::Serialize (reloaded, original, rewritten));

        Assert::IsTrue (memcmp (original.data(), rewritten.data(), original.size()) != 0,
            L"a re-encoded track must reach the file, not be copied over");

        //  And the edit is the one that arrived, not merely some difference.
        AssertSucceeded (NibbleImageCodec::Load (rewritten, img));
        AssertSucceeded (NibblizationLayer::Denibblize (img, DiskFormat::Dsk, recovered, report));

        Assert::AreEqual ((int) 0x5A,
                          (int) recovered[3 * 16 * NibblizationLayer::kSectorByteSize],
            L"the edited byte must read back from the rewritten container");
    }



    TEST_METHOD (Serialize_PadsAShortTrackWithSyncBytes)
    {
        DiskImage     img;
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0x00);
        vector<Byte>  out;
        size_t        i       = 0;
        size_t        syncRun = 0;
        size_t        longest = 0;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, img));
        AssertSucceeded (NibbleImageCodec::Serialize (img, vector<Byte>(), out));

        Assert::AreEqual (NibbleImageCodec::kNibImageSize, out.size());

        //  Every byte written must be a legal nibble, or the head stalls over
        //  the padding on the next mount.
        for (i = 0; i < NibbleImageCodec::kNibTrackSize; i++)
        {
            Assert::IsTrue ((out[i] & 0x80) != 0, L"padding must have the high bit set");

            syncRun = (out[i] == 0xFF) ? syncRun + 1 : 0;

            if (syncRun > longest)
            {
                longest = syncRun;
            }
        }

        Assert::IsTrue (longest > 0, L"a padded track ends in a run of sync bytes");
    }
};
