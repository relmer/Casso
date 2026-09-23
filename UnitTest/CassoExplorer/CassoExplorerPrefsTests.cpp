#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
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
        saved.listView            = 6;
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

        AssertSucceeded (saved.Save (kBase, fs));
        AssertSucceeded (loaded.Load (kBase, fs));

        Assert::AreEqual (saved.theme, loaded.theme);
        Assert::IsFalse  (loaded.previewVisible);
        Assert::AreEqual (saved.hostNaming, loaded.hostNaming);
        Assert::AreEqual (6, loaded.listView);
        Assert::AreEqual (10,  loaded.placement.x);
        Assert::AreEqual (600, loaded.placement.h);
        Assert::IsTrue   (loaded.placement.maximized);
        Assert::IsTrue   (loaded.placement.valid);
        Assert::AreEqual (300, loaded.treeWidthDip);
        Assert::AreEqual (400, loaded.previewWidthDip);
        Assert::AreEqual ((size_t) 2, loaded.tabs.size());
        Assert::IsTrue   (loaded.tabs[1] == Location::MakeDiskDirectory (L"C:\\Disks\\a.po", "SUBDIR"));
        Assert::AreEqual ((size_t) 3, loaded.columnWidthsDip.size());
        Assert::AreEqual (240, loaded.columnWidthsDip[0]);
        Assert::AreEqual (0,   loaded.columnWidthsDip[1], L"A column that fits itself stores nothing");
        Assert::AreEqual (90,  loaded.columnWidthsDip[2]);
        Assert::AreEqual ((size_t) 2, loaded.typedPaths.size());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\Merlin.po"), loaded.typedPaths[0]);
        Assert::AreEqual (std::wstring (L"C:\\Games"),            loaded.typedPaths[1]);
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
};
