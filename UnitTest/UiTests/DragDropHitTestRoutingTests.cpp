#include "Pch.h"

#include "Ui/DragDropTarget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





TEST_CLASS (DragDropHitTestRoutingTests)
{
public:

    TEST_METHOD (PickAtClient_Returns_Drive_Tags)
    {
        HitTester  hitTester;
        HitRect    first  = { RECT { 10, 10, 60, 60 }, HitSlot::Custom, 0 };
        HitRect    second = { RECT { 70, 10, 120, 60 }, HitSlot::Custom, 1 };



        hitTester.Register (first);
        hitTester.Register (second);

        Assert::AreEqual (0,  DragDropTarget::PickAtClient (hitTester, 20, 20));
        Assert::AreEqual (1,  DragDropTarget::PickAtClient (hitTester, 80, 20));
        Assert::AreEqual (-1, DragDropTarget::PickAtClient (hitTester, 200, 20));
    }
};
