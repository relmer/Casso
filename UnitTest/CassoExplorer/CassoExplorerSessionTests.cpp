#include "Pch.h"
#include "CassoExplorer/Model/CassoExplorerSession.h"
#include "../UiTests/InMemoryFileSystem.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerSessionTests
//
//  The windows Casso Explorer reopens. Every tab has to come back as it was:
//  where it is, the history both ways, the selection, and how the view was
//  set; and every window where it was, with its active tab. A path in a
//  script the system's code page cannot represent comes back too, unlike in
//  the preferences file's narrow text.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerSessionTests)
{
public:

    static BrowserModel::Tab  MakeTab()
    {
        BrowserModel::Tab  tab;

        tab.location        = Location::MakeDiskDirectory (L"C:\\Disks\\\x65E5\x672C.po", "GAMES/ARCADE");
        tab.back            = { Location::MakeHostFolder (L"C:\\Disks"), Location::MakeDiskImage (L"C:\\Disks\\\x65E5\x672C.po") };
        tab.forward         = { Location::MakeShellFolder (L"::{F02C1A0D-BE21-4350-88B0-7367FC96EF3C}", L"Network") };
        tab.selection       = { L"LODE RUNNER", L"CHOPLIFTER" };
        tab.sortColumn      = CatalogModel::Column::Size;
        tab.sortDescending  = true;
        tab.groupBy         = RowGrouping::Field::Type;
        tab.groupDescending = true;
        tab.previewScroll   = 37;
        tab.disassemble     = true;

        return tab;
    }


    static CassoExplorerSessionWindow  MakeWindow()
    {
        CassoExplorerSessionWindow  window;

        window.tabs                = { BrowserModel::Tab(), MakeTab() };
        window.activeTab           = 1;
        window.placement.x         = -1200;
        window.placement.y         = 40;
        window.placement.w         = 800;
        window.placement.h         = 600;
        window.placement.maximized = true;
        window.placement.valid     = true;

        return window;
    }


    TEST_METHOD (RoundTrip_KeepsEveryWindowAndTab)
    {
        std::vector<CassoExplorerSessionWindow>  read;
        CassoExplorerSessionWindow               second = MakeWindow();

        second.activeTab = 0;

        Assert::IsTrue   (CassoExplorerSession::TryRead (CassoExplorerSession::Write ({ MakeWindow(), second }), read));
        Assert::AreEqual ((size_t) 2, read.size(), L"Every window");
        Assert::AreEqual ((size_t) 1, read[0].activeTab, L"with its active tab");
        Assert::AreEqual ((size_t) 0, read[1].activeTab);
        Assert::AreEqual (-1200, read[0].placement.x, L"where it was, on any monitor");
        Assert::AreEqual (600,   read[0].placement.h);
        Assert::IsTrue   (read[0].placement.maximized);
        Assert::IsTrue   (read[0].placement.valid);
        Assert::AreEqual ((size_t) 2, read[0].tabs.size());
        Assert::IsTrue   (read[0].tabs[0].location.kind == Location::Kind::None, L"A Home tab comes back as Home");
    }


    TEST_METHOD (RoundTrip_KeepsTheTabsLocationHistoryAndView)
    {
        std::vector<CassoExplorerSessionWindow>  read;
        BrowserModel::Tab                        sent = MakeTab();

        Assert::IsTrue (CassoExplorerSession::TryRead (CassoExplorerSession::Write ({ MakeWindow() }), read));

        const BrowserModel::Tab  & tab = read[0].tabs[1];

        Assert::IsTrue   (tab.location == sent.location);
        Assert::AreEqual (sent.location.path, tab.location.path, L"A path the code page cannot write comes back whole");
        Assert::AreEqual ((size_t) 2, tab.back.size());
        Assert::IsTrue   (tab.back[1] == sent.back[1]);
        Assert::AreEqual ((size_t) 1, tab.forward.size());
        Assert::AreEqual (std::wstring (L"Network"), tab.forward[0].label, L"A shell folder's display name comes back too");
        Assert::AreEqual ((size_t) 2, tab.selection.size());
        Assert::AreEqual (std::wstring (L"CHOPLIFTER"), tab.selection[1]);
        Assert::IsTrue   (tab.sortColumn == CatalogModel::Column::Size);
        Assert::IsTrue   (tab.sortDescending);
        Assert::IsTrue   (tab.groupBy == RowGrouping::Field::Type);
        Assert::IsTrue   (tab.groupDescending);
        Assert::AreEqual (37, tab.previewScroll);
        Assert::IsTrue   (tab.disassemble);
    }


    TEST_METHOD (Read_LeavesOutWhatDoesNotReadBack)
    {
        // A tab with a sort this build does not have is left out; a window
        // with no tab left is left out; the rest opens.
        std::vector<CassoExplorerSessionWindow>  read;
        std::string                              text = CassoExplorerSession::Write ({ MakeWindow(), MakeWindow() });
        size_t                                   key  = text.find ("\"sortColumn\"", text.find ("\"sortColumn\"") + 1);   // the first window's second tab
        size_t                                   at   = text.find_first_of ("0123456789", key);

        Assert::IsTrue   (key != std::string::npos && at != std::string::npos);
        Assert::AreEqual ('3', text[at], L"The sort column is written as a number");
        text.replace (at, 1, "99");

        Assert::IsTrue   (CassoExplorerSession::TryRead (text, read));
        Assert::AreEqual ((size_t) 2, read.size());
        Assert::AreEqual ((size_t) 1, read[0].tabs.size(), L"The tab that does not read back is left out");
        Assert::AreEqual ((size_t) 0, read[0].activeTab, L"and an active tab past the last is the first");

        Assert::IsTrue   (CassoExplorerSession::TryRead ("{\"windows\":[{\"tabs\":[]},{\"placement\":{}}]}", read));
        Assert::AreEqual ((size_t) 0, read.size(), L"A window with no tabs is no window");

        Assert::IsFalse  (CassoExplorerSession::TryRead ("not json", read));
        Assert::IsFalse  (CassoExplorerSession::TryRead ("{\"tabs\":[]}", read));
    }


    TEST_METHOD (File_SavesAndLoadsBesideThePreferences)
    {
        InMemoryFileSystem                       fs;
        std::vector<CassoExplorerSessionWindow>  read;

        Assert::AreEqual (S_OK, CassoExplorerSession::Load (fs, L"C:\\Base", read), L"No file is no session");
        Assert::AreEqual ((size_t) 0, read.size());

        Assert::AreEqual (S_OK, CassoExplorerSession::Save (fs, L"C:\\Base", { MakeWindow() }));
        Assert::IsTrue   (fs.Exists (L"C:\\Base\\CassoExplorerSession.json"));

        Assert::AreEqual (S_OK, CassoExplorerSession::Load (fs, L"C:\\Base", read));
        Assert::AreEqual ((size_t) 1, read.size());
        Assert::AreEqual ((size_t) 2, read[0].tabs.size());
    }
};
