#include "Pch.h"

#include "Widgets/DxuiTreeView.h"

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

    DxuiTreeView  MakeTallTree (int rows, int heightPx)
    {
        DxuiTreeView               tv;
        std::vector<DxuiTreeNode>  nodes;
        int                        i = 0;

        for (i = 0; i < rows; i++)
        {
            nodes.push_back (MakeNode (L"folder", DxuiTreeCapabilityFlag::Optional, false));
        }

        tv.SetRect (RECT { 0, 0, 200, heightPx });
        tv.SetRowHeight (20);
        tv.SetNodes (std::move (nodes));
        return tv;
    }


    TEST_METHOD (Scroll_TakesTheRowsPastTheFoldWithinReach)
    {
        //  Ten rows of 20 in a 100-high tree: five show, five are below it.
        DxuiTreeView  tv = MakeTallTree (10, 100);

        Assert::AreEqual (5, tv.GetRowCap());
        Assert::AreEqual (5, tv.GetMaxTopRow());
        Assert::IsTrue   (tv.IsScrollbarVisible());

        tv.ScrollRows (3);
        Assert::AreEqual (3, tv.GetTopRow());

        //  The rows move under the pointer with the view.
        Assert::AreEqual (3, tv.HitTestRow (10, 5));
        Assert::AreEqual (7, tv.HitTestRow (10, 85));

        //  Neither end runs past its rows.
        tv.ScrollRows (99);
        Assert::AreEqual (5, tv.GetTopRow());
        tv.ScrollRows (-99);
        Assert::AreEqual (0, tv.GetTopRow());
    }


    TEST_METHOD (Scroll_IsNotOfferedWhenEveryRowFits)
    {
        DxuiTreeView  tv = MakeTallTree (3, 100);

        Assert::IsFalse (tv.IsScrollbarVisible());
        Assert::AreEqual (0, tv.GetMaxTopRow());

        tv.ScrollRows (2);
        Assert::AreEqual (0, tv.GetTopRow());
    }


    TEST_METHOD (EnsureRowVisible_ScrollsTheLeastThatShowsTheRow)
    {
        DxuiTreeView  tv = MakeTallTree (10, 100);

        tv.EnsureRowVisible (7);
        Assert::AreEqual (3, tv.GetTopRow());   // 7 becomes the last row shown

        tv.EnsureRowVisible (2);
        Assert::AreEqual (2, tv.GetTopRow());   // and now the first

        //  A row already in view moves nothing.
        tv.EnsureRowVisible (4);
        Assert::AreEqual (2, tv.GetTopRow());
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





////////////////////////////////////////////////////////////////////////////////
//
//  TreeViewNavigationTests
//
//  The navigation-tree additions: ids, children fetched on first expand,
//  hidden checkboxes, selection reports and arrow keys that walk to a parent.
//  The checklist tests above run unchanged against the same control.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TreeViewNavigationTests)
{
public:

    static DxuiTreeNode MakeLazy (const wchar_t * id, const wchar_t * label)
    {
        DxuiTreeNode  n;

        n.id             = id;
        n.label          = label;
        n.expanded       = false;
        n.childrenLoaded = false;

        return n;
    }

    static DxuiTreeView MakeNavigationTree (int & outFetches)
    {
        DxuiTreeView               tv;
        std::vector<DxuiTreeNode>  roots;
        RECT                       rect = { 0, 0, 300, 400 };

        roots.push_back (MakeLazy (L"casso:", L"Casso"));
        roots.push_back (MakeLazy (L"pc:",    L"This PC"));

        tv.SetRect           (rect);
        tv.SetRowHeight      (20);
        tv.SetShowCheckboxes (false);
        tv.SetNodes          (std::move (roots));
        tv.SetFocused        (true);

        outFetches = 0;

        return tv;
    }

    TEST_METHOD (LazyChildren_AreFetchedOnceOnFirstExpand)
    {
        int           fetches = 0;
        DxuiTreeView  tv      = MakeNavigationTree (fetches);
        std::wstring  askedFor;

        tv.SetChildProvider ([&fetches, &askedFor] (const std::wstring & id)
        {
            std::vector<DxuiTreeNode>  children;
            DxuiTreeNode               folder;

            fetches++;
            askedFor = id;

            folder.id       = id + L"C:\\Disks";
            folder.label    = L"Disks";
            folder.expanded = false;
            children.push_back (folder);

            return children;
        });

        Assert::AreEqual (2, tv.GetVisibleCount());
        Assert::IsTrue   (DxuiTreeView::CanExpand (*tv.GetNodeAt (0)), L"an unasked row shows a twisty");

        Assert::IsTrue   (tv.OnKey (VK_RIGHT));
        Assert::AreEqual (1, fetches);
        Assert::AreEqual (std::wstring (L"casso:"), askedFor);
        Assert::AreEqual (3, tv.GetVisibleCount());

        Assert::IsTrue   (tv.OnKey (VK_LEFT));
        Assert::IsTrue   (tv.OnKey (VK_RIGHT));
        Assert::AreEqual (1, fetches, L"a second expand uses the children already fetched");
        Assert::AreEqual (std::wstring (L"casso:C:\\Disks"), tv.GetNodeAt (1)->id);
    }

    TEST_METHOD (LazyChildren_AnEmptyAnswerLeavesNoTwisty)
    {
        int           fetches = 0;
        DxuiTreeView  tv      = MakeNavigationTree (fetches);

        tv.SetChildProvider ([&fetches] (const std::wstring &) { fetches++; return std::vector<DxuiTreeNode>(); });

        Assert::IsTrue   (tv.OnKey (VK_RIGHT));
        Assert::AreEqual (2, tv.GetVisibleCount());
        Assert::IsFalse  (DxuiTreeView::CanExpand (*tv.GetNodeAt (0)));

        Assert::IsTrue   (tv.OnKey (VK_RIGHT));
        Assert::AreEqual (1, fetches, L"a row known to be empty is not asked again");
    }

    TEST_METHOD (Selection_IsReportedByIdOnArrowsAndClicks)
    {
        int                        fetches = 0;
        DxuiTreeView               tv      = MakeNavigationTree (fetches);
        std::vector<std::wstring>  selected;

        tv.SetOnSelect ([&selected] (const std::wstring & id) { selected.push_back (id); });

        Assert::IsTrue   (tv.OnKey (VK_DOWN));
        Assert::AreEqual (std::wstring (L"pc:"), selected.back());
        Assert::AreEqual (std::wstring (L"pc:"), tv.GetHighlightedId());

        //  A click on the label of the first row.
        Assert::IsTrue   (tv.OnLButtonDown (100, 5));
        Assert::IsTrue   (tv.OnLButtonUp   (100, 5));
        Assert::AreEqual (std::wstring (L"casso:"), selected.back());
        Assert::AreEqual (0, tv.FindRowById (L"casso:"));
        Assert::AreEqual (-1, tv.FindRowById (L"nowhere"));
    }

    TEST_METHOD (HiddenCheckboxes_AreNotHitAndNeverToggle)
    {
        int           fetches = 0;
        DxuiTreeView  tv      = MakeNavigationTree (fetches);
        int           toggles = 0;

        tv.SetOnToggle ([&toggles] (const std::wstring &, bool) { toggles++; });

        //  Where the checkbox would be on a checklist tree.
        Assert::IsFalse (tv.HitTestCheckbox (20, 5, 0));

        Assert::IsTrue  (tv.OnLButtonDown (20, 5));
        Assert::IsTrue  (tv.OnLButtonUp   (20, 5));
        Assert::IsTrue  (tv.OnKey (VK_SPACE));
        Assert::AreEqual (0, toggles);
        Assert::IsFalse (tv.GetNodeAt (0)->checked);
    }

    TEST_METHOD (LeftArrow_OnACollapsedChildMovesToItsParent)
    {
        int                        fetches = 0;
        DxuiTreeView               tv      = MakeNavigationTree (fetches);
        std::vector<std::wstring>  expanded;

        tv.SetChildProvider ([] (const std::wstring & id)
        {
            DxuiTreeNode  leaf;

            leaf.id    = id + L"leaf";
            leaf.label = L"leaf";

            return std::vector<DxuiTreeNode> { leaf };
        });

        tv.SetOnExpand ([&expanded] (const std::wstring & id, bool isOpen) { if (isOpen) { expanded.push_back (id); } });

        Assert::IsTrue   (tv.OnKey (VK_DOWN));     // This PC
        Assert::IsTrue   (tv.OnKey (VK_RIGHT));    // expand
        Assert::AreEqual (std::wstring (L"pc:"), expanded.back());
        Assert::IsTrue   (tv.OnKey (VK_DOWN));     // the leaf
        Assert::AreEqual (std::wstring (L"pc:leaf"), tv.GetHighlightedId());

        Assert::IsTrue   (tv.OnKey (VK_LEFT));
        Assert::AreEqual (std::wstring (L"pc:"), tv.GetHighlightedId(), L"Left on a leaf walks to its parent");
        Assert::AreEqual (3, tv.GetVisibleCount(), L"and collapses nothing");

        Assert::IsTrue   (tv.OnKey (VK_LEFT));
        Assert::AreEqual (2, tv.GetVisibleCount(), L"Left on an open parent collapses it");
    }

    TEST_METHOD (Ids_FindNodesThatAreNotVisible)
    {
        int           fetches = 0;
        DxuiTreeView  tv      = MakeNavigationTree (fetches);

        tv.SetChildProvider ([] (const std::wstring & id)
        {
            DxuiTreeNode  child;

            child.id    = id + L"child";
            child.label = L"child";

            return std::vector<DxuiTreeNode> { child };
        });

        Assert::IsTrue  (tv.SetRowExpanded (0, true));
        Assert::IsTrue  (tv.SetRowExpanded (0, false));
        Assert::IsFalse (tv.SetRowExpanded (0, false), L"already closed");

        Assert::IsNotNull (tv.FindNodeById (L"casso:child"), L"a collapsed child is still in the tree");
        Assert::AreEqual  (-1, tv.FindRowById (L"casso:child"));
    }
};
