#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/InfocomSaveBuilder.h"
#include "../FileTypes/WizardryScenarioBuilder.h"
#include "BrowserPartsFixture.h"
#include "CassoExplorer/Browser/Preview/DiskImagePreviewer.h"
#include "Core/TextEncoding.h"
#include "FileTypes/WizardryRoster.h"
#include "Machines/Apple2/Common/PascalVolume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SavedGamePreviewTests
//
//  A Wizardry disk previews as the saved game in its SCENARIO.DATA or
//  PLAYER.DATA, without opening the disk.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SavedGamePreviewTests)
{
public:

    static constexpr const char *  kScenarioPath = "C:\\Disks\\scenario.dsk";
    static constexpr size_t        kDiskBytes    = 143360;


    struct Host
    {
        BrowserPartsFixture  fixture;
        DiskImagePreviewer   previewer { fixture.parts, fixture.previewParts };

        void Seed (const std::vector<Byte> & bytes, const char * path)
        {
            fixture.Seed (path, bytes);
        }

        //  The preview of the image at the path, made afresh.
        const PreviewContent & Select (const char * path)
        {
            fixture.preview      = PreviewContent();
            fixture.preview.kind = PreviewContent::Kind::Text;

            previewer.PreviewImageCatalog (TextEncoding::NarrowToWide (path));

            return fixture.preview;
        }
    };


    //  A blank Pascal disk with one data file on it.
    static std::vector<Byte> MakeDisk (const char * name, const std::vector<Byte> & bytes)
    {
        std::vector<Byte>  disk (kDiskBytes, 0);
        std::vector<Byte>  next;
        FilePayload        payload;



        PascalVolume::WriteBlank ("WIZ", PascalVolume::MakeDate (1981, 9, 1), disk);

        payload.bytes = bytes;
        payload.type  = PascalVolume::kKindData;

        AssertSucceeded (PascalVolume (disk).Write (FilePath::Parse (name), payload, next));

        return next;
    }


    static bool HasText (const PreviewContent & preview, const std::wstring & text)
    {
        for (const Paragraph & paragraph : FlattenRecordTables (preview.paragraphs))
        {
            std::wstring  joined;

            for (const Run & run : paragraph.runs)
            {
                joined += run.text;
            }

            if (joined.find (text) != std::wstring::npos)
            {
                return true;
            }
        }

        return false;
    }


    TEST_METHOD (ScenarioDisk_PreviewsAsTheSavedGame)
    {
        Host  host;

        host.Seed (MakeDisk ("SCENARIO.DATA", WizardryScenarioBuilder::MakeScenario()), "C:\\Disks\\scenario.dsk");

        const PreviewContent &  preview = host.Select (kScenarioPath);

        Assert::IsTrue (preview.kind == PreviewContent::Kind::Document, L"The disk previews as its saved game");
        Assert::IsTrue (HasText (preview, L"GANDALF"), L"with the roster");
        Assert::IsTrue (preview.hasSavedGame && preview.savedGameShown);
    }


    TEST_METHOD (ScenarioDisk_ShowingTheDisk_PreviewsItsCatalog)
    {
        Host  host;

        host.Seed (MakeDisk ("SCENARIO.DATA", WizardryScenarioBuilder::MakeScenario()), "C:\\Disks\\scenario.dsk");
        host.Select (kScenarioPath);

        host.fixture.viewState.SetSavedGameShown (false);

        const PreviewContent &  preview = host.Select (kScenarioPath);

        Assert::IsTrue  (preview.kind == PreviewContent::Kind::Catalog, L"The disk previews as its catalog");
        Assert::IsTrue  (preview.hasSavedGame, L"with the saved game a choice");
        Assert::IsFalse (preview.savedGameShown);

        host.fixture.viewState.SetSavedGameShown (true);
        Assert::IsTrue  (host.Select (kScenarioPath).kind == PreviewContent::Kind::Document, L"and back");
    }


    //  Wizardry V's roster is in PLAYER.DATA, and the disk has no SCENARIO.DATA.
    TEST_METHOD (WizardryVDisk_PreviewsAsTheSavedGame)
    {
        Host               host;
        std::vector<Byte>  roster (2 * WizardryRoster::kCharacterBytes, 0);



        memcpy (&roster[WizardryRoster::kNameAt], "\x06" "ALDRIC", 7);
        roster[WizardryRoster::kRaceAt] = 3;
        memcpy (&roster[WizardryRoster::kCharacterBytes + WizardryRoster::kNameAt], "\x05" "UNSET", 6);
        roster[WizardryRoster::kCharacterBytes + WizardryRoster::kStatusAt] = WizardryRoster::kStatusLost;

        host.Seed (MakeDisk ("PLAYER.DATA", roster), "C:\\Disks\\wizardry5.dsk");

        const PreviewContent &  preview = host.Select ("C:\\Disks\\wizardry5.dsk");

        Assert::IsTrue (preview.kind == PreviewContent::Kind::Document, L"The disk previews as its saved game");
        Assert::IsTrue (HasText (preview, L"ALDRIC   Level 0 Dwarf Fighter, Unaligned"));
        Assert::IsTrue (preview.hasSavedGame && preview.savedGameShown);
    }


    //  An Infocom save disk has no file system: it previews as its saved
    //  positions, and the disk as its details.
    TEST_METHOD (InfocomSaveDisk_PreviewsAsTheSavedGame)
    {
        Host  host;

        host.Seed (InfocomSaveBuilder::MakeDisk (InfocomSaveBuilder::MakeMemory (0), 2), "C:\\Disks\\zork-save.dsk");

        const PreviewContent &  preview = host.Select ("C:\\Disks\\zork-save.dsk");

        Assert::IsTrue (preview.kind == PreviewContent::Kind::Document, L"The disk previews as its saved game");
        Assert::IsTrue (HasText (preview, L"Room\thall"));
        Assert::IsTrue (preview.hasSavedGame && preview.savedGameShown);

        host.fixture.viewState.SetSavedGameShown (false);

        const PreviewContent &  details = host.Select ("C:\\Disks\\zork-save.dsk");

        Assert::IsTrue  (details.kind == PreviewContent::Kind::Details, L"The disk previews as its details");
        Assert::IsTrue  (details.hasSavedGame);
        Assert::IsFalse (details.savedGameShown);
    }


    TEST_METHOD (OtherPascalDisk_PreviewsAsItsCatalog)
    {
        Host  host;

        host.Seed (MakeDisk ("NOTES.DATA", WizardryScenarioBuilder::MakeScenario()), "C:\\Disks\\notes.dsk");

        Assert::IsTrue  (host.Select ("C:\\Disks\\notes.dsk").kind == PreviewContent::Kind::Catalog);
        Assert::IsFalse (host.fixture.preview.hasSavedGame, L"with no saved game to choose");
    }


    TEST_METHOD (DamagedScenario_PreviewsAsTheCatalog)
    {
        Host  host;

        host.Seed (MakeDisk ("SCENARIO.DATA", std::vector<Byte> (1024, 0xFF)), "C:\\Disks\\damaged.dsk");

        Assert::IsTrue (host.Select ("C:\\Disks\\damaged.dsk").kind == PreviewContent::Kind::Catalog, L"A SCENARIO.DATA that doesn't decode is previewed as the catalog");
    }
};
