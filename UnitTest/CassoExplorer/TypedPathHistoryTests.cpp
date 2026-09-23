#include "Pch.h"
#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/TypedPathHistory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TypedPathHistoryTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TypedPathHistoryTests)
{
public:

    static void AddMany (TypedPathHistory & history, int first, int last)
    {
        int  i = 0;

        for (i = first; i <= last; i++)
        {
            history.Add (L"C:\\Disks\\" + std::to_wstring (i));
        }
    }



    TEST_METHOD (NewestFirst)
    {
        TypedPathHistory  history;

        history.Add (L"C:\\Disks");
        history.Add (L"C:\\Games");

        Assert::AreEqual ((size_t) 2, history.GetEntries().size());
        Assert::AreEqual (std::wstring (L"C:\\Games"), history.GetEntries()[0]);
        Assert::AreEqual (std::wstring (L"C:\\Disks"), history.GetEntries()[1]);
    }



    TEST_METHOD (ARepeatMovesToTheTopWithoutADuplicate)
    {
        TypedPathHistory  history;

        history.Add (L"C:\\Disks");
        history.Add (L"C:\\Games");
        history.Add (L"C:\\Disks");

        Assert::AreEqual ((size_t) 2, history.GetEntries().size());
        Assert::AreEqual (std::wstring (L"C:\\Disks"), history.GetEntries()[0]);
        Assert::AreEqual (std::wstring (L"C:\\Games"), history.GetEntries()[1]);
    }



    TEST_METHOD (ARepeatIsMatchedWithoutRegardToCase)
    {
        TypedPathHistory  history;

        history.Add (L"C:\\Disks\\Merlin.po");
        history.Add (L"c:\\disks\\MERLIN.PO");

        //  One entry, and the text as it was last typed.
        Assert::AreEqual ((size_t) 1, history.GetEntries().size());
        Assert::AreEqual (std::wstring (L"c:\\disks\\MERLIN.PO"), history.GetEntries()[0]);
    }



    TEST_METHOD (TheOldestFallsOffPastTenEntries)
    {
        TypedPathHistory  history;

        AddMany (history, 1, 12);

        Assert::AreEqual ((size_t) TypedPathHistory::kMaxEntries, history.GetEntries().size());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\12"), history.GetEntries()[0]);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\3"),  history.GetEntries().back());
    }



    TEST_METHOD (BlankTextIsNotHeld)
    {
        TypedPathHistory  history;

        history.Add (L"");
        history.Add (L"   ");

        Assert::IsTrue (history.GetEntries().empty());
    }



    TEST_METHOD (ClearEmptiesTheList)
    {
        TypedPathHistory  history;

        history.Add (L"C:\\Disks");
        history.Clear();

        Assert::IsTrue (history.GetEntries().empty());
    }



    TEST_METHOD (ResetKeepsTheStoredOrderAndAppliesTheRules)
    {
        TypedPathHistory           history;
        std::vector<std::wstring>  stored;
        int                        i = 0;

        stored.push_back (L"C:\\Games");
        stored.push_back (L"C:\\Disks");
        stored.push_back (L"c:\\games");

        history.Reset (stored);

        //  Newest first is preserved, and the repeat collapses onto the copy
        //  nearest the top.
        Assert::AreEqual ((size_t) 2, history.GetEntries().size());
        Assert::AreEqual (std::wstring (L"C:\\Games"), history.GetEntries()[0]);
        Assert::AreEqual (std::wstring (L"C:\\Disks"), history.GetEntries()[1]);

        //  A hand-edited file holding more than the limit is trimmed.
        stored.clear();

        for (i = 0; i < 40; i++)
        {
            stored.push_back (L"C:\\Disks\\" + std::to_wstring (i));
        }

        history.Reset (stored);

        Assert::AreEqual ((size_t) TypedPathHistory::kMaxEntries, history.GetEntries().size());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\0"), history.GetEntries()[0]);
    }
};
