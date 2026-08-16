#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FixtureProvider.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/Dos33Volume.h"
#include "Devices/Disk/ProDosVolume.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/VolumeImage.h"
#include "Devices/Disk/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CrossFormatExtractionTests
//
//  SC-004: the same content, held as .dsk, .do, .po and .woz, must extract
//  byte-identically from every one.
//
//  The claim is worth a suite of its own because the two axes that decide it are
//  INDEPENDENT and both silent when wrong. Sector order (.po against the rest)
//  and container shape (.woz against the rest) are unrelated to the filesystem
//  the volume carries, so a ProDOS volume in DOS sector order and a DOS 3.3
//  volume in ProDOS order are both ordinary. A mistake on either axis does not
//  crash or shorten anything -- it returns different bytes that still look like
//  a file. Only comparing the four against each other says so.
//
//  Every variant here is DERIVED from one real disk rather than authored, so the
//  four are the same content by construction and any difference is this code's.
//
//  DAMAGE CASES. The bottom of the file raises the three ways a logical sector
//  ends up zeroed from the decoder, where they are already pinned, to the
//  command the user actually runs. That is a separate claim: the decoder can
//  report data loss perfectly and the listing can still present it as a clean
//  disk, which is the shape GH #115 had. A .woz is the only format that can
//  carry the damage at all -- a sector image has no bit stream to be wrong.
//
//  WHAT REAL DISKS CANNOT REACH is covered separately, in
//  ProDosSyntheticShapeTests.cpp: ProDOS tree storage and random-access text.
//  Both need a constructed volume, which is a different kind of evidence from
//  everything here and is kept apart so the two are not confused.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (CrossFormatExtractionTests)
{
public:

    static constexpr const char *  kDos33Disk  = "Disks/Merlin-proDos2.23.dsk";
    static constexpr const char *  kProDosDisk = "Disks/Merlin-proProdos2.33-a.dsk";

    //  One content, four containers. The path is what tells VolumeImage which
    //  container it holds, so the name carries the meaning and not the bytes.
    struct Variant
    {
        std::string   path;
        vector<Byte>  bytes;
    };

    vector<Byte> LoadFixture (const char * relativePath)
    {
        FixtureProvider  fixtures;
        vector<Byte>     bytes;

        AssertSucceeded (fixtures.OpenFixture (relativePath, bytes));
        Assert::IsTrue (bytes.size() > 0, L"a fixture must not be empty");

        return bytes;
    }

    //  A DOS-logical sector buffer rendered into each of the four containers.
    //  .dsk and .do differ only in name -- both are DOS logical order, and that
    //  is exactly why the pair is worth carrying: it pins that the extension
    //  does not accidentally trigger a reorder.
    std::vector<Variant> MakeAllFourFormats (const vector<Byte> & sectors)
    {
        std::vector<Variant>  variants;
        DiskImage             img;
        vector<Byte>          po;
        vector<Byte>          woz;

        VolumeImage::DosLogicalToProDosFile (sectors, po);

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, img));
        AssertSucceeded (WozLoader::Serialize (img, woz));

        variants.push_back (Variant { "image.dsk", sectors });
        variants.push_back (Variant { "image.do",  sectors });
        variants.push_back (Variant { "image.po",  po      });
        variants.push_back (Variant { "image.woz", woz     });

        return variants;
    }

    vector<Byte> LoadThrough (const Variant & variant)
    {
        SectorDecodeReport  report;
        vector<Byte>        sectors;

        AssertSucceeded (VolumeImage::Load (variant.bytes, variant.path, sectors, report));

        Assert::IsFalse (report.HasDataLoss(),
            L"a variant built from a clean disk must decode without loss");

        return sectors;
    }

    void AssertEveryFormatYieldsTheSameSectors (const char * fixture)
    {
        vector<Byte>          original = LoadFixture (fixture);
        std::vector<Variant>  variants = MakeAllFourFormats (original);

        for (const Variant & variant : variants)
        {
            vector<Byte>  sectors = LoadThrough (variant);

            Assert::AreEqual (original.size(), sectors.size(),
                L"every container must yield a full sector image");

            Assert::IsTrue (original == sectors,
                L"every container must yield the SAME sector image, byte for byte");
        }
    }

    TEST_METHOD (AllFourFormats_OfADos33Disk_YieldTheSameSectorImage)
    {
        AssertEveryFormatYieldsTheSameSectors (kDos33Disk);
    }

    TEST_METHOD (AllFourFormats_OfAProDosDisk_YieldTheSameSectorImage)
    {
        // The ProDOS volume is itself stored in DOS sector order, so this
        // variant set covers a ProDOS filesystem written into a .po file --
        // the one combination where a reorder applied twice, or not at all,
        // still produces a buffer of the right length.
        AssertEveryFormatYieldsTheSameSectors (kProDosDisk);
    }

    TEST_METHOD (TheFourVariants_AreGenuinelyDifferentContainers)
    {
        // Without this, the suite above could pass while comparing four copies
        // of one file. The .po must be a real reorder and the .woz a real bit
        // stream, or "identical extraction across formats" says nothing.
        vector<Byte>          original = LoadFixture (kDos33Disk);
        std::vector<Variant>  variants = MakeAllFourFormats (original);

        Assert::IsTrue (variants[2].bytes.size() == original.size(),
            L"a .po is the same length as the sector image it reorders");

        Assert::IsFalse (variants[2].bytes == original,
            L"but it is NOT the same bytes -- the reorder must actually move sectors");

        Assert::IsTrue (variants[3].bytes.size() > original.size(),
            L"a .woz carries nibbles and headers, so it is larger than the sectors");

        Assert::IsTrue (variants[3].bytes[0] == 'W' && variants[3].bytes[1] == 'O'
                     && variants[3].bytes[2] == 'Z',
            L"and it really is a WOZ container");
    }

    //  Reads every catalog entry from every container and requires the payloads
    //  to agree. Entries occupying no sectors are the disk's decorative heading
    //  rows -- they name no file to read, so they are skipped rather than
    //  expected to fail.
    void AssertEveryFileExtractsIdenticallyFromEveryFormat (const char * fixture, bool isDos33)
    {
        vector<Byte>          original = LoadFixture (fixture);
        std::vector<Variant>  variants = MakeAllFourFormats (original);
        VolumeListing         listing;
        int                   compared = 0;

        {
            Dos33Volume   dos (original);
            ProDosVolume  pro (original);
            IVolume     & volume = isDos33 ? static_cast<IVolume &> (dos)
                                           : static_cast<IVolume &> (pro);

            AssertSucceeded (volume.Enumerate (listing));
        }

        for (const FileEntry & entry : listing.entries)
        {
            FilePayload  reference;
            bool         readable = entry.sizeUnits != 0 && !entry.isDirectory;
            size_t       i        = 0;

            if (!readable)
            {
                continue;
            }

            {
                Dos33Volume   dos (original);
                ProDosVolume  pro (original);
                IVolume     & volume = isDos33 ? static_cast<IVolume &> (dos)
                                               : static_cast<IVolume &> (pro);

                AssertSucceeded (volume.Read (FilePath::Parse (entry.name), reference));
            }

            for (i = 0; i < variants.size(); i++)
            {
                vector<Byte>  sectors = LoadThrough (variants[i]);
                Dos33Volume   dos (sectors);
                ProDosVolume  pro (sectors);
                IVolume     & volume = isDos33 ? static_cast<IVolume &> (dos)
                                               : static_cast<IVolume &> (pro);
                FilePayload   got;

                AssertSucceeded (volume.Read (FilePath::Parse (entry.name), got));

                Assert::AreEqual (reference.bytes.size(), got.bytes.size(),
                    L"a file's length must not depend on the container it came from");

                Assert::IsTrue (reference.bytes == got.bytes,
                    L"a file's BYTES must not depend on the container it came from");

                Assert::AreEqual (reference.type, got.type);
                Assert::AreEqual (reference.hasLoadAddress, got.hasLoadAddress);
                Assert::AreEqual (reference.loadAddress, got.loadAddress);
            }

            compared++;
        }

        Assert::IsTrue (compared > 0, L"the volume must hold files for this to mean anything");
    }

    TEST_METHOD (EveryFileOnADos33Disk_ExtractsIdenticallyFromAllFourFormats)
    {
        // Forty-three real files across four containers. Binaries whose load
        // address comes from an inline header and text files that end at their
        // first terminator both travel this path.
        AssertEveryFileExtractsIdenticallyFromEveryFormat (kDos33Disk, true);
    }

    TEST_METHOD (EveryFileOnAProDosDisk_ExtractsIdenticallyFromAllFourFormats)
    {
        AssertEveryFileExtractsIdenticallyFromEveryFormat (kProDosDisk, false);
    }

    //  ------------------------------------------------------------------
    //  Damage, raised from the decoder to the command.
    //  ------------------------------------------------------------------

    static constexpr const char *  kWozPath = "C:\\disks\\damaged.woz";

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

    //  Writes a checksum that cannot be right for this header. Flipping a bit in
    //  the ENCODED byte would not do: 4-and-4 forces the odd bits to 1, so
    //  clearing one changes the decoded value only when that bit was already
    //  set -- a corruption that silently does nothing for half a track.
    static void PatchFieldChecksum (vector<Byte> & bits, size_t addrAt, Byte volume, Byte track)
    {
        Byte  sector = static_cast<Byte> (((bits[addrAt + 7] << 1) | 1) & bits[addrAt + 8]);
        Byte  wrong  = static_cast<Byte> ((volume ^ track ^ sector) ^ 0xFF);

        bits[addrAt + 9]  = static_cast<Byte> ((wrong >> 1) | 0xAA);
        bits[addrAt + 10] = static_cast<Byte> (wrong | 0xAA);
    }

    //  A real disk as a WOZ, with one track altered. Track 20 is well clear of
    //  the catalog on track 17, so the listing itself still reads and the test
    //  is about how damage is REPORTED rather than about a broken catalog.
    vector<Byte> MakeWozWithAlteredTrack (int track, bool wipeIt)
    {
        vector<Byte>  sectors = LoadFixture (kDos33Disk);
        DiskImage     img;
        vector<Byte>  woz;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, img));

        if (wipeIt)
        {
            vector<Byte> &  bits = img.GetTrackBitsForWrite (track);

            std::fill (bits.begin(), bits.end(), static_cast<Byte> (0));
        }
        else
        {
            vector<Byte> &  bits   = img.GetTrackBitsForWrite (track);
            size_t          addrAt = FindAddressField (bits, 5);

            Assert::AreNotEqual (SIZE_MAX, addrAt, L"the track must carry address fields to corrupt");

            PatchFieldChecksum (bits, addrAt, NibblizationLayer::kDefaultVolume,
                                static_cast<Byte> (track));
        }

        AssertSucceeded (WozLoader::Serialize (img, woz));

        return woz;
    }

    DiskCommandResult RunListOn (const vector<Byte> & imageBytes)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options;

        io.files[kWozPath]  = imageBytes;
        io.stamps[kWozPath] = FileStamp { imageBytes.size(), 100 };

        options.subcommand     = CommandLineOptions::Subcommand::Disk;
        options.disk.verb      = CommandLineOptions::DiskOptions::Verb::List;
        options.disk.imagePath = kWozPath;

        return runner.Run (options);
    }

    TEST_METHOD (List_PartiallyDecodableTrack_SaysTheListingIsIncomplete)
    {
        // The failure this forbids: the catalog is on an undamaged track, so
        // every entry reads and the listing looks complete. Nothing about the
        // output betrays that a data track came back partly as zeros unless the
        // command says so -- and a script that only reads stdout would file the
        // result as a clean disk.
        //
        // Exit 1 rather than 2: the entries that DID read are what the developer
        // recovering an old disk needs, so withholding them would be the wrong
        // answer. The status carries the distinction for a script; the wording
        // has to carry it for whoever reads the log.
        DiskCommandResult  result = RunListOn (MakeWozWithAlteredTrack (20, false));

        Assert::AreEqual (DiskCommandRunner::kWithComplaints, result.exitStatus,
            L"damage earns complaints, not silence and not a refusal");

        Assert::IsTrue (result.output.find ("MERLIN") != std::string::npos,
            L"and the entries that did read are still delivered");

        Assert::IsTrue (result.diagnostics.find ("could not be decoded") != std::string::npos,
            L"the diagnostic must name unrecovered sectors as unrecovered");

        Assert::IsTrue (result.diagnostics.find ("INCOMPLETE") != std::string::npos,
            L"and must say the LISTING is incomplete, not merely that the disk is damaged");
    }

    TEST_METHOD (List_WhollyUnformattedTrack_IsCleanBecauseBlankIsNotDamage)
    {
        // The counterpart, and the reason the decoder distinguishes the two at
        // all. A blank track really is zeros; reporting it as damage would make
        // every freshly formatted or partly used disk complain, and would make
        // blank disks unwritable once the write path consults the same report.
        DiskCommandResult  result = RunListOn (MakeWozWithAlteredTrack (20, true));

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus,
            L"an unformatted track is blank, and blank is not damage");

        Assert::IsTrue (result.diagnostics.find ("could not be decoded") == std::string::npos,
            L"so nothing may be reported as lost");

        Assert::IsTrue (result.output.find ("DISK VOLUME 254") != std::string::npos,
            L"and the listing is delivered normally");
    }

    TEST_METHOD (Get_FromADamagedWoz_DeliversTheFileAndStillComplains)
    {
        // A file living entirely on undamaged tracks extracts correctly, and the
        // command still reports the damage elsewhere on the disk. Both halves
        // matter: suppressing the file would withhold recoverable data, and
        // suppressing the complaint would let a caller treat a damaged image as
        // sound because this one read happened to succeed.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options;
        DiskCommandResult   result;
        vector<Byte>        expected = LoadFixture ("Merlin/MAKE DUMP");
        vector<Byte>        woz      = MakeWozWithAlteredTrack (20, false);
        size_t              i        = 0;

        io.files[kWozPath]  = woz;
        io.stamps[kWozPath] = FileStamp { woz.size(), 100 };

        options.subcommand     = CommandLineOptions::Subcommand::Disk;
        options.disk.verb      = CommandLineOptions::DiskOptions::Verb::Get;
        options.disk.imagePath = kWozPath;
        options.disk.path      = "MAKE DUMP";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kWithComplaints, result.exitStatus);
        Assert::IsTrue (result.hasPayload, L"the file itself must still be delivered");

        // The fixture is the complete stored file, header included; the reader
        // consumes the four header bytes.
        Assert::AreEqual (expected.size() - 4, result.payload.size());

        for (i = 0; i < result.payload.size(); i++)
        {
            if (result.payload[i] != expected[i + 4])
            {
                Assert::Fail (L"damage on another track may not alter this file's bytes");
            }
        }

        Assert::IsTrue (result.diagnostics.find ("INCOMPLETE") != std::string::npos,
            L"and the damage elsewhere on the disk is still reported");
    }
};
