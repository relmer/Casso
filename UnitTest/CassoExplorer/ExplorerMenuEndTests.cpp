#include "Pch.h"

#include "ExplorerWindowPartsRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenuEndTests
//
//  The bottom of a context menu with Settings' Context menu page at its
//  defaults, as File Explorer has it: a separator and Customize menu, without Show
//  more options.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ExplorerMenuEndTests)
{
public:

    TEST_METHOD (AddMenuEnd_DefaultLayout_HasCustomizeMenuOnly)
    {
        ExplorerWindowPartsRig          rig;
        std::vector<DxuiPopupMenuItem>  items;
        bool                            ran = false;



        rig.Open();
        rig.parts->menus.AddMenuEnd (items, [&ran]() { ran = true; });

        Assert::AreEqual ((size_t) 2, items.size());
        Assert::IsTrue   (items[0].kind == DxuiPopupMenuItem::Kind::Separator);
        Assert::IsTrue   (items[1].command != nullptr && items[1].command->label == L"Customize menu");
        Assert::IsFalse  (ran);
    }
};
