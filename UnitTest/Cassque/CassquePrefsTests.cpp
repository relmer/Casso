#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "Cassque/Model/CassquePrefs.h"
#include "Config/GlobalUserPrefs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassquePrefsTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassquePrefsTests)
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
        CassquePrefs        prefs;

        AssertSucceeded (prefs.Load (kBase, fs));

        Assert::AreEqual (std::string (CassquePrefs::kThemeFollowSystem), prefs.theme);
        Assert::IsTrue   (prefs.previewVisible);
        Assert::AreEqual (std::string (CassquePrefs::kNamingDescriptive), prefs.hostNaming);
        Assert::IsFalse  (prefs.placement.valid);
        Assert::AreEqual (CassquePrefs::kDefaultTreeWidthDip, prefs.treeWidthDip);
        Assert::IsTrue   (prefs.tabs.empty());
    }



    TEST_METHOD (RoundTrip_EveryField)
    {
        InMemoryFileSystem  fs;
        CassquePrefs        saved;
        CassquePrefs        loaded;

        saved.theme               = CassquePrefs::kThemeDark;
        saved.previewVisible      = false;
        saved.hostNaming          = CassquePrefs::kNamingCiderPress;
        saved.placement.x         = 10;
        saved.placement.y         = 20;
        saved.placement.w         = 800;
        saved.placement.h         = 600;
        saved.placement.maximized = true;
        saved.placement.valid     = true;
        saved.treeWidthDip        = 300;
        saved.previewWidthDip     = 400;
        saved.tabs.push_back (Location::MakeHostFolder (L"C:\\Disks"));
        saved.tabs.push_back (Location::MakeDiskDirectory (L"C:\\Disks\\a.po", "SUBDIR"));

        AssertSucceeded (saved.Save (kBase, fs));
        AssertSucceeded (loaded.Load (kBase, fs));

        Assert::AreEqual (saved.theme, loaded.theme);
        Assert::IsFalse  (loaded.previewVisible);
        Assert::AreEqual (saved.hostNaming, loaded.hostNaming);
        Assert::AreEqual (10,  loaded.placement.x);
        Assert::AreEqual (600, loaded.placement.h);
        Assert::IsTrue   (loaded.placement.maximized);
        Assert::IsTrue   (loaded.placement.valid);
        Assert::AreEqual (300, loaded.treeWidthDip);
        Assert::AreEqual (400, loaded.previewWidthDip);
        Assert::AreEqual ((size_t) 2, loaded.tabs.size());
        Assert::IsTrue   (loaded.tabs[1] == Location::MakeDiskDirectory (L"C:\\Disks\\a.po", "SUBDIR"));
    }



    TEST_METHOD (Theme_SeededFromCassoOnFirstRunOnly)
    {
        InMemoryFileSystem  fs;
        CassquePrefs        prefs;

        WriteCassoTheme (fs, "DarkModern");

        AssertSucceeded  (prefs.Load (kBase, fs));
        Assert::AreEqual (std::string (CassquePrefs::kThemeDarkModern), prefs.theme);

        AssertSucceeded (prefs.Save (kBase, fs));

        //  Casso changes its mind; the browser keeps its own.
        WriteCassoTheme (fs, "RetroTerminal");

        AssertSucceeded  (prefs.Load (kBase, fs));
        Assert::AreEqual (std::string (CassquePrefs::kThemeDarkModern), prefs.theme);
    }



    TEST_METHOD (Theme_MappingAndUnknownValues)
    {
        InMemoryFileSystem  fs;
        CassquePrefs        prefs;

        Assert::AreEqual (std::string (CassquePrefs::kThemeSkeuomorphic),  CassquePrefs::MapCassoTheme ("Skeuomorphic"));
        Assert::AreEqual (std::string (CassquePrefs::kThemeRetroTerminal), CassquePrefs::MapCassoTheme ("RetroTerminal"));
        Assert::AreEqual (std::string (CassquePrefs::kThemeFollowSystem),  CassquePrefs::MapCassoTheme ("SomethingElse"));
        Assert::AreEqual (std::string (CassquePrefs::kThemeFollowSystem),  CassquePrefs::MapCassoTheme (""));

        //  A stored theme this build does not know keeps the default.
        AssertSucceeded  (fs.WriteAllText (CassquePrefs::GetFilePath (kBase), "{ \"theme\": \"Plaid\", \"hostNaming\": \"Odd\" }"));
        AssertSucceeded  (prefs.Load (kBase, fs));
        Assert::AreEqual (std::string (CassquePrefs::kThemeFollowSystem), prefs.theme);
        Assert::AreEqual (std::string (CassquePrefs::kNamingDescriptive), prefs.hostNaming);
    }



    TEST_METHOD (Load_AFileThatWillNotParseIsReported)
    {
        InMemoryFileSystem  fs;
        CassquePrefs        prefs;
        HRESULT             hr = S_OK;

        AssertSucceeded (fs.WriteAllText (CassquePrefs::GetFilePath (kBase), "{ not json"));

        hr = prefs.Load (kBase, fs);

        Assert::IsTrue (FAILED (hr));
    }
};
