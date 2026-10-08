#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Config/GlobalUserPrefs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerPrefsTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerPrefsTests)
{
public:

    static constexpr const wchar_t *  kBase = L"C:\\Users\\me\\AppData\\Local\\Casso";



    static void WriteCassoTheme (InMemoryFileSystem & fs, const char * theme)
    {
        std::string  text = std::string ("{ \"global\": { \"activeTheme\": \"") + theme + "\" }, \"machines\": {} }";

        AssertSucceeded (fs.WriteAllText (GlobalUserPrefs::GetFilePath (kBase), text));
    }



    TEST_METHOD (Defaults_WhenNothingIsStored)
    {
        InMemoryFileSystem  fs;
        CassoExplorerPrefs  prefs;

        AssertSucceeded (prefs.Load (kBase, fs));

        Assert::AreEqual (std::string (CassoExplorerPrefs::kThemeFollowSystem), prefs.theme);
        Assert::IsTrue   (prefs.previewVisible);
        Assert::AreEqual (std::string (CassoExplorerPrefs::kNamingDescriptive), prefs.hostNaming);
        Assert::IsFalse  (prefs.placement.valid);
        Assert::AreEqual (CassoExplorerPrefs::kDefaultTreeWidthDip, prefs.treeWidthDip);
        Assert::IsTrue   (prefs.tabs.empty());
    }



    TEST_METHOD (RoundTrip_EveryField)
    {
        InMemoryFileSystem  fs;
        CassoExplorerPrefs  saved;
        CassoExplorerPrefs  loaded;

        saved.theme               = CassoExplorerPrefs::kThemeDark;
        saved.previewVisible      = false;
        saved.hostNaming          = CassoExplorerPrefs::kNamingCiderPress;
        saved.folderViews.Remember (L"C:\\Disks", DxuiListView::View::Tiles);
        saved.placement.x         = 10;
        saved.placement.y         = 20;
        saved.placement.w         = 800;
        saved.placement.h         = 600;
        saved.placement.maximized = true;
        saved.placement.valid     = true;
        saved.treeWidthDip        = 300;
        saved.previewWidthDip     = 400;
        saved.typedPaths.push_back (L"C:\\Disks\\Merlin.po");
        saved.typedPaths.push_back (L"C:\\Games");
        saved.columnWidthsDip = { 240, 0, 90 };
        saved.tabs.push_back (Location::MakeHostFolder (L"C:\\Disks"));
        saved.tabs.push_back (Location::MakeDiskDirectory (L"C:\\Disks\\a.po", "SUBDIR"));
        saved.tabs.push_back (Location::MakeRoot (TreeModel::kThisPcRootId));

        AssertSucceeded (saved.Save (kBase, fs));
        AssertSucceeded (loaded.Load (kBase, fs));

        Assert::AreEqual (saved.theme, loaded.theme);
        Assert::IsFalse  (loaded.previewVisible);
        Assert::AreEqual (saved.hostNaming, loaded.hostNaming);
        Assert::IsTrue   (loaded.folderViews.GetView (L"c:\\disks\\", FolderViews::FolderType::Generic) == DxuiListView::View::Tiles);
        Assert::AreEqual (10,  loaded.placement.x);
        Assert::AreEqual (600, loaded.placement.h);
        Assert::IsTrue   (loaded.placement.maximized);
        Assert::IsTrue   (loaded.placement.valid);
        Assert::AreEqual (300, loaded.treeWidthDip);
        Assert::AreEqual (400, loaded.previewWidthDip);
        Assert::AreEqual ((size_t) 3, loaded.tabs.size());
        Assert::IsTrue   (loaded.tabs[1] == Location::MakeDiskDirectory (L"C:\\Disks\\a.po", "SUBDIR"));
        Assert::IsTrue   (loaded.tabs[2] == Location::MakeRoot (TreeModel::kThisPcRootId), L"A root's tab comes back");
        Assert::AreEqual ((size_t) 3, loaded.columnWidthsDip.size());
        Assert::AreEqual (240, loaded.columnWidthsDip[0]);
        Assert::AreEqual (0,   loaded.columnWidthsDip[1], L"A column that fits itself stores nothing");
        Assert::AreEqual (90,  loaded.columnWidthsDip[2]);
        Assert::AreEqual ((size_t) 2, loaded.typedPaths.size());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\Merlin.po"), loaded.typedPaths[0]);
        Assert::AreEqual (std::wstring (L"C:\\Games"),            loaded.typedPaths[1]);
    }



    //  A folder keeps its sort and grouping with its view; a folder never
    //  given one opens by name, up, ungrouped, in its type's view.
    TEST_METHOD (FolderViews_KeepSortAndGroupPerFolder)
    {
        InMemoryFileSystem  fs;
        CassoExplorerPrefs  saved;
        CassoExplorerPrefs  loaded;
        FolderViewEntry     entry;

        entry                 = saved.folderViews.GetEntry (L"C:\\Disks", FolderViews::FolderType::Generic);
        entry.sortColumn      = 3;
        entry.sortDescending  = true;
        entry.groupBy         = 2;
        entry.groupDescending = true;
        saved.folderViews.Remember (entry);

        //  A later view change keeps the rest.
        saved.folderViews.Remember (L"c:\\disks", DxuiListView::View::Tiles);

        AssertSucceeded (saved.Save (kBase, fs));
        AssertSucceeded (loaded.Load (kBase, fs));

        entry = loaded.folderViews.GetEntry (L"C:\\Disks\\", FolderViews::FolderType::Generic);
        Assert::IsTrue   (entry.view == DxuiListView::View::Tiles);
        Assert::AreEqual (3, entry.sortColumn);
        Assert::IsTrue   (entry.sortDescending);
        Assert::AreEqual (2, entry.groupBy);
        Assert::IsTrue   (entry.groupDescending);

        entry = loaded.folderViews.GetEntry (L"C:\\Other", FolderViews::FolderType::Pictures);
        Assert::IsTrue   (entry.view == FolderViews::GetDefaultView (FolderViews::FolderType::Pictures));
        Assert::AreEqual (0, entry.sortColumn);
        Assert::IsFalse  (entry.sortDescending);
        Assert::AreEqual (0, entry.groupBy);
    }



    //  A folder's own column widths come back with it; another folder has none.
    TEST_METHOD (FolderViews_KeepColumnWidthsPerFolder)
    {
        InMemoryFileSystem  fs;
        CassoExplorerPrefs  saved;
        CassoExplorerPrefs  loaded;
        FolderViewEntry     entry;

        entry                 = saved.folderViews.GetEntry (L"C:\\Disks", FolderViews::FolderType::Generic);
        entry.columnWidthsDip = { 300, 0, 120 };
        saved.folderViews.Remember (entry);

        AssertSucceeded (saved.Save (kBase, fs));
        AssertSucceeded (loaded.Load (kBase, fs));

        entry = loaded.folderViews.GetEntry (L"c:\\disks", FolderViews::FolderType::Generic);
        Assert::AreEqual ((size_t) 3, entry.columnWidthsDip.size());
        Assert::AreEqual (300, entry.columnWidthsDip[0]);
        Assert::AreEqual (0,   entry.columnWidthsDip[1]);
        Assert::AreEqual (120, entry.columnWidthsDip[2]);

        Assert::IsTrue (loaded.folderViews.GetEntry (L"C:\\Other", FolderViews::FolderType::Generic).columnWidthsDip.empty());
    }


    //  A folder's column order and choice of columns come back with it, an
    //  empty choice included; a folder that made none follows the latest.
    TEST_METHOD (FolderViews_KeepColumnOrderAndChoicePerFolder)
    {
        InMemoryFileSystem  fs;
        CassoExplorerPrefs  saved;
        CassoExplorerPrefs  loaded;
        FolderViewEntry     entry;

        entry               = saved.folderViews.GetEntry (L"C:\\Disks", FolderViews::FolderType::Generic);
        entry.columnOrder   = { 0, 2, 1 };
        entry.columnsChosen = true;
        entry.hiddenColumns = { 3 };
        saved.folderViews.Remember (entry);

        entry               = saved.folderViews.GetEntry (L"C:\\All", FolderViews::FolderType::Generic);
        entry.columnsChosen = true;
        saved.folderViews.Remember (entry);

        saved.hiddenColumns = { 4, 5 };

        AssertSucceeded (saved.Save (kBase, fs));
        AssertSucceeded (loaded.Load (kBase, fs));

        entry = loaded.folderViews.GetEntry (L"c:\\disks", FolderViews::FolderType::Generic);
        Assert::IsTrue   (entry.columnOrder == std::vector<int> ({ 0, 2, 1 }));
        Assert::IsTrue   (entry.columnsChosen);
        Assert::IsTrue   (entry.hiddenColumns == std::vector<int> ({ 3 }));

        entry = loaded.folderViews.GetEntry (L"C:\\All", FolderViews::FolderType::Generic);
        Assert::IsTrue   (entry.columnsChosen, L"Every column shown is a choice of its own");
        Assert::IsTrue   (entry.hiddenColumns.empty());

        Assert::IsFalse  (loaded.folderViews.GetEntry (L"C:\\Other", FolderViews::FolderType::Generic).columnsChosen);
        Assert::IsTrue   (loaded.hiddenColumns == std::vector<int> ({ 4, 5 }));
    }


    TEST_METHOD (Theme_SeededFromCassoOnFirstRunOnly)
    {
        InMemoryFileSystem  fs;
        CassoExplorerPrefs  prefs;

        WriteCassoTheme (fs, "DarkModern");

        AssertSucceeded  (prefs.Load (kBase, fs));
        Assert::AreEqual (std::string (CassoExplorerPrefs::kThemeDarkModern), prefs.theme);

        AssertSucceeded (prefs.Save (kBase, fs));

        //  Casso changes its mind; the browser keeps its own.
        WriteCassoTheme (fs, "RetroTerminal");

        AssertSucceeded  (prefs.Load (kBase, fs));
        Assert::AreEqual (std::string (CassoExplorerPrefs::kThemeDarkModern), prefs.theme);
    }



    TEST_METHOD (Theme_MappingAndUnknownValues)
    {
        InMemoryFileSystem  fs;
        CassoExplorerPrefs  prefs;

        Assert::AreEqual (std::string (CassoExplorerPrefs::kThemeSkeuomorphic),  CassoExplorerPrefs::MapCassoTheme ("Skeuomorphic"));
        Assert::AreEqual (std::string (CassoExplorerPrefs::kThemeRetroTerminal), CassoExplorerPrefs::MapCassoTheme ("RetroTerminal"));
        Assert::AreEqual (std::string (CassoExplorerPrefs::kThemeFollowSystem),  CassoExplorerPrefs::MapCassoTheme ("SomethingElse"));
        Assert::AreEqual (std::string (CassoExplorerPrefs::kThemeFollowSystem),  CassoExplorerPrefs::MapCassoTheme (""));

        //  A stored theme this build does not know keeps the default.
        AssertSucceeded  (fs.WriteAllText (CassoExplorerPrefs::GetFilePath (kBase), "{ \"theme\": \"Plaid\", \"hostNaming\": \"Odd\" }"));
        AssertSucceeded  (prefs.Load (kBase, fs));
        Assert::AreEqual (std::string (CassoExplorerPrefs::kThemeFollowSystem), prefs.theme);
        Assert::AreEqual (std::string (CassoExplorerPrefs::kNamingDescriptive), prefs.hostNaming);
    }



    TEST_METHOD (Load_AFileThatWillNotParseIsReported)
    {
        InMemoryFileSystem  fs;
        CassoExplorerPrefs  prefs;
        HRESULT             hr    = S_OK;

        AssertSucceeded (fs.WriteAllText (CassoExplorerPrefs::GetFilePath (kBase), "{ not json"));

        hr = prefs.Load (kBase, fs);

        Assert::IsTrue (FAILED (hr));
    }



    TEST_METHOD (NavPaneOptions_FollowExplorerUntilSet)
    {
        CassoExplorerPrefs  saved;
        CassoExplorerPrefs  loaded;

        //  Only the option set here is kept; the rest stay unset and keep
        //  following File Explorer's.
        saved.navShowNetwork = false;
        AssertSucceeded (loaded.FromJson (saved.ToJson()));

        Assert::IsTrue  (loaded.navShowNetwork.has_value());
        Assert::IsFalse (*loaded.navShowNetwork);
        Assert::IsFalse (loaded.navShowThisPc.has_value());
        Assert::IsFalse (loaded.navShowLibraries.has_value());
        Assert::IsFalse (loaded.navShowAllFolders.has_value());
        Assert::IsFalse (loaded.navExpandToCurrent.has_value());
    }
};
