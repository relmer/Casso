#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TreeViewTests
//
//  Pure-logic coverage for hit-testing, keyboard navigation, and the
//  capability-flag driven checkbox behavior. Rendering is not
//  exercised (Paint would require a GPU). The hardware-tree-shape
//  tests in HardwareTreeTests build on top of these primitives.
//
////////////////////////////////////////////////////////////////////////////////



TEST_CLASS (TreeViewTests)
{
public:

    DxuiTreeNode MakeNode (const wchar_t * label,
                       DxuiTreeCapabilityFlag flag,
                       bool checked,
                       const wchar_t * lockReason = L"")
    {
        DxuiTreeNode  n;
        n.label          = label;
        n.capabilityFlag = flag;
        n.checked        = checked;
        n.lockReason     = lockReason;
        return n;
    }


    DxuiTreeView  MakeFlatTree()
    {
        DxuiTreeView               tv;
        std::vector<DxuiTreeNode>  nodes;
        RECT                       rect  = { 0, 0, 200, 220 };

        nodes.push_back (MakeNode (L"speaker",  DxuiTreeCapabilityFlag::Required,       true));
        nodes.push_back (MakeNode (L"joystick", DxuiTreeCapabilityFlag::Optional,       false));
        nodes.push_back (MakeNode (L"mb-rom",   DxuiTreeCapabilityFlag::PlatformLocked, true, L"Bus design relies on this ROM."));

        tv.SetRect (rect);
        tv.SetRowHeight (20);
        tv.SetNodes (std::move (nodes));
        return tv;
    }

    TEST_METHOD (Flatten_VisibleCountMatchesNodeCount)
    {
        DxuiTreeView  tv = MakeFlatTree();
        Assert::AreEqual (3, tv.GetVisibleCount());
    }

    TEST_METHOD (IsInteractive_OnlyOptionalRowsInteractive)
    {
        DxuiTreeView  tv = MakeFlatTree();

        Assert::IsFalse (tv.IsInteractive (0));   // required
        Assert::IsTrue  (tv.IsInteractive (1));   // optional
        Assert::IsFalse (tv.IsInteractive (2));   // platform-locked
    }

    TEST_METHOD (HitTestRow_MapsYToRowIndex)
    {
        DxuiTreeView  tv = MakeFlatTree();

        Assert::AreEqual (0, tv.HitTestRow ( 10,  5));
        Assert::AreEqual (1, tv.HitTestRow ( 10, 25));
        Assert::AreEqual (2, tv.HitTestRow ( 10, 45));
        Assert::AreEqual (-1, tv.HitTestRow ( 10, 250));
    }

    TEST_METHOD (Click_OnOptionalCheckbox_Toggles)
    {
        DxuiTreeView  tv           = MakeFlatTree();
        int           x            = 20;   // depth=0, twisty 0..16, checkbox 16..32
        int           y            = 25;
        bool          checkedAfter = false;

        Assert::IsTrue (tv.OnLButtonDown (x, y));
        Assert::IsTrue (tv.OnLButtonUp   (x, y));

        checkedAfter = tv.GetNodeAt (1)->checked;
        Assert::IsTrue (checkedAfter,
            L"Optional row's checkbox click must toggle the underlying node.");
    }

    //  The label toggles the row as the checkbox does, once the tree has
    //  been painted and the label's extent is known. The mock renderer
    //  measures 7 DIP per character, so "joystick" ends 56 past its start,
    //  which is 36: twisty 0..16, checkbox 16..32, then a 4 DIP gap.
    TEST_METHOD (Click_OnOptionalLabel_Toggles)
    {
        DxuiTreeView          tv = MakeFlatTree();
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;



        tv.Paint (painter, text, theme);

        Assert::IsTrue (tv.OnLButtonDown (60, 25));
        Assert::IsTrue (tv.OnLButtonUp   (60, 25));
        Assert::IsTrue (tv.GetNodeAt (1)->checked, L"a click on the label toggles the row");

        Assert::IsTrue  (tv.OnLButtonDown (150, 25));
        Assert::IsTrue  (tv.OnLButtonUp   (150, 25));
        Assert::IsTrue  (tv.GetNodeAt (1)->checked, L"past the label's end a click only selects");
        Assert::AreEqual (1, tv.GetHighlight());
    }

    TEST_METHOD (Click_OnRequiredLabel_NoToggle)
    {
        DxuiTreeView          tv = MakeFlatTree();
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;



        tv.Paint (painter, text, theme);

        Assert::IsTrue (tv.OnLButtonDown (50, 5));
        Assert::IsTrue (tv.OnLButtonUp   (50, 5));
        Assert::IsTrue (tv.GetNodeAt (0)->checked, L"a Required row stays on");
    }

    TEST_METHOD (Click_OnRequiredCheckbox_NoToggle)
    {
        DxuiTreeView  tv            = MakeFlatTree();
        int           x             = 20;
        int           y             = 5;
        bool          beforeChecked = tv.GetNodeAt (0)->checked;

        Assert::IsTrue (tv.OnLButtonDown (x, y));
        Assert::IsTrue (tv.OnLButtonUp   (x, y));

        Assert::AreEqual (beforeChecked, tv.GetNodeAt (0)->checked,
            L"Required row click must not flip the checked state.");
    }

    TEST_METHOD (Click_OnPlatformLockedCheckbox_NoToggle)
    {
        DxuiTreeView  tv = MakeFlatTree();
        int           x  = 20;
        int           y  = 45;

        Assert::IsTrue (tv.OnLButtonDown (x, y));
        Assert::IsTrue (tv.OnLButtonUp   (x, y));

        Assert::IsTrue (tv.GetNodeAt (2)->checked,
            L"Platform-locked row must remain in its locked-checked state.");
    }

    TEST_METHOD (KeyboardNav_UpDownMovesHighlight)
    {
        DxuiTreeView  tv = MakeFlatTree();
        tv.SetFocused (true);

        Assert::IsTrue (tv.OnKey (VK_DOWN));
        Assert::AreEqual (1, tv.GetHighlight());

        Assert::IsTrue (tv.OnKey (VK_DOWN));
        Assert::AreEqual (2, tv.GetHighlight());

        Assert::IsTrue (tv.OnKey (VK_DOWN));
        Assert::AreEqual (2, tv.GetHighlight(),
            L"Down at last row must not wrap or overshoot.");

        Assert::IsTrue (tv.OnKey (VK_UP));
        Assert::AreEqual (1, tv.GetHighlight());
    }

    TEST_METHOD (KeyboardToggle_OnlyAffectsInteractiveRow)
    {
        DxuiTreeView  tv = MakeFlatTree();
        tv.SetFocused (true);

        Assert::IsTrue (tv.OnKey (VK_DOWN));   // highlight = 1 (optional)
        Assert::IsTrue (tv.OnKey (VK_SPACE));
        Assert::IsTrue (tv.GetNodeAt (1)->checked);

        Assert::IsTrue (tv.OnKey (VK_DOWN));   // highlight = 2 (platform-locked)
        Assert::IsTrue (tv.OnKey (VK_SPACE));
        Assert::IsTrue (tv.GetNodeAt (2)->checked,
            L"Space on a platform-locked row must leave the lock intact.");
    }

    TEST_METHOD (ParentChild_ExpandCollapsedNotShowChildren)
    {
        DxuiTreeView               tv;
        std::vector<DxuiTreeNode>  nodes;
        DxuiTreeNode               parent;
        DxuiTreeNode               child;
        RECT                       rect   = { 0, 0, 200, 200 };

        child.label  = L"sub";
        child.checked = true;
        parent.label = L"parent";
        parent.expanded = false;
        parent.children.push_back (child);
        nodes.push_back (parent);

        tv.SetRect (rect);
        tv.SetRowHeight (20);
        tv.SetNodes (std::move (nodes));

        Assert::AreEqual (1, tv.GetVisibleCount(),
            L"Collapsed parent must hide its child.");

        tv.SetFocused (true);
        Assert::IsTrue (tv.OnKey (VK_RIGHT));
        Assert::AreEqual (2, tv.GetVisibleCount(),
            L"Right-arrow on a collapsed parent must expand it.");

        Assert::IsTrue (tv.OnKey (VK_LEFT));
        Assert::AreEqual (1, tv.GetVisibleCount(),
            L"Left-arrow on an expanded parent must collapse it.");
    }
};

