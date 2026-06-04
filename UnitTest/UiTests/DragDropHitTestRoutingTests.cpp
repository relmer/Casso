#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





TEST_CLASS (DragDropHitTestRoutingTests)
{
public:

    TEST_METHOD (PickAtClient_Returns_Drive_Tags)
    {
        DxuiHitTester  hitTester;
        DxuiHitRect    first  = { RECT { 10, 10, 60, 60 }, DxuiHitSlot::Custom, 0 };
        DxuiHitRect    second = { RECT { 70, 10, 120, 60 }, DxuiHitSlot::Custom, 1 };



        hitTester.Register (first);
        hitTester.Register (second);

        Assert::AreEqual (0,  DxuiDragDropTarget::PickAtClient (hitTester, 20, 20));
        Assert::AreEqual (1,  DxuiDragDropTarget::PickAtClient (hitTester, 80, 20));
        Assert::AreEqual (-1, DxuiDragDropTarget::PickAtClient (hitTester, 200, 20));
    }
};
