#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutDockBackTests
//
//  A pane that was alone in its group, auto-hidden and then docked again,
//  returns to its own place beside the panes it was split from, at the size
//  it had, rather than tabbed into a neighbor's group.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiPaneLayoutDockBackTests
{
    static const RECT  s_kArea = { 0, 0, 1000, 600 };



    //  code | regs, with console below code.
    static DxuiPaneLayout MakeSample()
    {
        DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");



        layout.Add        (L"regs",    L"");
        layout.Add        (L"console", L"");
        layout.DockToSide (L"console", L"code", DxuiDockSide::Bottom);
        return layout;
    }



    static RECT GetRectOf (const DxuiPaneLayout & layout, const std::wstring & pane)
    {
        RECT  found = {};



        for (const DxuiPaneLayout::GroupRect & group : layout.Arrange (s_kArea, nullptr, nullptr))
        {
            found = (group.active == pane) ? group.rect : found;
        }

        return found;
    }



    TEST_CLASS (DxuiPaneLayoutDockBackTests)
    {
    public:

        TEST_METHOD (LonePaneDocksBackToItsOwnPlace)
        {
            DxuiPaneLayout  layout = MakeSample();
            RECT            before = GetRectOf (layout, L"console");
            RECT            after  = {};



            Assert::IsTrue   (layout.AutoHide (L"console", DxuiDockSide::Bottom));
            Assert::IsTrue   (layout.DockBack (L"console"));

            after = GetRectOf (layout, L"console");

            Assert::AreEqual ((size_t) 1, layout.GetGroup (L"console").size(), L"alone again, not tabbed with code");
            Assert::AreEqual (before.left,   after.left);
            Assert::AreEqual (before.top,    after.top);
            Assert::AreEqual (before.right,  after.right);
            Assert::AreEqual (before.bottom, after.bottom);
        }


        TEST_METHOD (FirstSideAndRatioAreKept)
        {
            constexpr float  kRatio = 0.3f;
            DxuiPaneLayout   layout = MakeSample();
            RECT             before = {};
            RECT             after  = {};



            layout.DockToSide (L"regs", L"code", DxuiDockSide::Left);

            for (const DxuiPaneLayout::SplitRect & split : layout.ArrangeSplits (s_kArea, nullptr, nullptr))
            {
                Assert::IsTrue (layout.SetRatio (split.path, kRatio));
            }

            before = GetRectOf (layout, L"regs");

            Assert::IsTrue   (layout.AutoHide (L"regs", DxuiDockSide::Left));
            Assert::IsTrue   (layout.DockBack (L"regs"));

            after = GetRectOf (layout, L"regs");

            Assert::AreEqual ((size_t) 1, layout.GetGroup (L"regs").size());
            Assert::AreEqual (before.left,   after.left);
            Assert::AreEqual (before.top,    after.top);
            Assert::AreEqual (before.right,  after.right);
            Assert::AreEqual (before.bottom, after.bottom);
        }


        TEST_METHOD (TabbedPaneStillDocksBackIntoItsGroup)
        {
            DxuiPaneLayout  layout = MakeSample();



            layout.TabWith (L"regs", L"console");

            Assert::IsTrue   (layout.AutoHide (L"regs", DxuiDockSide::Right));
            Assert::IsTrue   (layout.DockBack (L"regs"));
            Assert::AreEqual ((size_t) 2, layout.GetGroup (L"console").size());
        }


        //  The first tab of three, auto-hidden and docked again, is the first
        //  tab again, and the active one.
        TEST_METHOD (TabbedPaneDocksBackToItsPlaceAmongTheTabs)
        {
            DxuiPaneLayout  layout = MakeSample();



            layout.TabWith   (L"code", L"console");
            layout.TabWith   (L"regs", L"console");
            layout.TabWithAt (L"code", L"console", 0);
            Assert::IsTrue   (layout.GetGroup (L"console") == std::vector<std::wstring> { L"code", L"console", L"regs" });

            Assert::IsTrue   (layout.AutoHide (L"code", DxuiDockSide::Left));
            Assert::IsTrue   (layout.DockBack (L"code"));

            Assert::IsTrue   (layout.GetGroup (L"console") == std::vector<std::wstring> { L"code", L"console", L"regs" }, L"first again, not last");
            Assert::AreEqual (0, layout.GetRoot()->active, L"and active");
        }


        //  When the group has fewer tabs than the saved index, the pane is
        //  inserted at the end.
        TEST_METHOD (APlacePastTheGroupsEndIsTheEnd)
        {
            DxuiPaneLayout  layout = MakeSample();



            layout.TabWith  (L"code", L"console");
            layout.TabWith  (L"regs", L"console");
            Assert::IsTrue  (layout.GetGroup (L"console") == std::vector<std::wstring> { L"console", L"code", L"regs" });

            Assert::IsTrue  (layout.AutoHide (L"regs", DxuiDockSide::Right));
            Assert::IsTrue  (layout.Close    (L"code"));
            Assert::IsTrue  (layout.DockBack (L"regs"));

            Assert::IsTrue  (layout.GetGroup (L"console") == std::vector<std::wstring> { L"console", L"regs" });
        }


        //  The index is saved with the layout and read back. A layout saved
        //  without it loads, and DockBack inserts the pane at the end.
        TEST_METHOD (ThePlaceSurvivesTheTextAndOlderTextStillLoads)
        {
            DxuiPaneLayout  layout   = MakeSample();
            DxuiPaneLayout  restored;
            DxuiPaneLayout  older;
            std::wstring    text;
            std::wstring    oldText;
            size_t          at       = 0;



            layout.TabWith   (L"code", L"console");
            layout.TabWith   (L"regs", L"console");
            layout.TabWithAt (L"code", L"console", 0);
            Assert::IsTrue   (layout.AutoHide (L"code", DxuiDockSide::Left));

            text = layout.ToText();
            at   = text.find (L" index 0");
            Assert::IsTrue   (at != std::wstring::npos, L"the hide line has the tab's index");

            Assert::IsTrue   (DxuiPaneLayout::TryParse (text, restored));
            Assert::AreEqual (0, restored.GetAutoHidden().front().homeIndex);
            Assert::IsTrue   (restored.DockBack (L"code"));
            Assert::IsTrue   (restored.GetGroup (L"console") == std::vector<std::wstring> { L"code", L"console", L"regs" });

            oldText = text;
            oldText.erase (at, wcslen (L" index 0"));

            Assert::IsTrue   (DxuiPaneLayout::TryParse (oldText, older), L"a layout saved without the index");
            Assert::AreEqual (-1, older.GetAutoHidden().front().homeIndex);
            Assert::IsTrue   (older.DockBack (L"code"));
            Assert::IsTrue   (older.GetGroup (L"console") == std::vector<std::wstring> { L"console", L"regs", L"code" });
        }


        TEST_METHOD (ANegativePlaceIsRefused)
        {
            DxuiPaneLayout  layout = MakeSample();
            DxuiPaneLayout  parsed;
            std::wstring    text;



            layout.TabWith (L"regs", L"console");
            Assert::IsTrue (layout.AutoHide (L"regs", DxuiDockSide::Right));

            text = layout.ToText();
            text.replace (text.find (L" index 1"), wcslen (L" index 1"), L" index -1");

            Assert::IsFalse (DxuiPaneLayout::TryParse (text, parsed));
        }
    };
}
