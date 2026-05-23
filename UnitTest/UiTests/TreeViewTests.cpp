#include "Pch.h"

#include "Ui/Widgets/TreeView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TreeViewTests
//
//  Pure-logic coverage for hit-testing, keyboard navigation, and the
//  capability-flag driven checkbox behaviour. Rendering is not
//  exercised (Paint would require a GPU). The hardware-tree-shape
//  tests in HardwareTreeTests build on top of these primitives.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    TreeNode MakeNode (const wchar_t * label,
                       TreeCapabilityFlag flag,
                       bool checked,
                       const wchar_t * lockReason = L"")
    {
        TreeNode  n;
        n.label          = label;
        n.capabilityFlag = flag;
        n.checked        = checked;
        n.lockReason     = lockReason;
        return n;
    }


    TreeView  MakeFlatTree ()
    {
        TreeView                  tv;
        std::vector<TreeNode>     nodes;
        RECT                      rect = { 0, 0, 200, 220 };

        nodes.push_back (MakeNode (L"speaker",  TreeCapabilityFlag::Required,       true));
        nodes.push_back (MakeNode (L"joystick", TreeCapabilityFlag::Optional,       false));
        nodes.push_back (MakeNode (L"mb-rom",   TreeCapabilityFlag::PlatformLocked, true, L"Bus design relies on this ROM."));

        tv.SetRect (rect);
        tv.SetRowHeight (20);
        tv.SetNodes (std::move (nodes));
        return tv;
    }
}


TEST_CLASS (TreeViewTests)
{
public:

    TEST_METHOD (Flatten_VisibleCountMatchesNodeCount)
    {
        TreeView  tv = MakeFlatTree();
        Assert::AreEqual (3, tv.VisibleCount());
    }

    TEST_METHOD (IsInteractive_OnlyOptionalRowsInteractive)
    {
        TreeView  tv = MakeFlatTree();

        Assert::IsFalse (tv.IsInteractive (0));   // required
        Assert::IsTrue  (tv.IsInteractive (1));   // optional
        Assert::IsFalse (tv.IsInteractive (2));   // platform-locked
    }

    TEST_METHOD (HitTestRow_MapsYToRowIndex)
    {
        TreeView  tv = MakeFlatTree();

        Assert::AreEqual (0, tv.HitTestRow ( 10,  5));
        Assert::AreEqual (1, tv.HitTestRow ( 10, 25));
        Assert::AreEqual (2, tv.HitTestRow ( 10, 45));
        Assert::AreEqual (-1, tv.HitTestRow ( 10, 250));
    }

    TEST_METHOD (Click_OnOptionalCheckbox_Toggles)
    {
        TreeView  tv     = MakeFlatTree();
        int       x      = 20;   // depth=0, twisty 0..16, checkbox 16..32
        int       y      = 25;
        bool      checkedAfter = false;

        Assert::IsTrue (tv.OnLButtonDown (x, y));
        Assert::IsTrue (tv.OnLButtonUp   (x, y));

        checkedAfter = tv.NodeAt (1)->checked;
        Assert::IsTrue (checkedAfter,
            L"Optional row's checkbox click must toggle the underlying node.");
    }

    TEST_METHOD (Click_OnRequiredCheckbox_NoToggle)
    {
        TreeView  tv = MakeFlatTree();
        int       x  = 20;
        int       y  = 5;
        bool      beforeChecked = tv.NodeAt (0)->checked;

        Assert::IsTrue (tv.OnLButtonDown (x, y));
        Assert::IsTrue (tv.OnLButtonUp   (x, y));

        Assert::AreEqual (beforeChecked, tv.NodeAt (0)->checked,
            L"Required row click must not flip the checked state.");
    }

    TEST_METHOD (Click_OnPlatformLockedCheckbox_NoToggle)
    {
        TreeView  tv = MakeFlatTree();
        int       x  = 20;
        int       y  = 45;

        Assert::IsTrue (tv.OnLButtonDown (x, y));
        Assert::IsTrue (tv.OnLButtonUp   (x, y));

        Assert::IsTrue (tv.NodeAt (2)->checked,
            L"Platform-locked row must remain in its locked-checked state.");
    }

    TEST_METHOD (KeyboardNav_UpDownMovesHighlight)
    {
        TreeView  tv = MakeFlatTree();
        tv.SetFocused (true);

        Assert::IsTrue (tv.OnKey (VK_DOWN));
        Assert::AreEqual (1, tv.Highlight());

        Assert::IsTrue (tv.OnKey (VK_DOWN));
        Assert::AreEqual (2, tv.Highlight());

        Assert::IsTrue (tv.OnKey (VK_DOWN));
        Assert::AreEqual (2, tv.Highlight(),
            L"Down at last row must not wrap or overshoot.");

        Assert::IsTrue (tv.OnKey (VK_UP));
        Assert::AreEqual (1, tv.Highlight());
    }

    TEST_METHOD (KeyboardToggle_OnlyAffectsInteractiveRow)
    {
        TreeView  tv = MakeFlatTree();
        tv.SetFocused (true);

        Assert::IsTrue (tv.OnKey (VK_DOWN));   // highlight = 1 (optional)
        Assert::IsTrue (tv.OnKey (VK_SPACE));
        Assert::IsTrue (tv.NodeAt (1)->checked);

        Assert::IsTrue (tv.OnKey (VK_DOWN));   // highlight = 2 (platform-locked)
        Assert::IsTrue (tv.OnKey (VK_SPACE));
        Assert::IsTrue (tv.NodeAt (2)->checked,
            L"Space on a platform-locked row must leave the lock intact.");
    }

    TEST_METHOD (ParentChild_ExpandCollapsedNotShowChildren)
    {
        TreeView                  tv;
        std::vector<TreeNode>     nodes;
        TreeNode                  parent;
        TreeNode                  child;
        RECT                      rect = { 0, 0, 200, 200 };

        child.label  = L"sub";
        child.checked = true;
        parent.label = L"parent";
        parent.expanded = false;
        parent.children.push_back (child);
        nodes.push_back (parent);

        tv.SetRect (rect);
        tv.SetRowHeight (20);
        tv.SetNodes (std::move (nodes));

        Assert::AreEqual (1, tv.VisibleCount(),
            L"Collapsed parent must hide its child.");

        tv.SetFocused (true);
        Assert::IsTrue (tv.OnKey (VK_RIGHT));
        Assert::AreEqual (2, tv.VisibleCount(),
            L"Right-arrow on a collapsed parent must expand it.");

        Assert::IsTrue (tv.OnKey (VK_LEFT));
        Assert::AreEqual (1, tv.VisibleCount(),
            L"Left-arrow on an expanded parent must collapse it.");
    }
};
