#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/InfocomSaveBuilder.h"
#include "../FileTypes/WizardryScenarioBuilder.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/PascalVolume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SavedGamePreviewTests
//
//  A Wizardry scenario disk selected in a folder previews as the saved game
//  in its SCENARIO.DATA, without opening the disk.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SavedGamePreviewTests)
{
public:

    static constexpr const wchar_t *  kDisks     = L"C:\\Disks";
    static constexpr size_t           kDiskBytes = 143360;


    struct Host
    {
        InMemoryFileSystem    fs;
        FakeDiskFileIo        io;
        CassoExplorerBrowser  browser { fs, io };

        Host()
        {
            browser.GetTreeModel().SetKnownFolders ({ kDisks });
            browser.GetTreeModel().SetDrives ({});
            browser.GetTreeModel().SetDirectoryProbe ([] (const std::wstring & path) { return _wcsicmp (path.c_str(), kDisks) == 0; });
        }

        void Seed (const std::vector<Byte> & bytes, const char * path)
        {
            AssertSucceeded (fs.WriteAllText (TextEncoding::NarrowToWide (path), std::string (bytes.begin(), bytes.end())));

            io.files[path]  = bytes;
            io.stamps[path] = FileStamp { bytes.size(), 100 };
        }

        //  The preview of the file in the folder, selected alone.
        const PreviewContent & Select (const wchar_t * name)
        {
            const std::vector<CatalogRow> &  rows = browser.GetRows();
            int                              row  = -1;

            browser.GetNavigator().NavigateToLocation (Location::MakeHostFolder (kDisks));

            for (size_t i = 0; i < rows.size(); i++)
            {
                if (rows[i].name == name)
                {
                    row = (int) i;
                }
            }

            Assert::AreNotEqual (-1, row, name);
            browser.GetSelection().SetSelectedRows ({ row });

            return browser.GetPreviewer().GetPreview();
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

        const PreviewContent &  preview = host.Select (L"scenario.dsk");

        Assert::IsTrue (preview.kind == PreviewContent::Kind::Document, L"The disk previews as its saved game");
        Assert::IsTrue (HasText (preview, L"GANDALF"), L"with the roster");
        Assert::IsTrue (preview.hasSavedGame && preview.savedGameShown);
    }


    TEST_METHOD (ScenarioDisk_ShowingTheDisk_PreviewsItsCatalog)
    {
        Host  host;

        host.Seed (MakeDisk ("SCENARIO.DATA", WizardryScenarioBuilder::MakeScenario()), "C:\\Disks\\scenario.dsk");
        host.Select (L"scenario.dsk");

        host.browser.GetPreviewer().SetSavedGameShown (false);

        const PreviewContent &  preview = host.browser.GetPreviewer().GetPreview();

        Assert::IsTrue  (preview.kind == PreviewContent::Kind::Catalog, L"The disk previews as its catalog");
        Assert::IsTrue  (preview.hasSavedGame, L"with the saved game a choice");
        Assert::IsFalse (preview.savedGameShown);

        host.browser.GetPreviewer().SetSavedGameShown (true);
        Assert::IsTrue  (host.browser.GetPreviewer().GetPreview().kind == PreviewContent::Kind::Document, L"and back");
    }


    //  An Infocom save disk has no file system: it previews as its saved
    //  positions, and the disk as its details.
    TEST_METHOD (InfocomSaveDisk_PreviewsAsTheSavedGame)
    {
        Host  host;

        host.Seed (InfocomSaveBuilder::MakeDisk (InfocomSaveBuilder::MakeMemory (0), 2), "C:\\Disks\\zork-save.dsk");

        const PreviewContent &  preview = host.Select (L"zork-save.dsk");

        Assert::IsTrue (preview.kind == PreviewContent::Kind::Document, L"The disk previews as its saved game");
        Assert::IsTrue (HasText (preview, L"release 5: hall, score 7, 12 moves"));
        Assert::IsTrue (preview.hasSavedGame && preview.savedGameShown);

        host.browser.SetPreviewSavedGame (false);

        Assert::IsTrue  (host.browser.GetPreview().kind == PreviewContent::Kind::Details, L"The disk previews as its details");
        Assert::IsTrue  (host.browser.GetPreview().hasSavedGame);
        Assert::IsFalse (host.browser.GetPreview().savedGameShown);
    }


    TEST_METHOD (OtherPascalDisk_PreviewsAsItsCatalog)
    {
        Host  host;

        host.Seed (MakeDisk ("NOTES.DATA", WizardryScenarioBuilder::MakeScenario()), "C:\\Disks\\notes.dsk");

        Assert::IsTrue  (host.Select (L"notes.dsk").kind == PreviewContent::Kind::Catalog);
        Assert::IsFalse (host.browser.GetPreviewer().GetPreview().hasSavedGame, L"with no saved game to choose");
    }


    TEST_METHOD (DamagedScenario_PreviewsAsTheCatalog)
    {
        Host  host;

        host.Seed (MakeDisk ("SCENARIO.DATA", std::vector<Byte> (1024, 0xFF)), "C:\\Disks\\damaged.dsk");

        Assert::IsTrue (host.Select (L"damaged.dsk").kind == PreviewContent::Kind::Catalog, L"A SCENARIO.DATA that doesn't decode leaves the catalog");
    }
};
