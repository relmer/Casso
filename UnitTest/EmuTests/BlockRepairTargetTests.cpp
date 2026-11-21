#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "FakeDriveSpace.h"
#include "Ui/DiskInspector/Blocks/BlockRepairTarget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepairTargetTests
//
//  The repair confirmation's introduction, and where a repaired copy goes
//  for an image file and for a card's partition.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockRepairTargetTests)
{
public:

    static constexpr uint64_t  kImageBytes  = 819200;
    static constexpr uint64_t  kVolumeBytes = 33553920;

    static BlockRepairSource MakeFileSource (FakeDiskFileIo & io)
    {
        BlockRepairSource  source;



        io.files["C:\\walk\\vol.po"] = vector<Byte> (1, 0);

        source.imagePath    = L"C:\\walk\\vol.po";
        source.hostFileName = L"vol.po";

        return source;
    }


    TEST_METHOD (Intro_HasThreeLinesWithoutACaution)
    {
        int           lines = 0;
        std::wstring  intro = BlockRepairTarget::BuildIntro (false, L"vol.po", L"vol.po", false, false, lines);



        Assert::AreEqual (BlockRepairTarget::kRepairIntroLines, lines);
        Assert::IsTrue (intro.find (L"\n\n") == std::wstring::npos);
    }


    TEST_METHOD (Intro_SuspectVolumeAddsTheCaution)
    {
        int           lines = 0;
        std::wstring  intro = BlockRepairTarget::BuildIntro (false, L"vol.po", L"vol.po", true, true, lines);



        Assert::AreEqual (BlockRepairTarget::kRepairIntroLines + BlockRepairTarget::kCautionLines, lines);
        Assert::IsTrue (intro.find (L"another DOS") != std::wstring::npos);
    }


    TEST_METHOD (Intro_CardPartitionIsntChanged)
    {
        int           lines = 0;
        std::wstring  intro = BlockRepairTarget::BuildIntro (true, L"GAMES", L"GAMES", false, false, lines);



        Assert::IsTrue (intro.find (L"on the card isn't changed") != std::wstring::npos);
    }


    TEST_METHOD (Destination_CardPartitionIsChosenByTheUser)
    {
        FakeDiskFileIo          io;
        FakeDriveSpace          drives;
        BlockRepairSource       source;
        BlockRepairDestination  destination;



        source.isCard      = true;
        source.volumeLabel = L"GAMES";
        source.volumeBytes = kVolumeBytes;
        destination        = BlockRepairTarget::ChooseDestination (source, io, drives);

        Assert::IsTrue (destination.isUserChoice);
        Assert::AreEqual (std::wstring (L"GAMES (repaired).po"), destination.host.wstring());
        Assert::IsTrue (destination.rest.empty());
        Assert::IsTrue (destination.why.empty());
        Assert::AreEqual (kVolumeBytes, destination.bytes);
    }


    TEST_METHOD (Destination_UnlabeledDosPartitionIsAPartitionDsk)
    {
        FakeDiskFileIo     io;
        FakeDriveSpace     drives;
        BlockRepairSource  source;



        source.isCard         = true;
        source.isSectorVolume = true;

        Assert::AreEqual (std::wstring (L"Partition (repaired).dsk"), BlockRepairTarget::ChooseDestination (source, io, drives).host.wstring());
    }


    TEST_METHOD (Destination_FileWithRoomGoesBesideTheImage)
    {
        FakeDiskFileIo          io;
        FakeDriveSpace          drives;
        BlockRepairDestination  destination;



        drives.fileBytes = kImageBytes;
        destination      = BlockRepairTarget::ChooseDestination (MakeFileSource (io), io, drives);

        Assert::IsFalse (destination.isUserChoice);
        Assert::IsTrue (destination.why.empty());
        Assert::AreEqual (std::wstring (L"C:\\walk"), destination.host.parent_path().wstring());
        Assert::AreNotEqual (std::wstring (L"vol.po"), destination.host.filename().wstring());
        Assert::AreEqual (kImageBytes, destination.bytes);
    }


    TEST_METHOD (Destination_RemovableDriveAsksTheUser)
    {
        FakeDiskFileIo          io;
        FakeDriveSpace          drives;
        BlockRepairDestination  destination;



        drives.isRemovable = true;
        destination        = BlockRepairTarget::ChooseDestination (MakeFileSource (io), io, drives);

        Assert::IsTrue (destination.isUserChoice);
        Assert::IsTrue (destination.why.find (L"removable drive") != std::wstring::npos);
    }


    TEST_METHOD (Destination_FullDriveAsksTheUser)
    {
        FakeDiskFileIo          io;
        FakeDriveSpace          drives;
        BlockRepairDestination  destination;



        drives.hasRoom = false;
        destination    = BlockRepairTarget::ChooseDestination (MakeFileSource (io), io, drives);

        Assert::IsTrue (destination.isUserChoice);
        Assert::IsTrue (destination.why.find (L"doesn't have room") != std::wstring::npos);
    }
};
