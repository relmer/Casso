#include "Pch.h"
#include "EhmTestHelper.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Machines/Apple2/Common/ProDosStartup.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SosTests
//
//  Apple's SOS 1.1 disks read and changed as the ProDOS volumes they are:
//  System Utilities and System Demo list every file CiderPress II lists,
//  each file reads, a file written to System Demo, whose directory blocks
//  have 12 entries, is listed with the others and the volume checks
//  clean and starts an Apple ///. A disk made to start both computers
//  starts an Apple II with ProDOS in Casso, and a file copied from SOS to
//  ProDOS has the type and aux type it had. The disks come from
//  scripts/FetchStockDisks.ps1. An Apple /// boot of a changed disk is in
//  MAME, by scripts/MameSnapshot.ps1, since Casso has no Apple ///.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SosTests)
{
public:

    static constexpr Byte  kTypeSos = 0x0C;



    static VolumeListing List (const std::vector<Byte> & sectors)
    {
        VolumeListing  listing;



        AssertSucceeded (ProDosVolume (sectors).Enumerate (listing));

        return listing;
    }


    //  Each file reads, and has the length in the listing.
    static void CheckEveryFileReads (const std::vector<Byte> & sectors, const VolumeListing & listing)
    {
        for (const FileEntry & entry : listing.entries)
        {
            FilePayload  payload;

            AssertSucceeded (ProDosVolume (sectors).Read (FilePath::Parse (entry.name), payload));
            Assert::AreEqual ((size_t) entry.eofBytes, payload.bytes.size(), TextEncoding::Utf8ToWide (entry.name).c_str());
        }
    }


    static std::vector<std::string> GetNames (const VolumeListing & listing)
    {
        std::vector<std::string>  names;



        for (const FileEntry & entry : listing.entries)
        {
            names.push_back (entry.name);
        }

        return names;
    }


    TEST_METHOD (SystemUtilities_ListsEveryFileAndStartsAnAppleIII)
    {
        std::vector<Byte>  sectors = GuestSession::RequireSosUtilitiesDisk();
        VolumeListing      listing = List (sectors);



        Assert::AreEqual (std::string ("UTILITIES"), listing.volumeName);
        Assert::IsTrue (GetNames (listing) == std::vector<std::string> { "SOS.KERNEL", "SOS.DRIVER", "SOS.INTERP", "SYSTEM.PASCAL", "SYSTEM.MISCINFO", "SYSTEM.STARTUP" });
        Assert::IsTrue (listing.startup == StartupSystems { false, true });
        Assert::AreEqual (kTypeSos, listing.entries[0].type);

        CheckEveryFileReads (sectors, listing);
    }


    TEST_METHOD (SystemDemo_ListsEveryFileInDirectoryBlocksOfTwelve)
    {
        std::vector<Byte>  sectors = GuestSession::RequireSosDemoDisk();
        VolumeListing      listing = List (sectors);



        Assert::AreEqual (std::string ("SYSTEM.DEMO"), listing.volumeName);
        Assert::AreEqual ((size_t) 13, listing.entries.size(), L"the count CiderPress II lists");
        Assert::AreEqual (std::string ("HORSES.INV"), listing.entries.back().name);
        Assert::IsTrue (listing.startup.appleIII);

        CheckEveryFileReads (sectors, listing);
    }


    TEST_METHOD (SystemDemo_AFileWrittenToItIsListedAndTheVolumeChecksClean)
    {
        std::vector<Byte>      sectors = GuestSession::RequireSosDemoDisk();
        std::vector<Byte>      changed;
        FilePayload            payload;
        FilePayload            readBack;
        VolumeListing          listing;
        VolumeIntegrityReport  before;
        VolumeIntegrityReport  after;



        payload.bytes.assign (3000, 0xA5);
        payload.type       = 0x04;
        payload.auxType    = 0;
        payload.hasAuxType = true;

        AssertSucceeded (ProDosVolume (sectors).BuildIntegrityReport (before));
        AssertSucceeded (ProDosVolume (sectors).Write (FilePath::Parse ("NOTES"), payload, changed));
        AssertSucceeded (ProDosVolume (changed).BuildIntegrityReport (after));

        listing = List (changed);

        Assert::AreEqual ((size_t) 14, listing.entries.size());
        Assert::IsTrue (after.IsSafeToCommitAfter (before));
        Assert::IsTrue (listing.startup.appleIII, L"the disk starts an Apple /// after the write");

        AssertSucceeded (ProDosVolume (changed).Read (FilePath::Parse ("NOTES"), readBack));
        Assert::IsTrue (readBack.bytes == payload.bytes);
    }


    TEST_METHOD (ProDosAndSos_StartsAnAppleIIWithProDosInCasso)
    {
        BlankDiskSpec      spec;
        BootPayload        payload;
        std::vector<Byte>  image;
        std::vector<Byte>  sectors;
        SectorDecodeReport report;
        TestMachine        machine ("Apple2e");



        spec.format     = DiskFormat::Po;
        spec.contents   = BlankDiskContents::ProDos;
        spec.bootable   = true;
        spec.bootSystem = ProDosBootSystem::Both;
        spec.volumeName = "BOTH";

        payload.proDosUsersDisk = GuestSession::RequireProDosUsersDisk();
        payload.sosSystemDisk   = GuestSession::RequireSosUtilitiesDisk();

        AssertSucceeded (BlankDiskBuilder::Build (spec, payload, image));
        AssertSucceeded (VolumeImage::Load (image, "both.po", sectors, report));

        Assert::IsTrue (List (sectors).startup == StartupSystems { true, true });

        GuestSession::BootToPrompt (machine, sectors);

        Assert::IsTrue (GuestSession::AnyRowContains (GuestSession::TypeAndCollect (machine, "CAT"), "SOS.KERNEL"), L"ProDOS lists the SOS files");
    }


    TEST_METHOD (CopyFromSosToProDos_KeepsTheTypeAndAuxType)
    {
        std::vector<Byte>  sos     = GuestSession::RequireSosUtilitiesDisk();
        std::vector<Byte>  prodos  = GuestSession::RequireProDosUsersDisk();
        std::vector<Byte>  changed;
        FilePayload        payload;
        VolumeListing      listing;
        const FileEntry  * copied  = nullptr;



        AssertSucceeded (ProDosVolume (sos).Read (FilePath::Parse ("SOS.INTERP"), payload));
        AssertSucceeded (ProDosVolume (prodos).Write (FilePath::Parse ("SOS.INTERP"), payload, changed));

        listing = List (changed);

        for (const FileEntry & entry : listing.entries)
        {
            copied = entry.name == "SOS.INTERP" ? &entry : copied;
        }

        Assert::IsNotNull (copied);
        Assert::AreEqual (kTypeSos, copied->type);
        Assert::AreEqual ((Word) 0x00D4, copied->auxType);
    }
};
