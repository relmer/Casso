#include "Pch.h"

#include "EhmTestHelper.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/FilePath.h"
#include "FileTypes/FileFormatTable.h"
#include "FileTypes/WizardryScenario.h"
#include "Machines/Apple2/Common/InfocomVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GameDataTests
//
//  The game data viewers on real game disks: the SCENARIO.DATA of Wizardry
//  I, II and III, each identified and listed with the scenario's name,
//  monsters, items and first maze level, and Zork I, release 88, whose
//  story is found on its disk and decoded. The front door and the mailbox are
//  in the room the game opens in, and the leaflet is in the mailbox.
//  Wizardry's lists are checked against facts from its manual. The disks
//  come from scripts/FetchStockDisks.ps1.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GameDataTests)
{
public:

    //  SCENARIO.DATA from a Wizardry scenario disk, decoded.
    static DecodedFile DecodeScenario (const char * diskName)
    {
        std::vector<Byte>         image   = GuestSession::RequireFetchedDisk (diskName);
        std::vector<Byte>         sectors;
        SectorDecodeReport        report;
        std::unique_ptr<IVolume>  volume;
        FilePayload               payload;
        FileEntry                 entry;
        DecodedFile               decoded;
        std::wstring              error;
        FileFormat                format  = FileFormat::None;



        AssertSucceeded (VolumeImage::Load (image, diskName, sectors, report));
        Assert::IsTrue  (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Pascal);

        volume = VolumeImage::CreateVolume (VolumeKind::Pascal, sectors);
        AssertSucceeded (volume->Read (FilePath::Parse ("SCENARIO.DATA"), payload));

        entry.name = "SCENARIO.DATA";
        entry.type = payload.type;
        format     = FileFormatTable::Identify (entry, VolumeKind::Pascal, payload.bytes);
        Assert::IsTrue (format == FileFormat::WizardryScenario);

        AssertSucceeded (FileFormatTable::Decode (format, payload.bytes, FileFacts::From (entry, VolumeKind::Pascal), DecodeOptions(), decoded, error));
        Assert::IsTrue  (decoded.notes.empty());

        return decoded;
    }


    static bool HasLineStarting (const DecodedFile & file, const std::wstring & start)
    {
        return std::any_of (file.lines.begin(), file.lines.end(), [&] (const std::wstring & line) { return line.starts_with (start); });
    }


    //  Runs the machine a slice at a time until a row of the text screen
    //  contains `needle`, or until `cycleCap` cycles have run.
    static bool RunUntilScreenShows (MachineHost & host, const std::string & needle, uint64_t cycleCap)
    {
        constexpr uint64_t  kSlice = 1'000'000ULL;

        std::vector<std::string>  rows;
        uint64_t                  spent    = 0;
        bool                      onScreen = false;



        while (!onScreen && spent < cycleCap)
        {
            spent += host.RunCycles (kSlice);

            rows.clear();
            GuestSession::CollectRows (host, rows);
            onScreen = GuestSession::AnyRowContains (rows, needle);
        }

        return onScreen;
    }


    TEST_METHOD (WizardryI_ListsTheProvingGrounds)
    {
        DecodedFile  file = DecodeScenario ("wizardry1-scenario.dsk");



        Assert::AreEqual (std::wstring (L"PROVING GROUNDS OF THE MAD OVERLORD!"), file.lines[0]);
        Assert::AreEqual (std::wstring (L"Wizardry scenario: 10 maze levels, 101 monsters, 101 items, 20 characters"), file.lines[1]);
        Assert::IsTrue   (HasLineStarting (file, L"   0  BUBBLY SLIME     BUBBLY SLIMES    SLIME            1d3+1       12"));
        Assert::IsTrue   (HasLineStarting (file, L"   1  LONG SWORD       SWORD            Weapon            25"), L"Boltac's price in the manual");

        //  The first level's stairs up to the castle are in its southwest
        //  corner, where a party enters.
        Assert::IsTrue   (HasLineStarting (file, L"MAZE LEVEL 10"));
        Assert::IsTrue   (HasLineStarting (file, L"|St                              |"));
    }


    TEST_METHOD (WizardryII_ListsTheKnightOfDiamonds)
    {
        DecodedFile  file = DecodeScenario ("wizardry2-scenario.dsk");



        Assert::AreEqual (std::wstring (L"THE KNIGHT OF DIAMONDS"), file.lines[0]);
        Assert::AreEqual (std::wstring (L"Wizardry scenario: 6 maze levels, 78 monsters, 130 items, 20 characters"), file.lines[1]);
        Assert::IsTrue   (HasLineStarting (file, L"   0  DINK             DINKS            LITTLE OLD MAN"));
        Assert::IsFalse  (HasLineStarting (file, L"MAZE LEVEL 7"));
    }


    TEST_METHOD (WizardryIII_ListsTheLegacyOfLlylgamyn)
    {
        DecodedFile  file = DecodeScenario ("wizardry3-scenario.dsk");



        Assert::AreEqual (std::wstring (L"THE LEGACY OF LLYLGAMYN"), file.lines[0]);
        Assert::AreEqual (std::wstring (L"Wizardry scenario: 6 maze levels, 78 monsters, 104 items, 20 characters"), file.lines[1]);
        Assert::IsTrue   (HasLineStarting (file, L"   0  MOAT MONSTER     MOAT MONSTERS    GIANT SERPENT"));
        Assert::IsTrue   (HasLineStarting (file, L"   1  ORB OF EARITHIN  CRYSTAL SPHERE   Special"));
    }


    //  The story found and decoded, with the objects of the room the game
    //  opens in.
    TEST_METHOD (ZorkI_DecodesAsTheGameRuns)
    {
        std::vector<Byte>          image   = GuestSession::RequireFetchedDisk ("zork1.dsk");
        std::vector<Byte>          sectors;
        SectorDecodeReport         report;
        VolumeListing              listing;
        FilePayload                payload;
        FileEntry                  entry;
        DecodedFile                decoded;
        std::wstring               error;
        std::wstring               text;



        AssertSucceeded (VolumeImage::Load (image, "zork1.dsk", sectors, report));
        Assert::IsTrue  (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Infocom);

        InfocomVolume  volume (sectors);

        AssertSucceeded  (volume.Enumerate (listing));
        Assert::AreEqual (std::string ("STORY.Z3"), listing.entries[0].name);
        AssertSucceeded  (volume.Read (FilePath::Parse ("STORY.Z3"), payload));

        entry.name = "STORY.Z3";
        AssertSucceeded (FileFormatTable::Decode (FileFormat::InfocomStory, payload.bytes, FileFacts::From (entry, VolumeKind::Infocom), DecodeOptions(), decoded, error));

        for (const std::wstring & line : decoded.lines)
        {
            text += line + L"\n";
        }

        Assert::IsTrue (text.find (L"Release 88, serial number 840726") != std::wstring::npos);
        Assert::IsTrue (text.find (L"which matches") != std::wstring::npos);
        Assert::IsTrue (text.find (L"[180] West of House\n      [181] door\n      [160] small mailbox\n        [161] leaflet") != std::wstring::npos);
    }


    //  On a //e the interpreter prints an 80-column prompt, and N selects the
    //  40-column screen, where the room the game opens in is read back.
    static void AssertZorkReachesWestOfHouse (MachineHost & machine)
    {
        constexpr uint64_t  kBootCap = 200'000'000ULL;

        std::vector<std::string>  rows;
        bool                      atPrompt = false;
        bool                      opened   = false;



        atPrompt = RunUntilScreenShows (machine, "80-COLUMN DISPLAY?", kBootCap);
        Assert::IsTrue (atPrompt, L"the interpreter loads and prints the 80-column prompt");

        AssertSucceeded (KeystrokeInjector::InjectKey (machine, (Byte) 'N'));

        opened = RunUntilScreenShows (machine, "SMALL MAILBOX", kBootCap);
        Assert::IsTrue (opened, L"the story loads and prints the first room");

        GuestSession::CollectRows (machine, rows);
        Assert::IsTrue (GuestSession::AnyRowContains (rows, "REVISION 88 / SERIAL NUMBER 840726"));
        Assert::IsTrue (GuestSession::AnyRowIs       (rows, "WEST OF HOUSE"));
        Assert::IsTrue (GuestSession::AnyRowIs       (rows, "THERE IS A SMALL MAILBOX HERE."));
    }


    //  The disk booted as a user boots it: mounted, then the power cycled.
    TEST_METHOD (ZorkI_BootsToWestOfHouse)
    {
        TestMachine        machine ("Apple2eEnhanced");
        std::vector<Byte>  image = GuestSession::RequireFetchedDisk ("zork1.dsk");



        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "zork1.dsk", DiskFormat::Dsk, image));
        machine.PowerCycle();

        AssertZorkReachesWestOfHouse (machine);
    }


    //  The interpreter relies on the text window the monitor's RESET sets up,
    //  so this boot goes through RESET as a real machine's does.
    TEST_METHOD (ZorkI_BootsThroughGuestSessionMount)
    {
        TestMachine        machine ("Apple2eEnhanced");
        std::vector<Byte>  image = GuestSession::RequireFetchedDisk ("zork1.dsk");



        GuestSession::Mount (machine, image);

        AssertZorkReachesWestOfHouse (machine);
    }
};
