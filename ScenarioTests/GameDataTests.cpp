#include "Pch.h"

#include "EhmTestHelper.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/FilePath.h"
#include "FileTypes/FileFormatTable.h"
#include "FileTypes/WizardryScenario.h"
#include "Machines/Apple2/Common/InfocomSave.h"
#include "Machines/Apple2/Common/InfocomVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GameDataTests
//
//  The game data viewers on real game disks: the SCENARIO.DATA of Wizardry
//  I, II and III, each identified with the scenario's name and the saved
//  game's roster, the owner's Wizardry I disk with a game in progress and
//  the party's items, and Zork I, release 88, whose
//  story is found on its disk and decoded. The front door and the mailbox are
//  in the room the game opens in, and the leaflet is in the mailbox.
//  The stock disks come from scripts/FetchStockDisks.ps1, and the disk with
//  a game in progress was read from the owner's disk on the //c.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GameDataTests)
{
public:

    //  SCENARIO.DATA from a Wizardry scenario disk, decoded.
    static DecodedFile DecodeScenario (const char * diskName)
    {
        return DecodeScenario (GuestSession::RequireFetchedDisk (diskName), diskName);
    }


    static DecodedFile DecodeScenario (const std::vector<Byte> & image, const char * diskName)
    {
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


    //  Every slot on this disk is free.
    TEST_METHOD (WizardryI_HasTheProvingGroundsWithNoCharacters)
    {
        DecodedFile  file = DecodeScenario ("wizardry1-scenario.dsk");



        Assert::AreEqual (std::wstring (L"PROVING GROUNDS OF THE MAD OVERLORD!"), file.lines[0]);
        Assert::AreEqual (std::wstring (L"Wizardry saved game: 0 characters"), file.lines[1]);
        Assert::IsTrue   (HasLineStarting (file, L"No characters."));
        Assert::IsFalse  (HasLineStarting (file, L"   0  BUBBLY SLIME"), L"the scenario's monsters aren't listed");
        Assert::IsFalse  (HasLineStarting (file, L"MAZE LEVEL"));
    }


    //  This disk has a party saved on it, with the Knight of Diamonds' gear.
    TEST_METHOD (WizardryII_ListsTheSavedParty)
    {
        DecodedFile  file = DecodeScenario ("wizardry2-scenario.dsk");



        Assert::AreEqual (std::wstring (L"THE KNIGHT OF DIAMONDS"), file.lines[0]);
        Assert::AreEqual (std::wstring (L"Wizardry saved game: 4 characters"), file.lines[1]);
        Assert::IsTrue   (HasLineStarting (file, L"THUG             HUMAN   FIGHTER     38  OK        338/338       121657      7811260  NEUTRAL"));
        Assert::IsTrue   (HasLineStarting (file, L"    *KOD'S GAUNTLETS"));
        Assert::IsTrue   (HasLineStarting (file, L"     DAMIEN STONE"));
    }


    TEST_METHOD (WizardryIII_HasTheLegacyOfLlylgamyn)
    {
        DecodedFile  file = DecodeScenario ("wizardry3-scenario.dsk");



        Assert::AreEqual (std::wstring (L"THE LEGACY OF LLYLGAMYN"), file.lines[0]);
        Assert::AreEqual (std::wstring (L"Wizardry saved game: 0 characters"), file.lines[1]);
    }


    //  The owner's Wizardry I disk, read from it on the //c: the party of
    //  five, each with the inventory the game has for them, and none of the
    //  deleted characters. No one is in the maze.
    TEST_METHOD (WizardryI_PlayedDisk_ListsThePartyAndTheirItems)
    {
        std::vector<Byte>  image = GuestSession::FindTestDisk ("wizardry-played.dsk");
        DecodedFile        file;



        if (image.empty())
        {
            Logger::WriteMessage ("SKIPPED: wizardry-played.dsk, read from the owner's disk, is not on this machine\n");
            return;
        }

        file = DecodeScenario (image, "wizardry-played.dsk");

        Assert::AreEqual (std::wstring (L"Wizardry saved game: 5 characters"), file.lines[1]);
        Assert::IsTrue   (HasLineStarting (file, L"BART             HUMAN   SAMURAI      2  OK          29/29           32         1441  GOOD"));
        Assert::IsTrue   (HasLineStarting (file, L"    *MURASAMA BLADE"));
        Assert::IsTrue   (HasLineStarting (file, L"     BLADE CUSINART'"), L"carried, not equipped");
        Assert::IsTrue   (HasLineStarting (file, L"DIRTY DAN        GNOME   SAMURAI      2  OK          18/18        20713         1441  NEUTRAL"));
        Assert::IsFalse  (HasLineStarting (file, L"CHUNKY"), L"a deleted character");
        Assert::IsFalse  (HasLineStarting (file, L"MAZE LEVEL"));
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


    //  Types `text` a key at a time, then RETURN when `enter`, and runs on
    //  long enough for the interpreter's reply to reach the screen.
    static void TypeToZork (MachineHost & machine, const std::string & text, bool enter = true)
    {
        constexpr uint64_t  kPerKey   = 900'000ULL;
        constexpr uint64_t  kToAnswer = 6'000'000ULL;

        for (char c : text)
        {
            AssertSucceeded (KeystrokeInjector::InjectKey (machine, (Byte) c));
            machine.RunCycles (kPerKey);
        }

        if (enter)
        {
            AssertSucceeded (KeystrokeInjector::InjectKey (machine, (Byte) '\r'));
        }

        machine.RunCycles (kToAnswer);
    }


    //  A game saved in Casso to a blank disk in drive 2, at position 0, is
    //  summarized from that disk alone, as on the status line: the kitchen,
    //  ten points for coming in through the window, after four moves.
    TEST_METHOD (ZorkI_ASaveIsSummarizedWithoutTheStoryDisk)
    {
        constexpr uint64_t  kSaveCap = 100'000'000ULL;

        TestMachine                         machine ("Apple2eEnhanced");
        std::vector<Byte>                   image  = GuestSession::RequireFetchedDisk ("zork1.dsk");
        std::vector<Byte>                   blank (143360, 0);
        std::vector<Byte>                   saved;
        std::vector<InfocomSave::Position>  positions;
        SectorDecodeReport                  report;



        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "zork1.dsk", DiskFormat::Dsk, image));
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "save.dsk",  DiskFormat::Dsk, blank));
        machine.PowerCycle();

        AssertZorkReachesWestOfHouse (machine);

        TypeToZork (machine, "N");
        TypeToZork (machine, "E");
        TypeToZork (machine, "OPEN WINDOW");
        TypeToZork (machine, "W");
        Assert::IsTrue (RunUntilScreenShows (machine, "KITCHEN", kSaveCap));

        //  Position 0, drive 2, slot 6, yes, and RETURN with the disk in.
        TypeToZork (machine, "SAVE");
        TypeToZork (machine, "", true);
        TypeToZork (machine, "2", false);
        TypeToZork (machine, "", true);
        TypeToZork (machine, "", true);
        TypeToZork (machine, "", true);
        Assert::IsTrue (RunUntilScreenShows (machine, "OK.", kSaveCap), L"the interpreter saves the game");

        saved = GuestSession::DecodeThroughTheDrive (*machine.GetDiskStore().GetImage (6, 1), report);
        InfocomSave::FindPositions (saved, positions);

        Assert::AreEqual ((size_t) 1, positions.size());
        Assert::AreEqual (std::string ("release 88, serial number 840726, Kitchen, score 10, 4 moves"), InfocomSave::Describe (positions[0]));
    }
};
