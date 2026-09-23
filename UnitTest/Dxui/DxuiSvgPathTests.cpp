#include "Pch.h"
#include "../EhmTestHelper.h"

#include "CppUnitTest.h"

#include "Render/DxuiSvgPath.h"



using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSvgPathTests
//
//  The parse a two-tone icon depends on. The sub-shapes must come out
//  separate and in order, since an icon's layers pick them by index; every
//  command must land where SVG says it does, relative or absolute; and the
//  number grammar's corners -- no separators, a second dot starting a new
//  number, arc flags run together -- must read the way a browser reads them,
//  because published icon paths use all of them.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiSvgPathTests)
{
public:

    static void AssertPoint (const DxuiSvgPoint & p, float x, float y, const wchar_t * what)
    {
        Assert::AreEqual (x, p.x, 0.0001f, what);
        Assert::AreEqual (y, p.y, 0.0001f, what);
    }

    TEST_METHOD (EachMovetoStartsASubshape)
    {
        std::vector<DxuiSvgSubpath>  subs;

        Assert::IsTrue (DxuiSvgPath::Parse (L"M1 1L2 2Z M5 5L6 6Z M9 9L10 10Z", subs));
        Assert::AreEqual ((size_t) 3, subs.size());
        AssertPoint (subs[0].start, 1, 1, L"first");
        AssertPoint (subs[1].start, 5, 5, L"second");
        AssertPoint (subs[2].start, 9, 9, L"third");
        Assert::IsTrue (subs[0].closed && subs[1].closed && subs[2].closed, L"each is closed");
    }


    TEST_METHOD (RelativeCommandsMoveFromTheCurrentPoint)
    {
        std::vector<DxuiSvgSubpath>  subs;

        Assert::IsTrue (DxuiSvgPath::Parse (L"m10 10 l5 0 h-2 v3 z", subs));
        Assert::AreEqual ((size_t) 1, subs.size());
        Assert::AreEqual ((size_t) 3, subs[0].segments.size());
        AssertPoint (subs[0].segments[0].to, 15, 10, L"l is relative");
        AssertPoint (subs[0].segments[1].to, 13, 10, L"h is relative");
        AssertPoint (subs[0].segments[2].to, 13, 13, L"v is relative");
    }


    TEST_METHOD (CoordinatesAfterAMovetoAreLines)
    {
        std::vector<DxuiSvgSubpath>  subs;

        Assert::IsTrue (DxuiSvgPath::Parse (L"M0 0 4 0 4 4", subs));
        Assert::AreEqual ((size_t) 1, subs.size());
        Assert::AreEqual ((size_t) 2, subs[0].segments.size(), L"two implicit lines");
        AssertPoint (subs[0].segments[1].to, 4, 4, L"the second");
    }


    TEST_METHOD (NumbersNeedNoSeparators)
    {
        std::vector<DxuiSvgSubpath>  subs;

        //  "0.5.5" is two numbers, and "-" starts a new one.
        Assert::IsTrue (DxuiSvgPath::Parse (L"M0.5.5L1-1", subs));
        AssertPoint (subs[0].start, 0.5f, 0.5f, L"a second dot starts a number");
        AssertPoint (subs[0].segments[0].to, 1, -1, L"a minus starts a number");
    }


    TEST_METHOD (SmoothCubicReflectsTheLastControlPoint)
    {
        std::vector<DxuiSvgSubpath>  subs;

        Assert::IsTrue (DxuiSvgPath::Parse (L"M0 0C0 10 10 10 10 0S20 -10 20 0", subs));
        Assert::AreEqual ((size_t) 2, subs[0].segments.size());
        AssertPoint (subs[0].segments[1].c1, 10, -10, L"the reflection of (10,10) about (10,0)");
        AssertPoint (subs[0].segments[1].to, 20, 0, L"where it ends");
    }


    TEST_METHOD (QuadraticBecomesTheCubicItEquals)
    {
        std::vector<DxuiSvgSubpath>  subs;

        Assert::IsTrue (DxuiSvgPath::Parse (L"M0 0Q3 6 6 0", subs));
        Assert::IsTrue (subs[0].segments[0].kind == DxuiSvgSegment::Kind::Cubic);
        AssertPoint (subs[0].segments[0].c1, 2, 4, L"two thirds toward the control from the start");
        AssertPoint (subs[0].segments[0].c2, 4, 4, L"two thirds toward the control from the end");
    }


    TEST_METHOD (ArcFlagsMayRunTogether)
    {
        std::vector<DxuiSvgSubpath>  subs;

        Assert::IsTrue (DxuiSvgPath::Parse (L"M0 0a5 5 0 105 5", subs));
        Assert::IsTrue (subs[0].segments[0].kind == DxuiSvgSegment::Kind::Arc);
        Assert::IsTrue (subs[0].segments[0].largeArc,  L"large-arc 1");
        Assert::IsFalse (subs[0].segments[0].clockwise, L"sweep 0");
        AssertPoint (subs[0].segments[0].to, 5, 5, L"relative end point");
    }


    TEST_METHOD (FluentAddCircle_IsAPlusAndARing)
    {
        //  The published 20-pixel add_circle: the plus first, then the ring's
        //  outer and inner edges, which the icon's layers rely on.
        const wchar_t * d = L"M6 10C6 9.72386 6.22386 9.5 6.5 9.5H9.5V6.5C9.5 6.22386 9.72386 6 10 6C10.2761 6 10.5 6.22386 10.5 6.5V9.5H13.5"
                            L"C13.7761 9.5 14 9.72386 14 10C14 10.2761 13.7761 10.5 13.5 10.5H10.5V13.5C10.5 13.7761 10.2761 14 10 14"
                            L"C9.72386 14 9.5 13.7761 9.5 13.5V10.5H6.5C6.22386 10.5 6 10.2761 6 10Z"
                            L"M10 18C14.4183 18 18 14.4183 18 10C18 5.58172 14.4183 2 10 2C5.58172 2 2 5.58172 2 10C2 14.4183 5.58172 18 10 18Z"
                            L"M10 17C6.13401 17 3 13.866 3 10C3 6.13401 6.13401 3 10 3C13.866 3 17 6.13401 17 10C17 13.866 13.866 17 10 17Z";
        std::vector<DxuiSvgSubpath>  subs;

        Assert::IsTrue   (DxuiSvgPath::Parse (d, subs));
        Assert::AreEqual ((size_t) 3, subs.size());
        AssertPoint      (subs[0].start, 6, 10,  L"the plus");
        AssertPoint      (subs[1].start, 10, 18, L"the ring's outer edge");
        AssertPoint      (subs[2].start, 10, 17, L"the ring's inner edge");
    }


    TEST_METHOD (Malformed_ReturnsFalse)
    {
        std::vector<DxuiSvgSubpath>  subs;

        Assert::IsFalse (DxuiSvgPath::Parse (L"M1", subs),    L"a moveto missing its y");
        Assert::IsFalse (DxuiSvgPath::Parse (L"1 2", subs),   L"numbers before any command");
        Assert::IsFalse (DxuiSvgPath::Parse (nullptr, subs),  L"no path at all");
    }
};
