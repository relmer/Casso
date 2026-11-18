#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "CassoExplorer/Browser/BrowserCells.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/ProDosStartup.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SosVolumeTests
//
//  SOS volumes, which are ProDOS volumes as SOS writes them: a directory
//  whose header has fewer than 13 entries a block is read and written within
//  that count, which computers a volume starts, SOS installed from a user's
//  SOS disk, and `disk create` for SOS and for both systems. The SOS disk is
//  synthetic: a loader block with the SOS loader's opening bytes, and the
//  files SOS.KERNEL, SOS.DRIVER and SOS.INTERP.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SosVolumeTests)
{
public:

    static constexpr size_t  kImageBytes   = 143360;
    static constexpr size_t  kEntryLength  = 0x27;
    static constexpr size_t  kFirstEntry   = 0x04;
    static constexpr Byte    kTwelve       = 12;
    static constexpr Word    kLoadAddress  = 0x2000;



    static vector<Byte> MakeVolume (Byte entriesPerBlock)
    {
        vector<Byte>  sectors (kImageBytes, 0);



        AssertSucceeded (ProDosSkeleton::Write (sectors, "SOSDISK"));

        sectors[ProDosSkeleton::GetBlockByteOffset (sectors.size(), ProDosSkeleton::kDirKeyBlock, ProDosSkeleton::kEntriesPerBlockAt)] = entriesPerBlock;

        return sectors;
    }


    static FilePayload MakePayload()
    {
        FilePayload  payload;



        payload.bytes.assign (100, 0x5A);
        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = kLoadAddress;
        payload.hasLoadAddress = true;

        return payload;
    }


    static Byte GetStorage (const vector<Byte> & sectors, int block, size_t slot)
    {
        return (Byte) (sectors[ProDosSkeleton::GetBlockByteOffset (sectors.size(), block, kFirstEntry + slot * kEntryLength)] & 0xF0);
    }


    TEST_METHOD (TwelveEntriesABlock_IsDetectedAsProDos)
    {
        vector<Byte>  twelve = MakeVolume (kTwelve);
        vector<Byte>  zero   = MakeVolume (0);
        vector<Byte>  over   = MakeVolume (14);



        Assert::IsTrue (VolumeImage::DetectFilesystem (twelve) == VolumeKind::ProDos);
        Assert::IsTrue (VolumeImage::DetectFilesystem (zero)   != VolumeKind::ProDos);
        Assert::IsTrue (VolumeImage::DetectFilesystem (over)   != VolumeKind::ProDos);
    }


    TEST_METHOD (TwelveEntriesABlock_FilesAreWrittenAndListedWithinTheCount)
    {
        constexpr int  kFiles = 14;



        vector<Byte>   sectors = MakeVolume (kTwelve);
        vector<Byte>   next;
        VolumeListing  listing;
        int            i       = 0;



        for (i = 0; i < kFiles; i++)
        {
            AssertSucceeded (ProDosVolume (sectors).Write (FilePath::Parse (std::format ("FILE{}", i)), MakePayload(), next));
            sectors = std::move (next);
        }

        AssertSucceeded (ProDosVolume (sectors).Enumerate (listing));

        Assert::AreEqual ((size_t) kFiles, listing.entries.size());
        Assert::AreEqual ((Byte) 0, GetStorage (sectors, ProDosSkeleton::kDirKeyBlock, kTwelve), L"the key block's 13th slot stays empty");
        Assert::AreEqual ((Byte) 0, GetStorage (sectors, ProDosSkeleton::kDirKeyBlock + 1, kTwelve), L"the next block's 13th slot stays empty");
        Assert::AreNotEqual ((Byte) 0, GetStorage (sectors, ProDosSkeleton::kDirKeyBlock + 1, 0));
    }


    TEST_METHOD (TwelveEntriesABlock_AnEntryInTheThirteenthSlotIsNotListed)
    {
        vector<Byte>   sectors = MakeVolume (kTwelve);
        vector<Byte>   next;
        VolumeListing  listing;
        size_t         at      = ProDosSkeleton::GetBlockByteOffset (kImageBytes, ProDosSkeleton::kDirKeyBlock, kFirstEntry + kTwelve * kEntryLength);



        AssertSucceeded (ProDosVolume (sectors).Write (FilePath::Parse ("REAL"), MakePayload(), next));
        sectors = std::move (next);

        //  A stray record past the header's count, as SOS would never read.
        sectors[at]     = 0x14;
        sectors[at + 1] = 'G';
        sectors[at + 2] = 'H';
        sectors[at + 3] = 'O';
        sectors[at + 4] = 'S';

        AssertSucceeded (ProDosVolume (sectors).Enumerate (listing));

        Assert::AreEqual ((size_t) 1, listing.entries.size());
        Assert::AreEqual (std::string ("REAL"), listing.entries[0].name);
    }


    //  A synthetic SOS disk: the SOS loader's jump and text in block 0, and
    //  the three system files.
    static vector<Byte> MakeSosDisk()
    {
        static constexpr Byte  kLoader[] = { 0x4C, 0x6E, 0xA0, 'S', 'O', 'S', ' ', 'B', 'O', 'O', 'T', ' ', ' ', '1', '.', '1' };



        vector<Byte>  sectors = MakeVolume (13);
        vector<Byte>  next;
        FilePayload   payload;
        size_t        i       = 0;



        for (const char * name : ProDosStartup::kpszSosFiles)
        {
            payload.bytes.assign (1000, (Byte) name[4]);
            payload.type       = 0x0C;
            payload.auxType    = 0;
            payload.hasAuxType = true;

            AssertSucceeded (ProDosVolume (sectors).Write (FilePath::Parse (name), payload, next));
            sectors = std::move (next);
        }

        for (i = 0; i < sizeof (kLoader); i++)
        {
            sectors[ProDosSkeleton::GetBlockByteOffset (sectors.size(), 0, i)] = kLoader[i];
        }

        return sectors;
    }


    static void SetAppleIILoader (vector<Byte> & sectors)
    {
        static constexpr Byte  kLoader[] = { 0x01, 0x38, 0xB0, 0x03 };



        size_t  i = 0;



        for (i = 0; i < sizeof (kLoader); i++)
        {
            sectors[ProDosSkeleton::GetBlockByteOffset (sectors.size(), 0, i)] = kLoader[i];
        }
    }


    static StartupSystems GetStartup (const vector<Byte> & sectors)
    {
        VolumeListing  listing;



        AssertSucceeded (ProDosVolume (sectors).Enumerate (listing));

        return listing.startup;
    }


    TEST_METHOD (Startup_ASosDiskStartsAnAppleIII)
    {
        StartupSystems  startup = GetStartup (MakeSosDisk());



        Assert::IsTrue  (startup.appleIII);
        Assert::IsFalse (startup.appleII);
        Assert::IsFalse (GetStartup (MakeVolume (13)).appleIII, L"a blank volume starts nothing");
    }


    TEST_METHOD (InstallSos_OnABlankVolumePutsTheLoaderInBlockZero)
    {
        vector<Byte>  target = MakeVolume (13);
        std::string   error;



        AssertSucceeded (ProDosStartup::InstallSos (target, MakeSosDisk(), error));

        Assert::AreEqual ((Byte) 0x4C, target[ProDosSkeleton::GetBlockByteOffset (target.size(), 0, 0)]);
        Assert::IsTrue (GetStartup (target).appleIII);
    }


    TEST_METHOD (InstallSos_BesideAnAppleIILoaderPutsTheLoaderInBlockOne)
    {
        vector<Byte>  target = MakeVolume (13);
        vector<Byte>  next;
        FilePayload   payload;
        std::string   error;



        SetAppleIILoader (target);

        payload.bytes.assign (500, 0xEA);
        payload.type           = ProDosVolume::kTypeSystem;
        payload.loadAddress    = kLoadAddress;
        payload.hasLoadAddress = true;

        AssertSucceeded (ProDosVolume (target).Write (FilePath::Parse ("PRODOS"), payload, next));
        target = std::move (next);

        AssertSucceeded (ProDosStartup::InstallSos (target, MakeSosDisk(), error));

        Assert::AreEqual ((Byte) 0x01, target[ProDosSkeleton::GetBlockByteOffset (target.size(), 0, 0)], L"the Apple II loader stays in block 0");
        Assert::AreEqual ((Byte) 0x4C, target[ProDosSkeleton::GetBlockByteOffset (target.size(), 1, 0)]);
        Assert::IsTrue (GetStartup (target) == StartupSystems { true, true });
    }


    TEST_METHOD (InstallSos_FailsAndChangesNothingWithoutAnSosDisk)
    {
        vector<Byte>  target = MakeVolume (13);
        vector<Byte>  before = target;
        std::string   error;



        AssertFailed (ProDosStartup::InstallSos (target, MakeVolume (13), error));

        Assert::IsTrue (target == before);
        Assert::IsFalse (error.empty());
    }


    TEST_METHOD (Write_AnUpperCaseNameHasNoCaseWord)
    {
        vector<Byte>   sectors = MakeVolume (13);
        vector<Byte>   next;
        VolumeListing  listing;
        size_t         first   = ProDosSkeleton::GetBlockByteOffset (kImageBytes, ProDosSkeleton::kDirKeyBlock, kFirstEntry + kEntryLength + 0x1C);
        size_t         second  = ProDosSkeleton::GetBlockByteOffset (kImageBytes, ProDosSkeleton::kDirKeyBlock, kFirstEntry + 2 * kEntryLength + 0x1C);



        AssertSucceeded (ProDosVolume (sectors).Write (FilePath::Parse ("UPPER"), MakePayload(), next));
        AssertSucceeded (ProDosVolume (next).Write (FilePath::Parse ("Mixed"), MakePayload(), sectors));

        Assert::AreEqual ((Byte) 0, sectors[first + 1], L"SOS reads this byte as the minimum version");
        Assert::AreNotEqual ((Byte) 0, sectors[second + 1]);

        AssertSucceeded (ProDosVolume (sectors).Enumerate (listing));
        Assert::AreEqual (std::string ("Mixed"), listing.entries[1].name);
    }


    TEST_METHOD (Create_SosAndBothDescribeWhatTheDiskStarts)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options;
        DiskCommandResult   result;
        vector<Byte>        sectors;
        SectorDecodeReport  report;



        io.files["C:\\src\\sos.dsk"] = MakeSosDisk();

        options.disk.command      = CommandLineOptions::DiskOptions::Command::Create;
        options.disk.imagePath    = "C:\\disks\\new.po";
        options.disk.formatName   = "sos";
        options.disk.bootable     = true;
        options.disk.bootableFrom = "C:\\src\\sos.dsk";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());
        Assert::IsTrue (result.output.ends_with ("bootable on an Apple ///\n"), TextEncoding::NarrowToWide (result.output).c_str());

        AssertSucceeded (VolumeImage::Load (io.files[options.disk.imagePath], options.disk.imagePath, sectors, report));
        Assert::IsTrue (GetStartup (sectors) == StartupSystems { false, true });
    }


    TEST_METHOD (ListingNote_DescribesEachCombination)
    {
        Assert::AreEqual (std::string(), ProDosStartup::DescribeListingNote ({ true, false }, true));
        Assert::AreEqual (std::string ("Not bootable: no PRODOS file on the volume"), ProDosStartup::DescribeListingNote ({ false, false }, false));
        Assert::AreEqual (std::string ("Starts an Apple II with ProDOS and an Apple /// with SOS"), ProDosStartup::DescribeListingNote ({ true, true }, true));
        Assert::IsTrue (ProDosStartup::DescribeListingNote ({ false, true }, false).starts_with ("Starts an Apple /// with SOS."));
    }


    TEST_METHOD (StatusTip_SaysWhatAVolumeStarts)
    {
        VolumeListing  listing;
        BrowserStatus  status;



        listing.volumeName = "UTILITIES";
        listing.startup    = { false, true };

        BrowserCells::DescribeVolumeSpace (L"C:\\disks\\utilities.dsk", listing, VolumeKind::ProDos, status);
        Assert::AreEqual (std::wstring (L"utilities.dsk, ProDOS volume /UTILITIES, starts an Apple ///"), status.freeSpaceTip);

        listing.startup = { true, true };

        BrowserCells::DescribeVolumeSpace (L"C:\\disks\\utilities.dsk", listing, VolumeKind::ProDos, status);
        Assert::IsTrue (status.freeSpaceTip.ends_with (L", starts an Apple II and an Apple ///"));
    }


    TEST_METHOD (NewDiskChoices_SosFormatsAreProDosVolumes)
    {
        using Choices = CassoExplorerNewDiskChoices;

        constexpr int  k800K   = 1;
        constexpr int  kHdv    = 5;



        Assert::IsTrue (Choices::IsProDosFamily (Choices::kFormatSos));
        Assert::IsTrue (Choices::IsProDosFamily (Choices::kFormatProDosSos));
        Assert::AreEqual (std::string ("sos"),        Choices::MakeRequest (Choices::kFormatSos, 0, L"", true).formatName);
        Assert::AreEqual (std::string ("prodos+sos"), Choices::MakeRequest (Choices::kFormatProDosSos, 0, L"", true).formatName);
        Assert::IsTrue (Choices::ValidateSize (Choices::kFormatSos, kHdv, k800K).empty(), L"an SOS volume can be a hard disk image");
        Assert::IsTrue (Choices::ValidateVolume (Choices::kFormatSos, L"UTILITIES").empty());
        Assert::AreEqual ((size_t) 15, Choices::GetVolumeMaxLength (Choices::kFormatProDosSos));
    }
};
