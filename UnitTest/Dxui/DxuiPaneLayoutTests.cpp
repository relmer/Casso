#include "Pch.h"

#include "Core/DxuiPaneLayout.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutTests
//
//  The docking model with no controls (FR-038 to FR-044, R-028): every
//  operation a user can make, the area each group gets, the load-time
//  repairs, and the text form's round trip.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiPaneLayoutTests
{
    static const RECT  s_kArea = { 0, 0, 1000, 600 };



    //  code | regs, with console below code: the debugger's shape in small.
    static DxuiPaneLayout MakeSample()
    {
        DxuiPaneLayout  layout = DxuiPaneLayout::MakeSingle (L"code");



        layout.Add        (L"regs",    L"");
        layout.Add        (L"console", L"");
        layout.DockToSide (L"console", L"code", DxuiDockSide::Bottom);
        return layout;
    }



    static const DxuiPaneLayout::GroupRect & FindRect (const std::vector<DxuiPaneLayout::GroupRect> & groups, const wchar_t * pane)
    {
        for (const DxuiPaneLayout::GroupRect & group : groups)
        {
            if (std::find (group.panes.begin(), group.panes.end(), pane) != group.panes.end())
            {
                return group;
            }
        }

        Assert::Fail ((std::wstring (L"no group for ") + pane).c_str());
        return groups.front();
    }



    TEST_CLASS (OperationTests)
    {
    public:

        TEST_METHOD (DockToSideSplitsTheTargetsGroup)
        {
            DxuiPaneLayout                            layout = MakeSample();
            std::vector<DxuiPaneLayout::GroupRect>    groups = layout.Arrange (s_kArea, nullptr, nullptr);



            Assert::AreEqual ((size_t) 3, groups.size());
            Assert::IsTrue   (FindRect (groups, L"console").rect.top >= FindRect (groups, L"code").rect.bottom, L"console below code");
            Assert::IsTrue   (FindRect (groups, L"regs").rect.left  >= FindRect (groups, L"code").rect.right,  L"regs right of both");
        }


        TEST_METHOD (TabWithJoinsTheGroupAndActivatesThePane)
        {
            DxuiPaneLayout  layout = MakeSample();



            Assert::IsTrue   (layout.TabWith (L"regs", L"console"));
            Assert::AreEqual ((size_t) 2, layout.GetGroup (L"console").size());
            Assert::AreEqual ((size_t) 2, layout.Arrange (s_kArea, nullptr, nullptr).size(), L"regs' group closed");
            Assert::AreEqual (std::wstring (L"regs"), FindRect (layout.Arrange (s_kArea, nullptr, nullptr), L"regs").active);
        }


        TEST_METHOD (ADropOntoItsOwnPlaceIsANoOp)
        {
            DxuiPaneLayout  layout = MakeSample();
            std::wstring    before = layout.ToText();



            Assert::IsFalse  (layout.DockToSide (L"code", L"code", DxuiDockSide::Left));
            Assert::IsFalse  (layout.TabWith (L"code", L"code"));
            layout.TabWith   (L"regs", L"console");
            before = layout.ToText();
            Assert::IsFalse  (layout.TabWith (L"regs", L"console"), L"already in that group");
            Assert::AreEqual (before, layout.ToText());
        }


        TEST_METHOD (FloatAndDockBackReturnsItBesideItsNeighbor)
        {
            DxuiPaneLayout  layout = MakeSample();



            layout.TabWith (L"regs", L"console");

            Assert::IsTrue   (layout.Float (L"regs", L"mon2", RECT { 10, 20, 310, 220 }));
            Assert::IsTrue   (layout.IsFloating (L"regs"));
            Assert::IsFalse  (layout.IsDocked (L"regs"));
            Assert::IsTrue   (layout.DockBack (L"regs"));
            Assert::AreEqual ((size_t) 2, layout.GetGroup (L"console").size(), L"back in the group it left");
        }


        TEST_METHOD (AutoHideAndRestore)
        {
            DxuiPaneLayout  layout = MakeSample();



            Assert::IsTrue  (layout.AutoHide (L"console", DxuiDockSide::Bottom));
            Assert::IsTrue  (layout.IsAutoHidden (L"console"));
            Assert::AreEqual ((size_t) 2, layout.Arrange (s_kArea, nullptr, nullptr).size());
            Assert::IsFalse (layout.AutoHide (L"console", DxuiDockSide::Left), L"already hidden");
            Assert::IsTrue  (layout.DockBack (L"console"));
            Assert::IsTrue  (layout.IsDocked (L"console"));
        }


        TEST_METHOD (NestedTabGroupsInSplits)
        {
            DxuiPaneLayout  layout = MakeSample();



            layout.Add        (L"stack", L"regs");
            layout.Add        (L"watch", L"");
            layout.DockToSide (L"watch", L"stack", DxuiDockSide::Top);

            Assert::AreEqual ((size_t) 2, layout.GetGroup (L"regs").size());
            Assert::AreEqual ((size_t) 4, layout.Arrange (s_kArea, nullptr, nullptr).size());
        }


        TEST_METHOD (ArrowMovesToTheGroupInThatDirection)
        {
            DxuiPaneLayout  layout = MakeSample();



            Assert::IsTrue   (layout.MoveByArrow (L"console", DxuiDockSide::Right, s_kArea, nullptr, nullptr));
            Assert::AreEqual ((size_t) 2, layout.GetGroup (L"regs").size(), L"tabbed into regs");
        }


        TEST_METHOD (ArrowWithNothingThereDocksToTheEdge)
        {
            DxuiPaneLayout                          layout = MakeSample();
            std::vector<DxuiPaneLayout::GroupRect>  groups;



            Assert::IsTrue (layout.MoveByArrow (L"code", DxuiDockSide::Top, s_kArea, nullptr, nullptr));
            groups = layout.Arrange (s_kArea, nullptr, nullptr);
            Assert::AreEqual ((long) 0,    FindRect (groups, L"code").rect.top);
            Assert::AreEqual ((long) 1000, FindRect (groups, L"code").rect.right, L"the whole width");
            Assert::IsFalse (layout.MoveByArrow (L"code", DxuiDockSide::Top, s_kArea, nullptr, nullptr), L"already against it");
        }


        TEST_METHOD (CloseCollapsesTheSplit)
        {
            DxuiPaneLayout  layout = MakeSample();



            Assert::IsTrue   (layout.Close (L"console"));
            Assert::AreEqual ((size_t) 2, layout.Arrange (s_kArea, nullptr, nullptr).size());
            Assert::AreEqual ((long) 600, FindRect (layout.Arrange (s_kArea, nullptr, nullptr), L"code").rect.bottom);
            Assert::IsFalse  (layout.Close (L"console"));
        }
    };



    TEST_CLASS (ArrangeTests)
    {
    public:

        TEST_METHOD (RatiosYieldToEachPanesMinimum)
        {
            DxuiPaneLayout  layout = MakeSample();
            auto            minSize = [] (const std::wstring & pane) { return (pane == L"regs") ? SIZE { 700, 0 } : SIZE { 100, 100 }; };



            Assert::AreEqual ((long) 700, 1000 - FindRect (layout.Arrange (s_kArea, nullptr, minSize), L"regs").rect.left,
                              L"regs gets its 700 though the ratio says 500");
        }


        TEST_METHOD (MinimumsThatDoNotFitShareInProportion)
        {
            DxuiPaneLayout  layout  = MakeSample();
            auto            minSize = [] (const std::wstring &) { return SIZE { 800, 0 }; };



            Assert::AreEqual ((long) 500, FindRect (layout.Arrange (s_kArea, nullptr, minSize), L"regs").rect.left);
        }


        TEST_METHOD (AHiddenPaneKeepsItsPlaceAndGivesUpItsArea)
        {
            DxuiPaneLayout  layout = MakeSample();
            auto            shown  = [] (const std::wstring & pane) { return pane != L"console"; };



            Assert::AreEqual ((long) 600, FindRect (layout.Arrange (s_kArea, shown, nullptr), L"code").rect.bottom);
            Assert::IsTrue   (layout.IsDocked (L"console"), L"still in the tree, for when it is shown again");
            Assert::AreEqual ((long) 300, FindRect (layout.Arrange (s_kArea, nullptr, nullptr), L"console").rect.top);
        }


        TEST_METHOD (SplitsAreWhereArrangeDividesTheArea)
        {
            DxuiPaneLayout                            layout = MakeSample();
            std::vector<DxuiPaneLayout::SplitRect>    splits = layout.ArrangeSplits (s_kArea, nullptr, nullptr);
            std::vector<DxuiPaneLayout::GroupRect>    groups = layout.Arrange (s_kArea, nullptr, nullptr);



            Assert::AreEqual ((size_t) 2, splits.size());
            Assert::IsTrue   (splits[0].horizontal, L"the root: code and console | regs");
            Assert::AreEqual (std::wstring (L""), splits[0].path);
            Assert::AreEqual (FindRect (groups, L"regs").rect.left, splits[0].position);
            Assert::AreEqual (std::wstring (L"0"), splits[1].path, L"the root's first side");
            Assert::AreEqual (FindRect (groups, L"console").rect.top, splits[1].position);
        }


        TEST_METHOD (ASplitWithAHiddenSideHasNothingToDrag)
        {
            DxuiPaneLayout  layout = MakeSample();
            auto            shown  = [] (const std::wstring & pane) { return pane != L"console"; };



            Assert::AreEqual ((size_t) 1, layout.ArrangeSplits (s_kArea, shown, nullptr).size());
        }


        TEST_METHOD (SetRatioMovesTheSplitWithinLimits)
        {
            DxuiPaneLayout  layout = MakeSample();



            Assert::IsTrue   (layout.SetRatio (L"", 0.8f));
            Assert::AreEqual ((long) 800, layout.ArrangeSplits (s_kArea, nullptr, nullptr)[0].position);
            Assert::IsTrue   (layout.SetRatio (L"", 2.0f));
            Assert::AreEqual ((long) 950, layout.ArrangeSplits (s_kArea, nullptr, nullptr)[0].position, L"clamped");
            Assert::IsFalse  (layout.SetRatio (L"00", 0.5f), L"a group, not a split");
            Assert::IsFalse  (layout.SetRatio (L"011", 0.5f));
        }


        TEST_METHOD (AHiddenActiveTabShowsTheNextOne)
        {
            DxuiPaneLayout  layout = MakeSample();
            auto            shown  = [] (const std::wstring & pane) { return pane != L"regs"; };



            layout.TabWith   (L"regs", L"code");
            Assert::AreEqual (std::wstring (L"code"), FindRect (layout.Arrange (s_kArea, shown, nullptr), L"code").active);
        }
    };



    TEST_CLASS (PersistenceTests)
    {
    public:

        TEST_METHOD (TheTextRoundTrips)
        {
            DxuiPaneLayout  layout = MakeSample();
            DxuiPaneLayout  read;



            layout.Add      (L"memory \"2\"", L"regs");
            layout.Float    (L"console", L"MON\\2", RECT { 10, 20, 310, 220 });
            layout.Add      (L"trace", L"");
            layout.AutoHide (L"trace", DxuiDockSide::Left);

            Assert::IsTrue   (DxuiPaneLayout::TryParse (layout.ToText(), read));
            Assert::AreEqual (layout.ToText(), read.ToText());
            Assert::IsTrue   (read.IsDocked (L"memory \"2\""), L"a name with a quote");
            Assert::AreEqual (std::wstring (L"MON\\2"), read.GetFloating().at (0).monitorKey);
            Assert::AreEqual ((long) 310, read.GetFloating().at (0).rectDip.right);
        }


        TEST_METHOD (TheTextCarriesItsVersion)
        {
            Assert::AreEqual ((size_t) 0, MakeSample().ToText().find (L"dxui-layout 1\n"));
        }


        TEST_METHOD (AnotherVersionOrMalformedTextIsRefused)
        {
            DxuiPaneLayout  read  = MakeSample();
            std::wstring    good  = read.ToText();
            std::wstring    other = good;



            other.replace (0, 13, L"dxui-layout 9");

            Assert::IsFalse  (DxuiPaneLayout::TryParse (other, read));
            Assert::IsFalse  (DxuiPaneLayout::TryParse (L"", read));
            Assert::IsFalse  (DxuiPaneLayout::TryParse (L"dxui-layout 1\ntree (split h 0.5 (tabs 0 \"a\")", read), L"unclosed");
            Assert::IsFalse  (DxuiPaneLayout::TryParse (L"dxui-layout 1\ntree (tabs 0)", read), L"an empty group");
            Assert::IsFalse  (DxuiPaneLayout::TryParse (L"dxui-layout 1\ntree (tabs 0 \"a\" \"a\")", read), L"a pane twice");
            Assert::IsFalse  (DxuiPaneLayout::TryParse (L"dxui-layout 1\ntree none\nwobble", read), L"an unknown line");
            Assert::AreEqual (good, read.ToText(), L"a refused text leaves the layout as it was");
        }


        TEST_METHOD (AnUnknownPaneIsDroppedOnLoad)
        {
            DxuiPaneLayout  layout = MakeSample();



            layout.Float       (L"regs", L"mon1", RECT { 0, 0, 100, 100 });
            layout.DropUnknown ([] (const std::wstring & pane) { return pane != L"console" && pane != L"regs"; });

            Assert::IsFalse  (layout.Contains (L"console"));
            Assert::IsFalse  (layout.Contains (L"regs"));
            Assert::AreEqual ((size_t) 1, layout.Arrange (s_kArea, nullptr, nullptr).size());
        }


        TEST_METHOD (AFloatingPaneOnAMissingMonitorOpensOnThePrimary)
        {
            DxuiPaneLayout                         layout = MakeSample();
            std::vector<DxuiPaneLayout::Monitor>   topology;



            layout.Float (L"regs", L"gone", RECT { 3000, 100, 3400, 400 });
            layout.Float (L"console", L"side", RECT { -1800, 50, -1500, 250 });
            topology.push_back ({ L"side", RECT { -1920, 0, 0, 1040 },  false });
            topology.push_back ({ L"main", RECT { 0, 0, 1920, 1040 },   true  });

            layout.PlaceOnMonitors (topology);

            Assert::AreEqual (std::wstring (L"main"), layout.GetFloating().at (0).monitorKey);
            Assert::AreEqual ((long) 0,   layout.GetFloating().at (0).rectDip.left);
            Assert::AreEqual ((long) 400, layout.GetFloating().at (0).rectDip.right,  L"at its saved size");
            Assert::AreEqual ((long) 300, layout.GetFloating().at (0).rectDip.bottom);
            Assert::AreEqual ((long) -1800, layout.GetFloating().at (1).rectDip.left, L"a present monitor is left alone");
        }
    };
}
