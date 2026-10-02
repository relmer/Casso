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
    };
}
