#include "Pch.h"

#include "Core/IntRange.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  IntRangeTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (IntRangeTests)
{
public:

    TEST_METHOD (Limit_CapsAtTheCeilingAndKeepsTheFloor)
    {
        Assert::AreEqual (5,  IntRange::Limit (5, 10, 1),  L"a value inside the range is returned");
        Assert::AreEqual (10, IntRange::Limit (40, 10, 1), L"a value above the ceiling is capped");
        Assert::AreEqual (1,  IntRange::Limit (-3, 10, 1), L"a value below the floor is raised to it");
        Assert::AreEqual (1,  IntRange::Limit (5, 0, 1),   L"a ceiling below the floor returns the floor");
    }
};
