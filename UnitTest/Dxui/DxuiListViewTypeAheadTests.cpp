#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewTypeAheadTests
//
//  Typing at a list jumps to a row, as Explorer's does: characters typed
//  close together build a prefix, the same character again steps to the next
//  row starting with it, and a pause starts over.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewTypeAheadTests)
{
public:

    struct Fixture
    {
        DxuiListView  list;
        int64_t       now = 10000;

        Fixture()
        {
            DxuiDpiScaler                                 scaler;
            std::vector<std::vector<DxuiListView::Cell>>  rows;

            scaler.SetDpi (96);

            for (const wchar_t * name : { L"alpha", L"beta", L"cherry", L"Charlie", L"delta", L"Temp", L"Tools", L"tango" })
            {
                rows.push_back ({ DxuiListView::Cell { name, false } });
            }

            list.SetColumns           ({ DxuiListView::Column { L"Name", 200 } });
            list.SetMultiSelect       (true);
            list.SetKeyboardColumnNav (true);
            list.SetRows              (std::move (rows));
            list.Layout               (RECT { 0, 0, 400, 600 }, scaler);
            list.SetClock             ([this] () { return now; });
        }

        bool  Type (wchar_t ch, bool ctrl = false)
        {
            DxuiKeyEvent  ev;

            ev.kind = DxuiKeyEventKind::Char;
            ev.vk   = (WPARAM) ch;
            ev.ctrl = ctrl;

            return list.OnKey (ev);
        }

        void  Wait (int64_t ms)
        {
            now += ms;
        }
    };



    TEST_METHOD (ALetter_SelectsTheFirstRowStartingWithIt)
    {
        Fixture  f;

        Assert::IsTrue   (f.Type (L'c'));
        Assert::AreEqual (2, f.list.GetSelectedRow(), L"cherry");
    }



    TEST_METHOD (TheSameLetterAgain_StepsToTheNextMatch)
    {
        Fixture  f;

        f.Type (L'c');
        f.Type (L'c');

        Assert::AreEqual (3, f.list.GetSelectedRow(), L"Charlie, the next row starting with c");
    }



    TEST_METHOD (LettersTypedTogether_FindAPrefix)
    {
        Fixture  f;

        f.Type (L't');
        f.Type (L'o');

        Assert::AreEqual (6, f.list.GetSelectedRow(), L"Tools, not Temp or tango");
    }



    TEST_METHOD (MatchingIgnoresCase)
    {
        Fixture  f;

        f.Type (L'T');
        f.Type (L'A');

        Assert::AreEqual (7, f.list.GetSelectedRow(), L"tango, typed in capitals");
    }



    TEST_METHOD (APause_StartsANewSearch)
    {
        Fixture  f;

        f.Type (L't');
        f.Wait (1500);
        f.Type (L'o');

        //  Not "to": a fresh search for o, which nothing starts with.
        Assert::AreEqual (5, f.list.GetSelectedRow(), L"Still on Temp, from the first t");
    }



    TEST_METHOD (SteppingWrapsAroundTheEnd)
    {
        Fixture  f;

        f.Type (L't');   // Temp
        f.Type (L't');   // Tools
        f.Type (L't');   // tango
        f.Type (L't');   // back to Temp

        Assert::AreEqual (5, f.list.GetSelectedRow());
    }



    TEST_METHOD (NoMatch_LeavesTheSelectionAlone)
    {
        Fixture  f;

        f.Type (L'd');
        f.Wait (1500);
        f.Type (L'z');

        Assert::AreEqual (4, f.list.GetSelectedRow(), L"delta stays selected");
    }



    TEST_METHOD (ACtrlChord_IsLeftToTheHost)
    {
        Fixture  f;

        Assert::IsFalse (f.Type (L'c', true));
        Assert::AreEqual (-1, f.list.GetSelectedRow(), L"Nothing selected");
    }
};
