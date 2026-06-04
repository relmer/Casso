#include "Pch.h"

#include "CppUnitTest.h"
using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace UiTests
{

TEST_CLASS (HitTesterTests)
{
public:

    TEST_METHOD (Pick_ReturnsLatestRegisteredOnOverlap)
    {
        DxuiHitTester  tester;
        DxuiHitRect    a = { { 0, 0, 100, 100 }, DxuiHitSlot::Client, 1 };
        DxuiHitRect    b = { { 10, 10, 50, 50 }, DxuiHitSlot::Custom, 2 };

        tester.Register (a);
        tester.Register (b);

        const DxuiHitRect *  hit = tester.Pick (20, 20);

        Assert::IsNotNull (hit);
        Assert::AreEqual (2, hit->tag);
    }


    TEST_METHOD (Pick_NullOutsideAnyRect)
    {
        DxuiHitTester  tester;

        tester.Register ({ { 0, 0, 10, 10 }, DxuiHitSlot::Client, 1 });

        Assert::IsNull (tester.Pick (50, 50));
    }


    TEST_METHOD (Pick_InclusiveLeftTop_ExclusiveRightBottom)
    {
        DxuiHitTester  tester;

        tester.Register ({ { 0, 0, 10, 10 }, DxuiHitSlot::Client, 1 });

        Assert::IsNotNull (tester.Pick (0, 0));
        Assert::IsNotNull (tester.Pick (9, 9));
        Assert::IsNull    (tester.Pick (10, 10));
    }


    TEST_METHOD (Clear_RemovesAllRegistrations)
    {
        DxuiHitTester  tester;

        tester.Register ({ { 0, 0, 10, 10 }, DxuiHitSlot::Client, 1 });
        tester.Clear();

        Assert::IsNull (tester.Pick (5, 5));
    }
};

}   // namespace UiTests
