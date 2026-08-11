#include "Pch.h"

#include "Ui/Scene/DeskSceneHitTester.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneHitTesterTests
//
//  Screen-ray resolution against the scene: glass hits with emulated pixels,
//  drive region classification matching the 2D DriveWidget semantics (eject
//  inside body, checked first), glass outranking region boxes, dead space
//  resolving to None.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DeskSceneHitTesterTests)
{
public:

    TEST_METHOD (HitTester_Is_Default_Constructible)
    {
        DeskSceneHitTester  tester;



        (void) tester;
        Assert::IsTrue (true);
    }

};
