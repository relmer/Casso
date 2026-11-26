#include "Pch.h"

#include "EhmTestHelper.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "Core/NumberFormat.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/FilePath.h"
#include "FileTypes/FileFormatTable.h"
#include "FileTypes/WizardryScenario.h"
#include "Core/UnicodeSymbols.h"
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


    //  Each line of a decoded file, or each paragraph of a document with its
    //  runs joined.
    static std::vector<std::wstring> GetTexts (const DecodedFile & file)
    {
        std::vector<std::wstring>  texts = file.lines;

        for (const Paragraph & paragraph : FlattenRecordTables (file.paragraphs))
        {
            std::wstring  text;

            for (const Run & run : paragraph.runs)
            {
                text += run.text;
            }

            texts.push_back (text);
        }

        return texts;
    }


    static bool HasLineStarting (const DecodedFile & file, const std::wstring & start)
    {
        std::vector<std::wstring>  texts = GetTexts (file);

        return std::any_of (texts.begin(), texts.end(), [&] (const std::wstring & line) { return line.starts_with (start); });
    }


    static bool HasLine (const DecodedFile & file, const std::wstring & text)
    {
        std::vector<std::wstring>  texts = GetTexts (file);

        return std::find (texts.begin(), texts.end(), text) != texts.end();
    }


    static bool HasPicture (const DecodedFile & file)
    {
        return std::any_of (file.paragraphs.begin(), file.paragraphs.end(), [] (const Paragraph & paragraph) { return !paragraph.picture.empty(); });
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
            spent += host.GetRunner().RunCycles (kSlice);

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



        Assert::AreEqual (std::wstring (L"Wizardry I: Proving Grounds of the Mad Overlord"), GetTexts (file)[0]);
        Assert::AreEqual (std::wstring (L"Saved game: 0 characters"), GetTexts (file)[1]);
        Assert::IsTrue   (HasLineStarting (file, L"No characters."));
        Assert::IsFalse  (HasLineStarting (file, L"BUBBLY SLIME"), L"the scenario's monsters aren't listed");
        Assert::IsFalse  (HasPicture (file));
    }


    //  This disk has a party saved on it, with the Knight of Diamonds' gear.
    TEST_METHOD (WizardryII_ListsTheSavedParty)
    {
        DecodedFile  file = DecodeScenario ("wizardry2-scenario.dsk");



        Assert::AreEqual (std::wstring (L"Wizardry II: The Knight of Diamonds"), GetTexts (file)[0]);
        Assert::AreEqual (std::wstring (L"Saved game: 4 characters"), GetTexts (file)[1]);
        Assert::IsTrue   (HasLineStarting (file, L"THUG   Level 38 Human Fighter, Neutral"));
        Assert::IsTrue   (HasLineStarting (file, L"Status OK   HP 338/338   Gold 121,657   Exp 7,811,260"));
        Assert::IsTrue   (HasLineStarting (file, std::wstring (L"      KOD'S GAUNTLETS\t") + s_kpszCheckMark + L" equipped"));
        Assert::IsTrue   (HasLine (file, L"      DAMIEN STONE\t"), L"in the inventory, not equipped");
    }


    TEST_METHOD (WizardryIII_HasTheLegacyOfLlylgamyn)
    {
        DecodedFile  file = DecodeScenario ("wizardry3-scenario.dsk");



        Assert::AreEqual (std::wstring (L"Wizardry III: Legacy of Llylgamyn"), GetTexts (file)[0]);
        Assert::AreEqual (std::wstring (L"Saved game: 0 characters"), GetTexts (file)[1]);
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

        Assert::AreEqual (std::wstring (L"Saved game: 5 characters"), GetTexts (file)[1]);
        Assert::IsTrue   (HasLineStarting (file, L"BART   Level 2 Human Samurai, Good"));
        Assert::IsTrue   (HasLineStarting (file, std::wstring (L"      MURASAMA BLADE\t") + s_kpszCheckMark + L" equipped"));
        Assert::IsTrue   (HasLine (file, L"      BLADE CUSINART'\t"), L"in the inventory, not equipped");
        Assert::IsTrue   (HasLineStarting (file, L"DIRTY DAN   Level 2 Gnome Samurai, Neutral"));
        Assert::IsFalse  (HasLineStarting (file, L"CHUNKY"), L"a deleted character");
        Assert::IsFalse  (HasPicture (file));
    }


    //  Types `text` into Wizardry a key at a time, with time for the game to
    //  read each key.
    static void TypeToWizardry (MachineHost & machine, const std::string & text)
    {
        constexpr uint64_t  kPerKey = 600'000ULL;

        for (char c : text)
        {
            AssertSucceeded (KeystrokeInjector::InjectKey (machine, (Byte) c));
            machine.GetRunner().RunCycles (kPerKey);
        }
    }


    //  The number after `label` on a row of the screen, as Wizardry's
    //  character sheet has them ("GOLD          180", "HITS   9/  9"), or -1.
    //  `second` takes the number after the slash.
    static int64_t FindScreenNumber (const std::vector<std::string> & rows, const std::string & label, bool second = false)
    {
        std::regex   pattern (label + (second ? " +[0-9]+/ *([0-9]+)" : " +([0-9]+)"));
        std::smatch  match;
        int64_t      value = -1;



        for (const std::string & row : rows)
        {
            if (value < 0 && std::regex_search (row, match, pattern))
            {
                value = std::stoll (match[1].str());
            }
        }

        return value;
    }


    //  Wizardry I run in Casso: booted from its write-protected boot disk,
    //  which its startup code requires, with a character created in the
    //  Training Grounds and saved to the scenario disk. The decoded
    //  SCENARIO.DATA has the character as the game's sheet draws it: its
    //  name, level, race, class and alignment, status, hit points, gold and
    //  experience. The bonus points are rolled, so each one goes to the first
    //  score that takes it, which makes the character a fighter, and the sheet
    //  is read back rather than assumed.
    TEST_METHOD (WizardryI_ACharacterMadeInTheGame_DecodesAsItsSheet)
    {
        constexpr uint64_t  kCap = 200'000'000ULL;

        TestMachine                machine  ("Apple2Plus");
        std::vector<Byte>          boot     = GuestSession::FindTestDisk ("wizardry1-boot.dsk");
        std::vector<Byte>          scenario = GuestSession::FindTestDisk ("wizardry1-scenario.dsk");
        std::vector<std::string>   sheet;
        std::vector<Byte>          saved;
        SectorDecodeReport         report;
        DecodedFile                file;
        int64_t                    level    = 0;



        if (boot.empty() || scenario.empty())
        {
            Logger::WriteMessage ("SKIPPED: wizardry1-boot.dsk and wizardry1-scenario.dsk are not in the test disks folder\n");
            return;
        }

        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "wizardry1-boot.dsk", DiskFormat::Dsk, boot));
        machine.GetDiskStore().GetImage (6, 0)->SetUserWriteProtected (true);
        machine.PowerCycle();

        //  The title, then the scenario disk in drive 1 for the game.
        machine.GetRunner().RunCycles (60'000'000ULL);
        TypeToWizardry (machine, " ");
        Assert::IsTrue (RunUntilScreenShows (machine, "S)TART GAME", kCap), L"the menu after the title");

        TypeToWizardry (machine, "S");
        Assert::IsTrue (RunUntilScreenShows (machine, "SCENARIO MASTER IN DRV 1", kCap));

        machine.GetDiskStore().Eject (6, 0);
        machine.GetRunner().RunCycles (2'000'000ULL);
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "wizardry1-scenario.dsk", DiskFormat::Dsk, scenario));
        TypeToWizardry (machine, "\r");
        Assert::IsTrue (RunUntilScreenShows (machine, "MASTER/DUPLICATE IN DRV 1", kCap));

        TypeToWizardry (machine, "\r");
        Assert::IsTrue (RunUntilScreenShows (machine, "YOU MAY GO TO:", kCap), L"the castle");

        //  The Training Grounds, by the edge of town.
        TypeToWizardry (machine, "E");
        Assert::IsTrue (RunUntilScreenShows (machine, "T)RAINING GROUNDS", kCap));
        TypeToWizardry (machine, "T");
        Assert::IsTrue (RunUntilScreenShows (machine, "NAME >", kCap));

        TypeToWizardry (machine, "CASSO\r");
        Assert::IsTrue (RunUntilScreenShows (machine, "CREATE IT", kCap));
        TypeToWizardry (machine, "Y");
        Assert::IsTrue (RunUntilScreenShows (machine, "ENTER A PASSWORD", kCap));
        TypeToWizardry (machine, "\r");
        Assert::IsTrue (RunUntilScreenShows (machine, "AGAIN", kCap));
        TypeToWizardry (machine, "\r");
        Assert::IsTrue (RunUntilScreenShows (machine, "CHOOSE A RACE", kCap));
        TypeToWizardry (machine, "A");
        Assert::IsTrue (RunUntilScreenShows (machine, "CHOOSE AN ALIGNMENT", kCap));
        TypeToWizardry (machine, "A");
        Assert::IsTrue (RunUntilScreenShows (machine, "ENTER [+,-]", kCap));

        for (int score = 0; score < 6; score++)
        {
            TypeToWizardry (machine, std::string (10, '+') + "\r");
        }

        TypeToWizardry (machine, "\x1B");
        Assert::IsTrue (RunUntilScreenShows (machine, "CHOOSE A CLASS", kCap));
        TypeToWizardry (machine, "A");
        Assert::IsTrue (RunUntilScreenShows (machine, "KEEP THIS CHARACTER", kCap));
        TypeToWizardry (machine, "Y");
        Assert::IsTrue (RunUntilScreenShows (machine, "NAME >", kCap), L"the Training Grounds again, with the character made");

        //  The character's sheet, as the game draws it.
        TypeToWizardry (machine, "CASSO\r");
        Assert::IsTrue (RunUntilScreenShows (machine, "PASSWORD >", kCap));
        TypeToWizardry (machine, "\r");
        Assert::IsTrue (RunUntilScreenShows (machine, "I)NSPECT", kCap));
        TypeToWizardry (machine, "I");
        Assert::IsTrue (RunUntilScreenShows (machine, "STATUS OK", kCap));

        GuestSession::CollectRows (machine, sheet);
        Assert::IsTrue (GuestSession::AnyRowContains (sheet, "CASSO HUMAN G-FIGHTER"));

        level = FindScreenNumber (sheet, "LEVEL");
        Assert::IsTrue (level == 1 && FindScreenNumber (sheet, "GOLD") >= 0 && FindScreenNumber (sheet, "HITS") > 0, L"the sheet has its numbers");

        //  Out of the game by the edge of town, which is when the game writes the
        //  roster to the scenario disk.
        TypeToWizardry (machine, "L");
        Assert::IsTrue (RunUntilScreenShows (machine, "PRESS [RET] TO LEAVE", kCap));
        TypeToWizardry (machine, "\r");
        Assert::IsTrue (RunUntilScreenShows (machine, "NAME >", kCap));
        TypeToWizardry (machine, "\r");
        Assert::IsTrue (RunUntilScreenShows (machine, "YOU MAY GO TO", kCap));
        TypeToWizardry (machine, "E");
        Assert::IsTrue (RunUntilScreenShows (machine, "L)EAVE THE", kCap));
        TypeToWizardry (machine, "L");
        machine.GetRunner().RunCycles (60'000'000ULL);

        //  The scenario disk as the game left it, decoded.
        saved = GuestSession::DecodeThroughTheDrive (*machine.GetDiskStore().GetImage (6, 0), report);
        file  = DecodeScenario (saved, "wizardry1-scenario.dsk");

        Assert::IsTrue (HasLineStarting (file, L"Saved game: 1 character"));
        Assert::IsTrue (HasLineStarting (file, std::format (L"CASSO   Level {} Human Fighter, Good", level)));
        Assert::IsTrue (HasLineStarting (file, std::format (L"Status OK   HP {}/{}   Gold {}   Exp {}",
                                                            FindScreenNumber (sheet, "HITS"), FindScreenNumber (sheet, "HITS", true),
                                                            NumberFormat::FormatInteger (FindScreenNumber (sheet, "GOLD")),
                                                            NumberFormat::FormatInteger (FindScreenNumber (sheet, "EXP")))));
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


    //  Wizardry V's Side A with two characters made in Casso: a dwarf fighter
    //  and an elf mage. The roster is in PLAYER.DATA, inside a volume larger
    //  than the disk, and the item text is in the game's coded text.
    TEST_METHOD (WizardryV_PlayerDataHasTheRoster)
    {
        std::vector<Byte>         image = GuestSession::FindTestDisk ("wizardry5-a-saved.po");
        std::vector<Byte>         sectors;
        SectorDecodeReport        report;
        std::unique_ptr<IVolume>  volume;
        FilePayload               payload;
        FileEntry                 entry;
        DecodedFile               decoded;
        std::wstring              error;
        FileFormat                format = FileFormat::None;



        if (image.empty())
        {
            Logger::WriteMessage ("SKIPPED: wizardry5-a-saved.po is not in the test disks folder\n");
            return;
        }

        AssertSucceeded (VolumeImage::Load (image, "wizardry5-a-saved.po", sectors, report));
        volume = VolumeImage::CreateVolume (VolumeKind::Pascal, sectors);
        AssertSucceeded (volume->Read (FilePath::Parse ("PLAYER.DATA"), payload));

        entry.name = "PLAYER.DATA";
        entry.type = payload.type;
        format     = FileFormatTable::Identify (entry, VolumeKind::Pascal, payload.bytes);
        Assert::IsTrue (format == FileFormat::WizardryRoster);

        DecodeOptions  options;

        options.diskSectors = sectors;
        AssertSucceeded (FileFormatTable::Decode (format, payload.bytes, FileFacts::From (entry, VolumeKind::Pascal), options, decoded, error));

        Assert::IsTrue (HasLineStarting (decoded, L"Wizardry V: Heart of the Maelstrom"));
        Assert::IsTrue (HasLineStarting (decoded, L"      LONG SWORD"), L"ALDRIC's items, by the game's names");
        Assert::IsTrue (HasLineStarting (decoded, L"      LEATHER ARMOR"));
        Assert::IsTrue (HasLineStarting (decoded, L"Saved game: 2 characters"));
        Assert::IsTrue (HasLineStarting (decoded, L"ALDRIC   Level 1 Dwarf Fighter, Good"));
        Assert::IsTrue (HasLineStarting (decoded, L"MIRIEL   Level 1 Elf Mage, Neutral"));
        Assert::IsTrue (HasLineStarting (decoded, L"Status OK   HP 7/7   Age 20"));
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
    //  long enough for the interpreter to print its response.
    static void TypeToZork (MachineHost & machine, const std::string & text, bool enter = true)
    {
        constexpr uint64_t  kPerKey   = 900'000ULL;
        constexpr uint64_t  kToAnswer = 6'000'000ULL;

        for (char c : text)
        {
            AssertSucceeded (KeystrokeInjector::InjectKey (machine, (Byte) c));
            machine.GetRunner().RunCycles (kPerKey);
        }

        if (enter)
        {
            AssertSucceeded (KeystrokeInjector::InjectKey (machine, (Byte) '\r'));
        }

        machine.GetRunner().RunCycles (kToAnswer);
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
        Assert::AreEqual (std::string ("Zork I (release 88): Kitchen, score 10 of 350, 4 moves"), InfocomSave::Describe (positions[0]));

        Assert::IsTrue (positions[0].carried.empty(), L"Nothing is carried yet");
        Assert::IsTrue (positions[0].here == std::vector<InfocomSave::Item> ({ { 0, L"kitchen table" }, { 1, L"brown sack" }, { 2, L"lunch" }, { 2, L"clove of garlic" },
                                                                            { 1, L"glass bottle" }, { 2, L"quantity of water" } }),
                        L"The table, and the sack and bottle on it");
    }


    //  A story disk from the test disks folder booted with a blank disk in
    //  drive 2, then `steps` typed: a step is a line typed and RETURN, or
    //  keys alone after a "!", and "~" is a space for [MORE]. What drive 2
    //  then has saved. Empty when the story disk is not on this machine.
    static std::vector<InfocomSave::Position> SaveInfocomGame (const char * fileName, DiskFormat format, std::initializer_list<const char *> steps)
    {
        constexpr uint64_t  kBootCycles = 150'000'000ULL;
        constexpr uint64_t  kMoreCycles = 30'000'000ULL;
        constexpr uint64_t  kStepCycles = 20'000'000ULL;

        TestMachine                         machine ("Apple2eEnhanced");
        std::vector<Byte>                   image = GuestSession::FindTestDisk (fileName);
        std::vector<Byte>                   blank (143360, 0);
        std::vector<InfocomSave::Position>  positions;
        SectorDecodeReport                  report;



        if (image.empty())
        {
            Logger::WriteMessage (std::format ("SKIPPED: {} is not in the test disks folder\n", fileName).c_str());
            return positions;
        }

        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, fileName,   format,           image));
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "save.dsk", DiskFormat::Dsk, blank));
        machine.PowerCycle();
        machine.GetRunner().RunCycles (kBootCycles);

        for (std::string step : steps)
        {
            if (step == "~")
            {
                AssertSucceeded (KeystrokeInjector::InjectKey (machine, (Byte) ' '));
                machine.GetRunner().RunCycles (kMoreCycles);
            }
            else if (step.starts_with ("!"))
            {
                TypeToZork (machine, step.substr (1), false);
            }
            else
            {
                TypeToZork (machine, step, true);
            }

            machine.GetRunner().RunCycles (kStepCycles);
        }

        InfocomSave::FindPositions (GuestSession::DecodeThroughTheDrive (*machine.GetDiskStore().GetImage (6, 1), report), positions);

        return positions;
    }


    //  Zork II, from its original disk: the lamp taken, the sword left on the
    //  ground, saved at position 0 in drive 2.
    TEST_METHOD (ZorkII_ASaveHasTheLampCarriedAndTheSwordHere)
    {
        std::vector<InfocomSave::Position>  positions = SaveInfocomGame ("zork2.woz", DiskFormat::Woz, { "!N", "TAKE LAMP", "SAVE", "", "!2", "", "", "" });



        if (!positions.empty())
        {
            Assert::AreEqual (std::string ("Zork II (release 48): Inside the Barrow, score 0 of 400, 1 move"), InfocomSave::Describe (positions[0]));
            Assert::IsTrue   (positions[0].carried == std::vector<InfocomSave::Item> ({ { 0, L"lamp" } }));
            Assert::IsTrue   (positions[0].here    == std::vector<InfocomSave::Item> ({ { 0, L"sword" } }));
        }
    }


    //  Cutthroats begins with the player in bed, wearing a watch, and the
    //  diving gear in the closet.
    TEST_METHOD (Cutthroats_ThePlayerInBedHasTheWatch)
    {
        std::vector<InfocomSave::Position>  positions = SaveInfocomGame ("cutthroats.dsk", DiskFormat::Dsk, { "!N", "", "~", "", "SAVE", "!0", "!6", "!2", "" });



        if (!positions.empty())
        {
            Assert::AreEqual (std::string ("Cutthroats (release 23): Your Room, time 8:00 AM"), InfocomSave::Describe (positions[0]));
            Assert::IsTrue   (positions[0].carried == std::vector<InfocomSave::Item> ({ { 0, L"wrist watch" } }));
            Assert::IsTrue   (positions[0].here[0] == InfocomSave::Item ({ 0, L"closet" }));
            Assert::IsTrue   (positions[0].here[1] == InfocomSave::Item ({ 1, L"wet suit" }));
        }
    }


    //  Spellbreaker's player object is called "it", and a global before it
    //  has Sneffle, another character in the room.
    TEST_METHOD (Spellbreaker_ThePlayerHasTheSpellBook)
    {
        std::vector<InfocomSave::Position>  positions = SaveInfocomGame ("spellbreaker.dsk", DiskFormat::Dsk, { "!N", "", "~", "", "SAVE", "!0", "!2", "!6", "!Y", "" });



        if (!positions.empty())
        {
            Assert::AreEqual (std::string ("Spellbreaker (release 63): Council Chamber, score 0 of 600, 0 moves"), InfocomSave::Describe (positions[0]));
            Assert::IsTrue   (positions[0].carried[0] == InfocomSave::Item ({ 0, L"spell book" }));
            Assert::IsTrue   (std::find (positions[0].here.begin(), positions[0].here.end(), InfocomSave::Item ({ 0, L"Sneffle" })) != positions[0].here.end());
        }
    }


    //  Zork III's early interpreter asks for the position, slot and drive a
    //  key each, and saves from track 3. Its serial number is "ZORK 3".
    TEST_METHOD (ZorkIII_AnEarlyInterpretersSaveIsFound)
    {
        std::vector<InfocomSave::Position>  positions = SaveInfocomGame ("zork3.po", DiskFormat::Po, { "~", "SAVE", "!0", "!6", "!2", "" });



        if (!positions.empty())
        {
            Assert::AreEqual (std::string ("Zork III (release 10): Endless Stair, score 0 of 7, 0 moves"), InfocomSave::Describe (positions[0]));
            Assert::IsTrue   (positions[0].carried.empty());
            Assert::IsTrue   (positions[0].here == std::vector<InfocomSave::Item> ({ { 0, L"lamp" } }));
        }
    }
};
