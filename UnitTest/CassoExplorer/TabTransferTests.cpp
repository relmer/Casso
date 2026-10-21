#include "Pch.h"
#include "CassoExplorer/Model/TabTransfer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TabTransferTests
//
//  A tab dragged to another window, which is another process, crosses as
//  text. Everything the tab was showing has to arrive: where it is, the
//  history both ways, the selection, and how the view was set. A path in a
//  script the system's code page cannot represent arrives too, unlike in the
//  preferences file's narrow text.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TabTransferTests)
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
        tab.hasCatalog      = true;

        return tab;
    }


    TEST_METHOD (RoundTrip_KeepsTheLocationHistoryAndView)
    {
        BrowserModel::Tab  sent     = MakeTab();
        BrowserModel::Tab  received;

        Assert::IsTrue (TabTransfer::TryDecode (TabTransfer::Encode (sent), received));

        Assert::IsTrue   (received.location == sent.location);
        Assert::AreEqual (sent.location.path, received.location.path, L"A path the code page cannot write arrives whole");
        Assert::AreEqual ((size_t) 2, received.back.size());
        Assert::IsTrue   (received.back[0] == sent.back[0]);
        Assert::IsTrue   (received.back[1] == sent.back[1]);
        Assert::AreEqual ((size_t) 1, received.forward.size());
        Assert::IsTrue   (received.forward[0] == sent.forward[0]);
        Assert::AreEqual (std::wstring (L"Network"), received.forward[0].label, L"A shell folder's display name arrives too");
        Assert::AreEqual ((size_t) 2, received.selection.size());
        Assert::AreEqual (std::wstring (L"CHOPLIFTER"), received.selection[1]);
        Assert::IsTrue   (received.sortColumn == CatalogModel::Column::Size);
        Assert::IsTrue   (received.sortDescending);
        Assert::IsTrue   (received.groupBy == RowGrouping::Field::Type);
        Assert::IsTrue   (received.groupDescending);
        Assert::AreEqual (37, received.previewScroll);
        Assert::IsTrue   (received.disassemble);
        Assert::IsFalse  (received.hasCatalog, L"The cached catalog stays behind");
    }


    TEST_METHOD (RoundTrip_HomeTabStaysHome)
    {
        BrowserModel::Tab  sent;
        BrowserModel::Tab  received;

        received.location = Location::MakeHostFolder (L"C:\\Elsewhere");

        Assert::IsTrue (TabTransfer::TryDecode (TabTransfer::Encode (sent), received));
        Assert::IsTrue (received.location.kind == Location::Kind::None, L"A new tab's Home has no path and reads back as Home");
    }


    TEST_METHOD (Encode_DropsTheOldestHistoryFirstToFit)
    {
        BrowserModel::Tab  sent     = MakeTab();
        BrowserModel::Tab  received;
        std::wstring       whole    = TabTransfer::Encode (sent);
        std::wstring       trimmed;
        size_t             i        = 0;

        for (i = 0; i < 40; i++)
        {
            sent.back.insert (sent.back.begin(), Location::MakeHostFolder (L"C:\\Old\\" + std::to_wstring (i)));
        }

        trimmed = TabTransfer::Encode (sent, whole.size());

        Assert::IsTrue   (trimmed.size() <= whole.size(), L"The text fits the room given");
        Assert::IsTrue   (TabTransfer::TryDecode (trimmed, received));
        Assert::IsTrue   (received.location == sent.location, L"and the tab is where it was");
        Assert::AreEqual ((size_t) 2, received.back.size(), L"and the most recent history is what remains");
        Assert::IsTrue   (received.back.back() == sent.back.back());
        Assert::AreEqual ((size_t) 1, received.forward.size());
    }


    TEST_METHOD (Decode_RefusesTextThatIsNotATab)
    {
        BrowserModel::Tab  received;
        std::wstring       text = TabTransfer::Encode (MakeTab());
        size_t             at   = text.find (L"\"sortColumn\":");

        Assert::IsFalse (TabTransfer::TryDecode (L"", received));
        Assert::IsFalse (TabTransfer::TryDecode (L"not json", received));
        Assert::IsFalse (TabTransfer::TryDecode (L"{\"location\":{}}", received));

        Assert::IsTrue  (at != std::wstring::npos);
        text.replace (at, wcslen (L"\"sortColumn\":"), L"\"sortColumn\":99,\"x\":");
        Assert::IsFalse (TabTransfer::TryDecode (text, received), L"A sort column this build does not have");
    }
};
