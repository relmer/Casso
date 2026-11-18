#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCalendarTests
//
//  The calendar's month grid, its month steps and what a point in it is on,
//  with no window.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiCalendarTests)
{
public:

    //  March 1988 began on a Tuesday, so its grid starts on Sunday 28
    //  February, a leap year's 29th follows, and the 1st is the third cell.
    TEST_METHOD (Grid_StartsOnTheSundayBeforeTheFirst)
    {
        auto  grid = DxuiCalendar::GetGrid (1988, 3);

        Assert::AreEqual (2,  grid[0].month);
        Assert::AreEqual (28, grid[0].day);
        Assert::AreEqual (29, grid[1].day, L"1988 is a leap year");
        Assert::AreEqual (3,  grid[2].month);
        Assert::AreEqual (1,  grid[2].day);
        Assert::AreEqual (29, DxuiCalendar::GetDaysInMonth (1988, 2));
        Assert::AreEqual (28, DxuiCalendar::GetDaysInMonth (1900, 2), L"1900 isn't a leap year");
    }



    TEST_METHOD (StepMonth_CrossesYears)
    {
        DxuiCalendar::Day  back = DxuiCalendar::StepMonth ({ 1988, 1, 1 }, -1);
        DxuiCalendar::Day  next = DxuiCalendar::StepMonth ({ 1988, 12, 1 }, 1);

        Assert::AreEqual (1987, back.year);
        Assert::AreEqual (12,   back.month);
        Assert::AreEqual (1989, next.year);
        Assert::AreEqual (1,    next.month);
    }



    TEST_METHOD (HitTest_FindsTheArrowsAndTheCells)
    {
        int  cell     = -1;
        int  gridTop  = DxuiCalendar::kPadDip + DxuiCalendar::kHeaderDip + DxuiCalendar::kWeekdayDip;

        Assert::IsTrue (DxuiCalendar::HitTest (DxuiCalendar::kPadDip + 4, DxuiCalendar::kPadDip + 4, cell) == DxuiCalendar::Hit::Previous);
        Assert::IsTrue (DxuiCalendar::HitTest (DxuiCalendar::kWidthDip - DxuiCalendar::kPadDip - 4, DxuiCalendar::kPadDip + 4, cell) == DxuiCalendar::Hit::Next);
        Assert::IsTrue (DxuiCalendar::HitTest (DxuiCalendar::kPadDip + DxuiCalendar::kCellDip * 2 + 4, gridTop + DxuiCalendar::kCellDip + 4, cell) == DxuiCalendar::Hit::Cell);
        Assert::AreEqual (9, cell, L"the second week's Tuesday");
        Assert::IsTrue (DxuiCalendar::HitTest (DxuiCalendar::kPadDip + 4, DxuiCalendar::kPadDip + DxuiCalendar::kHeaderDip + 4, cell) == DxuiCalendar::Hit::None,
                        L"the weekday letters are nothing");
    }
};
