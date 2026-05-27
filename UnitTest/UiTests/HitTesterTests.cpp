#include "Pch.h"

#include "CppUnitTest.h"

#include "../Casso/Ui/HitTester.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace UiTests
{

TEST_CLASS (HitTesterTests)
{
public:

    TEST_METHOD (Pick_ReturnsLatestRegisteredOnOverlap)
    {
        HitTester  tester;
        HitRect    a = { { 0, 0, 100, 100 }, HitSlot::Client, 1 };
        HitRect    b = { { 10, 10, 50, 50 }, HitSlot::Custom, 2 };

        tester.Register (a);
        tester.Register (b);

        const HitRect *  hit = tester.Pick (20, 20);

        Assert::IsNotNull (hit);
        Assert::AreEqual (2, hit->tag);
    }


    TEST_METHOD (Pick_NullOutsideAnyRect)
    {
        HitTester  tester;

        tester.Register ({ { 0, 0, 10, 10 }, HitSlot::Client, 1 });

        Assert::IsNull (tester.Pick (50, 50));
    }


    TEST_METHOD (Pick_InclusiveLeftTop_ExclusiveRightBottom)
    {
        HitTester  tester;

        tester.Register ({ { 0, 0, 10, 10 }, HitSlot::Client, 1 });

        Assert::IsNotNull (tester.Pick (0, 0));
        Assert::IsNotNull (tester.Pick (9, 9));
        Assert::IsNull    (tester.Pick (10, 10));
    }


    TEST_METHOD (Clear_RemovesAllRegistrations)
    {
        HitTester  tester;

        tester.Register ({ { 0, 0, 10, 10 }, HitSlot::Client, 1 });
        tester.Clear();

        Assert::IsNull (tester.Pick (5, 5));
    }
};

}   // namespace UiTests
