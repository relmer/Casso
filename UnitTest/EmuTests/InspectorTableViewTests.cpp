#include "Pch.h"

#include "Ui/DiskInspector/InspectorTableView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTableViewTests
//
//  A table placed away from the window's corner takes the presses over its
//  own rows and no others. Its list hit-tests from its own corner, so a
//  press passed on in window coordinates used to select nothing on the
//  table and swallow presses near the window's corner instead, where the
//  drive tabs and the zoom buttons are.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorTableViewTests)
{
public:

    static constexpr RECT  kBounds = { 20, 600, 420, 900 };

    static DxuiMouseEvent MakePress (int x, int y)
    {
        DxuiMouseEvent  ev;



        ev.kind        = DxuiMouseEventKind::Down;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = { x, y };

        return ev;
    }



    static void Setup (InspectorTableView & view)
    {
        vector<TableRow>  rows (20);
        DxuiDpiScaler     scaler;



        for (int i = 0; i < static_cast<int> (rows.size()); i++)
        {
            rows[i].quarterTrack = i;
            rows[i].cells        = { std::to_wstring (i), L"Bits" };
        }

        view.SetColumns ({ L"Track", L"Recorded" });
        view.SetRows    (std::move (rows));
        view.Layout     (kBounds, scaler);
    }



    TEST_METHOD (APressNearTheWindowsCornerIsNotTheTables)
    {
        InspectorViewContext  context;
        InspectorTableView    view (context);
        int                   selected = -1;



        Setup (view);
        view.SetOnSelect ([&selected] (const TableRow & row) { selected = row.quarterTrack; });

        Assert::IsFalse (view.OnMouse (MakePress (50, 40)), L"over the drive tabs");
        Assert::AreEqual (-1, selected);
    }



    TEST_METHOD (APressOnARowSelectsIt)
    {
        InspectorViewContext  context;
        InspectorTableView    view (context);
        int                   selected = -1;



        Setup (view);
        view.SetOnSelect ([&selected] (const TableRow & row) { selected = row.quarterTrack; });

        Assert::IsTrue (view.OnMouse (MakePress (kBounds.left + 30, kBounds.bottom - 20)));
        Assert::IsTrue (selected >= 0, L"a row under the press is selected");
    }
};
